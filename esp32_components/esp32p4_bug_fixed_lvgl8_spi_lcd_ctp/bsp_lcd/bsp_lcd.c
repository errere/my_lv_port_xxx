// /* ST7789V driver for ESP32

//    This example code is in the Public Domain (or CC0 licensed, at your option.)

//    Unless required by applicable law or agreed to in writing, this
//    software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
//    CONDITIONS OF ANY KIND, either express or implied.
// */

#include "bsp_lcd.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <esp_system.h>
#include <esp_log.h>

#include <driver/gpio.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

const static char *TAG = "lcd";

/* SPI传输指令结构体，由于本函数退出时SPI控制器仍在使用DMA传输，因此使用静态变量在全局区申请
   以保证在函数退出时此变量依然可用。
 */
static spi_transaction_t lcd_trans[6]; // 5个指令传输+1个数据传输

/**
 * @brief 使用查询方式向LCD发送一个指令，用于LCD初始化阶段
 *
 * @param spi SPI句柄
 * @param cmd 要发送的指令字节
 */
static void lcd_send_cmd(spi_device_handle_t spi, const uint8_t cmd)
{
    /* 创建并初始化SPI传输指令结构体 */
    spi_transaction_t trans;
    memset(&trans, 0, sizeof(trans));

    /* 配置传输 */
    trans.length = 8;       // 指令为1字节8位
    trans.tx_buffer = &cmd; // 指令字节的地址
    trans.user = (void *)0; // D/C=0

    /* 启动传输 */
    ESP_ERROR_CHECK(
        spi_device_polling_transmit(spi, &trans));
}

/**
 * @brief 使用查询方式向LCD发送一组数据，用于LCD初始化阶段
 *
 * @param spi SPI句柄
 * @param data 要发送的数据指针
 * @param len 数据长度
 */
static void lcd_send_data(spi_device_handle_t spi, const uint8_t *data, uint8_t len)
{
    /* 创建并初始化SPI传输指令结构体 */
    spi_transaction_t trans;
    memset(&trans, 0, sizeof(trans));

    /* 由于DMA传输要求缓冲区位于内部内存中，因此申请一段可用于DMA的内存并将数据拷入 */
    uint8_t *buff = heap_caps_malloc(len, MALLOC_CAP_DMA); // 申请可用于DMA的内存
    memcpy(buff, data, len);

    /* 配置传输。 */
    trans.length = len * 8; // 数据长度，单位为位
    trans.tx_buffer = buff;
    trans.user = (void *)1; // D/C=1

    /* 启动传输 */
    ESP_ERROR_CHECK(
        spi_device_polling_transmit(spi, &trans));

    /* 释放内存 */
    free(buff);
}

/**
 * @brief SPI开始一次传输前的回调函数，用于控制D/C引脚
 *
 * @param spi SPI传输指令结构体
 */
static void lcd_spi_pre_transfer_callback(spi_transaction_t *spi)
{
    uint32_t dc = (uint32_t)(spi->user);
    /* 设置D/C引脚的电平 */
    gpio_set_level(LCD_PIN_DC, dc);
}

/**
 * @brief 初始化LCD所用的总线，并返回SPI句柄
 *
 * @return spi_device_handle_t* SPI句柄
 */
