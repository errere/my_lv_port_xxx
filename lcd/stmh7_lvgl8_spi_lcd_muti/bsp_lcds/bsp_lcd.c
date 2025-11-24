#include "bsp_lcd.h"

#include "FreeRTOS.h"
#include "task.h"

#include "elog.h"

#include <string.h>
#include <stdlib.h>

#define TAG "stlcd"
#define BSP_LCD_MAGIC 0x3c

#define BSP_LCD_HAL_TIMEOUT 0xffff

//!"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefghijklmnopqrstuvwxyz{|}~

#define __LCD_DELAY_MS(x) vTaskDelay(pdMS_TO_TICKS(x));

#define ST_BSP_LCD_RETURN_ON_FALSE(x, ret, tagx, msg, ...) \
    do                                                     \
    {                                                      \
        if (!(x))                                          \
        {                                                  \
            elog_e(tagx, msg, ##__VA_ARGS__);              \
            return ret;                                    \
        }                                                  \
    } while (0)

/*=========global spi callback reg record==========*/
#if USE_HAL_SPI_REGISTER_CALLBACKS == 1
static void LCD_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi);
#endif

typedef struct
{
    SPI_HandleTypeDef *hspix;
    volatile uint8_t *dma_buf_ptr;
    volatile uint8_t dma_turns;
    volatile uint16_t dma_trans_tail;
    volatile uint8_t dma_busy;

    void (*cplt_cb)();

    stm_lcd_gpio_desc_t sw_nss[SW_NSS_MAX];
    uint8_t sw_nss_num;

    uint8_t init_done;
} stlcd_spi_dma_trans_desc_t;

static stlcd_spi_dma_trans_desc_t lcd_dma_trans_desc[SYS_SPI_MAX] __attribute__((section(".nocacheable")));

/*=========internal tools==========*/

static inline void lcd_write_io(stm_lcd_gpio_desc_t iox, uint8_t state)
{
    HAL_GPIO_WritePin(iox.gpiox, iox.pinx, state);
}

static inline void lcd_set_sw_nss(stm_lcd_desc_t *handle, uint8_t state)
{
    if (handle->use_hw_nss == 0)
    {
        lcd_write_io(handle->nss_io, state);
    }
}

static void lcd_hw_reset(stm_lcd_desc_t *handle)
{
    if (handle->rst_io.gpiox != 0x00)
    {
        elog_i(TAG, "lcd hw rst");
        lcd_write_io(handle->rst_io, 1);
        __LCD_DELAY_MS(10);
        lcd_write_io(handle->rst_io, 0);
        __LCD_DELAY_MS(10);
        lcd_write_io(handle->rst_io, 1);
    }
}

static stm_lcd_err_t lcd_send_cmd(stm_lcd_desc_t *handle, const uint8_t cmd)
{
    // elog_i(TAG, "write cmd %02x", cmd);
    lcd_write_io(handle->dc_io, 0);
    lcd_set_sw_nss(handle, 0);

    HAL_StatusTypeDef ret = HAL_SPI_Transmit(handle->lcd_hspi, &cmd, 1, BSP_LCD_HAL_TIMEOUT);

    lcd_set_sw_nss(handle, 1);

    ST_BSP_LCD_RETURN_ON_FALSE((ret == HAL_OK), LCD_ERR_IO, TAG, "write cmd io error %d", ret);

    return LCD_ERR_OK;
}
static stm_lcd_err_t lcd_send_dat(stm_lcd_desc_t *handle, const uint8_t *dat, uint16_t len)
{
    // elog_hexdump("write datas", 8, dat, len);
    lcd_write_io(handle->dc_io, 1);
    lcd_set_sw_nss(handle, 0);

    HAL_StatusTypeDef ret = HAL_SPI_Transmit(handle->lcd_hspi, dat, len, BSP_LCD_HAL_TIMEOUT);

    lcd_set_sw_nss(handle, 1);

    ST_BSP_LCD_RETURN_ON_FALSE((ret == HAL_OK), LCD_ERR_IO, TAG, "poll data io error %d", ret);

    return LCD_ERR_OK;
}

/*=========APIs==========*/

void st_lcd_base_init(void){
    memset(&lcd_dma_trans_desc,0x00,sizeof(lcd_dma_trans_desc));
}

