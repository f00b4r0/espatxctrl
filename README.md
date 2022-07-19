# espatxctrl

Host control software for [ATXctrl](http://hacks.slashdirt.org/hw/atxctrl/), ESP-idf implementation.

Supports the [WT32-ETH01](http://www.wireless-tag.com/portfolio/wt32-eth01/) ESP32 module by default,
should work fine with any ESP32 module.

## License

GPLv2-only - http://www.gnu.org/licenses/gpl-2.0.html

Copyright: (C) 2021-2022 Thibaut VARÈNE

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License version 2,
as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See LICENSE.md for details

## Features

The software implements a simple telnet interface accessible on standard port 23
(basic networking implemented through [simple_network](http://vcs.slashdirt.org/git/sw/ESP/simple_network.git)):

- Asks for (cleartext) password (to prevent accidental access) 
- Offers a set of commands for toggling/reading GPIO connected to **ATXctrl**
- Allows setting console baudrate and saving it to non-volatile storage
- Provides full serial console passthrough over telnet session
- Provides a basic push OTA firmware update system, rollback-compatible, through [simple_pushota](http://vcs.slashdirt.org/git/sw/ESP/simple_pushota.git)

### Full list of commands:

```
baudrate <value> set
baudrate {get|save}
firmware {get|set|save|quit}
{ledhdd|ledpower} get
{power|reset} [long] press
console
help
quit
```

Commands are self-explanatory, possibly needing explanation: `firmware set` triggers the push OTA update system,
`firmware save` marks the newly booted firmware as valid and prevents rollback, `firwmare quit` requests rollback and reboots.

To push a new firmware to the device once the push OTA system is started, use e.g.:

`curl <esphost>:<OTA_PORT> --data-binary @build/espatxctrl.bin`

## Building

### Configure the project

Possibly adjust the default "password" string ("admin") in `main/cmdparse.l` (look for `/* password */`),
and confirm that the ports and baudrate defined in `platform.h` suits you, then run:

`idf.py reconfigure`

Note: you may want to adjust the hostname under the lwIP component.

### Build and Flash

Build the project and flash it to the board:

`idf.py flash`

See the ESP-idf Getting Started Guide for full steps to configure and use ESP-IDF to build projects.