static spi_device_handle_t lcd_bus_init()
{
    /* 初始化非SPI的GPIO */
    gpio_reset_pin(LCD_PIN_DC);
    gpio_reset_pin(LCD_PIN_RST);
    gpio_reset_pin(LCD_PIN_MOSI);
    gpio_reset_pin(LCD_PIN_CLK);
    gpio_reset_pin(LCD_PIN_CS);
    gpio_set_direction(LCD_PIN_DC, GPIO_MODE_OUTPUT); // DC引脚为输出模式
    gpio_set_direction(LCD_PIN_RST, GPIO_MODE_OUTPUT);
    gpio_set_level(LCD_PIN_RST, 0);
    vTaskDelay(10);
    gpio_set_level(LCD_PIN_RST, 1);
    /* 创建SPI句柄 */
    spi_device_handle_t spi;

    /* SPI总线配置 */
    spi_bus_config_t buscfg = {
        .miso_io_num = -1,
        .mosi_io_num = LCD_PIN_MOSI,
        .sclk_io_num = LCD_PIN_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_X_PIXELS * LCD_Y_PIXELS * 2 + 8,
        
    };
    /* 根据buscfg的内容初始化SPI总线 */
    ESP_ERROR_CHECK(
        spi_bus_initialize(LCD_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));

    /* LCD外设配置 */
    spi_device_interface_config_t lcdcfg = {
        .clock_speed_hz = LCD_SPI_CLOCK_MHZ * 1000000,               // SPI总线时钟频率，Hz
        .mode = 0,                                                   // SPI模式0，CPOL=0，CPHA=0
        .spics_io_num = LCD_PIN_CS,                                  // CS引脚
        .queue_size = sizeof(lcd_trans) / sizeof(spi_transaction_t), // 传输队列深度
        .pre_cb = lcd_spi_pre_transfer_callback,                     // SPI每次传输前的回调函数，用于控制D/C脚
        .clock_source = SPI_CLK_SRC_SPLL,
    };
    /* 根据lcdcfg的内容将LCD挂载到SPI总线上 */
    ESP_ERROR_CHECK(
        spi_bus_add_device(LCD_SPI_HOST, &lcdcfg, &spi));

    return spi;
}

/**
 * @brief 初始化LCD控制器的寄存器
 *
 * @param spi SPI句柄
 */
