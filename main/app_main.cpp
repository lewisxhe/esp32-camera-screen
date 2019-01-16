/* ESPRESSIF MIT License
 *
 * Copyright (c) 2018 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
 *
 * Permission is hereby granted for use on all ESPRESSIF SYSTEMS products, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "app_main.h"
#include "esp_partition.h"

#include "esp_log.h"
#include "image_util.h"
#include "fb_gfx.h"

#include "app_screen.h"
// #include "image.h"

// #include <errno.h>
// #include <sys/stat.h>
// #include <string.h>
// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"
// #include "esp_system.h"
#include "esp_log.h"
// #include "lwip/sockets.h"

static const char *TAG = "main";

en_fsm_state g_state = WAIT_FOR_WAKEUP;
int g_is_enrolling = 0;
int g_is_deleting = 0;


extern CEspLcd *tft;

#if 1


#define FACE_COLOR_WHITE  0x00FFFFFF
#define FACE_COLOR_BLACK  0x00000000
#define FACE_COLOR_RED    0x000000FF
#define FACE_COLOR_GREEN  0x0000FF00
#define FACE_COLOR_BLUE   0x00FF0000
#define FACE_COLOR_YELLOW (FACE_COLOR_RED | FACE_COLOR_GREEN)
#define FACE_COLOR_CYAN   (FACE_COLOR_BLUE | FACE_COLOR_GREEN)
#define FACE_COLOR_PURPLE (FACE_COLOR_BLUE | FACE_COLOR_RED)

#define ENROLL_CONFIRM_TIMES    3
#define FACE_ID_SAVE_NUMBER     10


face_id_list st_face_list = {0};
dl_matrix3du_t *aligned_face = NULL;

/**
 * @brief  Face
 * @note
 * @param  number:
 * @retval
 */
static const char *number_suffix(int32_t number)
{
    uint8_t n = number % 10;

    if (n == 0)
        return "zero";
    else if (n == 1)
        return "st";
    else if (n == 2)
        return "nd";
    else if (n == 3)
        return "rd";
    else
        return "th";
}

static void rgb_print(dl_matrix3du_t *image_matrix, uint32_t color, const char *str)
{
    fb_data_t fb;
    fb.width = image_matrix->w;
    fb.height = image_matrix->h;
    fb.data = image_matrix->item;
    fb.bytes_per_pixel = 3;
    fb.format = FB_BGR888;
    fb_gfx_print(&fb, (fb.width - (strlen(str) * 14)) / 2, 10, color, str);
}

static int rgb_printf(dl_matrix3du_t *image_matrix, uint32_t color, const char *format, ...)
{
    char loc_buf[64];
    char *temp = loc_buf;
    int len;
    va_list arg;
    va_list copy;
    va_start(arg, format);
    va_copy(copy, arg);
    len = vsnprintf(loc_buf, sizeof(loc_buf), format, arg);
    va_end(copy);
    if (len >= sizeof(loc_buf)) {
        temp = (char *)malloc(len + 1);
        if (temp == NULL) {
            return 0;
        }
    }
    vsnprintf(temp, len + 1, format, arg);
    va_end(arg);
    rgb_print(image_matrix, color, temp);
    if (len > 64) {
        free(temp);
    }
    return len;
}

static void draw_face_boxes(dl_matrix3du_t *image_matrix, box_array_t *boxes)
{
    int x, y, w, h, i;
    uint32_t color = FACE_COLOR_YELLOW;
    fb_data_t fb;
    fb.width = image_matrix->w;
    fb.height = image_matrix->h;
    fb.data = image_matrix->item;
    fb.bytes_per_pixel = 3;
    fb.format = FB_BGR888;
    for (i = 0; i < boxes->len; i++) {
        // rectangle box
        x = (int)boxes->box[i].box_p[0];
        y = (int)boxes->box[i].box_p[1];
        w = (int)boxes->box[i].box_p[2] - x + 1;
        h = (int)boxes->box[i].box_p[3] - y + 1;
        fb_gfx_drawFastHLine(&fb, x, y, w, color);
        fb_gfx_drawFastHLine(&fb, x, y + h - 1, w, color);
        fb_gfx_drawFastVLine(&fb, x, y, h, color);
        fb_gfx_drawFastVLine(&fb, x + w - 1, y, h, color);
    }
}

#endif



