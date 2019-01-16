
#include "app_screen.h"

#include "rom/tjpgd.h"

#include <errno.h>
#include <sys/stat.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "lwip/sockets.h"

#include "app_main.h"

#define TAG "[Screen]"

CEspLcd *tft = NULL;

int image_debug = 0;


#define JPG_IMAGE_LINE_BUF_SIZE 512
// === Special coordinates constants ===


typedef struct __attribute__((__packed__))
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
} color_t;

typedef struct {
    uint16_t x1;
    uint16_t y1;
    uint16_t x2;
    uint16_t y2;
} dispWin_t;

dispWin_t dispWin = {
    .x1 = 0,
    .y1 = 0,
    .x2 = TFT_WITDH,
    .y2 = TFT_HEIGHT,
};

typedef struct {
    FILE *fhndl; // File handler for input function
    int x;           // image top left point X position
    int y;           // image top left point Y position
    int s;
    bool found_header;
    uint8_t *membuff;       // memory buffer containing the image
    uint32_t bufsize;       // size of the memory buffer
    uint32_t bufptr;        // memory buffer current position
    color_t *linbuf[2]; // memory buffer used for display output
    uint8_t linbuf_idx;
} JPGIODEV;

// User defined call-back function to input JPEG data from memory buffer
//-------------------------
static UINT tjd_buf_input(
    JDEC *jd,       // Decompression object
    BYTE *buff, // Pointer to the read buffer (NULL:skip)
    UINT nd         // Number of bytes to read/skip from input stream
)
{
    // Device identifier for the session (5th argument of jd_prepare function)
    JPGIODEV *dev = (JPGIODEV *)jd->device;
    if (!dev->membuff)
        return 0;
    if (dev->bufptr >= (dev->bufsize + 2))
        return 0; // end of stream

    if ((dev->bufptr + nd) > (dev->bufsize + 2))
        nd = (dev->bufsize + 2) - dev->bufptr;

    if (buff) {
        // Read nd bytes from the input strem
        memcpy(buff, dev->membuff + dev->bufptr, nd);
        dev->bufptr += nd;
        return nd; // Returns number of bytes read
    } else {
        // Remove nd bytes from the input stream
        dev->bufptr += nd;
        return nd;
    }
}


