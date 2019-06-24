#include "app_main.h"
#include "esp_partition.h"
#include "esp_log.h"
#include "image_util.h"
#include "fb_gfx.h"
#include "app_screen.h"
#include "esp_log.h"
#include "app_sd.h"
#include "app_sensor.h"
#include "iot_lvgl.h"
#include "app_httpd.h"

static const char *TAG = "[main]";

extern const uint8_t image_jpg_start[]   asm("_binary_plus_jpg_start");
#define IMAGES_SIZE 28827

en_fsm_state g_state = WAIT_FOR_WAKEUP;

extern CEspLcd *tft;
static struct bme280_dev dev;
EventGroupHandle_t evGroup;

//*********************************************************
//*********************************************************
//*********************************************************
/*
Turn on the FACE_DETECT_IN_SCREEN macro,
face recognition will be displayed in the display,
and the microphone will be disabled. The web page will not be viewable.
Only face input and camera parameters can be adjusted.
*/
// #define FACE_DETECT_IN_SCREEN

// #define ENABLE_BME280   //! Turning on the macro will launch the GUI and BME280 sensor



#ifdef FACE_DETECT_IN_SCREEN
#include "fd_forward.h"
#include "dl_lib.h"
#include "fr_forward.h"
#include "camera_index.h"
#include "esp_http_server.h"

#define ENROLL_CONFIRM_TIMES 5
#define FACE_ID_SAVE_NUMBER 7

#define FACE_COLOR_WHITE  0x00FFFFFF
#define FACE_COLOR_BLACK  0x00000000
#define FACE_COLOR_RED    0x000000FF
#define FACE_COLOR_GREEN  0x0000FF00
#define FACE_COLOR_BLUE   0x00FF0000
#define FACE_COLOR_YELLOW (FACE_COLOR_RED | FACE_COLOR_GREEN)
#define FACE_COLOR_CYAN   (FACE_COLOR_BLUE | FACE_COLOR_GREEN)
#define FACE_COLOR_PURPLE (FACE_COLOR_BLUE | FACE_COLOR_RED)


typedef struct {
    size_t size; //number of values used for filtering
    size_t index; //current value index
    size_t count; //value count
    int sum;
    int *values;  //array to be filled with values
} ra_filter_t;


static httpd_handle_t camera_httpd = NULL;
static mtmn_config_t mtmn_config;
static face_id_list id_list;
static int8_t detection_enabled = 1;
static int8_t recognition_enabled = 1;
static int8_t is_enrolling = 0;
static ra_filter_t ra_filter;

