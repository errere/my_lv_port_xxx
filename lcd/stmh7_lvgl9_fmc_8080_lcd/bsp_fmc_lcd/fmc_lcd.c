#include "fmc_lcd.h"

#include "FreeRTOS.h"
#include "task.h"
#include "fmc.h"
#include "stm32h7xx_hal_sram.h"

#include "gpio.h"
#include "stm32h7xx_hal_gpio.h"

#include "string.h"

#include "dma.h"
#include "stm32h7xx_hal_dma.h"

// dummy write(nwe pulse)
// https://community.st.com/t5/stm32-mcus-touchgfx-and-gui/stm32h743ii-fmc-8080-lcd-spurious-writes/m-p/354191

/*
我通过将 fmc SRAM 重新映射到 0xC0000000 解决了这个问题；
HAL_SetFMCMemorySwappingConfig（FMC_SWAPBMAP_SDRAM_SRAM）；
为什么这样做有效？
0xC0000000 下的内存作为设备内存访问，
而 0xC0000000 下的内存则作为设备内存访问。
0x60000000作为普通内存访问。
“普通内存允许 CPU 以高效的方式安排字节、半字和字的加载和存储（编译器不知道内存区域类型）。”
您可以参考 AN4838 和 AN4839 了解更多信息。
*/

// https://zhuanlan.zhihu.com/p/678451478

/* LCD_HANDLE 地址结构体 */
typedef struct
{
    __IO uint16_t LCD_REG;
    __IO uint16_t LCD_RAM;
} FMC_LCD_HandleTypeDef;

#define FMC_SRAM_BASE 0xC0000000 // 0X60000000
#define LCD_FMC_NEX 1
#define LCD_FMC_AX 21
#define LCD_BASE (uint32_t)((FMC_SRAM_BASE + (0X4000000 * (LCD_FMC_NEX - 1))) | (((1 << LCD_FMC_AX) * 2) - 2))
#define LCD_HANDLE ((FMC_LCD_HandleTypeDef *)LCD_BASE)

#define LCD_RST_GPIO_PORT GPIOD
#define LCD_RST_GPIO_PIN GPIO_PIN_6

#define LCD_X_OFFSET 0
#define LCD_Y_OFFSET 0

#define FMC_LCD_DELAY(x) vTaskDelay(pdMS_TO_TICKS(x))

// dma stm
volatile static uint8_t SPI_BUSY;
volatile static uint16_t *bf;
volatile static uint8_t lcd_dma_turn;   // dma transmit trigger times
volatile static uint16_t transmit_tail; // last dma transmit which length less than 0xffff
static void (*dma_done_cplt)() = NULL;

static inline void lcd_trans_config_poll(uint16_t cmd, uint16_t *data_buf, size_t data_len)
{
    cmd = cmd; /* 使用-O2优化的时候,必须插入的延时 */
    LCD_HANDLE->LCD_REG = cmd;

    if (data_len)
    {
        for (size_t i = 0; i < data_len; i++)
        {
            data_buf[i] = data_buf[i]; /* 使用-O2优化的时候,必须插入的延时 */
            LCD_HANDLE->LCD_RAM = data_buf[i];
        }
    }
}

static inline void lcd_trans_data_poll(uint16_t *data_buf, size_t data_len)
{

    for (size_t i = 0; i < data_len; i++)
    {
        data_buf[i] = data_buf[i]; /* 使用-O2优化的时候,必须插入的延时 */
        LCD_HANDLE->LCD_RAM = data_buf[i];
    }
}

static inline void fmc_lcd_set_window(int x_start, int y_start, int x_end, int y_end)
{

    x_start += LCD_X_OFFSET;
    y_start += LCD_Y_OFFSET;

    // define an area of frame memory where MCU can access
    lcd_trans_config_poll(0x2a, (uint16_t[]){
                                      (x_start >> 8) & 0xFF,
                                      x_start & 0xFF,
                                      ((x_end) >> 8) & 0xFF,
                                      (x_end) & 0xFF,
                                  },
                            4);
    lcd_trans_config_poll(0x2b, (uint16_t[]){
                                      (y_start >> 8) & 0xFF,
                                      y_start & 0xFF,
                                      ((y_end) >> 8) & 0xFF,
                                      (y_end) & 0xFF,
                                  },
                            4);

    LCD_HANDLE->LCD_REG = 0x2c;
}