// User defined call-back function to output RGB bitmap to display device
//----------------------
static UINT tjd_output(
    JDEC *jd,           // Decompression object of current session
    void *bitmap, // Bitmap data to be output
    JRECT *rect     // Rectangular region to output
)
{
    // Device identifier for the session (5th argument of jd_prepare function)
    JPGIODEV *dev = (JPGIODEV *)jd->device;

    // ** Put the rectangular into the display device **
    int x;
    int y;
    int dleft, dtop, dright, dbottom;
    BYTE *src = (BYTE *)bitmap;

    int left = rect->left + dev->x;
    int top = rect->top + dev->y;
    int right = rect->right + dev->x;
    int bottom = rect->bottom + dev->y;

    if ((left > dispWin.x2) || (top > dispWin.y2))
        return 1; // out of screen area, return
    if ((right < dispWin.x1) || (bottom < dispWin.y1))
        return 1; // out of screen area, return

    if (left < dispWin.x1)
        dleft = dispWin.x1;
    else
        dleft = left;
    if (top < dispWin.y1)
        dtop = dispWin.y1;
    else
        dtop = top;
    if (right > dispWin.x2)
        dright = dispWin.x2;
    else
        dright = right;
    if (bottom > dispWin.y2)
        dbottom = dispWin.y2;
    else
        dbottom = bottom;

    if ((dleft > dispWin.x2) || (dtop > dispWin.y2))
        return 1; // out of screen area, return
    if ((dright < dispWin.x1) || (dbottom < dispWin.y1))
        return 1; // out of screen area, return

    uint32_t len = ((dright - dleft + 1) * (dbottom - dtop + 1)); // calculate length of data

    if ((len > 0) && (len <= JPG_IMAGE_LINE_BUF_SIZE)) {
        uint8_t *dest = (uint8_t *)(dev->linbuf[dev->linbuf_idx]);

        for (y = top; y <= bottom; y++) {
            for (x = left; x <= right; x++) {
                // Clip to display area
                if ((x >= dleft) && (y >= dtop) && (x <= dright) && (y <= dbottom)) {
                    *dest++ = (*src++) & 0xFC;
                    *dest++ = (*src++) & 0xFC;
                    *dest++ = (*src++) & 0xFC;
                } else
                    src += 3; // skip
            }
        }

        // ESP_LOGI(TAG, "x1:%d y1:%d x2:%d y2:%d\n", dleft, dtop, dright, dbottom);
        tft->transmitCmdData(LCD_CASET, MAKEWORD(dleft >> 8, dleft & 0xFF, dright >> 8, dright & 0xFF));
        tft->transmitCmdData(LCD_PASET, MAKEWORD(dtop >> 8, dtop & 0xFF, dbottom >> 8, dbottom & 0xFF));
        tft->transmitCmd(LCD_RAMWR); // write to RAM

        uint16_t *p = (uint16_t *)malloc(sizeof(uint16_t) * len);
        if (!p) {
            ESP_LOGE(TAG, "malloc fail");
            return 0;
        }
        for (uint32_t i = 0; i < len; i++) {
            p[i] = tft->color565(dev->linbuf[dev->linbuf_idx][i].r, dev->linbuf[dev->linbuf_idx][i].g, dev->linbuf[dev->linbuf_idx][i].b);
        }
        tft->_fastSendBuf(p, len);
        free(p);

        dev->linbuf_idx = ((dev->linbuf_idx + 1) & 1);
    } else {
        ESP_LOGE(TAG, "Data size error: %d jpg: (%d,%d,%d,%d) disp: (%d,%d,%d,%d)\r\n", len, left, top, right, bottom, dleft, dtop, dright, dbottom);
        return 0; // stop decompression
    }
    return 1; // Continue to decompression
}