static void facenet_stream(void)
{
    esp_err_t res = ESP_OK;
    g_state = START_DETECT;
    int16_t __height, __witdh;
    camera_fb_t *fb = NULL;
    size_t _jpg_buf_len = 0;
    uint8_t *_jpg_buf = NULL;
    dl_matrix3du_t *image_matrix = NULL;

    int face_id = -1;

    // int64_t fr_start = 0;
    // int64_t fr_ready = 0;
    // int64_t fr_face = 0;
    // int64_t fr_recognize = 0;
    // int64_t fr_encode = 0;

    mtmn_config_t mtmn_config = mtmn_init_config();

    static int64_t last_frame = 0;
    if (!last_frame) {
        last_frame = esp_timer_get_time();
    }

    ESP_LOGI(TAG, "Get count %d\n", st_face_list.count);

    while (true) {
        // update fsm state
        if (g_is_enrolling) {
            g_state = START_ENROLL;
        } else if (g_is_deleting) {
            g_is_deleting = 0;
            g_state = START_DELETE;
        } else if (g_state != START_ENROLL) {
            if (st_face_list.count == 0)
                g_state = START_DETECT;
            else
                g_state = START_RECOGNITION;
        }

        ESP_LOGD(TAG, "State: %d, head:%d, tail:%d, count:%d", g_state, st_face_list.head, st_face_list.tail, st_face_list.count);
        // exec event
        if (g_state == START_DELETE) {
            uint8_t left = delete_face_id_in_flash(&st_face_list);
            ESP_LOGW(TAG, "%d ID Left", left);
            g_state = START_DETECT;
            continue;
        }

        // Detection Start
        fb = esp_camera_fb_get();
        if (!fb) {
            ESP_LOGE(TAG, "Camera capture failed");
            res = ESP_FAIL;
            break;
        }

        __height = fb->height;
        __witdh = fb->width;
        // fr_start = esp_timer_get_time();
        // fr_ready = fr_start;
        // fr_face = fr_start;
        // fr_encode = fr_start;
        // fr_recognize = fr_start;
        image_matrix = dl_matrix3du_alloc(1, fb->width, fb->height, 3);
        if (!image_matrix) {
            ESP_LOGE(TAG, "dl_matrix3du_alloc failed");
            res = ESP_FAIL;
            break;
        }

        if (!fmt2rgb888(fb->buf, fb->len, fb->format, image_matrix->item)) {
            ESP_LOGW(TAG, "fmt2rgb888 failed");
            //res = ESP_FAIL;
            //dl_matrix3du_free(image_matrix);
            //break;
        }

        // fr_ready = esp_timer_get_time();
        box_array_t *net_boxes = face_detect(image_matrix, &mtmn_config);
        // fr_face = esp_timer_get_time();
        // Detection End

        // fr_recognize = fr_face;
        if (net_boxes) {
            ESP_LOGI(TAG, "g_state : %u ", g_state);
            if ((g_state == START_ENROLL || g_state == START_RECOGNITION)
                    && (align_face(net_boxes, image_matrix, aligned_face) == ESP_OK)) {
                if (g_state == START_ENROLL) {
                    rgb_print(image_matrix, FACE_COLOR_YELLOW, "START ENROLLING");
                    ESP_LOGD(TAG, "START ENROLLING");

                    int left_sample_face = enroll_face_id_to_flash(&st_face_list, aligned_face);
                    ESP_LOGD(TAG, "Face ID %d Enrollment: Taken the %d%s sample",
                             st_face_list.tail,
                             ENROLL_CONFIRM_TIMES - left_sample_face,
                             number_suffix(ENROLL_CONFIRM_TIMES - left_sample_face));
                    // gpio_set_level(GPIO_LED_RED, 0);
                    rgb_printf(image_matrix, FACE_COLOR_CYAN, "\nThe %u%s sample",
                               ENROLL_CONFIRM_TIMES - left_sample_face,
                               number_suffix(ENROLL_CONFIRM_TIMES - left_sample_face));

                    if (left_sample_face == 0) {
                        ESP_LOGI(TAG, "Enrolled Face ID: %d", st_face_list.tail);
                        rgb_printf(image_matrix, FACE_COLOR_CYAN, "\n\nEnrolled Face ID: %d", st_face_list.tail);
                        g_is_enrolling = 0;
                        g_state = START_RECOGNITION;
                    }
                } else {
                    face_id = recognize_face(&st_face_list, aligned_face);

                    if (face_id >= 0) {
                        // gpio_set_level(GPIO_LED_RED, 1);
                        rgb_printf(image_matrix, FACE_COLOR_GREEN, "Hello ID %u", face_id);
                        ESP_LOGI(TAG, "Hello ID %u", face_id);
                    } else {
                        rgb_print(image_matrix, FACE_COLOR_RED, "\nWHO?");
                        ESP_LOGI(TAG, "Who ? ");
                    }
                }
            }
            draw_face_boxes(image_matrix, net_boxes);
            free(net_boxes->box);
            free(net_boxes->landmark);
            free(net_boxes);

            // fr_recognize = esp_timer_get_time();
            if (!fmt2jpg(image_matrix->item, fb->width * fb->height * 3, fb->width, fb->height, PIXFORMAT_RGB888, 90, &_jpg_buf, &_jpg_buf_len)) {
                ESP_LOGE(TAG, "fmt2jpg failed");
                dl_matrix3du_free(image_matrix);
                res = ESP_FAIL;
            }
            esp_camera_fb_return(fb);
            fb = NULL;
        } else {
            _jpg_buf = fb->buf;
            _jpg_buf_len = fb->len;
            //
            // tft->drawBitmapnotswap(0, 0, (const uint16_t *)_jpg_buf, __witdh, __height);
        }
        dl_matrix3du_free(image_matrix);
        // fr_encode = esp_timer_get_time();

        TFT_jpg_image(CENTER, CENTER, 0, _jpg_buf, _jpg_buf_len);
        // uint8_t *out = NULL;
        // size_t outlen = 0;
        // if (fmt2bmp(_jpg_buf, _jpg_buf_len, __witdh, __height, PIXFORMAT_JPEG, &out, &outlen)) {
        //     tft->drawBitmapnotswap(0, 0, (const uint16_t *)out, __witdh, __height);
        //     free(out);
        //     out = NULL;
        // }


        if (fb) {
            esp_camera_fb_return(fb);
            fb = NULL;
            _jpg_buf = NULL;
        } else if (_jpg_buf) {
            free(_jpg_buf);
            _jpg_buf = NULL;
        }
        if (res != ESP_OK) {
            break;
        }
        // int64_t fr_end = esp_timer_get_time();

        // int64_t ready_time = (fr_ready - fr_start) / 1000;
        // int64_t face_time = (fr_face - fr_ready) / 1000;
        // int64_t recognize_time = (fr_recognize - fr_face) / 1000;
        // int64_t encode_time = (fr_encode - fr_recognize) / 1000;
        // int64_t process_time = (fr_encode - fr_start) / 1000;

        // int64_t frame_time = fr_end - last_frame;
        // last_frame = fr_end;
        // frame_time /= 1000;
        // ESP_LOGD(TAG, "MJPG: %uKB %ums (%.1ffps), %u+%u+%u+%u=%u",
        //          (uint32_t)(_jpg_buf_len / 1024),
        //          (uint32_t)frame_time, 1000.0 / (uint32_t)frame_time,
        //          (uint32_t)ready_time, (uint32_t)face_time, (uint32_t)recognize_time, (uint32_t)encode_time, (uint32_t)process_time);
    }

    last_frame = 0;
    g_state = WAIT_FOR_WAKEUP;
}