stm_lcd_err_t st_lcd_init(stm_lcd_desc_t *handle, SPI_HandleTypeDef *hspix, uint8_t spi_id,
                          uint8_t use_dma, uint8_t use_hw_nss,
                          uint16_t lcd_res_x, uint16_t lcd_res_y,
                          uint16_t lcd_offset_x, uint16_t lcd_offset_y,
                          stm_lcd_gpio_desc_t dc_pin, stm_lcd_gpio_desc_t rst_pin, stm_lcd_gpio_desc_t nss_pin,
                          void (*spi_cplt)())
{
    ST_BSP_LCD_RETURN_ON_FALSE((handle != NULL), LCD_ERR_CODE, TAG, "assert fail handle null");
    ST_BSP_LCD_RETURN_ON_FALSE((handle->init_ok != BSP_LCD_MAGIC), LCD_ERR_CODE, TAG, "this object is initd");
    ST_BSP_LCD_RETURN_ON_FALSE((spi_id < SYS_SPI_MAX), LCD_ERR_CODE, TAG, "assert fail spi id more than max,id = %d", spi_id);

    memset(handle, 0x00, sizeof(stm_lcd_desc_t));

    handle->lcd_hspi = hspix;
    handle->use_dma = use_dma;
    handle->use_hw_nss = use_hw_nss;
    handle->res_x = lcd_res_x;
    handle->res_y = lcd_res_y;
    handle->nss_io = nss_pin;
    handle->rst_io = rst_pin;
    handle->dc_io = dc_pin;
    handle->spi_id = spi_id;
    handle->offset_x = lcd_offset_x;
    handle->offset_y = lcd_offset_y;

    // check dc pin valid
    ST_BSP_LCD_RETURN_ON_FALSE((handle->dc_io.gpiox != NULL), LCD_ERR_CODE, TAG, "assert fail dc gpio null");
    ST_BSP_LCD_RETURN_ON_FALSE((handle->dc_io.pinx != NULL), LCD_ERR_CODE, TAG, "assert fail dc pin null");

    if (handle->use_hw_nss == 0)
    {
        ST_BSP_LCD_RETURN_ON_FALSE((handle->nss_io.gpiox != NULL), LCD_ERR_CODE, TAG, "assert fail nss gpio null in sw nss mode");
        ST_BSP_LCD_RETURN_ON_FALSE((handle->nss_io.pinx != NULL), LCD_ERR_CODE, TAG, "assert fail nss pin null sw nss mode");
    } // use_hw_nss == 0

    if (handle->use_dma == 1)
    {
        if (lcd_dma_trans_desc[handle->spi_id].init_done != BSP_LCD_MAGIC)
        {
            // init dma desc
            memset(&(lcd_dma_trans_desc[handle->spi_id]), 0x00, sizeof(stlcd_spi_dma_trans_desc_t));
            lcd_dma_trans_desc[handle->spi_id].hspix = handle->lcd_hspi;
            lcd_dma_trans_desc[handle->spi_id].cplt_cb = spi_cplt;
            lcd_dma_trans_desc[handle->spi_id].init_done = BSP_LCD_MAGIC;
        }

        if (lcd_dma_trans_desc[handle->spi_id].sw_nss_num < SW_NSS_MAX)
        {
            lcd_dma_trans_desc[handle->spi_id].sw_nss[lcd_dma_trans_desc[handle->spi_id].sw_nss_num] = handle->nss_io;
            lcd_dma_trans_desc[handle->spi_id].sw_nss_num++;
        }
        else
        {
            elog_e(TAG, "sw nss number %d more than limit%d", lcd_dma_trans_desc[handle->spi_id].sw_nss_num, SW_NSS_MAX);
            return LCD_ERR_CODE;
        }

#if USE_HAL_SPI_REGISTER_CALLBACKS == 1
        HAL_SPI_RegisterCallback(handle->lcd_hspi, HAL_SPI_TX_COMPLETE_CB_ID, LCD_SPI_TxCpltCallback);
#endif
    } // use_dma == 1

    handle->init_ok = BSP_LCD_MAGIC;

    lcd_hw_reset(handle);

    return LCD_ERR_OK;
}

stm_lcd_err_t lcd_set_offset(stm_lcd_desc_t *handle, int16_t x, int16_t y)
{
    ST_BSP_LCD_RETURN_ON_FALSE((handle->init_ok == BSP_LCD_MAGIC), LCD_ERR_CODE, TAG, "this object is not initd");
    handle->offset_x = x;
    handle->offset_y = y;
    return LCD_ERR_OK;
}