void fmc_test()
{

    // for (;;)
    // {
    //     lcd_trans_config_poll(0xffff,(uint16_t[]){0xffff,0xffff},2);
    //     vTaskDelay(1);
    // }

    // fmc_lcd_set_window(10, 10, 100, 100);

    // static uint16_t x[90 * 90];
    // memset(x, 0x00, 90 * 90);
    // lcd_trans_data_poll(x, 90 * 90);

    fmc_lcd_draw_rect_single(0, 0, 480, 320, 0b1111100000000000);
    vTaskDelay(300);
    fmc_lcd_draw_rect_single(0, 0, 480, 320, 0b0000011111100000);
    vTaskDelay(300);
    fmc_lcd_draw_rect_single(0, 0, 480, 320, 0b0000000000011111);
    vTaskDelay(300);
}

static void fmc_lcd_init_reg(void)
{

    lcd_trans_config_poll(0xF0, (uint16_t[]){0xC3}, 1 * sizeof(uint16_t));

    lcd_trans_config_poll(0xF0, (uint16_t[]){0x96}, 1 * sizeof(uint16_t));
    // Memory Access Control
    // vec ,  ic is down
    // lcd_trans_config_poll( 0x36, (uint16_t[]){0x48}, 1*sizeof(uint16_t));
    // vec ,  ic is up
    // lcd_trans_config_poll( 0x36, (uint16_t[]){0x88}, 1*sizeof(uint16_t));
    // hor ,  ic is right
    //lcd_trans_config_poll(0x36, (uint16_t[]){0x28}, 1 * sizeof(uint16_t));
    // hor ,  ic is left
    lcd_trans_config_poll( 0x36, (uint16_t[]){0xe8}, 1*sizeof(uint16_t));

    lcd_trans_config_poll(0x3A, (uint16_t[]){0x55}, 1 * sizeof(uint16_t));
    lcd_trans_config_poll(0xB4, (uint16_t[]){0x01}, 1 * sizeof(uint16_t));

    lcd_trans_config_poll(0xB1, (uint16_t[]){0x80, 0x10}, 2 * sizeof(uint16_t));
    lcd_trans_config_poll(0xB5, (uint16_t[]){0x1F, 0x50, 0x00, 0x20}, 4 * sizeof(uint16_t));
    lcd_trans_config_poll(0xB6, (uint16_t[]){0x8A, 0x07, 0x3B}, 3 * sizeof(uint16_t));

    lcd_trans_config_poll(0xC0, (uint16_t[]){0x80, 0x64}, 2 * sizeof(uint16_t));

    lcd_trans_config_poll(0xC1, (uint16_t[]){0x13}, 1 * sizeof(uint16_t));

    lcd_trans_config_poll(0xC2, (uint16_t[]){0xA7}, 1 * sizeof(uint16_t));

    lcd_trans_config_poll(0xC5, (uint16_t[]){0x09}, 1 * sizeof(uint16_t));

    lcd_trans_config_poll(0xE8, (uint16_t[]){0x40, 0x8a, 0x00, 0x00, 0x29, 0x19, 0xA5, 0x33}, 8 * sizeof(uint16_t));

    lcd_trans_config_poll(0xE0, (uint16_t[]){0xF0, 0x06, 0x0B, 0x07, 0x06, 0x05, 0x2E, 0x33, 0x47, 0x3A, 0x17, 0x16, 0x2E, 0x31}, 14 * sizeof(uint16_t));

    lcd_trans_config_poll(0xE1, (uint16_t[]){0xF0, 0x09, 0x0D, 0x09, 0x08, 0x23, 0x2E, 0x33, 0x46, 0x38, 0x13, 0x13, 0x2C, 0x32}, 14 * sizeof(uint16_t));

    lcd_trans_config_poll(0xF0, (uint16_t[]){0x3C}, 1 * sizeof(uint16_t));

    lcd_trans_config_poll(0xF0, (uint16_t[]){0x69}, 1 * sizeof(uint16_t));

    lcd_trans_config_poll(0x2A, (uint16_t[]){0x00, 0x00, 0x01, 0x3F}, 4 * sizeof(uint16_t));

    lcd_trans_config_poll(0x2B, (uint16_t[]){0x00, 0x00, 0x01, 0xDF}, 4 * sizeof(uint16_t));

    lcd_trans_config_poll(0x35, (uint16_t[]){0x00}, 1 * sizeof(uint16_t));

    lcd_trans_config_poll(0x11, NULL, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
    lcd_trans_config_poll(0x29, NULL, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
    lcd_trans_config_poll(0x21, NULL, 0);
}

static void fmc_lcd_gpio_reset()
{
    HAL_GPIO_WritePin(LCD_RST_GPIO_PORT, LCD_RST_GPIO_PIN, 0);
    FMC_LCD_DELAY(10);
    HAL_GPIO_WritePin(LCD_RST_GPIO_PORT, LCD_RST_GPIO_PIN, 1);
}



static void fmc_lcd_dma_cplt_isr_cb(DMA_HandleTypeDef *_hdma)
{
    if (_hdma == &hdma_memtomem_dma2_stream0)
    {
        if (SPI_BUSY > lcd_dma_turn)
        {
            // elog_i("i","finish");
            SPI_BUSY = 0;
            bf = NULL;
            // elog_d("lcd","ok");
            if (dma_done_cplt != NULL)
            {
                dma_done_cplt();
            }
        }
        else if (SPI_BUSY == lcd_dma_turn)
        {
            // last(data <= 0xffff)
            if (transmit_tail == 0)
            {
                // data == 0xffff
                HAL_DMA_Start_IT(&hdma_memtomem_dma2_stream0, (uint32_t)(bf), (uint32_t)(&LCD_HANDLE->LCD_RAM), 0xffff);
                // HAL_SPI_Transmit_DMA(&BSP_LCD_HSPI, (uint8_t *)bf, 0xffff);
                bf += 0xffff;
                SPI_BUSY++;
            }
            else
            {
                // data < 0xffff
                HAL_DMA_Start_IT(&hdma_memtomem_dma2_stream0, (uint32_t)(bf), (uint32_t)(&LCD_HANDLE->LCD_RAM), transmit_tail);
                // HAL_SPI_Transmit_DMA(&BSP_LCD_HSPI, (uint8_t *)bf, transmit_tail);
                bf += transmit_tail;
                SPI_BUSY++;
            }
        }
        else
        {
            // data > 0xffff
            HAL_DMA_Start_IT(&hdma_memtomem_dma2_stream0, (uint32_t)(bf), (uint32_t)(&LCD_HANDLE->LCD_RAM), 0xffff);
            // HAL_SPI_Transmit_DMA(&BSP_LCD_HSPI, (uint8_t *)bf, 0xffff);
            bf += 0xffff;
            SPI_BUSY++;
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void fmc_lcd_init()
{
    HAL_DMA_RegisterCallback(&hdma_memtomem_dma2_stream0, HAL_DMA_XFER_CPLT_CB_ID, fmc_lcd_dma_cplt_isr_cb);
    fmc_lcd_gpio_reset();
    FMC_LCD_DELAY(100);
    fmc_lcd_init_reg();
}

void fmc_lcd_draw_rect_single(int x_start, int y_start, int x_end, int y_end, const uint16_t c)
{

    x_start += LCD_X_OFFSET;
    y_start += LCD_Y_OFFSET;

    // define an area of frame memory where MCU can access
    lcd_trans_config_poll(0x2a, (uint16_t[]){
                                      (x_start >> 8) & 0xFF,
                                      x_start & 0xFF,
                                      ((x_end) >> 8) & 0xFF,
                                      (x_end) & 0xFF,
                                  },
                            4);
    lcd_trans_config_poll(0x2b, (uint16_t[]){
                                      (y_start >> 8) & 0xFF,
                                      y_start & 0xFF,
                                      ((y_end) >> 8) & 0xFF,
                                      (y_end) & 0xFF,
                                  },
                            4);

    size_t len = (x_end - x_start + 1) * (y_end - y_start + 1);
    LCD_HANDLE->LCD_REG = 0x2c;
    // transfer frame buffer
    for (size_t i = 0; i < len; i++)
    {
        volatile uint16_t tmp = c; /* 使用-O2优化的时候,必须插入的延时 */
        LCD_HANDLE->LCD_RAM = tmp;
    }
}

void fmc_lcd_draw_rect(int x_start, int y_start, int x_end, int y_end, uint16_t *c)
{

    x_start += LCD_X_OFFSET;
    y_start += LCD_Y_OFFSET;

    // define an area of frame memory where MCU can access
    lcd_trans_config_poll(0x2a, (uint16_t[]){
                                      (x_start >> 8) & 0xFF,
                                      x_start & 0xFF,
                                      ((x_end) >> 8) & 0xFF,
                                      (x_end) & 0xFF,
                                  },
                            4);
    lcd_trans_config_poll(0x2b, (uint16_t[]){
                                      (y_start >> 8) & 0xFF,
                                      y_start & 0xFF,
                                      ((y_end) >> 8) & 0xFF,
                                      (y_end) & 0xFF,
                                  },
                            4);

    size_t transmit_length = (x_end - x_start + 1) * (y_end - y_start + 1);
    LCD_HANDLE->LCD_REG = 0x2c;

    // transfer frame buffer
    for (size_t i = 0; i < transmit_length; i++)
    {
        volatile uint16_t tmp = c[i]; /* 使用-O2优化的时候,必须插入的延时 */
        LCD_HANDLE->LCD_RAM = tmp;
    }
}

void fmc_lcd_draw_rect_DMA(int x_start, int y_start, int x_end, int y_end, uint16_t *c)
{
    fmc_lcd_wait_dma();

    x_start += LCD_X_OFFSET;
    y_start += LCD_Y_OFFSET;

    // define an area of frame memory where MCU can access
    lcd_trans_config_poll(0x2a, (uint16_t[]){
                                      (x_start >> 8) & 0xFF,
                                      x_start & 0xFF,
                                      ((x_end) >> 8) & 0xFF,
                                      (x_end) & 0xFF,
                                  },
                            4);
    lcd_trans_config_poll(0x2b, (uint16_t[]){
                                      (y_start >> 8) & 0xFF,
                                      y_start & 0xFF,
                                      ((y_end) >> 8) & 0xFF,
                                      (y_end) & 0xFF,
                                  },
                            4);

    size_t transmit_length = (x_end - x_start + 1) * (y_end - y_start + 1);

    LCD_HANDLE->LCD_REG = 0x2c; // start write cmd

    lcd_dma_turn = transmit_length / 0xffff; // 0xffff turns

    transmit_tail = transmit_length % 0xffff; // less 0xffff turns

    // have less 0xffff turn
    if (transmit_tail != 0)
    {
        lcd_dma_turn++;
    }

    SPI_BUSY = 1;

    // no data
    if (transmit_length == 0)
    {
        SPI_BUSY = 0;
        return;
    }

    bf = (volatile uint16_t*)c;
    // SCB_CleanDCache();
    if (lcd_dma_turn == 1)
    {
        // last(data <= 0xffff)
        if (transmit_tail == 0)
        {
            // data == 0xffff
            HAL_DMA_Start_IT(&hdma_memtomem_dma2_stream0, (uint32_t)(bf), (uint32_t)(&LCD_HANDLE->LCD_RAM), 0xffff);
            // HAL_SPI_Transmit_DMA(&BSP_LCD_HSPI, (uint8_t *)bf, 0xffff);
            bf += 0xffff;
            SPI_BUSY++;
        }
        else
        {
            // data < 0xffff
            HAL_DMA_Start_IT(&hdma_memtomem_dma2_stream0, (uint32_t)(bf), (uint32_t)(&LCD_HANDLE->LCD_RAM), transmit_tail);
            // HAL_SPI_Transmit_DMA(&BSP_LCD_HSPI, (uint8_t *)bf, transmit_tail);
            bf += transmit_tail;
            SPI_BUSY++;
        }
    }
    else
    {
        // data > 0xffff
        HAL_DMA_Start_IT(&hdma_memtomem_dma2_stream0, (uint32_t)(bf), (uint32_t)(&LCD_HANDLE->LCD_RAM), 0xffff);
        // HAL_SPI_Transmit_DMA(&BSP_LCD_HSPI, (uint8_t *)bf, 0xffff);
        bf += 0xffff;
        SPI_BUSY++;
    }

    // // transfer frame buffer
    // for (size_t i = 0; i < transmit_length; i++)
    // {
    //     volatile uint16_t tmp = c[i]; /* 使用-O2优化的时候,必须插入的延时 */
    //     LCD_HANDLE->LCD_RAM = tmp;
    // }
}

void fmc_lcd_wait_dma()
{
    while (SPI_BUSY != 0)
    {
        /* code */
        // elog_i("i","%d",SPI_BUSY);
        // __LCD_DELAY_MS(1);
    }
}

void lcd_set_dma_cplt_cb(void (*cplt)())
{
    dma_done_cplt = cplt;
}

// eof
