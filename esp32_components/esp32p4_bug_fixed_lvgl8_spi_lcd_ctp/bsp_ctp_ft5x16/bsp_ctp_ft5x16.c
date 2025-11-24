#include "bsp_ctp_ft5x16.h"
#include "esp_log.h"
#include "driver/gpio.h"

const static char *TAG = "bsp_ft6x36";

#define CTP_SADDR (0x38)

#define CTP_INT_PIN (8)
#define CTP_RST_PIN (7)

// https://docs.espressif.com/projects/esp-chip-errata/zh_CN/latest/esp32p4/03-errata-description/shared/i2c-fail-in-multiple-reads-operation.html
#define CTP_P4_IIC_LIMIT

static i2c_master_dev_handle_t *dev_iic;
static SemaphoreHandle_t *xSemIICFree;
static int ctp_xfer_time_out;

SemaphoreHandle_t xSemCTPAvailable;

static void ctp_isr_handle(void *arg)
{
    xSemaphoreGiveFromISR(xSemCTPAvailable, NULL);
}

static esp_err_t ft_reg_read(uint8_t reg, uint8_t *buf, uint16_t len)
{

    if (xSemIICFree != 0)
    {
        xSemaphoreTake(*xSemIICFree, portMAX_DELAY);
    }

    esp_err_t ret = ESP_OK;

#ifdef CTP_P4_IIC_LIMIT
    uint8_t op_reg = 0;
    for (uint16_t i = 0; i < len; i++)
    {
        op_reg = reg + i;
        ret = i2c_master_transmit_receive(*dev_iic, &op_reg, 1, (buf + i), 1, ctp_xfer_time_out);
        if (ret != ESP_OK)
        {
            goto err;
        }
    }

#else
    ret = i2c_master_transmit_receive(*dev_iic, &reg, 1, buf, len, ctp_xfer_time_out);
#endif

    // uint8_t ret = swi2c_read_operation(dev_iic, CTP_SADDR, &reg, 1, buf, len);
#ifdef CTP_P4_IIC_LIMIT
err:
#endif
    if (xSemIICFree != 0)
    {
        xSemaphoreGive(*xSemIICFree);
    }

    return ret;
}

// ctp dont need write
//  static esp_err_t ft_reg_write(uint8_t reg, uint8_t *buf, uint16_t len)
//  {
//      // if (xSemIICFree != 0)
//      // {
//      //     xSemaphoreTake(*xSemIICFree, portMAX_DELAY);
//      // }
//      // uint8_t cmd[2];
//      // cmd[0] = reg;
//      // i2c_master_transmit_multi_buffer_info_t xfer_desc[2] = {
//      //     {
//      //         .buffer_size = 1,
//      //         .write_buffer = cmd,
//      //     },
//      //     {
//      //         .buffer_size = len,
//      //         .write_buffer = buf,
//      //     },
//      // };
//      // esp_err_t ret = i2c_master_multi_buffer_transmit(*dev_iic, xfer_desc, 2, xfer_time_out);
//      // if (xSemIICFree != 0)
//      // {
//      //     xSemaphoreGive(*xSemIICFree);
//      // }
//      // return ret;
//      return ESP_OK;
//  }

static esp_err_t ft_read_touch_point(uint8_t base, uint16_t *x, uint16_t *y, uint8_t *evt)
{
    uint8_t values[4];
    esp_err_t io_ret = ft_reg_read(base, values, 4);
    *evt = (values[0] >> 6) & 0x3; // Event Flag
    *x = (((uint16_t)(values[0] & 0x0f)) << 8) | values[1];
    *y = (((uint16_t)(values[2] & 0x0f)) << 8) | values[3];
    // ESP_LOGI(TAG, "x=%d\t\ty=%d\t\tf=%x", *x, *y, *evt);
    return io_ret;
}

/*================================================================================*/

esp_err_t ft_ctp_init(SemaphoreHandle_t *i2c_mux, i2c_master_dev_handle_t *i2c, int xfer_time_out)
{
    dev_iic = i2c;
    xSemIICFree = i2c_mux;
    ctp_xfer_time_out = xfer_time_out;

    // ctp rst
    gpio_reset_pin(CTP_RST_PIN);
    gpio_set_direction(CTP_RST_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(CTP_RST_PIN, 0);
    vTaskDelay(1);
    gpio_set_level(CTP_RST_PIN, 1);

    // ctp isr
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << CTP_INT_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = 0,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    gpio_config(&io_conf);
    // install gpio isr service
    gpio_install_isr_service(0);
    // hook isr handler for specific gpio pin
    gpio_isr_handler_add(CTP_INT_PIN, ctp_isr_handle, NULL);

    xSemCTPAvailable = xSemaphoreCreateBinary();
    assert(xSemCTPAvailable);

    // wait ctp ic boot with id check
    for (uint8_t i = 0; i < 20; i++)
    {
        uint8_t tmp;
        ft_reg_read(0xa3, &tmp, 1);
        ESP_LOGI(TAG, "ctp id :0x%02x", tmp);
        if ((tmp == 0x06) || (tmp == 0x36) || (tmp == 0x64))
        {
            ESP_LOGI(TAG, "ctp id ok");
            return ESP_OK;
        }
    }

    return ESP_FAIL;
}

uint8_t ft_ctp_read_touch_0(uint16_t *x, uint16_t *y, uint8_t *flag)
{
    return ft_read_touch_point(0x03, x, y, flag);
}

// eof