/**
 *
 * @brief send cmd sequence with data
 *
 * @param handle a pointer to lcd handle
 * @param seq command sequence format is [[cmd,dat_len,[dats]],.....]
 * @param len command sequence total length
 *
 * @return LCD_ERR_OK is ok or other is error
 *
 */
stm_lcd_err_t lcd_send_sequence(stm_lcd_desc_t *handle, const uint8_t *seq, uint16_t len)
{
    uint16_t send_index = 0;
    ST_BSP_LCD_RETURN_ON_FALSE((seq != NULL), LCD_ERR_CODE, TAG, "assert fail sequence buffer null");
    ST_BSP_LCD_RETURN_ON_FALSE((handle->init_ok == BSP_LCD_MAGIC), LCD_ERR_CODE, TAG, "this object is not initd");
    uint8_t dat_len_want_send = 0;
    for (;;)
    {

        ST_BSP_LCD_RETURN_ON_FALSE((lcd_send_cmd(handle, (*(seq + send_index))) == LCD_ERR_OK), LCD_ERR_IO, TAG, "send seq cmd error"); // send cmd
        send_index++;
        dat_len_want_send = *(seq + send_index);
        send_index++;
        if (dat_len_want_send > 0)
        {
            ST_BSP_LCD_RETURN_ON_FALSE((lcd_send_dat(handle, (seq + send_index), dat_len_want_send) == LCD_ERR_OK), LCD_ERR_IO, TAG, "send seq cmd error"); // write data
            send_index += dat_len_want_send;
        }

        if (send_index >= len - 1)
            break;
    }

    return LCD_ERR_OK;
}

stm_lcd_err_t lcd_get_res(stm_lcd_desc_t *handle, uint16_t *x, uint16_t *y)
{
    ST_BSP_LCD_RETURN_ON_FALSE((handle->init_ok == BSP_LCD_MAGIC), LCD_ERR_CODE, TAG, "this object is not initd");
    *x = handle->res_x;
    *y = handle->res_y;
    return LCD_ERR_OK;
}

stm_lcd_err_t lcd_draw_rect_poll(stm_lcd_desc_t *handle, int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t *buf)
{
    ST_BSP_LCD_RETURN_ON_FALSE((buf != NULL), LCD_ERR_CODE, TAG, "assert fail pixel buffer null");
    ST_BSP_LCD_RETURN_ON_FALSE((handle->init_ok == BSP_LCD_MAGIC), LCD_ERR_CODE, TAG, "this object is not initd");
    x0 += handle->offset_x;
    y0 += handle->offset_y;
    x1 += handle->offset_x;
    y1 += handle->offset_y;
    // x坐标数据
    ST_BSP_LCD_RETURN_ON_FALSE((lcd_send_cmd(handle, 0x2A) == LCD_ERR_OK), LCD_ERR_IO, TAG, "send x cmd error");
    ST_BSP_LCD_RETURN_ON_FALSE((lcd_send_dat(handle, (uint8_t[]){x0 >> 8, x0 & 0xff, x1 >> 8, x1 & 0xff}, 4) == LCD_ERR_OK), LCD_ERR_IO, TAG, "send x cmd error");

    // y坐标数据
    ST_BSP_LCD_RETURN_ON_FALSE((lcd_send_cmd(handle, 0x2B) == LCD_ERR_OK), LCD_ERR_IO, TAG, "send y cmd error");
    ST_BSP_LCD_RETURN_ON_FALSE((lcd_send_dat(handle, (uint8_t[]){y0 >> 8, y0 & 0xff, y1 >> 8, y1 & 0xff}, 4) == LCD_ERR_OK), LCD_ERR_IO, TAG, "send y cmd error");
    // 显存
    ST_BSP_LCD_RETURN_ON_FALSE((lcd_send_cmd(handle, 0x2C) == LCD_ERR_OK), LCD_ERR_IO, TAG, "send gram cmd error");

    uint32_t transmit_length = ((x1 - x0 + 1) * (y1 - y0 + 1) * 2);
    uint16_t turns = transmit_length / 0xffff;
    uint16_t tail = transmit_length % 0xffff;

    for (uint16_t i = 0; i < turns; i++)
    {
        ST_BSP_LCD_RETURN_ON_FALSE((lcd_send_dat(handle, buf, 0xffff) == LCD_ERR_OK), LCD_ERR_IO, TAG, "send gram cmd error");
        buf += 0xffff;
    }

    if (tail)
    {
        ST_BSP_LCD_RETURN_ON_FALSE((lcd_send_dat(handle, buf, tail) == LCD_ERR_OK), LCD_ERR_IO, TAG, "send gram cmd error");
    }
    return LCD_ERR_OK;
}

