#include "bsp_ctp_ft6x36u.h"

#include "i2c.h"
#include "stm32h7xx_hal_i2c.h"

#include "elog.h"

const static char *TAG = "bsp_ft6x36";

#define FT6X36_I2C_BUS hi2c2
#define FT6X36_SADDR 0x38
#define FT6X36_BUS_TIMEOUT 1000

#define CTP_IRQ_GPIO_PORT GPIOE
#define CTP_IRQ_GPIO_PIN GPIO_PIN_12

#define CTP_RST_GPIO_PORT GPIOE
#define CTP_RST_GPIO_PIN GPIO_PIN_11

SemaphoreHandle_t xSemTouchAvailable;

#define CTP_RETURN_ON_FALSE(x, ret, msg, ...) \
    do                                        \
    {                                         \
        if (!(x))                             \
        {                                     \
            elog_e(TAG, msg, ##__VA_ARGS__);  \
            return ret;                       \
        }                                     \
    } while (0)

// isr handle
BaseType_t ctp_isr_handle(uint16_t GPIO_Pin)
{
    BaseType_t yd = 0;
    if (GPIO_Pin == CTP_IRQ_GPIO_PIN)
    {
        // elog_i(TAG, "isr");
        if (HAL_GPIO_ReadPin(CTP_IRQ_GPIO_PORT, CTP_IRQ_GPIO_PIN) == 0)
        {
            // elog_i(TAG, "isr d");
            xSemaphoreGiveFromISR(xSemTouchAvailable, &yd);
        } // HAL_GPIO_ReadPin
    } // GPIO_Pin
    return yd;
} // ctp_isr_handle

static HAL_StatusTypeDef ctp_read_regs(uint8_t reg, uint8_t len, uint8_t *buf)
{
    return HAL_I2C_Mem_Read(&FT6X36_I2C_BUS, (FT6X36_SADDR << 1), reg, 1, buf, len, FT6X36_BUS_TIMEOUT);
}

static HAL_StatusTypeDef ctp_write_reg(uint8_t reg, uint8_t buf)
{
    return HAL_I2C_Mem_Write(&FT6X36_I2C_BUS, (FT6X36_SADDR << 1), reg, 1, &buf, 1, FT6X36_BUS_TIMEOUT);
}

uint8_t ctp_init()
{
    HAL_GPIO_WritePin(CTP_RST_GPIO_PORT, CTP_RST_GPIO_PIN, 1); // ctp rst
    vTaskDelay(10);
    xSemTouchAvailable = xSemaphoreCreateBinary();
    ELOG_ASSERT(xSemTouchAvailable);
    return 0;
}

uint8_t ctp_read_id()
{
    uint8_t id;
    CTP_RETURN_ON_FALSE((ctp_read_regs(FT6X36_PANEL_ID_REG, 1, &id) == HAL_OK), 1, "ctp io err");
    elog_i(TAG, "gt id = %x", id);

    CTP_RETURN_ON_FALSE((ctp_read_regs(FT6X36_CHIPSELECT_REG, 1, &id) == HAL_OK), 1, "ctp io err");
    elog_i(TAG, "gt chipselect = %x", id);

    CTP_RETURN_ON_FALSE((ctp_read_regs(FT6X36_DEV_MODE_REG, 1, &id) == HAL_OK), 1, "ctp io err");
    elog_i(TAG, "gt dev mode = %x", id);

    CTP_RETURN_ON_FALSE((ctp_read_regs(FT6X36_FIRMWARE_ID_REG, 1, &id) == HAL_OK), 1, "ctp io err");
    elog_i(TAG, "gt fw id = %x", id);

    CTP_RETURN_ON_FALSE((ctp_read_regs(FT6X36_RELEASECODE_REG, 1, &id) == HAL_OK), 1, "ctp io err");
    elog_i(TAG, "gt release code = %x", id);

    return 0;
}

uint8_t ctp_read(uint16_t *x, uint16_t *y)
{
    uint8_t id[6];
    CTP_RETURN_ON_FALSE((ctp_read_regs(FT6X36_GEST_ID_REG, 6, id) == HAL_OK), 1, "ctp io err");

    *x = (id[2] & 0x0f) << 8 | id[3];
    *y = (id[4] & 0x0f) << 8 | id[5];

    // elog_i(TAG, "x=%d,y=%d", *x, *y);
    return 0;
}

// eof