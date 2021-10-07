//
//  server.c
//
//
//  (C) 2021 Thibaut VARENE
//  License: GPLv2 - http://www.gnu.org/licenses/gpl-2.0.html
//

#include <string.h>
#include <sys/param.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"

#include "platform.h"

static const char *TAG = "server";

static int Gsock;
static bool Gwillecho;

ssize_t sockin(void *buf, size_t len)
{
	return (recv(Gsock, buf, len, 0));
}

ssize_t sockout(const void *buf, size_t len)
{
	return (send(Gsock, buf, len, 0));
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
	if (en) {
		Gwillecho = false;
		buf[1] = 0xFC;			// (server) WONT ECHO
	}
	else
		Gwillecho = true;
	sockout(buf, sizeof(buf));
}

int yylex_destroy(void);
int yyparse(void);

void server_task(void *pvParameters)
{
	// we only care about ipv4
	struct sockaddr_in dest_addr, source_addr;
	socklen_t addr_len;
	int lsock;

	dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	dest_addr.sin_family = AF_INET;
	dest_addr.sin_port = htons(TCP_PORT);

	while (1) {
		lsock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
		if (lsock < 0) {
			ESP_LOGE(TAG, "socket(): %d", errno);
			vTaskDelete(NULL);
			return;
		}

		if (setsockopt(lsock, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int))) {
			ESP_LOGE(TAG, "SO_REUSEADDR: %d", errno);
			goto out;
		}

		if (bind(lsock, (struct sockaddr *)&dest_addr, sizeof(dest_addr))) {
			ESP_LOGE(TAG, "bind(): %d", errno);
			goto out;
		}

		if (listen(lsock, 1)) {
			ESP_LOGE(TAG, "listen(): %d", errno);
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

		telnet_echo(false);
		sockout("pass? ", 6);

		yyparse();
		yylex_destroy();

		shutdown(Gsock, SHUT_RD);
		close(Gsock);
	}

out:
	close(lsock);
	vTaskDelete(NULL);
}

