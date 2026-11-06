<!-- experiment
id: E113
board: LP-EM-CC35X1
date: 2026-10-09
status: works
reached: verified
line: TI's Wi-Fi provisioning over Bluetooth works without TI's phone app. A Mac script writing four Bluetooth characteristics took the board from reset to a Wi-Fi IP address in 16.5 to 17.8 s, 7.13 s of it after the connect request.
stop: none
-->
# Does Wi-Fi provisioning over Bluetooth work on the TI LP-EM-CC35X1, and how long from power-up to joined?

**Short answer.** Yes. With TI's `ble_wifi_provisioning` example the board advertises as `TI CC3xxx`, accepts a network name, password and security type over Bluetooth LE, and joins the network. From reset to an IP address took 16.5 s and 17.8 s (two runs). About 9 s of that is the board starting and advertising; the join itself took 7.13 s after the connect request, both runs. It was driven from a Mac, so TI's phone app was not needed.

Terms: [vocab](../vocab.md) (provisioning, GATT, BLE). Errors and fixes: [LP-EM-CC35X1 FAQ](../../cc35x1/FAQ.md). 1 kB = 1,024 bytes.

**Question.** A Wi-Fi product with no screen needs a way to get the password in. Does TI's example do that, and how long does it take?

**Vendor position.** The example's README: install the "SimpleLink Connect" app on a phone, find "TI CC3xxx", open the "Wi-Fi Provisioning Over BLE" service, enter the network name, security type and password, press Connect (Wi-Fi SDK 10.20.00.39, read 2026-10-09).

**What was tried.** The example built unchanged (38 s) and was programmed through the Toolbox. A Python script on the Mac (`bleak`) found the board, wrote the network name to characteristic `0xCC01`, the password to `0xCC02` and the security type `2` (WPA2) to `0xCC04` of service `0xCC00`, then read `0xCC03`, which starts the connection and notifies the result. The network name and password stayed out of the repo.

**How far it got.** Verified. The board's serial log shows each field received, `WLAN CONNECT Requested`, the association, DHCP and an IPv4 address. The notifications were `01` at once and `02` after 6.26 s.

**The line.** No limit reached. The Bluetooth write of all three fields took 0.09 s, so nearly all of the 7.13 s is the Wi-Fi join.

**Ask.** Whether TI's phone app shows the same 6 s between pressing Connect and the green status.