void test_camera()
{
    while (1) {
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) {
            ESP_LOGE(TAG, "Camera capture failed");
        } else {
            TFT_jpg_image(CENTER, CENTER, 0, fb->buf, fb->len);
            // tft->drawBitmapnotswap(0, 0, (const uint16_t *)fb->buf, (int16_t)fb->width, (int16_t)fb->height);
            esp_camera_fb_return(fb);
            fb = NULL;
        }
    }
}

void app_lcd_task(void *pvParameters)
{
    test_camera();
    // facenet_stream();
}

extern "C"  void darwPicture(uint8_t *buf, int16_t width, int height)
{
    tft->drawBitmapnotswap(0, 0, (const uint16_t *)buf, (int16_t)width, (int16_t)height);
}

extern "C"  void app_main()
{
    app_lcd_init();

    app_speech_wakeup_init();

    g_state = WAIT_FOR_WAKEUP;

    vTaskDelay(30 / portTICK_PERIOD_MS);

    tft->drawString("Please say 'Hi LeXin' to the board", 0, 30);

    ESP_LOGI("esp-eye", "Version "VERSION);

    while (g_state == WAIT_FOR_WAKEUP)
        vTaskDelay(1000 / portTICK_PERIOD_MS);

    app_wifi_init();

    app_camera_init();

    xTaskCreatePinnedToCore(app_lcd_task, "app_lcd_task", 4096, NULL, 4, NULL, 0);

}
