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
	trans.length = 8;		// 指令为1字节8位
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
	gpio_set_direction(LCD_PIN_CS, GPIO_MODE_OUTPUT);

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
		.clock_speed_hz = LCD_SPI_CLOCK_MHZ * 1000000,								 // SPI总线时钟频率，Hz
		.mode = 0,													 // SPI模式0，CPOL=0，CPHA=0
		.spics_io_num = LCD_PIN_CS,									 // CS引脚
		.queue_size = sizeof(lcd_trans) / sizeof(spi_transaction_t), // 传输队列深度
		.pre_cb = lcd_spi_pre_transfer_callback,					 // SPI每次传输前的回调函数，用于控制D/C脚
	};
	/* 根据lcdcfg的内容将LCD挂载到SPI总线上 */
	ESP_ERROR_CHECK(
		spi_bus_add_device(LCD_SPI_HOST, &lcdcfg, &spi));

	return spi;
}

#define LCD_WR_REG(x) lcd_send_cmd(spi, x)
#define LCD_WR_DATA(x) lcd_send_data(spi, (uint8_t[]){x >> 8, (x & 0xff)}, 2)
#define delay_ms(x) vTaskDelay((x < 10) ? x : pdMS_TO_TICKS(x));
#define LCD_WR_DATA8(x) lcd_send_data(spi, (uint8_t[]){x}, 1)
/**
 * @brief 初始化LCD控制器的寄存器
 *
 * @param spi SPI句柄
 */
