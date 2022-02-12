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

#define TCP_PORT	CONFIG_ESPATXCTRL_LISTEN_PORT
#define SERIAL_BAUDRATE CONFIG_ESPATXCTRL_CONSOLE_UART_BAUDRATE


// the remainder of this file defines WT32-ETH01 specific constants and should not need modification

#define SERIAL_PORT	CONFIG_ESPATXCTRL_CONSOLE_UART_NUM	/* TXD2 RXD2 */
#define SERIAL_TXD	CONFIG_ESPATXCTRL_CONSOLE_UART_TXD
#define SERIAL_RXD	CONFIG_ESPATXCTRL_CONSOLE_UART_RXD

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

/*
 ESP32-ETH01 available IO:
  - 32 (CFG), 33 (485_EN)
  - 35, 36, 39: in only, no pullup/down
  - 2, 4, 12, 14, 15	- 2, 4, 12, 14, 15 are ADC2 used when WiFi is used; 2, 12, 15 are strapping pins
  MTDO (15) could be used as input since we don't care what value it has at boot (toggle debug TXD)

 usable: 4, 14, (15), 32, 33
 */

#define GPIO_OUTPUT_POWER	CONFIG_ESPATXCTRL_GPIO_OUT_POWER
#define GPIO_OUTPUT_RESET	CONFIG_ESPATXCTRL_GPIO_OUT_RESET
#define GPIO_OUTPUT_PIN_SEL	((1ULL<<GPIO_OUTPUT_POWER) | (1ULL<<GPIO_OUTPUT_RESET))
#define GPIO_INPUT_LEDPOWER	CONFIG_ESPATXCTRL_GPIO_IN_LEDPOWER
#define GPIO_INPUT_LEDHDD	CONFIG_ESPATXCTRL_GPIO_IN_LEDHDD
#define GPIO_INPUT_PIN_SEL	((1ULL<<GPIO_INPUT_LEDPOWER) | (1ULL<<GPIO_INPUT_LEDHDD))

#define XSTR(s) STR(s)
#define STR(s) #s

#ifdef CONFIG_ESPATXCTRL_HAS_LEDHB
 #define LED_GPIO		CONFIG_ESPATXCTRL_LEDHD_GPIO
#endif

#endif /* platform_h */
