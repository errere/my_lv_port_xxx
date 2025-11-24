#include "bsp_lcd.h"

#include "FreeRTOS.h"
#include "task.h"

#include "gpio.h"
#include "spi.h"

#include "stm32h7xx_hal_gpio.h"
#include "stm32h7xx_hal_spi.h"

#include "elog.h"

#define BSP_LCD_RST_GPIO_HANDLE GPIOE
#define BSP_LCD_RST_PIN GPIO_PIN_5
#define BSP_LCD_DC_GPIO_HANDLE GPIOE
#define BSP_LCD_DC_PIN GPIO_PIN_3

#define BSP_LCD_HSPI hspi4

// #include "elog.h"

//!"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefghijklmnopqrstuvwxyz{|}~

#define __LCD_DELAY_MS(x) vTaskDelay(pdMS_TO_TICKS(x));

volatile static uint8_t SPI_BUSY;
volatile static uint8_t *bf;

volatile static uint8_t lcd_dma_turn;   // dma transmit trigger times
volatile static uint16_t transmit_tail; // last dma transmit which length less than 0xffff

static void (*spi_cplt)() = NULL;

static void bsp_lcd_reset()
{
    HAL_GPIO_WritePin(BSP_LCD_RST_GPIO_HANDLE, BSP_LCD_RST_PIN, 0);
    __LCD_DELAY_MS(100);
    HAL_GPIO_WritePin(BSP_LCD_RST_GPIO_HANDLE, BSP_LCD_RST_PIN, 1);
}

static void bsp_lcd_send_data(const uint8_t *dat, uint16_t len)
{
    HAL_GPIO_WritePin(BSP_LCD_DC_GPIO_HANDLE, BSP_LCD_DC_PIN, 1);
    HAL_SPI_Transmit(&BSP_LCD_HSPI, dat, len, 0xffff);
}

static void bsp_lcd_send_cmd(const uint8_t cmd)
{
    HAL_GPIO_WritePin(BSP_LCD_DC_GPIO_HANDLE, BSP_LCD_DC_PIN, 0);
    HAL_SPI_Transmit(&BSP_LCD_HSPI, (uint8_t[]){cmd}, 1, 0xffff);
}

void bsp_lcd_init()
{
    transmit_tail = 0;
    lcd_dma_turn = 0;
    SPI_BUSY = 0;
    bf = NULL;
    // st7789
    elog_w("st7789", "this code modify for hor layout");
    bsp_lcd_reset();
    __LCD_DELAY_MS(120);
    /* 退出睡眠模式 */
    bsp_lcd_send_cmd(0x11);
    // vTaskDelay(pdMS_TO_TICKS(120));
    __LCD_DELAY_MS(120);
    /* 显存访问控制 */
    bsp_lcd_send_cmd(0x36);
    //  bsp_lcd_send_data((uint8_t[]){0x60}, 1); // MX=MV=1, MY=ML=MH=0, RGB=0 横屏, 芯片在右
    bsp_lcd_send_data((uint8_t[]){0xA0}, 1); // MY=MV=1, MX=ML=MH=0, RGB=0 横屏，芯片在左
    // bsp_lcd_send_data((uint8_t[]){0x00}, 1); // MX=MV=MY=ML=MH=0, RGB=0 竖屏，芯片在下
    /* 像素格式 */
    bsp_lcd_send_cmd(0x3A);
    bsp_lcd_send_data((uint8_t[]){0x05}, 1); // MCU mode, 16bit/pixel
    /* Porch设置 */
    bsp_lcd_send_cmd(0xB2);
    bsp_lcd_send_data((uint8_t[]){0x0C, 0x0C, 0x00, 0x33, 0x33}, 5);
    /* Gate设置 */
    bsp_lcd_send_cmd(0xB7);
    bsp_lcd_send_data((uint8_t[]){0x35}, 1); // Vgh=13.26V, Vgl=-10.43V
    /* VCOM设置 */
    bsp_lcd_send_cmd(0xBB);
    bsp_lcd_send_data((uint8_t[]){0x32}, 1); // VCOM=0.725V
    /* LCN设置 */
    // bsp_lcd_send_cmd( 0xC0);
    // bsp_lcd_send_data( (uint8_t[]){0x2C}, 1); //XOR: BGR, MX, MH
    /* VDV与VRH写使能 */
    bsp_lcd_send_cmd(0xC2);
    bsp_lcd_send_data((uint8_t[]){0x01}, 1); // CMDEN=1
    /* VRH设置 */
    bsp_lcd_send_cmd(0xC3);
    bsp_lcd_send_data((uint8_t[]){0x15}, 1); // Vrh=4.45+
    /* VDV设置 */
    bsp_lcd_send_cmd(0xC4);
    bsp_lcd_send_data((uint8_t[]){0x20}, 1); // Vdv=0
    /* 刷新率设置 */
    bsp_lcd_send_cmd(0xC6);
    bsp_lcd_send_data((uint8_t[]){0x0F}, 1); // 60Hz, no column inversion
    /* 电源控制1 */
    bsp_lcd_send_cmd(0xD0);
    bsp_lcd_send_data((uint8_t[]){0xA4, 0xA1}, 2); // AVDD=6.8V, AVCL=-4.8V, VDDS=2.3V
    /* 正电压伽马控制 */
    bsp_lcd_send_cmd(0xE0);
    bsp_lcd_send_data((uint8_t[]){0xd0, 0x08, 0x0e, 0x09, 0x09, 0x05, 0x31, 0x33, 0x48, 0x17, 0x14, 0x15, 0x31, 0x34}, 14);
    /* 负电压伽马控制 */
    bsp_lcd_send_cmd(0xE1);
    bsp_lcd_send_data((uint8_t[]){0xd0, 0x08, 0x0e, 0x09, 0x09, 0x05, 0x31, 0x33, 0x48, 0x17, 0x14, 0x15, 0x31, 0x34}, 14);
    /* IPS屏幕需要设置屏幕反显才能显示正确的颜色 */
    bsp_lcd_send_cmd(0x21); // 开启反显
    // bsp_lcd_send_cmd(0x20); // 关闭反显

    bsp_lcd_send_cmd(0x29);

    __LCD_DELAY_MS(200);
}

