#ifndef __BSP_LCD_H__
#define __BSP_LCD_H__

/*NOTE : FIX FOR MUTI LCD*/

#include <stdint.h>
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

#include "gpio.h"
#include "spi.h"

#include "stm32h7xx_hal_spi.h"
#include "stm32h7xx_hal_dma.h"
#include "stm32h7xx_hal_gpio.h"

#define SYS_SPI_MAX 6 // stm32h7 have 6 SPIs
#define SW_NSS_MAX 8  // sw nss pre spi bus
typedef enum
{
    LCD_ERR_OK = 0,
    LCD_ERR_IO = 1,
    LCD_ERR_CODE = 2,
} stm_lcd_err_t;

typedef struct
{
    GPIO_TypeDef *gpiox;
    uint16_t pinx;
} stm_lcd_gpio_desc_t;

typedef struct
{
    // config
    SPI_HandleTypeDef *lcd_hspi;
    uint8_t spi_id;

    uint8_t use_dma;
    uint8_t use_hw_nss;

    uint16_t res_x;
    uint16_t res_y;

    int16_t offset_x;
    int16_t offset_y;

    stm_lcd_gpio_desc_t nss_io;
    stm_lcd_gpio_desc_t dc_io;
    stm_lcd_gpio_desc_t rst_io;

    // status
    uint8_t init_ok;
} stm_lcd_desc_t;


void st_lcd_base_init(void);

/**
 *
 * @brief init the lcd
 *
 * @param handle a pointer to lcd handle
 * @param hspix lcd spi bus
 * @param spi_id a int number to identify spi bus , must use different number in different spi bus , recommend use spi num
 *
 * @param use_dma enable hw dma
 * @param use_hw_nss enable hw nss
 *
 * @param lcd_res_x lcd res x(not use)
 * @param lcd_res_y lcd res y(not use)
 *
 * @param lcd_offset_x lcd offset x
 * @param lcd_offset_y lcd offset y
 *
 * @param dc_pin dc gpio(essential)
 * @param rst_pin rst gpio(not essential)
 * @param nss_pin nss gpio(not essential in hw nss mode)
 *
 * @param spi_cplt spi cplt callback , one spi only have one , last cover early
 *
 * @return LCD_ERR_OK is ok or other is error
 *
 */
stm_lcd_err_t st_lcd_init(stm_lcd_desc_t *handle, SPI_HandleTypeDef *hspix, uint8_t spi_id,
                          uint8_t use_dma, uint8_t use_hw_nss,
                          uint16_t lcd_res_x, uint16_t lcd_res_y,
                          uint16_t lcd_offset_x, uint16_t lcd_offset_y,
                          stm_lcd_gpio_desc_t dc_pin, stm_lcd_gpio_desc_t rst_pin, stm_lcd_gpio_desc_t nss_pin,
                          void (*spi_cplt)());

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
stm_lcd_err_t lcd_send_sequence(stm_lcd_desc_t *handle, const uint8_t *seq, uint16_t len);

stm_lcd_err_t lcd_get_res(stm_lcd_desc_t *handle, uint16_t *x, uint16_t *y);

stm_lcd_err_t lcd_draw_rect_poll(stm_lcd_desc_t *handle, int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t *buf);

stm_lcd_err_t lcd_draw_rect_single(stm_lcd_desc_t *handle, int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t px);

stm_lcd_err_t lcd_draw_pixel(stm_lcd_desc_t *handle, int16_t x0, int16_t y0, uint16_t color);

stm_lcd_err_t lcd_wait_dma_cplt(stm_lcd_desc_t *handle, uint32_t timeout);

stm_lcd_err_t lcd_draw_rect_DMA(stm_lcd_desc_t *handle, int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t *buf, uint32_t timeout);

#endif