stm_lcd_err_t lcd_draw_rect_single(stm_lcd_desc_t *handle, int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t px)
{
    ST_BSP_LCD_RETURN_ON_FALSE((handle->init_ok == BSP_LCD_MAGIC), LCD_ERR_CODE, TAG, "this object is not initd");
    x0 += handle->offset_x;
    y0 += handle->offset_y;
    x1 += handle->offset_x;
    y1 += handle->offset_y;
    // x坐标数据
    ST_BSP_LCD_RETURN_ON_FALSE((lcd_send_cmd(handle, 0x2A) == LCD_ERR_OK), LCD_ERR_IO, TAG, "send x cmd error");
    ST_BSP_LCD_RETURN_ON_FALSE((lcd_send_dat(handle, (uint8_t[]){x0 >> 8, x0 & 0xff, x1 >> 8, x1 & 0xff}, 4) == LCD_ERR_OK), LCD_ERR_IO, TAG, "send x cmd error");

    // y坐标数据
    ST_BSP_LCD_RETURN_ON_FALSE((lcd_send_cmd(handle, 0x2B) == LCD_ERR_OK), LCD_ERR_IO, TAG, "send y cmd error");
    ST_BSP_LCD_RETURN_ON_FALSE((lcd_send_dat(handle, (uint8_t[]){y0 >> 8, y0 & 0xff, y1 >> 8, y1 & 0xff}, 4) == LCD_ERR_OK), LCD_ERR_IO, TAG, "send y cmd error");
    // 显存
    ST_BSP_LCD_RETURN_ON_FALSE((lcd_send_cmd(handle, 0x2C) == LCD_ERR_OK), LCD_ERR_IO, TAG, "send gram cmd error");

    uint32_t transmit_length = ((x1 - x0 + 1) * (y1 - y0 + 1) * 2);

    // byte swap
    px = ((px & 0xff00) >> 8) | ((px & 0x00ff) << 8);

    for (uint32_t i = 0; i < transmit_length; i++)
    {
        ST_BSP_LCD_RETURN_ON_FALSE((lcd_send_dat(handle, (const uint8_t *)&px, 2) == LCD_ERR_OK), LCD_ERR_IO, TAG, "send gram cmd error");
    }
    return LCD_ERR_OK;
}

stm_lcd_err_t lcd_draw_pixel(stm_lcd_desc_t *handle, int16_t x0, int16_t y0, uint16_t color)
{
    ST_BSP_LCD_RETURN_ON_FALSE((handle->init_ok == BSP_LCD_MAGIC), LCD_ERR_CODE, TAG, "this object is not initd");
    x0 += handle->offset_x;
    y0 += handle->offset_y;
    // x坐标数据
    ST_BSP_LCD_RETURN_ON_FALSE((lcd_send_cmd(handle, 0x2A) == LCD_ERR_OK), LCD_ERR_IO, TAG, "send x cmd error");
    ST_BSP_LCD_RETURN_ON_FALSE((lcd_send_dat(handle, (uint8_t[]){x0 >> 8, x0 & 0xff, x0 >> 8, x0 & 0xff}, 4) == LCD_ERR_OK), LCD_ERR_IO, TAG, "send x cmd error");

    // y坐标数据
    ST_BSP_LCD_RETURN_ON_FALSE((lcd_send_cmd(handle, 0x2B) == LCD_ERR_OK), LCD_ERR_IO, TAG, "send y cmd error");
    ST_BSP_LCD_RETURN_ON_FALSE((lcd_send_dat(handle, (uint8_t[]){y0 >> 8, y0 & 0xff, y0 >> 8, y0 & 0xff}, 4) == LCD_ERR_OK), LCD_ERR_IO, TAG, "send y cmd error");
    // 显存
    ST_BSP_LCD_RETURN_ON_FALSE((lcd_send_cmd(handle, 0x2C) == LCD_ERR_OK), LCD_ERR_IO, TAG, "send gram cmd error");

    color = ((color & 0xff00) >> 8) | ((color & 0x00ff) << 8);
    ST_BSP_LCD_RETURN_ON_FALSE((lcd_send_dat(handle, (const uint8_t *)&color, 2) == LCD_ERR_OK), LCD_ERR_IO, TAG, "send gram cmd error");
    return LCD_ERR_OK;
}