uint8_t bsp_lcd_draw_pixel(int16_t x0, int16_t y0, uint16_t color)
{
    // x坐标数据
    bsp_lcd_send_cmd(0x2A);
    bsp_lcd_send_data((uint8_t[]){x0 >> 8, x0 & 0xff, x0 >> 8, x0 & 0xff}, 4);
    // y坐标数据
    bsp_lcd_send_cmd(0x2B);
    bsp_lcd_send_data((uint8_t[]){y0 >> 8, y0 & 0xff, y0 >> 8, y0 & 0xff}, 4);
    // 显存
    bsp_lcd_send_cmd(0x2C);
    HAL_GPIO_WritePin(BSP_LCD_DC_GPIO_HANDLE, BSP_LCD_DC_PIN, 1);

    color = ((color & 0xff00) >> 8) | ((color & 0x00ff) << 8);

    bsp_lcd_send_data((const uint8_t *)&color, 2);
    return 0;
}

uint8_t bsp_lcd_draw_rect(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t *buf)
{
    // x坐标数据
    bsp_lcd_send_cmd(0x2A);
    bsp_lcd_send_data((uint8_t[]){x0 >> 8, x0 & 0xff, x1 >> 8, x1 & 0xff}, 4);
    // y坐标数据
    bsp_lcd_send_cmd(0x2B);
    bsp_lcd_send_data((uint8_t[]){y0 >> 8, y0 & 0xff, y1 >> 8, y1 & 0xff}, 4);
    // 显存
    bsp_lcd_send_cmd(0x2C);
    HAL_GPIO_WritePin(BSP_LCD_DC_GPIO_HANDLE, BSP_LCD_DC_PIN, 1);

    uint32_t transmit_length = ((x1 - x0 + 1) * (y1 - y0 + 1) * 2);
    uint16_t turns = transmit_length / 0xffff;
    uint16_t tail = transmit_length % 0xffff;

    for (uint16_t i = 0; i < turns; i++)
    {
        bsp_lcd_send_data(buf, 0xffff);
        buf += 0xffff;
    }

    if (tail)
    {
        bsp_lcd_send_data(buf, tail);
    }
    return 0;
}

