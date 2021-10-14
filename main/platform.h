/*
  Defines for WT32-ETH01

  PROG	-> BOARD
  RTS	-> EN
  DTR	-> GPIO 0
  GND	-> GND
  3.3V	-> 3.3V
  TX	-> RX0
  RX	-> TX0
 */

#ifndef platform_h
#define platform_h

#undef DEBUG

#define OTA_PORT	8888
#define TCP_PORT	23
#define SERIAL_BAUDRATE 115200


// the remainder of this file defines WT32-ETH01 specific constants and should not need modification

#define SERIAL_PORT	2	/* TXD2 RXD2 */
#define SERIAL_TXD	17
#define SERIAL_RXD	5

/*
 {"NAME":"WT32-ETH01","GPIO":[1,1,1,1,1,1,0,0,1,0,1,1,3840,576,5600,0,0,0,0,5568,0,0,0,0,0,0,0,0,1,1,0,1,1,0,0,1],"FLAG":0,"BASE":1}
 GPIO16 = Force Hi
 GPIO18 = ETH MDIO
 GPIO23 = ETH MDC
 #define ETH_TYPE          ETH_PHY_LAN8720
 #define ETH_CLKMODE       ETH_CLOCK_GPIO0_IN
 #define ETH_ADDR          1

 Preferred network: Ethernet
 Ethernet PHY type: LAN8710
 Ethernet PHY Address: 1
 GPIO MDC: GPIO-23
 GPIO MIO: GPIO-18
 GPIO POWER: GPIO-16
 Ethernet Clock: External crystal oscillator
 */

#define ETH_PHY_RST_GPIO	16
#define ETH_PHY_ADDR		1
#define ETH_MDC_GPIO		23
#define ETH_MDIO_GPIO		18

/*
 ESP32-ETH01 available IO:
  - 32 (CFG), 33 (485_EN)
  - 35, 36, 39: in only, no pullup/down
  - 2, 4, 12, 14, 15	- 2, 4, 12, 14, 15 are ADC2 used when WiFi is used; 2, 12, 15 are strapping pins
  MTDO (15) could be used as input since we don't care what value it has at boot (toggle debug TXD)

 usable: 4, 14, (15), 32, 33
 */

#define GPIO_OUTPUT_POWER	32
#define GPIO_OUTPUT_RESET	33
#define GPIO_OUTPUT_PIN_SEL	((1ULL<<GPIO_OUTPUT_POWER) | (1ULL<<GPIO_OUTPUT_RESET))
#define GPIO_INPUT_LEDPOWER	4
#define GPIO_INPUT_LEDHDD	14
#define GPIO_INPUT_PIN_SEL	((1ULL<<GPIO_INPUT_LEDPOWER) | (1ULL<<GPIO_INPUT_LEDHDD))

#define XSTR(s) STR(s)
#define STR(s) #s

#undef HAS_LEDHB
#define LED_GPIO	2

#endif /* platform_h */