stm_lcd_err_t lcd_wait_dma_cplt(stm_lcd_desc_t *handle, uint32_t timeout)
{
    uint32_t dma_wait_timer = 0;
    while (lcd_dma_trans_desc[handle->spi_id].dma_busy != 0)
    {
        /* code */
        // elog_i("i","%d",SPI_BUSY);
        dma_wait_timer++;
        if (dma_wait_timer > timeout)
        {
            return LCD_ERR_CODE;
        }
        __LCD_DELAY_MS(1);
    }

    return LCD_ERR_OK;
}

stm_lcd_err_t lcd_draw_rect_DMA(stm_lcd_desc_t *handle, int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t *buf, uint32_t timeout)
{
    ST_BSP_LCD_RETURN_ON_FALSE((buf != NULL), LCD_ERR_CODE, TAG, "assert fail pixel buffer null");
    ST_BSP_LCD_RETURN_ON_FALSE((handle->init_ok == BSP_LCD_MAGIC), LCD_ERR_CODE, TAG, "this object is not initd");
    ST_BSP_LCD_RETURN_ON_FALSE((handle->use_dma != 0), LCD_ERR_CODE, TAG, "dma not enable");

    ST_BSP_LCD_RETURN_ON_FALSE((lcd_wait_dma_cplt(handle, timeout) == LCD_ERR_OK), LCD_ERR_CODE, TAG, "wait dma timeout");

    x0 += handle->offset_x;
    y0 += handle->offset_y;
    x1 += handle->offset_x;
    y1 += handle->offset_y;

    // x坐标数据
    ST_BSP_LCD_RETURN_ON_FALSE((lcd_send_cmd(handle, 0x2A) == LCD_ERR_OK), LCD_ERR_IO, TAG, "send x cmd error");
    ST_BSP_LCD_RETURN_ON_FALSE((lcd_send_dat(handle, (uint8_t[]){x0 >> 8, x0 & 0xff, x1 >> 8, x1 & 0xff}, 4) == LCD_ERR_OK), LCD_ERR_IO, TAG, "send x cmd error");

    // y坐标数据
    ST_BSP_LCD_RETURN_ON_FALSE((lcd_send_cmd(handle, 0x2B) == LCD_ERR_OK), LCD_ERR_IO, TAG, "send y cmd error");
    ST_BSP_LCD_RETURN_ON_FALSE((lcd_send_dat(handle, (uint8_t[]){y0 >> 8, y0 & 0xff, y1 >> 8, y1 & 0xff}, 4) == LCD_ERR_OK), LCD_ERR_IO, TAG, "send y cmd error");
    // 显存
    ST_BSP_LCD_RETURN_ON_FALSE((lcd_send_cmd(handle, 0x2C) == LCD_ERR_OK), LCD_ERR_IO, TAG, "send gram cmd error");

    // prepare gpio for use low level spi api
    lcd_write_io(handle->dc_io, 1);
    lcd_set_sw_nss(handle, 0);

    // lcd_set_sw_nss(handle, 1);

    uint32_t transmit_length = ((x1 - x0 + 1) * (y1 - y0 + 1) * 2);

    lcd_dma_trans_desc[handle->spi_id].dma_turns = transmit_length / 0xffff; // 0xffff turns

    lcd_dma_trans_desc[handle->spi_id].dma_trans_tail = transmit_length % 0xffff; // less 0xffff turns

    // have less 0xffff turn
    if (lcd_dma_trans_desc[handle->spi_id].dma_trans_tail != 0)
    {
        lcd_dma_trans_desc[handle->spi_id].dma_turns++;
    }

    lcd_dma_trans_desc[handle->spi_id].dma_busy = 1;

    // elog_i("dma", "length:%ld , turn : %d , tai : %d", transmit_length, lcd_dma_trans_desc[handle->spi_id].dma_turns, lcd_dma_trans_desc[handle->spi_id].dma_trans_tail);

    // no data
    if (transmit_length == 0)
    {
        lcd_dma_trans_desc[handle->spi_id].dma_busy = 0;
        return LCD_ERR_OK;
    }

    lcd_dma_trans_desc[handle->spi_id].dma_buf_ptr = buf;

    // SCB_CleanDCache(); //use mpu

    if (lcd_dma_trans_desc[handle->spi_id].dma_turns == 1)
    {
        // last(data <= 0xffff)
        if (lcd_dma_trans_desc[handle->spi_id].dma_trans_tail == 0)
        {
            // data == 0xffff
            HAL_SPI_Transmit_DMA(handle->lcd_hspi, (uint8_t *)lcd_dma_trans_desc[handle->spi_id].dma_buf_ptr, 0xffff);
            lcd_dma_trans_desc[handle->spi_id].dma_buf_ptr += 0xffff;
            lcd_dma_trans_desc[handle->spi_id].dma_busy++;
        }
        else
        {
            // data < 0xffff
            HAL_SPI_Transmit_DMA(handle->lcd_hspi, (uint8_t *)lcd_dma_trans_desc[handle->spi_id].dma_buf_ptr, lcd_dma_trans_desc[handle->spi_id].dma_trans_tail);
            lcd_dma_trans_desc[handle->spi_id].dma_buf_ptr += lcd_dma_trans_desc[handle->spi_id].dma_trans_tail;
            lcd_dma_trans_desc[handle->spi_id].dma_busy++;
        }
    }
    else
    {
        // data > 0xffff
        HAL_SPI_Transmit_DMA(handle->lcd_hspi, (uint8_t *)lcd_dma_trans_desc[handle->spi_id].dma_buf_ptr, 0xffff);
        lcd_dma_trans_desc[handle->spi_id].dma_buf_ptr += 0xffff;
        lcd_dma_trans_desc[handle->spi_id].dma_busy++;
    }

    return LCD_ERR_OK;
}

