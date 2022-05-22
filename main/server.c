//
//  server.c
//
//
//  (C) 2021-2022 Thibaut VARENE
//  License: GPLv2 - http://www.gnu.org/licenses/gpl-2.0.html
//

/**
 * @file minimalistic telnet-compatible server.
 * Provides support for several commands, as well as passthrough serial console.
 */

#include <string.h>
#include <sys/param.h>
#include <sys/poll.h>
#include <fcntl.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"

#include "simple_pushota.h"

#include "platform.h"

#define KEEPALIVE_IDLE              5	// delay (s) before starting sending keepalives
#define KEEPALIVE_INTERVAL          5	// keepalive probes period (s)
#define KEEPALIVE_COUNT             3	// max unanswered before timeout

static const char *TAG = "server";

static int Gsock;
static bool Gwillecho;
static bool Gwantcons, Gwantota;

ssize_t sockin(void *buf, size_t len)
{
	return (recv(Gsock, buf, len, 0));
}

ssize_t sockout(const void *buf, size_t len)
{
	return (send(Gsock, buf, len, 0));
}

void want_console(void)
{
	Gwantcons = true;
}

void want_ota(void)
{
	Gwantota = true;
}

/**
 * Telnet protocol handler to be called from yylex() for WILL/WONT/DO/DONT
 * The app doesn't support anything: ignore WONT/DONT (per RFC); turn down DO/WILL
 * @param buf yytext (will be modified)
 */
void yytelnet(char *buf)
{
	unsigned char cmd;

	// buf comes from yylex() and is null-terminated: IAC / CMD / OPT / '\0'
	cmd = buf[1];
	switch (cmd) {
		case 0xfb:	// WILL
			cmd = 0xfe;	// DONT
			break;
		case 0xfd:	// DO
			if (buf[2] == 1 && Gwillecho)
				return;
			cmd = 0xfc;	// WONT
			break;
		case 0xfc:	// WONT
		case 0xfe:	// DONT
		default:	// invalid
			return;
	}

	buf[1] = cmd;
	sockout(buf, strlen(buf));
}

/**
 * Request telnet client local echo.
 * @param en if true, local echo will be enabled
 */
void telnet_echo(bool en)
{
	char buf[3] = { 0xFF, 0xFB, 0x01 };	// (server) WILL ECHO
	if (en)
		buf[1] = 0xFC;			// (server) WONT ECHO
	Gwillecho = !en;
	sockout(buf, sizeof(buf));
}

/**
 * Setup telnet client for console VT passthrough.
 * Mashup just enough telnet protocol to get what we need for proper VT support
 */
static int telnet_setvt(void)
{
	unsigned char buf[128];
	int len;

	// WILL GA - rfc858
	sockout((unsigned char[]){ 255, 251, 03 }, 3);
	len = sockin(buf, 3);
	if (len < 3 || buf[1] != 253) {	// make sure other end will
		ESP_LOGE(TAG, "DONT GA");
		return -1;
	}

	// WILL ECHO - rfc857
	sockout((unsigned char[]){ 255, 251, 01 }, 3);
	len = sockin(buf, 3);
	if (len < 3 || buf[1] != 253) {	// make sure other end will
		ESP_LOGE(TAG, "DONT ECHO");
		return -1;
	}

	/*
	 DO LINEMODE - rfc1184:
	 If DO LINEMODE is negotiated, the defaults are:
	    IAC SB LINEMODE MODE 0 IAC SE
	    IAC SB LINEMODE DONT FORWARDMASK IAC SE
	 */
	sockout((unsigned char[]){ 255, 253, 34 }, 3);
	// client will reply with will/wont and subopt neg as needed, which we don't care about
	len = sockin(buf, sizeof(buf));
	if (len < 3 || buf[1] != 251) {	// make sure other end will
		ESP_LOGE(TAG, "WONT LINEMODE");
		return -1;
	}

	return 0;
}

/**
 * Serial console / socket passthrough.
 */
static void start_console(void)
{
	char buf[256];
	struct pollfd fds[2];
	int nfds = 2, i, ret, uart;
	short re;

	uart = open("/dev/uart/" XSTR(SERIAL_PORT), O_RDWR|O_NONBLOCK);	// breaks without O_NONBLOCK - see note at the end
	if (uart < 0) {
		ESP_LOGE(TAG, "Unable to open serial port: %s", strerror(errno));
		return;
	}

	memset(fds, 0 , sizeof(fds));

	fds[0].fd = Gsock;
	fds[0].events = POLLIN;
	fds[1].fd = uart;
	fds[1].events = POLLIN;

	do {
		ret = poll(fds, nfds, 1000*120);	// 120s inactivity timeout
		if (ret == -1) {
			ESP_LOGE(TAG, "poll failed: %s", strerror(errno));
			break;
		}
		else if (ret == 0)	// timeout
			break;

		for (i = 0; i < nfds; i++) {
			re = fds[i].revents;
			if (!re)
				continue;

			if (re & POLLHUP)
				goto out;

			if (!(re & POLLIN)) {
				ESP_LOGD(TAG, "Unknown event on %d: %d", i, fds[i].revents);
				continue;
			}

			ret = read(fds[i].fd, buf, sizeof(buf));
			if (ret <= 0)	// error or connection closed
				break;

			ret = write((fds[i].fd == Gsock) ? uart : Gsock, buf, ret);
			if (ret < 0)	// error
				break;
		}
	} while (ret > 0);

out:
	close(uart);	// Gsock is handled by caller
}

