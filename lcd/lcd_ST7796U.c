#include <stdint.h>
#include <stdio.h>
#include <gpiod.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>

#define MAX_SPI_BUF 4096

#define LCD_DC   5  /* BCM.5引脚用来作为数据和命令切换引脚 */
#define LCD_REST 23 /* BCM.23引脚用来作为LCD重置 */

#define LCD_SCREEN_WIDTH  480
#define LCD_SCREEN_HEIGHT 320

#define LCD_SPI_MAX_SPEED      24000000
#define LCD_SPI_MODE           SPI_MODE_0
#define LCD_SPI_BITS_PER_WORD  8

#define LCD_SPI_MAX_CMD        128
struct spi_data {
    uint8_t cmd;
    uint8_t data[LCD_SPI_MAX_CMD];
    size_t data_len;
};

struct rgb565 {
    uint16_t r:5;
    uint16_t g:6;
    uint16_t b:5;
};

struct gpiod_line *g_lcd_dc;
struct gpiod_line *g_lcd_reset;

int lcd_spi_write(int fd, uint8_t *wbuf, size_t size)
{
    int ret = write(fd, wbuf, size);
    if (ret != size) {
        printf("failed to send data.\n");
    }
    return ret;
}

int lcd_spi_send_cmd(int fd, struct spi_data *spi_data)
{
    gpiod_line_set_value(g_lcd_dc, 0);
    lcd_spi_write(fd, &spi_data->cmd, 1);
    if (spi_data->data_len == 0) {
        return 0;
    }
    gpiod_line_set_value(g_lcd_dc, 1);
    lcd_spi_write(fd, spi_data->data, spi_data->data_len);
    return 0;
}

void lcd_spi_send_data(int fd, uint8_t *data, size_t len)
{
    gpiod_line_set_value(g_lcd_dc, 1);

    size_t pos = 0;
    int left_len = len;
    while (left_len > 0) {
        size_t send = MAX_SPI_BUF;
        if (left_len < MAX_SPI_BUF) {
            send = left_len;
        }
        lcd_spi_write(fd, data + pos, send);
        pos += send;
        left_len -= send;
    }
}

void lcd_set_window(int fd, uint16_t xstart, uint16_t ystart,
    uint16_t xend, uint16_t yend)
{
    struct spi_data data = { 0 };
    data.cmd = 0x2A;
    data.data[0] = xstart >> 8;
    data.data[1] = xstart & 0xFF;
    data.data[2] = xend >> 8;
    data.data[3] = xend & 0xFF;
    data.data_len = 4;
    lcd_spi_send_cmd(fd, &data);

    data.cmd = 0x2B;
    data.data[0] = ystart >> 8;
    data.data[1] = ystart & 0xFF;
    data.data[2] = yend >> 8;
    data.data[3] = yend & 0xFF;
    data.data_len = 4;
    lcd_spi_send_cmd(fd, &data);

    data.cmd = 0x2C;
    data.data_len = 0;
    lcd_spi_send_cmd(fd, &data);
}

void test_lcd(int fd)
{
    lcd_set_window(fd, 0, 0, LCD_SCREEN_WIDTH - 1, LCD_SCREEN_HEIGHT - 1);

    size_t size = LCD_SCREEN_WIDTH * LCD_SCREEN_HEIGHT;
    struct rgb565 *data = malloc(size * sizeof(struct rgb565));
    if (data == NULL) {
        printf("malloc failed\n");
        return;
    }
    memset(data, 0XFF, size * sizeof(struct rgb565));
    for (int i = 0; i < size; i++) {
        if (i / LCD_SCREEN_WIDTH < 64) {
            data[i].r = 0xFF;
            data[i].g = 0;
            data[i].b = 0;
        } else if ((i / LCD_SCREEN_WIDTH >= 64) && (i / LCD_SCREEN_WIDTH < 100)) {
            data[i].r = 0;
            data[i].g = 0xFF;
            data[i].b = 0;
        } else {
            data[i].r = 0;
            data[i].g = 0;
            data[i].b = 0xFF;
        }
        uint16_t temp = *(uint16_t *)&data[i];
        *(uint16_t *)&data[i] = (((temp >> 8) & 0xFF) + ((temp & 0xFF) << 8));
    }
    lcd_spi_send_data(fd, (uint8_t *)data, size * sizeof(struct rgb565));
}