static void lcd_reg_init(spi_device_handle_t spi)
{

	gpio_set_level(LCD_PIN_RST, 0);
	delay_ms(100);
	gpio_set_level(LCD_PIN_RST, 1);
	delay_ms(100);

#define USE_HORIZONTAL 0 // 设置横屏或者竖屏显示 0或1为竖屏 2或3为横屏
	//---------Start Initial Code ------//
	LCD_WR_REG(0xff);
	LCD_WR_DATA(0xa5);
	LCD_WR_REG(0xE7); // TE_output_en
	LCD_WR_DATA(0x10);
	LCD_WR_REG(0x35);  // TE_ interface_en
	LCD_WR_DATA(0x00); // 01
	LCD_WR_REG(0X36);  // Memory Access Control
	if (USE_HORIZONTAL == 0)
		LCD_WR_DATA(0xC0);
	else if (USE_HORIZONTAL == 1)
		LCD_WR_DATA(0x00);
	else if (USE_HORIZONTAL == 2)
		LCD_WR_DATA(0xA0);
	else
		LCD_WR_DATA(0x70);
	LCD_WR_REG(0x3A);
	LCD_WR_DATA(0x01); // 01---565/00---666
	LCD_WR_REG(0x40);
	LCD_WR_DATA(0x01); // 01:IPS/00:TN
	LCD_WR_REG(0x41);
	LCD_WR_DATA(0x03); // 01--8bit//03--16bit
	LCD_WR_REG(0x44);  // VBP
	LCD_WR_DATA(0x15);
	LCD_WR_REG(0x45); // VFP
	LCD_WR_DATA(0x15);
	LCD_WR_REG(0x7d); // vdds_trim[2:0]
	LCD_WR_DATA(0x03);

	LCD_WR_REG(0xc1);  // avdd_clp_en avdd_clp[1:0] avcl_clp_en avcl_clp[1:0]
	LCD_WR_DATA(0xbb); // 0xbb 88 a2
	LCD_WR_REG(0xc2);  // vgl_clp_en vgl_clp[2:0]
	LCD_WR_DATA(0x05);
	LCD_WR_REG(0xc3); // vgl_clp_en vgl_clp[2:0]
	LCD_WR_DATA(0x10);
	LCD_WR_REG(0xc6); // avdd_ratio_sel avcl_ratio_sel vgh_ratio_sel[1:0] vgl_ratio_sel[1:0]
	LCD_WR_DATA(0x3e);
	LCD_WR_REG(0xc7); // mv_clk_sel[1:0] avdd_clk_sel[1:0] avcl_clk_sel[1:0]
	LCD_WR_DATA(0x25);
	LCD_WR_REG(0xc8); // VGL_CLK_sel
	LCD_WR_DATA(0x21);
	LCD_WR_REG(0x7a);  // user_vgsp
	LCD_WR_DATA(0x51); // 58
	LCD_WR_REG(0x6f);  // user_gvdd
	LCD_WR_DATA(0x49); // 4F
	LCD_WR_REG(0x78);  // user_gvcl
	LCD_WR_DATA(0x57); // 70
	LCD_WR_REG(0xc9);
	LCD_WR_DATA(0x00);
	LCD_WR_REG(0x67);
	LCD_WR_DATA(0x11);
	// gate_ed
	LCD_WR_REG(0x51); // gate_st_o[7:0]
	LCD_WR_DATA(0x0a);
	LCD_WR_REG(0x52);  // gate_ed_o[7:0]
	LCD_WR_DATA(0x7D); // 7A
	LCD_WR_REG(0x53);  // gate_st_e[7:0]
	LCD_WR_DATA(0x0a);
	LCD_WR_REG(0x54);  // gate_ed_e[7:0]
	LCD_WR_DATA(0x7D); // 7A
	// sorce
	LCD_WR_REG(0x46); // fsm_hbp_o[5:0]
	LCD_WR_DATA(0x0a);
	LCD_WR_REG(0x47); // fsm_hfp_o[5:0]
	LCD_WR_DATA(0x2a);
	LCD_WR_REG(0x48); // fsm_hbp_e[5:0]
	LCD_WR_DATA(0x0a);
	LCD_WR_REG(0x49); // fsm_hfp_e[5:0]
	LCD_WR_DATA(0x1a);
	LCD_WR_REG(0x44); // VBP
	LCD_WR_DATA(0x15);
	LCD_WR_REG(0x45); // VFP
	LCD_WR_DATA(0x15);
	LCD_WR_REG(0x73);
	LCD_WR_DATA(0x08);
	LCD_WR_REG(0x74);
	LCD_WR_DATA(0x10); // 0A
	LCD_WR_REG(0x56);  // src_ld_wd[1:0] src_ld_st[5:0]
	LCD_WR_DATA(0x43);
	LCD_WR_REG(0x57); // pn_cs_en src_cs_st[5:0]
	LCD_WR_DATA(0x42);
	LCD_WR_REG(0x58); // src_cs_p_wd[6:0]
	LCD_WR_DATA(0x3c);
	LCD_WR_REG(0x59); // src_cs_n_wd[6:0]
	LCD_WR_DATA(0x64);
	LCD_WR_REG(0x5a); // src_pchg_st_o[6:0]
	LCD_WR_DATA(0x41);
	LCD_WR_REG(0x5b); // src_pchg_wd_o[6:0]
	LCD_WR_DATA(0x3C);
	LCD_WR_REG(0x5c); // src_pchg_st_e[6:0]
	LCD_WR_DATA(0x02);
	LCD_WR_REG(0x5d); // src_pchg_wd_e[6:0]
	LCD_WR_DATA(0x3c);
	LCD_WR_REG(0x5e); // src_pol_sw[7:0]
	LCD_WR_DATA(0x1f);
	LCD_WR_REG(0x60); // src_op_st_o[7:0]
	LCD_WR_DATA(0x80);
	LCD_WR_REG(0x61); // src_op_st_e[7:0]
	LCD_WR_DATA(0x3f);
	LCD_WR_REG(0x62); // src_op_ed_o[9:8] src_op_ed_e[9:8]
	LCD_WR_DATA(0x21);
	LCD_WR_REG(0x63); // src_op_ed_o[7:0]
	LCD_WR_DATA(0x07);
	LCD_WR_REG(0x64); // src_op_ed_e[7:0]
	LCD_WR_DATA(0xe0);
	LCD_WR_REG(0x65); // chopper
	LCD_WR_DATA(0x02);
	LCD_WR_REG(0xca); // avdd_mux_st_o[7:0]
	LCD_WR_DATA(0x20);
	LCD_WR_REG(0xcb); // avdd_mux_ed_o[7:0]
	LCD_WR_DATA(0x52);
	LCD_WR_REG(0xcc); // avdd_mux_st_e[7:0]
	LCD_WR_DATA(0x10);
	LCD_WR_REG(0xcD); // avdd_mux_ed_e[7:0]
	LCD_WR_DATA(0x42);
	LCD_WR_REG(0xD0); // avcl_mux_st_o[7:0]
	LCD_WR_DATA(0x20);
	LCD_WR_REG(0xD1); // avcl_mux_ed_o[7:0]
	LCD_WR_DATA(0x52);
	LCD_WR_REG(0xD2); // avcl_mux_st_e[7:0]
	LCD_WR_DATA(0x10);
	LCD_WR_REG(0xD3); // avcl_mux_ed_e[7:0]
	LCD_WR_DATA(0x42);
	LCD_WR_REG(0xD4); // vgh_mux_st[7:0]
	LCD_WR_DATA(0x0a);
	LCD_WR_REG(0xD5); // vgh_mux_ed[7:0]
	LCD_WR_DATA(0x32);
	// gammma boe4.3
	LCD_WR_REG(0x80); // gam_vrp0
	LCD_WR_DATA(0x00);
	LCD_WR_REG(0xA0); // gam_VRN0
	LCD_WR_DATA(0x00);
	LCD_WR_REG(0x81);  // gam_vrp1
	LCD_WR_DATA(0x06); // 07
	LCD_WR_REG(0xA1);  // gam_VRN1
	LCD_WR_DATA(0x08); // 06
	LCD_WR_REG(0x82);  // gam_vrp2
	LCD_WR_DATA(0x03); // 02
	LCD_WR_REG(0xA2);  // gam_VRN2
	LCD_WR_DATA(0x03); // 01
	LCD_WR_REG(0x86);  // gam_prp0
	LCD_WR_DATA(0x14); // 11
	LCD_WR_REG(0xA6);  // gam_PRN0
	LCD_WR_DATA(0x14); // 10
	LCD_WR_REG(0x87);  // gam_prp1
	LCD_WR_DATA(0x2C); // 27
	LCD_WR_REG(0xA7);  // gam_PRN1
	LCD_WR_DATA(0x26); // 27
	LCD_WR_REG(0x83);  // gam_vrp3
	LCD_WR_DATA(0x37);
	LCD_WR_REG(0xA3); // gam_VRN3
	LCD_WR_DATA(0x37);
	LCD_WR_REG(0x84); // gam_vrp4
	LCD_WR_DATA(0x35);
	LCD_WR_REG(0xA4); // gam_VRN4
	LCD_WR_DATA(0x35);
	LCD_WR_REG(0x85); // gam_vrp5
	LCD_WR_DATA(0x3f);
	LCD_WR_REG(0xA5); // gam_VRN5
	LCD_WR_DATA(0x3f);
	LCD_WR_REG(0x88);  // gam_pkp0
	LCD_WR_DATA(0x0A); // 0b
	LCD_WR_REG(0xA8);  // gam_PKN0
	LCD_WR_DATA(0x0A); // 0b
	LCD_WR_REG(0x89);  // gam_pkp1
	LCD_WR_DATA(0x13); // 14
	LCD_WR_REG(0xA9);  // gam_PKN1
	LCD_WR_DATA(0x12); // 13
	LCD_WR_REG(0x8a);  // gam_pkp2
	LCD_WR_DATA(0x18); // 1a
	LCD_WR_REG(0xAa);  // gam_PKN2
	LCD_WR_DATA(0x19); // 1a
	LCD_WR_REG(0x8b);  // gam_PKP3
	LCD_WR_DATA(0x0a);
	LCD_WR_REG(0xAb); // gam_PKN3
	LCD_WR_DATA(0x0a);
	LCD_WR_REG(0x8c);  // gam_PKP4
	LCD_WR_DATA(0x17); // 14
	LCD_WR_REG(0xAc);  // gam_PKN4
	LCD_WR_DATA(0x0B); // 08
	LCD_WR_REG(0x8d);  // gam_PKP5
	LCD_WR_DATA(0x1A); // 17
	LCD_WR_REG(0xAd);  // gam_PKN5
	LCD_WR_DATA(0x09); // 07
	LCD_WR_REG(0x8e);  // gam_PKP6
	LCD_WR_DATA(0x1A); // 16 //16
	LCD_WR_REG(0xAe);  // gam_PKN6
	LCD_WR_DATA(0x08); // 06 //13
	LCD_WR_REG(0x8f);  // gam_PKP7
	LCD_WR_DATA(0x1F); // 1B
	LCD_WR_REG(0xAf);  // gam_PKN7
	LCD_WR_DATA(0x00); // 07
	LCD_WR_REG(0x90);  // gam_PKP8
	LCD_WR_DATA(0x08); // 04
	LCD_WR_REG(0xB0);  // gam_PKN8
	LCD_WR_DATA(0x00); // 04
	LCD_WR_REG(0x91);  // gam_PKP9
	LCD_WR_DATA(0x10); // 0A
	LCD_WR_REG(0xB1);  // gam_PKN9
	LCD_WR_DATA(0x06); // 0A

	LCD_WR_REG(0x92);  // gam_PKP10
	LCD_WR_DATA(0x19); // 16
	LCD_WR_REG(0xB2);  // gam_PKN10
	LCD_WR_DATA(0x15); // 15
	LCD_WR_REG(0xff);
	LCD_WR_DATA(0x00);
	LCD_WR_REG(0x11);
	delay_ms(120);

	LCD_WR_REG(0x29);
	delay_ms(20);
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
	lcd_trans[0].length = 8;				   // 传输长度：1字节8位
	lcd_trans[0].user = (void *)0;			   // D/C=0，指令
	lcd_trans[0].flags = SPI_TRANS_USE_TXDATA; // 发送tx_data内的数据
	ESP_ERROR_CHECK(
		spi_device_queue_trans(spi, &lcd_trans[0], 0) // 队列长度足够，不阻塞直接发送，提高速度
	);

	/* 第2次传输：数据，Column范围 */
	lcd_trans[1].tx_data[0] = x0 >> 8;	 // 起始地址高8位
	lcd_trans[1].tx_data[1] = x0 & 0xFF; // 起始地址高8位
	lcd_trans[1].tx_data[2] = x1 >> 8;	 // 结束地址高8位
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