int yylex_destroy(void);
int yyparse(void);

void server_task(void *pvParameters)
{
	// we only care about ipv4
	struct sockaddr_in dest_addr, source_addr;
	socklen_t addr_len;
	int lsock, ret;

	dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	dest_addr.sin_family = AF_INET;
	dest_addr.sin_port = htons(TCP_PORT);

	while (1) {
		lsock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
		if (lsock < 0) {
			ESP_LOGE(TAG, "socket(): %s", strerror(errno));
			vTaskDelete(NULL);
			return;
		}

		if (setsockopt(lsock, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int))) {
			ESP_LOGE(TAG, "SO_REUSEADDR: %s", strerror(errno));
			goto out;
		}

		if (bind(lsock, (struct sockaddr *)&dest_addr, sizeof(dest_addr))) {
			ESP_LOGE(TAG, "bind(): %s", strerror(errno));
			goto out;
		}

		if (listen(lsock, 1)) {
			ESP_LOGE(TAG, "listen(): %s", strerror(errno));
			goto out;
		}

		ESP_LOGI(TAG, "Socket port %d", TCP_PORT);
		addr_len = sizeof(source_addr);

		Gsock = accept(lsock, (struct sockaddr *)&source_addr, &addr_len);
		close(lsock);	// only allow exactly one connection, others get ECONNREFUSED

		if (Gsock < 0) {
			ESP_LOGE(TAG, "accept(): %d", errno);
			continue;
		}

		// make sure unclean client shutdown won't DoS us
		if (setsockopt(Gsock, SOL_SOCKET, SO_KEEPALIVE, &(int){ 1 }, sizeof(int))) {
			ESP_LOGE(TAG, "SO_KEEPALIVE: %d", errno);
			goto out;
		}
		// assume cannot fail if the above succeeds
		setsockopt(Gsock, IPPROTO_TCP, TCP_KEEPIDLE, &(int){ KEEPALIVE_IDLE }, sizeof(int));
		setsockopt(Gsock, IPPROTO_TCP, TCP_KEEPINTVL, &(int){ KEEPALIVE_INTERVAL }, sizeof(int));
		setsockopt(Gsock, IPPROTO_TCP, TCP_KEEPCNT, &(int){ KEEPALIVE_COUNT }, sizeof(int));

		Gwantota = Gwantcons = false;

		telnet_echo(false);
		sockout("pass? ", 6);

		ret = yyparse();
		yylex_destroy();

		if (!ret && Gwantcons) {
			ESP_LOGI(TAG, "Starting console");
			if (telnet_setvt())
				ESP_LOGE(TAG, "VT setup failed");
			else
				start_console();
		}

		shutdown(Gsock, SHUT_RD);
		close(Gsock);

		if (!ret && Gwantota) {
			ESP_LOGI(TAG, "Starting OTA");
			ret = pushota(NULL);
			if (!ret)
				esp_restart();
		}

	}

out:
	close(lsock);
	vTaskDelete(NULL);
}

/*
 Note: this code theoretically does not require O_NONBLOCK for the uart fd (it does not on e.g. macOS),
 however in the ESP-IDF environment, if O_NONBLOCK is not specified the code fails as follows:
 As soon as any character is input on the socket, poll() fires once and it is sent to the uart,
 then poll() fires again to read from the UART, with a POLLIN event, however the subsequent read() blocks,
 even though it shouldn't when poll() returns with POLLIN.

 Instrumenting the start_console() function with ESP_LOGI() shows the following traces for the exact same socket input:
 ('0' is socket, '1' is uart)

 Without O_NONBLOCK:
 I (11969) tcp_server: Starting console
 I (11969) tcp_server: loop start
 I (12869) tcp_server: poll returned 1
 I (12869) tcp_server: revents 1 on 0
 I (12869) tcp_server: reading from 0
 I (12869) tcp_server: read 2 from 0
 I (12879) tcp_server: wrote 2 to 1
 I (12879) tcp_server: revents 0 on 1
 I (12889) tcp_server: loop start
 I (12889) tcp_server: poll returned 1
 I (12889) tcp_server: revents 0 on 0
 I (12899) tcp_server: revents 1 on 1
 I (12899) tcp_server: reading from 1

 With:
 I (373540) tcp_server: Starting console
 I (373540) tcp_server: loop start
 I (374870) tcp_server: poll returned 1
 I (374870) tcp_server: revents 1 on 0
 I (374870) tcp_server: reading from 0
 I (374880) tcp_server: read 2 from 0
 I (374880) tcp_server: wrote 2 to 1
 I (374880) tcp_server: revents 0 on 1
 I (374890) tcp_server: loop start
 I (374900) tcp_server: poll returned 1
 I (374900) tcp_server: revents 0 on 0
 I (374900) tcp_server: revents 1 on 1
 I (374910) tcp_server: reading from 1
 I (374910) tcp_server: read 2 from 1
 I (374920) tcp_server: wrote 2 to 0
 I (374920) tcp_server: loop start
 I (374920) tcp_server: poll returned 1
 I (374930) tcp_server: revents 0 on 0
 I (374930) tcp_server: revents 1 on 1
 I (374940) tcp_server: reading from 1
 I (374940) tcp_server: read 2 from 1
 I (374950) tcp_server: wrote 2 to 0
 I (374950) tcp_server: loop start

 It's worth noting that with O_NONBLOCK enabled, adding more debug output showed that EWOULDBLOCK/EAGAIN was never triggered.
 This only affects uart fd, not tcp socket.
 */
