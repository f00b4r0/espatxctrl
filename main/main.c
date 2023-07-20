//
//  main.c
//
//
//  (C) 2021 Thibaut VARENE
//  License: GPLv2 - http://www.gnu.org/licenses/gpl-2.0.html
//

/*
 * DONE:
 * - LED
 * - Network (ethernet / DHCP)
 * - Comm (TCP socket port 23)
 * - Parse cmd
 * - Console passthrough
 * - Runtime baudrate reconfig
 * - Save/restore baudrate config to/from NVS 
 * - OTA
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_vfs_dev.h"
#include "esp_log.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "driver/uart.h"

#include "simple_network.h"

#include "platform.h"

#define NVSBRKEY	"baudrate"

static nvs_handle_t Gnvsh;

static int nvsgetbr(uint32_t *br)
{
	return (nvs_get_u32(Gnvsh, NVSBRKEY, br));
}

int nvssetbr(uint32_t br)
{
	return (nvs_set_u32(Gnvsh, NVSBRKEY, br));
}

int nvssave(void)
{
	return (nvs_commit(Gnvsh));
}

void server_task(void *pvParameters);

#ifdef LED_GPIO
static void ledhb_task(void *pvParameter)
{
	uint32_t level = 0;

	gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);

	while (1) {
		gpio_set_level(LED_GPIO, level);
		level = !level;
		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}
}
#endif

void ethernet_main(void);

void app_main(void)
{
	// Initialize NVS
	esp_err_t err = nvs_flash_init();
	if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		// NVS partition was truncated and needs to be erased
		// Retry nvs_flash_init
		ESP_ERROR_CHECK(nvs_flash_erase());
		err = nvs_flash_init();
	}
	ESP_ERROR_CHECK(err);

	ESP_ERROR_CHECK(nvs_open("espatxctrl", NVS_READWRITE, &Gnvsh));

	ESP_ERROR_CHECK(esp_event_loop_create_default());

	uart_config_t uart_config = {
		.baud_rate = SERIAL_BAUDRATE,
		.data_bits = UART_DATA_8_BITS,
		.parity    = UART_PARITY_DISABLE,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,

	};

	nvsgetbr((uint32_t *)&uart_config.baud_rate);	// doesn't touch baudrate if not found

	ESP_ERROR_CHECK(uart_param_config(SERIAL_PORT, &uart_config));
	ESP_ERROR_CHECK(uart_set_pin(SERIAL_PORT, SERIAL_TXD, SERIAL_RXD, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
	ESP_ERROR_CHECK(uart_driver_install(SERIAL_PORT, UART_FIFO_LEN*2, UART_FIFO_LEN*2, 0, NULL, 0));

	esp_vfs_dev_uart_use_driver(SERIAL_PORT);

	// disable serial line endings translation which breaks everything
	esp_vfs_dev_uart_port_set_rx_line_endings(SERIAL_PORT, ESP_LINE_ENDINGS_LF);
	esp_vfs_dev_uart_port_set_tx_line_endings(SERIAL_PORT, ESP_LINE_ENDINGS_LF);

	simple_network_start();

	gpio_config_t io_conf;
	io_conf.intr_type = GPIO_INTR_DISABLE;
	io_conf.mode = GPIO_MODE_OUTPUT;
	io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
	io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
	io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
	ESP_ERROR_CHECK(gpio_config(&io_conf));

	io_conf.intr_type = GPIO_INTR_DISABLE;
	io_conf.mode = GPIO_MODE_INPUT;
	io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
	io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
	io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
	ESP_ERROR_CHECK(gpio_config(&io_conf));

#ifdef LED_GPIO
	xTaskCreate(ledhb_task, "lhb", 512, NULL, 1, NULL);
#endif
	xTaskCreate(server_task, "server", 4096, NULL, 5, NULL);

}