static ra_filter_t *ra_filter_init(ra_filter_t *filter, size_t sample_size)
{
    memset(&id_list, 0, sizeof(id_list));
    memset(&mtmn_config, 0, sizeof(mtmn_config));

    mtmn_config.min_face = 80;
    mtmn_config.pyramid = 0.7;
    mtmn_config.p_threshold.score = 0.6;
    mtmn_config.p_threshold.nms = 0.7;
    mtmn_config.r_threshold.score = 0.7;
    mtmn_config.r_threshold.nms = 0.7;
    mtmn_config.r_threshold.candidate_number = 4;
    mtmn_config.o_threshold.score = 0.7;
    mtmn_config.o_threshold.nms = 0.4;
    mtmn_config.o_threshold.candidate_number = 1;

    face_id_init(&id_list, FACE_ID_SAVE_NUMBER, ENROLL_CONFIRM_TIMES);

    memset(filter, 0, sizeof(ra_filter_t));

    filter->values = (int *)malloc(sample_size * sizeof(int));
    if (!filter->values) {
        free(filter);
        return NULL;
    }
    memset(filter->values, 0, sample_size * sizeof(int));

    filter->size = sample_size;
    return filter;
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
static void draw_face_boxes(dl_matrix3du_t *image_matrix, box_array_t *boxes, int face_id)
{
    int x, y, w, h, i;
    uint32_t color = FACE_COLOR_YELLOW;
    if (face_id < 0) {
        color = FACE_COLOR_RED;
    } else if (face_id > 0) {
        color = FACE_COLOR_GREEN;
    }
    fb_data_t fb;
    fb.width = image_matrix->w;
    fb.height = image_matrix->h;
    fb.data = image_matrix->item;
    fb.bytes_per_pixel = 3;
    fb.format = FB_BGR888;
    for (i = 0; i < boxes->len; i++) {
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

static int run_face_recognition(dl_matrix3du_t *image_matrix, box_array_t *net_boxes)
{
    dl_matrix3du_t *aligned_face = NULL;
    int matched_id = 0;

    aligned_face = dl_matrix3du_alloc(1, FACE_WIDTH, FACE_HEIGHT, 3);
    if (!aligned_face) {
        ESP_LOGE(TAG, "Could not allocate face recognition buffer");
        return matched_id;
    }
    if (align_face(net_boxes, image_matrix, aligned_face) == ESP_OK) {
        if (is_enrolling == 1) {
            int8_t left_sample_face = enroll_face(&id_list, aligned_face);

            if (left_sample_face == (ENROLL_CONFIRM_TIMES - 1)) {
                ESP_LOGD(TAG, "Enrolling Face ID: %d", id_list.tail);
            }
            ESP_LOGD(TAG, "Enrolling Face ID: %d sample %d", id_list.tail, ENROLL_CONFIRM_TIMES - left_sample_face);
            rgb_printf(image_matrix, FACE_COLOR_CYAN, "ID[%u] Sample[%u]", id_list.tail, ENROLL_CONFIRM_TIMES - left_sample_face);
            if (left_sample_face == 0) {
                is_enrolling = 0;
                ESP_LOGD(TAG, "Enrolled Face ID: %d", id_list.tail);
            }
        } else {
            matched_id = recognize_face(&id_list, aligned_face);
            if (matched_id >= 0) {
                ESP_LOGW(TAG, "Match Face ID: %u", matched_id);
                rgb_printf(image_matrix, FACE_COLOR_GREEN, "Hello id %u", matched_id);
            } else {
                ESP_LOGW(TAG, "No Match Found");
                rgb_print(image_matrix, FACE_COLOR_RED, "Who are you?");
                matched_id = -1;
            }
        }
    } else {
        ESP_LOGW(TAG, "Face Not Aligned");
    }

    dl_matrix3du_free(aligned_face);
    return matched_id;
}



static esp_err_t stream_handler()
{
    camera_fb_t *fb = NULL;
    esp_err_t res = ESP_OK;
    size_t _jpg_buf_len = 0;
    uint8_t *_jpg_buf = NULL;
    dl_matrix3du_t *image_matrix = NULL;
    int face_id = 0;

    // xEventGroupClearBits(evGroup, 1);

    // while(xEventGroupGetBits(evGroup) != 0)
    // {
    //     vTaskDelay(100/portTICK_PERIOD_MS);
    // }

    while (1) {
        face_id = 0;
        fb = esp_camera_fb_get();
        if (!fb) {
            ESP_LOGE(TAG, "Camera capture failed");
            res = ESP_FAIL;
        } else {

            if (!detection_enabled || fb->width > 400) {
                if (fb->format != PIXFORMAT_JPEG) {
                    bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
                    esp_camera_fb_return(fb);
                    fb = NULL;
                    if (!jpeg_converted) {
                        ESP_LOGE(TAG, "JPEG compression failed");
                        res = ESP_FAIL;
                    }
                } else {
                    _jpg_buf_len = fb->len;
                    _jpg_buf = fb->buf;
                }
            } else {

                image_matrix = dl_matrix3du_alloc(1, fb->width, fb->height, 3);

                if (!image_matrix) {
                    ESP_LOGE(TAG, "dl_matrix3du_alloc failed");
                    res = ESP_FAIL;
                } else {
                    if (!fmt2rgb888(fb->buf, fb->len, fb->format, image_matrix->item)) {
                        ESP_LOGE(TAG, "fmt2rgb888 failed");
                        res = ESP_FAIL;
                    } else {
                        box_array_t *net_boxes = NULL;
                        if (detection_enabled) {
                            net_boxes = face_detect(image_matrix, &mtmn_config);
                        }
                        if (net_boxes || fb->format != PIXFORMAT_JPEG) {
                            if (net_boxes) {
                                if (recognition_enabled) {
                                    face_id = run_face_recognition(image_matrix, net_boxes);
                                }
                                draw_face_boxes(image_matrix, net_boxes, face_id);
                                free(net_boxes->box);
                                free(net_boxes->landmark);
                                free(net_boxes);
                            }
                            if (!fmt2jpg(image_matrix->item, fb->width * fb->height * 3, fb->width, fb->height, PIXFORMAT_RGB888, 90, &_jpg_buf, &_jpg_buf_len)) {
                                ESP_LOGE(TAG, "fmt2jpg failed");
                                res = ESP_FAIL;
                            }
                            esp_camera_fb_return(fb);
                            fb = NULL;
                        } else {
                            _jpg_buf = fb->buf;
                            _jpg_buf_len = fb->len;
                        }
                    }
                    dl_matrix3du_free(image_matrix);
                }
            }
        }
        if (res != ESP_FAIL)

#ifndef ENABLE_BME280
            TFT_jpg_image(CENTER, 0, 0, -1, NULL, _jpg_buf, _jpg_buf_len);
#else
            TFT_jpg_image(CENTER, 50, 0, -1, NULL, _jpg_buf, _jpg_buf_len);
#endif

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
    }
    return res;
}

static esp_err_t cmd_handler(httpd_req_t *req)
{
    char  *buf;
    size_t buf_len;
    char variable[32] = {0,};
    char value[32] = {0,};

    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = (char *)malloc(buf_len);
        if (!buf) {
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            if (httpd_query_key_value(buf, "var", variable, sizeof(variable)) == ESP_OK &&
                    httpd_query_key_value(buf, "val", value, sizeof(value)) == ESP_OK) {
            } else {
                free(buf);
                httpd_resp_send_404(req);
                return ESP_FAIL;
            }
        } else {
            free(buf);
            httpd_resp_send_404(req);
            return ESP_FAIL;
        }
        free(buf);
    } else {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    int val = atoi(value);
    ESP_LOGI(TAG, "%s = %d", variable, val);
    sensor_t *s = esp_camera_sensor_get();
    int res = 0;

    if (!strcmp(variable, "framesize")) {
        if (s->pixformat == PIXFORMAT_JPEG) res = s->set_framesize(s, (framesize_t)val);
    } else if (!strcmp(variable, "quality")) res = s->set_quality(s, val);
    else if (!strcmp(variable, "contrast")) res = s->set_contrast(s, val);
    else if (!strcmp(variable, "brightness")) res = s->set_brightness(s, val);
    else if (!strcmp(variable, "saturation")) res = s->set_saturation(s, val);
    else if (!strcmp(variable, "gainceiling")) res = s->set_gainceiling(s, (gainceiling_t)val);
    else if (!strcmp(variable, "colorbar")) res = s->set_colorbar(s, val);
    else if (!strcmp(variable, "awb")) res = s->set_whitebal(s, val);
    else if (!strcmp(variable, "agc")) res = s->set_gain_ctrl(s, val);
    else if (!strcmp(variable, "aec")) res = s->set_exposure_ctrl(s, val);
    else if (!strcmp(variable, "hmirror")) res = s->set_hmirror(s, val);
    else if (!strcmp(variable, "vflip")) res = s->set_vflip(s, val);
    else if (!strcmp(variable, "awb_gain")) res = s->set_awb_gain(s, val);
    else if (!strcmp(variable, "agc_gain")) res = s->set_agc_gain(s, val);
    else if (!strcmp(variable, "aec_value")) res = s->set_aec_value(s, val);
    else if (!strcmp(variable, "aec2")) res = s->set_aec2(s, val);
    else if (!strcmp(variable, "dcw")) res = s->set_dcw(s, val);
    else if (!strcmp(variable, "bpc")) res = s->set_bpc(s, val);
    else if (!strcmp(variable, "wpc")) res = s->set_wpc(s, val);
    else if (!strcmp(variable, "raw_gma")) res = s->set_raw_gma(s, val);
    else if (!strcmp(variable, "lenc")) res = s->set_lenc(s, val);
    else if (!strcmp(variable, "special_effect")) res = s->set_special_effect(s, val);
    else if (!strcmp(variable, "wb_mode")) res = s->set_wb_mode(s, val);
    else if (!strcmp(variable, "ae_level")) res = s->set_ae_level(s, val);

    else if (!strcmp(variable, "face_detect")) {
        detection_enabled = val;
        if (!detection_enabled) {
            recognition_enabled = 0;
        }
    } else if (!strcmp(variable, "face_enroll")) is_enrolling = val;
    else if (!strcmp(variable, "face_recognize")) {
        recognition_enabled = val;
        if (recognition_enabled) {
            detection_enabled = val;
        }
    } else {
        res = -1;
    }

    if (res) {
        return httpd_resp_send_500(req);
    }
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, NULL, 0);
}

static esp_err_t status_handler(httpd_req_t *req)
{
    static char json_response[1024];

    sensor_t *s = esp_camera_sensor_get();
    char *p = json_response;
    *p++ = '{';
    p += sprintf(p, "\"framesize\":%u,", s->status.framesize);
    p += sprintf(p, "\"quality\":%u,", s->status.quality);
    p += sprintf(p, "\"brightness\":%d,", s->status.brightness);
    p += sprintf(p, "\"contrast\":%d,", s->status.contrast);
    p += sprintf(p, "\"saturation\":%d,", s->status.saturation);
    p += sprintf(p, "\"special_effect\":%u,", s->status.special_effect);
    p += sprintf(p, "\"wb_mode\":%u,", s->status.wb_mode);
    p += sprintf(p, "\"awb\":%u,", s->status.awb);
    p += sprintf(p, "\"awb_gain\":%u,", s->status.awb_gain);
    p += sprintf(p, "\"aec\":%u,", s->status.aec);
    p += sprintf(p, "\"aec2\":%u,", s->status.aec2);
    p += sprintf(p, "\"ae_level\":%d,", s->status.ae_level);
    p += sprintf(p, "\"aec_value\":%u,", s->status.aec_value);
    p += sprintf(p, "\"agc\":%u,", s->status.agc);
    p += sprintf(p, "\"agc_gain\":%u,", s->status.agc_gain);
    p += sprintf(p, "\"gainceiling\":%u,", s->status.gainceiling);
    p += sprintf(p, "\"bpc\":%u,", s->status.bpc);
    p += sprintf(p, "\"wpc\":%u,", s->status.wpc);
    p += sprintf(p, "\"raw_gma\":%u,", s->status.raw_gma);
    p += sprintf(p, "\"lenc\":%u,", s->status.lenc);
    p += sprintf(p, "\"hmirror\":%u,", s->status.hmirror);
    p += sprintf(p, "\"dcw\":%u,", s->status.dcw);
    p += sprintf(p, "\"colorbar\":%u", s->status.colorbar);
    p += sprintf(p, ",\"face_detect\":%u", detection_enabled);
    p += sprintf(p, ",\"face_enroll\":%u,", is_enrolling);
    p += sprintf(p, "\"face_recognize\":%u", recognition_enabled);
    *p++ = '}';
    *p++ = 0;
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, json_response, strlen(json_response));
}

static esp_err_t index_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    return httpd_resp_send(req, (const char *)index_html_gz, index_html_gz_len);
}

