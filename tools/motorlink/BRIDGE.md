# The bench bridge: the motor console from any browser

Chrome and Edge can talk to a USB board directly (Web Serial). Safari, Firefox and phone browsers cannot. The bridge is a small program on the computer the LaunchPad is plugged into: it holds the board's serial port and serves the console pages with a stand-in serial port that forwards over the network, so any browser that can reach the computer can drive the board.

## Run it

```bash
npm install --prefix ~/.local/share/fox-red-labs/console-bench serialport ws playwright-core   # once; only ws is needed to serve
NODE_PATH=~/.local/share/fox-red-labs/console-bench/node_modules \
  node tools/motorlink/phone_console.mjs --port /dev/cu.usbmodemXXXX --host <a private address of this computer>
```

It prints one link with a random token. Open it on a phone or in any browser on the same private network: the phone remote is the link as printed; the full console is the same link with `motor-console.html` before the `?`.

## Keep it private

- **Bind to a private address only.** Use an address on a network you control: a mesh VPN on your own devices, or your home network. The bridge refuses `0.0.0.0`. Never forward its port from your router or expose it on the internet: whoever reaches it can run the motor.
- **The token is the key.** Every page and connection needs it. It lives in `~/.local/state/fox-red-labs/phone-console.token` (owner-only) and survives restarts; delete the file to issue a new one. Do not paste the link into anything shared.
- **Safety does not depend on the link.** The board's safety guard stops the motor within 1 s when the page goes quiet (a phone locked or switched to another app), whatever the network does.

## Sharing the board with other tools

Only one program can hold the board's serial port. The bridge opens it only while a page is connected, and the bench scripts open it exclusively, so a second program gets "busy" at once instead of a garbled link. Disconnect the page before running a bench script, and the reverse.