uint8_t bsp_lcd_draw_rect_single(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t px)
{
    // x坐标数据
    bsp_lcd_send_cmd(0x2A);
    bsp_lcd_send_data((uint8_t[]){x0 >> 8, x0 & 0xff, x1 >> 8, x1 & 0xff}, 4);
    // y坐标数据
    bsp_lcd_send_cmd(0x2B);
    bsp_lcd_send_data((uint8_t[]){y0 >> 8, y0 & 0xff, y1 >> 8, y1 & 0xff}, 4);
    // 显存
    bsp_lcd_send_cmd(0x2C);
    HAL_GPIO_WritePin(BSP_LCD_DC_GPIO_HANDLE, BSP_LCD_DC_PIN, 1);

    uint32_t transmit_length = ((x1 - x0 + 1) * (y1 - y0 + 1) * 2);

    // byte swap
    px = ((px & 0xff00) >> 8) | ((px & 0x00ff) << 8);

    for (uint32_t i = 0; i < transmit_length; i++)
    {
        bsp_lcd_send_data((const uint8_t *)&px, 2);
    }
    return 0;
}

uint8_t bsp_lcd_draw_rect_DMA(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t *buf)
{
    bsp_lcd_wait_dma_down();
    // x坐标数据
    bsp_lcd_send_cmd(0x2A);
    bsp_lcd_send_data((uint8_t[]){x0 >> 8, x0 & 0xff, x1 >> 8, x1 & 0xff}, 4);
    // y坐标数据
    bsp_lcd_send_cmd(0x2B);
    bsp_lcd_send_data((uint8_t[]){y0 >> 8, y0 & 0xff, y1 >> 8, y1 & 0xff}, 4);
    // 显存
    bsp_lcd_send_cmd(0x2C);
    HAL_GPIO_WritePin(BSP_LCD_DC_GPIO_HANDLE, BSP_LCD_DC_PIN, 1);

    uint32_t transmit_length = ((x1 - x0 + 1) * (y1 - y0 + 1) * 2);

    lcd_dma_turn = transmit_length / 0xffff; // 0xffff turns

    transmit_tail = transmit_length % 0xffff; // less 0xffff turns

    // have less 0xffff turn
    if (transmit_tail != 0)
    {
        lcd_dma_turn++;
    }

    SPI_BUSY = 1;

    // elog_i("dma", "length:%ld , turn : %d , tai : %d", transmit_length, lcd_dma_turn, transmit_tail);

    // no data
    if (transmit_length == 0)
    {
        SPI_BUSY = 0;
        return 0;
    }

    bf = buf;

    if (lcd_dma_turn == 1)
    {
        // last(data <= 0xffff)
        if (transmit_tail == 0)
        {
            // data == 0xffff
            HAL_SPI_Transmit_DMA(&BSP_LCD_HSPI, (uint8_t *)bf, 0xffff);
            bf += 0xffff;
            SPI_BUSY++;
        }
        else
        {
            // data < 0xffff
            HAL_SPI_Transmit_DMA(&BSP_LCD_HSPI, (uint8_t *)bf, transmit_tail);
            bf += transmit_tail;
            SPI_BUSY++;
        }
    }
    else
    {
        // data > 0xffff
        HAL_SPI_Transmit_DMA(&BSP_LCD_HSPI, (uint8_t *)bf, 0xffff);
        bf += 0xffff;
        SPI_BUSY++;
    }

    return 0;
}

uint8_t bsp_lcd_wait_dma_down()
{
    while (SPI_BUSY != 0)
    {
        /* code */
        // elog_i("i","%d",SPI_BUSY);
        // __LCD_DELAY_MS(1);
    }

    return 0;
}

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
    //
    if (hspi == &BSP_LCD_HSPI)
    {
        if (SPI_BUSY > lcd_dma_turn)
        {
            // elog_i("i","finish");
            SPI_BUSY = 0;
            bf = NULL;
            // elog_d("lcd","ok");
            if (spi_cplt != NULL)
            {
                spi_cplt();
            }
        }
        else if (SPI_BUSY == lcd_dma_turn)
        {
            // last(data <= 0xffff)
            if (transmit_tail == 0)
            {
                // data == 0xffff
                HAL_SPI_Transmit_DMA(&BSP_LCD_HSPI, (uint8_t *)bf, 0xffff);
                bf += 0xffff;
                SPI_BUSY++;
            }
            else
            {
                // data < 0xffff
                HAL_SPI_Transmit_DMA(&BSP_LCD_HSPI, (uint8_t *)bf, transmit_tail);
                bf += transmit_tail;
                SPI_BUSY++;
            }
        }
        else
        {
            // data > 0xffff
            HAL_SPI_Transmit_DMA(&BSP_LCD_HSPI, (uint8_t *)bf, 0xffff);
            bf += 0xffff;
            SPI_BUSY++;
        }
    }
}

void lcd_set_dma_cplt_cb(void (*cplt)())
{
    spi_cplt = cplt;
}