// tft.jpgimage(X, Y, scale, file_name, buf, size]
// X & Y can be < 0 !
//==================================================================================
void TFT_jpg_image(int x,
                   int y,
                   uint8_t scale,
                   uint8_t *buf,
                   uint32_t size)
{
    JPGIODEV dev;
    char *work = NULL;   // Pointer to the working buffer (must be 4-byte aligned)
    UINT sz_work = 3800; // Size of the working buffer (must be power of 2)
    JDEC jd;             // Decompression object (70 bytes)
    JRESULT rc;


    dev.linbuf[0] = NULL;
    dev.linbuf[1] = NULL;
    dev.linbuf_idx = 0;

    // image from buffer
    dev.membuff = buf;
    dev.bufsize = size;
    dev.bufptr = 0;

    if (scale > 3)
        scale = 3;

    work = (char *)malloc(sz_work);
    if (work) {
        rc = jd_prepare(&jd, tjd_buf_input, (void *)work, sz_work, &dev);
        if (rc == JDR_OK) {
            // ESP_LOGI(TAG, "decode size: %u * %u", jd.width, jd.height);
            if (x == CENTER)
                x = ((dispWin.x2 - dispWin.x1 + 1 - (int)(jd.width >> scale)) / 2) + dispWin.x1;
            else if (x == RIGHT)
                x = dispWin.x2 + 1 - (int)(jd.width >> scale);

            if (y == CENTER)
                y = ((dispWin.y2 - dispWin.y1 + 1 - (int)(jd.height >> scale)) / 2) + dispWin.y1;
            else if (y == BOTTOM)
                y = dispWin.y2 + 1 - (int)(jd.height >> scale);

            if (x < ((dispWin.x2 - 1) * -1))
                x = (dispWin.x2 - 1) * -1;
            if (y < ((dispWin.y2 - 1)) * -1)
                y = (dispWin.y2 - 1) * -1;
            if (x > (dispWin.x2 - 1))
                x = dispWin.x2 - 1;
            if (y > (dispWin.y2 - 1))
                y = dispWin.y2 - 1;

            dev.x = x;
            dev.y = y;

            dev.linbuf[0] = (color_t *)heap_caps_malloc(JPG_IMAGE_LINE_BUF_SIZE * 3, MALLOC_CAP_DMA);
            if (dev.linbuf[0] == NULL) {
                if (image_debug)
                    ESP_LOGE(TAG, "Error allocating line buffer #0\r\n");
                goto exit;
            }
            dev.linbuf[1] = (color_t *)heap_caps_malloc(JPG_IMAGE_LINE_BUF_SIZE * 3, MALLOC_CAP_DMA);
            if (dev.linbuf[1] == NULL) {
                if (image_debug)
                    ESP_LOGE(TAG, "Error allocating line buffer #1\r\n");
                goto exit;
            }

            // Start to decode the JPEG file
            // TODO ...
            // disp_select();
            // DC_D;
            // CS_L;
            rc = jd_decomp(&jd, tjd_output, scale);
            // CS_H;
            // disp_deselect();

            if (rc != JDR_OK) {
                if (image_debug)
                    ESP_LOGE(TAG, "jpg decompression error %d\r\n", rc);
            }
            if (image_debug)
                ESP_LOGI(TAG, "Jpg size: %dx%d, position; %d,%d, scale: %d, bytes used: %d\r\n", jd.width, jd.height, x, y, scale, jd.sz_pool);
        } else {
            if (image_debug)
                ESP_LOGE(TAG, "jpg prepare error %d\r\n", rc);
        }
    } else {
        if (image_debug)
            ESP_LOGE(TAG, "work buffer allocation error\r\n");
    }

exit:
    if (work)
        free(work); // free work buffer
    if (dev.linbuf[0])
        free(dev.linbuf[0]);
    if (dev.linbuf[1])
        free(dev.linbuf[1]);
}