static void app_mhttpd_main()
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    httpd_uri_t index_uri = {
        .uri       = "/",
        .method    = HTTP_GET,
        .handler   = index_handler,
        .user_ctx  = NULL
    };

    httpd_uri_t status_uri = {
        .uri       = "/status",
        .method    = HTTP_GET,
        .handler   = status_handler,
        .user_ctx  = NULL
    };

    httpd_uri_t cmd_uri = {
        .uri       = "/control",
        .method    = HTTP_GET,
        .handler   = cmd_handler,
        .user_ctx  = NULL
    };

    ESP_LOGI(TAG, "Starting web server on port: '%d'", config.server_port);
    if (httpd_start(&camera_httpd, &config) == ESP_OK) {
        httpd_register_uri_handler(camera_httpd, &index_uri);
        httpd_register_uri_handler(camera_httpd, &cmd_uri);
        httpd_register_uri_handler(camera_httpd, &status_uri);
    }
}
#endif /*FACE_DETECT_IN_SCREEN*/

//*********************************************************
//*********************************************************
//*********************************************************

static lv_obj_t *chart = NULL;
static lv_chart_series_t *series = NULL;
static lv_chart_series_t *series1 = NULL;
static lv_obj_t *tabview = NULL;
static lv_obj_t *label = NULL;
static lv_obj_t *label1 = NULL;
static lv_obj_t *label2 = NULL;

