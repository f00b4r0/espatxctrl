//
//  ethernet.c
//
//
//  (C) 2021 Thibaut VARENE
//  License: GPLv2 - http://www.gnu.org/licenses/gpl-2.0.html
//


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_event.h"
#include "esp_log.h"

#include "platform.h"

static const char *TAG = "ethernet";

#ifdef DEBUG
/** Event handler for Ethernet events */
static void eth_event_handler(void *arg, esp_event_base_t event_base,
			      int32_t event_id, void *event_data)
{
	switch (event_id) {
		case ETHERNET_EVENT_CONNECTED:
			ESP_LOGI(TAG, "Link Up");
			break;
		case ETHERNET_EVENT_DISCONNECTED:
			ESP_LOGI(TAG, "Link Down");
			break;
		case ETHERNET_EVENT_START:
			ESP_LOGI(TAG, "Started");
			break;
		case ETHERNET_EVENT_STOP:
			ESP_LOGI(TAG, "Stopped");
			break;
		default:
			break;
	}
}
#endif /* DEBUG */

void ethernet_main(void)
{
	// prep the hardware

	eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
	mac_config.smi_mdc_gpio_num = ETH_MDC_GPIO;
	mac_config.smi_mdio_gpio_num = ETH_MDIO_GPIO;

	esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&mac_config);

	eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
	phy_config.phy_addr = ETH_PHY_ADDR;
	phy_config.reset_gpio_num = ETH_PHY_RST_GPIO;

	esp_eth_phy_t *phy = esp_eth_phy_new_lan8720(&phy_config);

	esp_eth_config_t config = ETH_DEFAULT_CONFIG(mac, phy);
	esp_eth_handle_t eth_handle = NULL;
	ESP_ERROR_CHECK(esp_eth_driver_install(&config, &eth_handle));

	// Initialize TCP/IP network interface (should be called only once in application)
	ESP_ERROR_CHECK(esp_netif_init());
	esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
	esp_netif_t *eth_netif = esp_netif_new(&cfg);

	// Set default handlers
	ESP_ERROR_CHECK(esp_eth_set_default_handlers(eth_netif));
#ifdef DEBUG
	// default event loop created in main()
	ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL));
#endif
	// attach Ethernet driver to TCP/IP stack
	ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handle)));

	// start Ethernet driver state machine
	ESP_ERROR_CHECK(esp_eth_start(eth_handle));
}