//====================================================================================
int TFT_bmp_image(int x, int y, uint8_t scale, char *fname, uint8_t *imgbuf, int size)
{
    FILE *fhndl = NULL;
    struct stat sb;
    int i, err = 0;
    int img_xsize, img_ysize, img_xstart, img_xlen, img_ystart, img_ylen;
    int img_pos, img_pix_pos, scan_lines, rd_len;
    uint8_t tmpc;
    uint16_t wtemp;
    uint32_t temp;
    int disp_xstart, disp_xend, disp_ystart, disp_yend;
    uint8_t buf[56];
    char err_buf[64];
    uint8_t *line_buf[2] = {NULL, NULL};
    uint8_t lb_idx = 0;
    uint8_t *scale_buf = NULL;
    uint8_t scale_pix;
    uint16_t co[3] = {0, 0, 0}; // RGB sum
    uint8_t npix;

    if (scale > 7)
        scale = 7;
    scale_pix = scale + 1; // scale factor ( 1~8 )

    if (fname) {
        // * File name is given, reading image from file
        if (stat(fname, &sb) != 0) {
            sprintf(err_buf, "opening file");
            err = -1;
            goto exit;
        }
        size = sb.st_size;
        fhndl = fopen(fname, "r");
        if (!fhndl) {
            sprintf(err_buf, "opening file");
            err = -2;
            goto exit;
        }

        i = fread(buf, 1, 54, fhndl); // read header
    } else {
        // * Reading image from buffer
        if ((imgbuf) && (size > 54)) {
            memcpy(buf, imgbuf, 54);
            i = 54;
        } else
            i = 0;
    }

    sprintf(err_buf, "reading header");
    if (i != 54) {
        err = -3;
        goto exit;
    }

    // ** Check image header and get image properties
    if ((buf[0] != 'B') || (buf[1] != 'M')) {
        err = -4;
        goto exit;
    } // accept only images with 'BM' id

    memcpy(&temp, buf + 2, 4); // file size
    if (temp != size) {
        err = -5;
        goto exit;
    }

    memcpy(&img_pos, buf + 10, 4); // start of pixel data

    memcpy(&temp, buf + 14, 4); // BMP header size
    if (temp != 40) {
        err = -6;
        goto exit;
    }

    memcpy(&wtemp, buf + 26, 2); // the number of color planes
    if (wtemp != 1) {
        err = -7;
        goto exit;
    }

    memcpy(&wtemp, buf + 28, 2); // the number of bits per pixel
    if (wtemp != 24) {
        err = -8;
        goto exit;
    }

    memcpy(&temp, buf + 30, 4); // the compression method being used
    if (temp != 0) {
        err = -9;
        goto exit;
    }

    memcpy(&img_xsize, buf + 18, 4); // the bitmap width in pixels
    memcpy(&img_ysize, buf + 22, 4); // the bitmap height in pixels

    // * scale image dimensions

    img_xlen = img_xsize / scale_pix; // image display horizontal size
    img_ylen = img_ysize / scale_pix; // image display vertical size

    if (x == CENTER)
        x = ((dispWin.x2 - dispWin.x1 + 1 - img_xlen) / 2) + dispWin.x1;
    else if (x == RIGHT)
        x = dispWin.x2 + 1 - img_xlen;

    if (y == CENTER)
        y = ((dispWin.y2 - dispWin.y1 + 1 - img_ylen) / 2) + dispWin.y1;
    else if (y == BOTTOM)
        y = dispWin.y2 + 1 - img_ylen;

    if ((x < ((dispWin.x2 + 1) * -1)) || (x > (dispWin.x2 + 1)) || (y < ((dispWin.y2 + 1) * -1)) || (y > (dispWin.y2 + 1))) {
        sprintf(err_buf, "out of display area (%d,%d", x, y);
        err = -10;
        goto exit;
    }

    // ** set display and image areas
    if (x < dispWin.x1) {
        disp_xstart = dispWin.x1;
        img_xstart = -x; // image pixel line X offset
        img_xlen += x;
    } else {
        disp_xstart = x;
        img_xstart = 0;
    }
    if (y < dispWin.y1) {
        disp_ystart = dispWin.y1;
        img_ystart = -y; // image pixel line Y offset
        img_ylen += y;
    } else {
        disp_ystart = y;
        img_ystart = 0;
    }
    disp_xend = disp_xstart + img_xlen - 1;
    disp_yend = disp_ystart + img_ylen - 1;
    if (disp_xend > dispWin.x2) {
        disp_xend = dispWin.x2;
        img_xlen = disp_xend - disp_xstart + 1;
    }
    if (disp_yend > dispWin.y2) {
        disp_yend = dispWin.y2;
        img_ylen = disp_yend - disp_ystart + 1;
    }

    if ((img_xlen < 8) || (img_ylen < 8) || (img_xstart >= (img_xsize - 2)) || ((img_ysize - img_ystart) < 2)) {
        sprintf(err_buf, "image too small");
        err = -11;
        goto exit;
    }

    // ** Allocate memory for 2 lines of image pixels
    line_buf[0] = (uint8_t *)heap_caps_malloc(img_xsize * 3, MALLOC_CAP_DMA);
    if (line_buf[0] == NULL) {
        sprintf(err_buf, "allocating line buffer #1");
        err = -12;
        goto exit;
    }

    line_buf[1] = (uint8_t *)heap_caps_malloc(img_xsize * 3, MALLOC_CAP_DMA);
    if (line_buf[1] == NULL) {
        sprintf(err_buf, "allocating line buffer #2");
        err = -13;
        goto exit;
    }

    if (scale) {
        // Allocate memory for scale buffer
        rd_len = img_xlen * 3 * scale_pix;
        scale_buf = (uint8_t *) malloc(rd_len * scale_pix);
        if (scale_buf == NULL) {
            sprintf(err_buf, "allocating scale buffer");
            err = -14;
            goto exit;
        }
    } else
        rd_len = img_xlen * 3;

    // ** ***************************************************** **
    // ** BMP images are stored in file from LAST to FIRST line **
    // ** ***************************************************** **

    /* Used variables:
        img_xsize       horizontal image size in pixels
        img_ysize       number of image lines
        img_xlen        image display horizontal scaled size in pixels
        img_ylen        image display vertical scaled size in pixels
        img_xstart      first pixel in line to be displayed
        img_ystart      first image line to be displayed
        img_xlen        number of pixels in image line to be displayed, starting with 'img_xstart'
        img_ylen        number of lines in image to be displayed, starting with 'img_ystart'
        rd_len          length of color data which are read from image line in bytes
     */

    // Set position in image to the first color data (beginning of the LAST line)
    img_pos += (img_ystart * (img_xsize * 3));
    if (fhndl) {
        if (fseek(fhndl, img_pos, SEEK_SET) != 0) {
            sprintf(err_buf, "file seek at %d", img_pos);
            err = -15;
            goto exit;
        }
    }

    if (image_debug)
        printf("BMP: image size: (%d,%d) scale: %d disp size: (%d,%d) img xofs: %d img yofs: %d at: %d,%d; line buf: 2* %d scale buf: %d\r\n",
               img_xsize, img_ysize, scale_pix, img_xlen, img_ylen, img_xstart, img_ystart, disp_xstart, disp_ystart, img_xsize * 3, ((scale) ? (rd_len * scale_pix) : 0));

    // * Select the display
    // disp_select();

    while ((disp_yend >= disp_ystart) && ((img_pos + (img_xsize * 3)) <= size)) {
        if (img_pos > size) {
            sprintf(err_buf, "EOF reached: %d > %d", img_pos, size);
            err = -16;
            goto exit1;
        }
        if (scale == 0) {
            // Read the line of color data into color buffer
            if (fhndl) {
                i = fread(line_buf[lb_idx], 1, img_xsize * 3, fhndl); // read line from file
                if (i != (img_xsize * 3)) {
                    sprintf(err_buf, "file read at %d (%d<>%d)", img_pos, i, img_xsize * 3);
                    err = -16;
                    goto exit1;
                }
            } else
                memcpy(line_buf[lb_idx], imgbuf + img_pos, img_xsize * 3);

            if (img_xstart > 0)
                memmove(line_buf[lb_idx], line_buf[lb_idx] + (img_xstart * 3), rd_len);
            // Convert colors BGR-888 (BMP) -> RGB-888 (DISPLAY) ===
            for (i = 0; i < rd_len; i += 3) {
                tmpc = line_buf[lb_idx][i + 2] & 0xfc;                              // save R
                line_buf[lb_idx][i + 2] = line_buf[lb_idx][i] & 0xfc; // B -> R
                line_buf[lb_idx][i] = tmpc;                                                     // R -> B
                line_buf[lb_idx][i + 1] &= 0xfc;                                            // G
            }
            img_pos += (img_xsize * 3);
        } else {
            // scale image, read 'scale_pix' lines and find the average color
            for (scan_lines = 0; scan_lines < scale_pix; scan_lines++) {
                if (img_pos > size)
                    break;
                if (fhndl) {
                    i = fread(line_buf[lb_idx], 1, img_xsize * 3, fhndl); // read line from file
                    if (i != (img_xsize * 3)) {
                        sprintf(err_buf, "file read at %d (%d<>%d)", img_pos, i, img_xsize * 3);
                        err = -17;
                        goto exit1;
                    }
                } else
                    memcpy(line_buf[lb_idx], imgbuf + img_pos, img_xsize * 3);
                img_pos += (img_xsize * 3);

                // copy only data which are displayed to scale buffer
                memcpy(scale_buf + (rd_len * scan_lines), line_buf[lb_idx] + img_xstart, rd_len);
            }

            // Populate display line buffer
            for (int n = 0; n < (img_xlen * 3); n += 3) {
                memset(co, 0, sizeof(co)); // initialize color sum
                npix = 0;                                    // initialize number of pixels in scale rectangle

                // sum all pixels in scale rectangle
                for (int sc_line = 0; sc_line < scan_lines; sc_line++) {
                    // Get colors position in scale buffer
                    img_pix_pos = (rd_len * sc_line) + (n * scale_pix);

                    for (int sc_col = 0; sc_col < scale_pix; sc_col++) {
                        co[0] += scale_buf[img_pix_pos];
                        co[1] += scale_buf[img_pix_pos + 1];
                        co[2] += scale_buf[img_pix_pos + 2];
                        npix++;
                    }
                }
                // Place the average in display buffer, convert BGR-888 (BMP) -> RGB-888 (DISPLAY)
                line_buf[lb_idx][n + 2] = (uint8_t)(co[0] / npix); // B
                line_buf[lb_idx][n + 1] = (uint8_t)(co[1] / npix); // G
                line_buf[lb_idx][n] = (uint8_t)(co[2] / npix);       // R
            }
        }
        tft->transmitCmdData(LCD_CASET, MAKEWORD(disp_xstart >> 8, disp_xstart & 0xFF, disp_xend >> 8, disp_xend & 0xFF));
        tft->transmitCmdData(LCD_PASET, MAKEWORD(disp_yend >> 8, disp_yend & 0xFF, disp_yend >> 8, disp_yend & 0xFF));
        tft->transmitCmd(LCD_RAMWR); // write to RAM

        uint16_t *p = (uint16_t *)malloc(sizeof(uint16_t) * img_xlen);
        if (!p) {
            ESP_LOGE(TAG, "malloc fail");
            return 0;
        }
        for (uint32_t i = 0; i < img_xlen; i++) {
            // p[i] = tft->color565(line_buf[lb_idx].r, line_buf[lb_idx].g, line_buf[lb_idx].b);
        }
        tft->_fastSendBuf(p, img_xlen);
        free(p);

        // wait_trans_finish(1);
        // send_data(disp_xstart, disp_yend, disp_xend, disp_yend, img_xlen, (color_t *)line_buf[lb_idx]);
        lb_idx = (lb_idx + 1) & 1; // change buffer

        disp_yend--;
    }
    err = 0;
exit1:
    // disp_deselect();
exit:
    if (scale_buf)
        free(scale_buf);
    if (line_buf[0])
        free(line_buf[0]);
    if (line_buf[1])
        free(line_buf[1]);
    if (fhndl)
        fclose(fhndl);
    if ((err) && (image_debug))
        printf("Error: %d [%s]\r\n", err, err_buf);

    return err;
}



