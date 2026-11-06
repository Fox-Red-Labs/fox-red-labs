# How do two TI LaunchPad radios talk to each other with no phone or hub: Bluetooth LE, Thread and Zigbee?

Three experiments where one TI LaunchPad kit talks to another over the air, with no phone, hub or home network in between: Bluetooth LE (CC2340R5 central, CC1354P10 peripheral), Thread (CC1354P10 leader, CC2340R5 joiner) and Zigbee (CC1354P10 coordinator, CC2340R5 router). Each folder has its own README with the result, the commands and the run it came from.

Not here: TI's SDKs and prebuilt images (you build with the commands in each README), and the raw radio logs, which print the test network's keys. Each README states its results. Terms: [docs/vocab.md](../docs/vocab.md). To fetch only this folder: `git sparse-checkout add board-to-board cc1354p10 cc2340r5 tools data`. The hardware, boards and tool versions are listed at the bottom of each README.