static void lcd_reg_init(spi_device_handle_t spi)
{
    // st7789v3
    lcd_send_cmd(spi, 0x11);

    vTaskDelay(pdMS_TO_TICKS(120)); // ms

    lcd_send_cmd(spi, 0x36);
    lcd_send_data(spi, (uint8_t[]){0x00}, 1);

    lcd_send_cmd(spi, 0x3A);
    lcd_send_data(spi, (uint8_t[]){0x55}, 1);

    lcd_send_cmd(spi, 0xB2);
    lcd_send_data(spi, (uint8_t[]){0x0C, 0x0C, 0x00, 0x33, 0x33}, 5);

    lcd_send_cmd(spi, 0xB7);
    lcd_send_data(spi, (uint8_t[]){0x56}, 1);

    lcd_send_cmd(spi, 0xBB);
    lcd_send_data(spi, (uint8_t[]){0x20}, 1);

    lcd_send_cmd(spi, 0xC0);
    lcd_send_data(spi, (uint8_t[]){0x2C}, 1);

    lcd_send_cmd(spi, 0xC2);
    lcd_send_data(spi, (uint8_t[]){0x01}, 1);

    lcd_send_cmd(spi, 0xC3);
    lcd_send_data(spi, (uint8_t[]){0x0F}, 1);

    lcd_send_cmd(spi, 0xC4);
    lcd_send_data(spi, (uint8_t[]){0x20}, 1);

    lcd_send_cmd(spi, 0xC6);
    lcd_send_data(spi, (uint8_t[]){0x0F}, 1);

    lcd_send_cmd(spi, 0xD0);
    lcd_send_data(spi, (uint8_t[]){0xA4, 0xA1}, 2);

    lcd_send_cmd(spi, 0xD6);
    lcd_send_data(spi, (uint8_t[]){0xA1}, 1);

    lcd_send_cmd(spi, 0xE0);
    lcd_send_data(spi, (uint8_t[]){0xF0, 0x00, 0x06, 0x06, 0x07, 0x05, 0x30, 0x44, 0x48, 0x38, 0x11, 0x10, 0x2E, 0x34}, 14);

    lcd_send_cmd(spi, 0xE1);
    lcd_send_data(spi, (uint8_t[]){0xF0, 0x0A, 0x0E, 0x0D, 0x0B, 0x27, 0x2F, 0x44, 0x47, 0x35, 0x12, 0x12, 0x2C, 0x32}, 14);

    lcd_send_cmd(spi, 0x21);
    lcd_send_cmd(spi, 0x29);

    // /* 退出睡眠模式 */
    // lcd_send_cmd(spi, 0x11);
    // /* IPS屏幕需要设置屏幕反显才能显示正确的颜色 */
    // lcd_send_cmd(spi, 0x21); // 开启反显
    // // lcd_send_cmd(spi, 0x20); //关闭反显
    // /* 显存访问控制 */
    // lcd_send_cmd(spi, 0x36);
    // // lcd_send_data(spi, (uint8_t[]){0x60}, 1); //MX=MV=1, MY=ML=MH=0, RGB=0 横屏, 芯片在右
    // // lcd_send_data(spi, (uint8_t[]){0xA0}, 1); //MY=MV=1, MX=ML=MH=0, RGB=0 横屏，芯片在左
    // lcd_send_data(spi, (uint8_t[]){0x00}, 1); // MX=MV=MY=ML=MH=0, RGB=0 竖屏，芯片在下
    // // lcd_send_data(spi, (uint8_t[]){0xe0}, 1); //bc
    // /* 像素格式 */
    // lcd_send_cmd(spi, 0x3A);
    // lcd_send_data(spi, (uint8_t[]){0x05}, 1); // MCU mode, 16bit/pixel
    // /* Porch设置 */
    // lcd_send_cmd(spi, 0xB2);
    // lcd_send_data(spi, (uint8_t[]){0x0C, 0x0C, 0x00, 0x33, 0x33}, 5);
    // /* Gate设置 */
    // lcd_send_cmd(spi, 0xB7);
    // lcd_send_data(spi, (uint8_t[]){0x35}, 1); // Vgh=13.26V, Vgl=-10.43V
    // /* VCOM设置 */
    // lcd_send_cmd(spi, 0xBB);
    // lcd_send_data(spi, (uint8_t[]){0x19}, 1); // VCOM=0.725V
    // /* LCN设置 */
    // lcd_send_cmd(spi, 0xC0);
    // lcd_send_data(spi, (uint8_t[]){0x2C}, 1); // XOR: BGR, MX, MH
    // /* VDV与VRH写使能 */
    // lcd_send_cmd(spi, 0xC2);
    // lcd_send_data(spi, (uint8_t[]){0x01, 0xFF}, 2); // CMDEN=1
    // /* VRH设置 */
    // lcd_send_cmd(spi, 0xC3);
    // lcd_send_data(spi, (uint8_t[]){0x12}, 1); // Vrh=4.45+
    // /* VDV设置 */
    // lcd_send_cmd(spi, 0xC4);
    // lcd_send_data(spi, (uint8_t[]){0x20}, 1); // Vdv=0
    // /* 刷新率设置 */
    // lcd_send_cmd(spi, 0xC6);
    // lcd_send_data(spi, (uint8_t[]){0x0F}, 1); // 60Hz, no column inversion
    // /* 电源控制1 */
    // lcd_send_cmd(spi, 0xD0);
    // lcd_send_data(spi, (uint8_t[]){0xA4, 0xA1}, 2); // AVDD=6.8V, AVCL=-4.8V, VDDS=2.3V
    // /* 正电压伽马控制 */
    // lcd_send_cmd(spi, 0xE0);
    // lcd_send_data(spi, (uint8_t[]){0xD0, 0x0D, 0x14, 0x0B, 0x0B, 0x07, 0x3A, 0x44, 0x50, 0x08, 0x13, 0x13, 0x2D, 0x32}, 14);
    // /* 负电压伽马控制 */
    // lcd_send_cmd(spi, 0xE1);
    // lcd_send_data(spi, (uint8_t[]){0xD0, 0x0D, 0x14, 0x0B, 0x0B, 0x07, 0x3A, 0x44, 0x50, 0x08, 0x13, 0x13, 0x2D, 0x32}, 14);
}