void app_lcd_init()
{
    lcd_conf_t lcd_pins = {
        .lcd_model = LCD_MOD_ST7789,
        .pin_num_miso = TFT_MISO,
        .pin_num_mosi = TFT_MOSI,
        .pin_num_clk = TFT_SCLK,
        .pin_num_cs = TFT_CS,
        .pin_num_dc = TFT_DC,
        .pin_num_rst = TFT_RST,
        .pin_num_bckl = TFT_BK,
        .clk_freq = 26 * 1000 * 1000,
        .rst_active_level = 0,
        .bckl_active_level = 1,
        .spi_host = HSPI_HOST,
        .init_spi_bus = true
    };

    /*Initialize SPI Handler*/
    if (tft == NULL) {
        tft = new CEspLcd(&lcd_pins, TFT_HEIGHT, TFT_WITDH);
        // camera_queue = xQueueCreate(CAMERA_CACHE_NUM - 1, sizeof(camera_evt_t));
    }

    /*screen initialize*/
    tft->invertDisplay(true);
    tft->setRotation(0);
    tft->fillScreen(COLOR_GREEN);
    // tft->drawBitmap(0, 0, esp_logo, 137, 26);
    // tft->drawString("Status: Initialize camera ...", 0, 30);
    // vTaskDelay(2000 / portTICK_PERIOD_MS);
}