static void gui_init(void)
{
    lv_obj_t *scr = lv_obj_create(NULL, NULL);
    lv_scr_load(scr);

    // lv_theme_t *th = lv_theme_zen_init(100, NULL);
    lv_theme_t *th = lv_theme_material_init(100, NULL);
    // lv_theme_t *th = lv_theme_night_init(100, NULL);
    // lv_theme_t *th = lv_theme_alien_init(100, NULL);
    lv_theme_set_current(th);

    tabview = lv_tabview_create(lv_scr_act(), NULL);

    lv_obj_t *tab1 = lv_tabview_add_tab(tabview, "BME280");
    lv_obj_t *tab2 = lv_tabview_add_tab(tabview, "CAMERA");
    (void)tab2;
    lv_tabview_set_tab_act(tabview, 0, false);

    chart = lv_chart_create(tab1, NULL);
    lv_obj_set_size(chart, 240, 110);
    lv_chart_set_point_count(chart, 20);
    lv_obj_align(chart, NULL, LV_ALIGN_CENTER, 0, 0);
    lv_chart_set_type(chart, (lv_chart_type_t)LV_CHART_TYPE_POINT | LV_CHART_TYPE_LINE);

    lv_chart_set_series_opa(chart, LV_OPA_70);
    lv_chart_set_series_width(chart, 4);
    lv_chart_set_range(chart, 0, 100);

    series = lv_chart_add_series(chart, LV_COLOR_RED);
    series1 = lv_chart_add_series(chart, LV_COLOR_BLUE);

    label = lv_label_create(tabview, NULL);

    lv_label_set_text(label, "xxxxxxxxxxxxx");
    lv_obj_align(label, NULL, LV_ALIGN_OUT_BOTTOM_MID, 0,  -20);

    label1 = lv_label_create(tabview, NULL);
    lv_label_set_text(label1, "xxxxxxxxxxxxx");
    lv_obj_align(label1, NULL, LV_ALIGN_OUT_BOTTOM_MID, 0, -40);

    label2 = lv_label_create(tabview, NULL);
    lv_label_set_text(label2, "xxxxxxxxxxxxx");
    lv_obj_align(label2, NULL, LV_ALIGN_OUT_BOTTOM_MID, 0, -60);

}