struct spi_data g_spi_data[] = {
    {0x11, {}, 0},
    {0x36, {0x48}, 1},
    {0x3A, {0x55}, 1},
    {0xF0, {0xC3}, 1},
    {0xF0, {0x96}, 1},
    {0xB4, {0x01}, 1},
    {0xB7, {0xC6}, 1},
    {0xB9, {0x02, 0xE0}, 2},
    {0xC0, {0x80, 0x07}, 2},
    {0xC1, {0x15}, 1},
    {0xC2, {0xA7}, 1},
    {0xC5, {0x07}, 1},
    {0xE8, {0x40, 0x8A, 0x00, 0x00, 0x29, 0x19, 0xA5, 0x33}, 8},
    {0xE0, {0xF0, 0x04, 0x0E, 0x03, 0x02, 0x13, 0x34, 0x44, 0x4A, 0x3A, 0x15, 0x15, 0x2F, 0x34}, 14},
    {0xE1, {0xF0, 0x0F, 0x16, 0x0C, 0x09, 0x05, 0x34, 0x33, 0x4A, 0x35, 0x11, 0x11, 0x2C, 0x32}, 14},
    {0xF0, {0x3C}, 1},
    {0xF0, {0x69}, 1},
    {0x21, {}, 0},
    {0x29, {}, 0},
    {0x36, {0x20}, 1},

#if 0
    {0x11, {}, 0},
    {0x26, {0x04}, 1},
    {0xB1, {0x0e, 0x10}, 2},
    {0xC0, {0x08, 0x00}, 2},
    {0xC1, {0x05}, 1},
    {0xC5, {0x38, 0x40}, 2},
    {0x3a, {0x05}, 1},
    {0x36, {0xA8}, 1},
    {0x2A, {0x00, 0x00, 0x00, 0x9F}, 4}, 
    {0x2B, {0x00, 0x00, 0x00, 0x7F}, 4},
    {0xB4, {0x00}, 1},
    {0xf2, {0x01}, 1},
    {0xE0, {0x3f, 0x22, 0x20, 0x30, 0x29, 0x0c, 0x4e, 0xb7, 0x3c, 0x19, 0x22, 0x1e, 0x02, 0x01, 0x00}, 15},
    {0xE1, {0x00, 0x1b, 0x1f, 0x0f, 0x16, 0x13, 0x31, 0x84, 0x43, 0x06, 0x1d, 0x21, 0x3d, 0x3e, 0x3f}, 15},
    {0x29, {}, 0},
    {0x2C, {}, 0},
#endif
};

int lcd_reset(int fd)
{
    size_t count = sizeof(g_spi_data) / sizeof(g_spi_data[0]);
    for (size_t i = 0; i < count; i++) {
        lcd_spi_send_cmd(fd, &g_spi_data[i]);
    }
    return 0;
}

void lcd_hard_reset(void)
{
    gpiod_line_set_value(g_lcd_reset, 0);
    usleep(200 * 1000);
    gpiod_line_set_value(g_lcd_reset, 1);
    usleep(500 * 1000);
}

int lcd_spi_init(void)
{
    int fd = open("/dev/spidev0.0", O_RDWR);
    if (fd < 0) {
        printf("failed to open spi device.\n");
	    return -1;
    }

    /* mode  */
    uint8_t mode = LCD_SPI_MODE;
    if (ioctl(fd, SPI_IOC_WR_MODE, &mode) != 0) {
        printf("SPI_IOC_WR_MODE: %d\n", errno);
        return -1;
    }
    if (ioctl(fd, SPI_IOC_RD_MODE, &mode) != 0) {
        printf("SPI_IOC_RD_MODE: %d\n", errno);
        return -1;
    }

    /* speed */
    uint32_t speed = LCD_SPI_MAX_SPEED;
    if (ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) != 0) {
        printf("SPI_IOC_WR_MAX_SPEED_HZ: %d\n", errno);
        return -1;
    }
    if (ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed) != 0) {
        printf("SPI_IOC_RD_MAX_SPEED_HZ: %d\n", errno);
        return -1;
    }

    /* bits per word */
    uint8_t bits = LCD_SPI_BITS_PER_WORD;
    if (ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits) != 0) {
        printf("SPI_IOC_WR_BITS_PER_WORD: %d\n", errno);
        return -1;
    }
    if (ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &bits) != 0) {
        printf("SPI_IOC_RD_BITS_PER_WORD: %d\n", errno);
        return -1;
    }
    return fd;
}

int main() {
    int spi_fd = lcd_spi_init();
    lcd_hard_reset();
    lcd_reset(spi_fd);
    test_lcd(spi_fd);

    while (true) {
        usleep(100);
    }
    return 0;
}