/**
 * @brief 设定要在LCD上写入的矩形区域
 *
 * @param spi SPI句柄
 * @param x0 矩形的左上角横坐标
 * @param y0 矩阵的左上角纵坐标
 * @param x1 矩形的右下角横坐标
 * @param y1 矩形的右下角纵坐标
 */
static void lcd_set_rect(spi_device_handle_t spi, uint16_t x0, uint16_t y0,
                         uint16_t x1, uint16_t y1)
{
    x0 += LCD_X_OFFSET;
    x1 += LCD_X_OFFSET;
    y0 += LCD_Y_OFFSET;
    y1 += LCD_Y_OFFSET;
    /* 由于存储在静态全局区，因此每次传输都需要初始化SPI传输指令结构体 */
    memset(lcd_trans, 0, sizeof(spi_transaction_t) * 5); // 前5个传输用于设置区域命令

    /* 第1次传输：指令，设置Column地址 */
    lcd_trans[0].tx_data[0] = 0x2A;
    lcd_trans[0].length = 8;                   // 传输长度：1字节8位
    lcd_trans[0].user = (void *)0;             // D/C=0，指令
    lcd_trans[0].flags = SPI_TRANS_USE_TXDATA; // 发送tx_data内的数据
    ESP_ERROR_CHECK(
        spi_device_queue_trans(spi, &lcd_trans[0], 0) // 队列长度足够，不阻塞直接发送，提高速度
    );

    /* 第2次传输：数据，Column范围 */
    lcd_trans[1].tx_data[0] = x0 >> 8;   // 起始地址高8位
    lcd_trans[1].tx_data[1] = x0 & 0xFF; // 起始地址高8位
    lcd_trans[1].tx_data[2] = x1 >> 8;   // 结束地址高8位
    lcd_trans[1].tx_data[3] = x1 & 0xFF; // 结束地址高8位
    lcd_trans[1].length = 4 * 8;
    lcd_trans[1].user = (void *)1; // D/C=1，数据
    lcd_trans[1].flags = SPI_TRANS_USE_TXDATA;
    ESP_ERROR_CHECK(
        spi_device_queue_trans(spi, &lcd_trans[1], 0));

    /* 第3次传输：指令，设置Page地址 */
    lcd_trans[2].tx_data[0] = 0x2B;
    lcd_trans[2].length = 8;
    lcd_trans[2].user = (void *)0;
    lcd_trans[2].flags = SPI_TRANS_USE_TXDATA;
    ESP_ERROR_CHECK(
        spi_device_queue_trans(spi, &lcd_trans[2], 0));

    /* 第4次传输：数据，Page范围 */
    lcd_trans[3].tx_data[0] = y0 >> 8;
    lcd_trans[3].tx_data[1] = y0 & 0xFF;
    lcd_trans[3].tx_data[2] = y1 >> 8;
    lcd_trans[3].tx_data[3] = y1 & 0xFF;
    lcd_trans[3].length = 4 * 8;
    lcd_trans[3].user = (void *)1;
    lcd_trans[3].flags = SPI_TRANS_USE_TXDATA;
    ESP_ERROR_CHECK(
        spi_device_queue_trans(spi, &lcd_trans[3], 0));

    /* 第5次传输：指令，开始写入图像数据 */
    lcd_trans[4].tx_data[0] = 0x2C;
    lcd_trans[4].length = 8;
    lcd_trans[4].user = (void *)0;
    lcd_trans[4].flags = SPI_TRANS_USE_TXDATA;
    ESP_ERROR_CHECK(
        spi_device_queue_trans(spi, &lcd_trans[4], 0));
}