static void screen_task(void *pvParameter)
{
    char buff[128];
    struct bme280_data comp_data;
    static int i = 0;

#ifdef FACE_DETECT_IN_SCREEN
    i = 1;
#endif
    lv_tabview_set_tab_act(tabview, i, true);

    while (1) {

        xEventGroupWaitBits(evGroup, 1, pdFALSE, pdFALSE, portMAX_DELAY);

        if (g_state != WAIT_FOR_WAKEUP) {
            i = i + 1 >= 2 ? 0 : i + 1;
            lv_tabview_set_tab_act(tabview, i, true);
            g_state = WAIT_FOR_WAKEUP;
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }

        if (!i) {

            int8_t rslt = bme280_get_sensor_data(BME280_ALL, &comp_data, &dev);

            if (rslt != BME280_OK) {
                vTaskDelay(500 / portTICK_PERIOD_MS);
                ESP_LOGE(TAG, "BME280 read error");
                continue;
            }

            lv_chart_set_next(chart, series, comp_data.temperature);
            lv_chart_set_next(chart, series1, comp_data.humidity );
            snprintf(buff, sizeof(buff), "press:%0.2fPa", comp_data.pressure);
            lv_label_set_text(label, buff);

            snprintf(buff, sizeof(buff), "hum  :%0.2f%%", comp_data.humidity);
            lv_label_set_text(label1, buff);

            snprintf(buff, sizeof(buff), "temp :%0.2f*C", comp_data.temperature);
            lv_label_set_text(label2, buff);

            vTaskDelay(500 / portTICK_PERIOD_MS);

        } else {
#ifdef FACE_DETECT_IN_SCREEN
            stream_handler();
#else
            camera_fb_t *fb = esp_camera_fb_get();
            if (!fb) {
                ESP_LOGE(TAG, "Camera capture failed");
            } else {
                TFT_jpg_image(CENTER, 50, 0, -1, NULL, fb->buf, fb->len);
                esp_camera_fb_return(fb);
                fb = NULL;
            }
#endif
        }
    }
}