#if USE_HAL_SPI_REGISTER_CALLBACKS == 1
static void LCD_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
#else
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
#endif
{
    uint8_t search_ok = 0;
    uint8_t index = 0;
    // search SPI status
    for (index = 0; index < SYS_SPI_MAX; index++)
    {
        if (lcd_dma_trans_desc[index].hspix == hspi)
        {
            // elog_i("dma", "find %d turn : %d , tai : %d", index, lcd_dma_trans_desc[index].dma_turns, lcd_dma_trans_desc[index].dma_trans_tail);
            search_ok = 1;
            break;
        }
    }
    if (search_ok == 0)
    {
        // spi state not found
        return;
    }

    if (lcd_dma_trans_desc[index].dma_busy > lcd_dma_trans_desc[index].dma_turns)
    {
        // elog_i("i", "finish %d", index);
        lcd_dma_trans_desc[index].dma_busy = 0;
        lcd_dma_trans_desc[index].dma_buf_ptr = NULL;

        // unselect all display in this bus
        for (uint8_t x = 0; x < lcd_dma_trans_desc[index].sw_nss_num; x++)
        {
            lcd_write_io(lcd_dma_trans_desc[index].sw_nss[x], 1);
        }

        if (lcd_dma_trans_desc[index].cplt_cb != NULL)
        {
            lcd_dma_trans_desc[index].cplt_cb();
        }
    }
    else if (lcd_dma_trans_desc[index].dma_busy == lcd_dma_trans_desc[index].dma_turns)
    {
        // last(data <= 0xffff)
        if (lcd_dma_trans_desc[index].dma_trans_tail == 0)
        {
            // data == 0xffff
            HAL_SPI_Transmit_DMA(lcd_dma_trans_desc[index].hspix, (const uint8_t *)lcd_dma_trans_desc[index].dma_buf_ptr, 0xffff);
            lcd_dma_trans_desc[index].dma_buf_ptr += 0xffff;
            lcd_dma_trans_desc[index].dma_busy++;
        }
        else
        {
            // data < 0xffff
            HAL_SPI_Transmit_DMA(lcd_dma_trans_desc[index].hspix, (const uint8_t *)lcd_dma_trans_desc[index].dma_buf_ptr, lcd_dma_trans_desc[index].dma_trans_tail);
            lcd_dma_trans_desc[index].dma_buf_ptr += lcd_dma_trans_desc[index].dma_trans_tail;
            lcd_dma_trans_desc[index].dma_busy++;
        }
    }
    else
    {
        // data > 0xffff
        HAL_SPI_Transmit_DMA(lcd_dma_trans_desc[index].hspix, (const uint8_t *)lcd_dma_trans_desc[index].dma_buf_ptr, 0xffff);
        lcd_dma_trans_desc[index].dma_buf_ptr += 0xffff;
        lcd_dma_trans_desc[index].dma_busy++;
    }
}