/**
 * @brief 在LCD上绘制矩形区域，使用DMA传输
 *
 * @param spi SPI句柄
 * @param x0 矩形的左上角横坐标
 * @param y0 矩阵的左上角纵坐标
 * @param x1 矩形的右下角横坐标
 * @param y1 矩形的右下角纵坐标
 * @param linedata 要显示的数据，必须存储在内部RAM中，否则idf会重新申请内存并拷贝数据
 */
void lcd_draw_rect(spi_device_handle_t spi, uint16_t x0, uint16_t y0, uint16_t x1,
                   uint16_t y1, const uint8_t *dat)
{
    /* 由于存储在静态全局区，因此每次传输都需要初始化SPI传输指令结构体 */
    memset(&lcd_trans[5], 0, sizeof(spi_transaction_t)); // 第6个传输用于传输矩形的显示数据

    /* SPI使用DMA传输，而DMA只支持内部RAM作为buffer。若传入的指针为非内部RAM区（PSRAM/ROM），
       则idf会自动重新申请一段位于DMA可用区域的内存，并将传入的数据复制到新申请的内存中。
       该内存在spi_device_get_trans_result函数中被释放，因此传输完成后必须调用传输结束函数。
     */
    // if (!esp_ptr_dma_capable(dat))
    // {
    //     ESP_LOGV(TAG, "data at 0x%08X is not capable for dma transfer, malloc new buffer and copy", (uint32_t)dat);
    // }

    /* 设置写入矩形区域 */
    lcd_set_rect(spi, x0, y0, x1, y1);

    /* 第6次传输：数据，图像数据 */
    lcd_trans[5].tx_buffer = dat;
    lcd_trans[5].length = ((x1 - x0 + 1) * (y1 - y0 + 1) * 2) * 8;
    lcd_trans[5].user = (void *)1;
    lcd_trans[5].flags = 0; // 发送tx_buffer指向的地址
    ESP_ERROR_CHECK(
        spi_device_queue_trans(spi, &lcd_trans[5], 0));

    /* 此处所有指令已经写入队列，SPI控制器正在后台使用DMA进行发送。在下一次传输之前，请调用
     * lcd_draw_rect_wait_busy函数来等待上次传输完成并释放内存。
     */
}

/**
 * @brief 等待LCD矩形区域绘制完成，并释放内存
 *
 * @param spi SPI句柄
 */
void lcd_draw_rect_wait_busy(spi_device_handle_t spi)
{
    /* 保存传输结果的指令结构体，会返回指向存放接收到数据的指针，由于驱动LCD的SPI总线是只写的，因此
     * 此结构体只是占位之用。
     */
    spi_transaction_t *rtrans;

    /* 查询并阻塞等待所有传输结束 */
    for (int i = 0; i < sizeof(lcd_trans) / sizeof(spi_transaction_t); i++)
    {
        ESP_ERROR_CHECK(
            spi_device_get_trans_result(spi, &rtrans, portMAX_DELAY));
    }
}

/**
 * @brief 开关LCD的显示
 *
 * @param spi SPI句柄
 * @param status 真或假控制LCD的开或关
 */
void lcd_display_switch(spi_device_handle_t spi, bool status)
{
    if (status)
    {
        lcd_send_cmd(spi, 0x29); // 开显示
        ESP_LOGW(TAG, "lcd display on");
    }
    else
    {
        lcd_send_cmd(spi, 0x29); // 关显示
        ESP_LOGW(TAG, "lcd display off");
    }
}

/**
 * @brief 初始化LCD
 *
 * @return spi_device_handle_t 用于控制LCD的SPI句柄
 */
spi_device_handle_t lcd_init()
{
    /* 初始化总线 */
    spi_device_handle_t spi = lcd_bus_init();

    /* 初始化LCD控制器寄存器 */
    lcd_reg_init(spi);

    ESP_LOGI(TAG, "lcd bus&regs initialized");
    return spi;
}