extern "C"  void app_main()
{
    char buff[256];

    //! Esp-who is now configured by default as a hardware I2C driver
    // hal_i2c_init();

    sdmmc_card_t *card = NULL;

    ESP_LOGI("esp-eye", "Version %s", VERSION);

    if (!(evGroup = xEventGroupCreate())) {
        ESP_LOGE(TAG, "evGroup Fail");
        while (1);
    }

    xEventGroupSetBits(evGroup, 1);

    bool isTrue = app_sd_init(&card);
    if (isTrue) {
        snprintf(buff, sizeof(buff), "SD Size: %lluMB\n", ((uint64_t) card->csd.capacity) * card->csd.sector_size / (1024 * 1024));
        if (esp_vfs_fat_sdmmc_unmount() != ESP_OK) {
            ESP_LOGE(TAG, "Unmount SDCard Fail");
        }
    } else {
        snprintf(buff, sizeof(buff), "SD Card not the found");
    }

    app_camera_init();

    lvgl_init();

    // TFT_jpg_image(CENTER, CENTER, 0, -1, NULL, (uint8_t *)image_jpg_start, IMAGES_SIZE);

    // vTaskDelay(3000 / portTICK_PERIOD_MS);

    tft->fillScreen(0xFFFF);
    lv_obj_t *label = lv_label_create(lv_scr_act(), NULL);
    lv_label_set_text(label, buff);
    lv_obj_align(label, NULL, LV_ALIGN_CENTER, 0, 0);
    vTaskDelay(3000 / portTICK_PERIOD_MS);

    tft->fillScreen(0xFFFF);
    if (setPowerBoostKeepOn(1)) {  //true set power keep on
        lv_label_set_text(label, "Power set keep On PASS");
        lv_obj_align(label, NULL, LV_ALIGN_CENTER, 0, 0);
        vTaskDelay(3000 / portTICK_PERIOD_MS);
    } else {
        lv_label_set_text(label, "Power set keep On FAIL");
        lv_obj_align(label, NULL, LV_ALIGN_CENTER, 0, 0);
        vTaskDelay(3000 / portTICK_PERIOD_MS);
    }

#ifndef FACE_DETECT_IN_SCREEN
    app_speech_wakeup_init();
#endif

#ifndef ENABLE_BME280
    app_speech_wakeup_init();
#endif

    g_state = WAIT_FOR_WAKEUP;

#ifdef ENABLE_BME280
    app_sensor_init(&dev);
#else
    lv_label_set_text(label, "Please say nihaotianmao!");
    lv_obj_align(label, NULL, LV_ALIGN_CENTER, 0, 0);
    while (g_state == WAIT_FOR_WAKEUP) {
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
#endif

    lv_obj_del(label);

#ifdef ENABLE_BME280
    gui_init();
#endif

    app_wifi_init();

    vTaskDelay(300 / portTICK_PERIOD_MS);

#ifdef FACE_DETECT_IN_SCREEN
    ra_filter_init(&ra_filter, 20);

    app_mhttpd_main();
#else
    // app_httpd_main();
#endif


#ifndef ENABLE_BME280
    while (1) {

#ifdef FACE_DETECT_IN_SCREEN
        stream_handler();
#else
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) {
            ESP_LOGE(TAG, "Camera capture failed");
        } else {
            TFT_jpg_image(CENTER, 0, 0, -1, NULL, fb->buf, fb->len);
            esp_camera_fb_return(fb);
            fb = NULL;
        }
#endif
    }
#else
    xTaskCreate(screen_task, "screen_task", 4096, NULL, 5, NULL);
#endif
}
