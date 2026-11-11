// First-run failures when the page asks the browser for a device (WebHID, WebUSB, Web Serial).
// Pure: no page, no board. deviceError(e, ctx) returns { kind, message, next, fold } with one plain sentence of what
// happened, one line of what to do, and the row of "Before you connect" (id env-<fold>) that explains it.
// ctx.api: "hid" | "usb" | "serial" (what was asked for).

const API_NAME = { hid: "WebHID", usb: "WebUSB", serial: "Web Serial" };
const NO_LIST = "Use a USB cable that carries data (not charge-only), power the board, and see the rows below if Chrome's list stays empty.";

/** The page cannot ask at all: Safari, Firefox, iPhone, an insecure origin, or a policy that removes the API. */
export function missingApi(api, secure = true, ua = (typeof navigator !== "undefined" && navigator.userAgent) || "") {
  // the device first: an iPhone, Safari or Firefox cannot reach USB on any address, so the address is not the reason
  if (/iPhone|iPad|iPod/.test(ua) || (/Macintosh/.test(ua) && typeof navigator !== "undefined" && navigator.maxTouchPoints > 1))
    return { kind: "browser", message: "iPhone and iPad browsers cannot reach USB boards.", next: "Open this page in Chrome or Edge on a computer, or Chrome on an Android phone. You can still browse the demos and Go further here.", fold: "browser" };
  if (/Firefox\//.test(ua) || (/Safari\//.test(ua) && !/Chrome\/|Chromium\/|Edg\//.test(ua)))
    return { kind: "browser", message: "This browser cannot reach USB boards.", next: "Open this page in Chrome or Edge on a computer; Safari and Firefox cannot.", fold: "browser" };
  if (!secure) return { kind: "browser", message: "This page is not on a secure address, so the browser hides USB access.", next: "Open it over https or from localhost (127.0.0.1).", fold: "policy" };
  const what = api === "serial" ? "serial ports" : "USB boards";
  return { kind: "browser", message: `This browser cannot reach ${what}.`, next: "Open the page in Chrome or Edge on a computer; Safari, Firefox and iPhone cannot.", fold: "browser" };
}

export function deviceError(e, ctx = {}) {
  const name = e && e.name, text = String((e && e.message) || e || "");
  const api = API_NAME[ctx.api] || "USB";
  if (e && e.kind && e.next !== undefined) {
    // Our own FlashError: only the debug probe's old firmware needs a row of its own.
    // The chooser was closed: WebHID and WebUSB answer with an empty list instead of NotFoundError.
    if (e.kind === "no-device" && /was chosen/.test(text)) return { kind: "chooser", message: "No board was picked.", next: `Press Connect and pick the XDS110. ${NO_LIST}`, fold: "nolist" };
    if (e.kind === "connect" && /did not understand/.test(text)) return { kind: "firmware", message: "The debug probe's firmware is too old for this page.", next: "Update the XDS110 firmware with TI's xdsdfu, or use the terminal route.", fold: "firmware" };
    return null;
  }
  if (name === "NotFoundError" && /no device selected|chooser|cancel|dismiss|no port selected/i.test(text) || (name === "NotFoundError" && !/open|disconnect|lost/i.test(text)))
    return { kind: "chooser", message: ctx.api === "serial" ? "No serial port was picked." : "No board was picked.", next: `Press Connect and pick the XDS110. ${NO_LIST}`, fold: "nolist" };
  if (name === "NotFoundError") return { kind: "gone", message: "The board was unplugged or reset while the page used it.", next: "Plug it back in and press Connect.", fold: "nolist" };
  // Chrome's HIDDevice.open() rejects with NotAllowedError "Failed to open the device." when another program holds the probe,
  // or on Linux when the hidraw node is not readable: not a browser policy, so it must not send the person to chrome://policy.
  if (name === "NotAllowedError" && /failed to open/i.test(text))
    return { kind: "busy", message: "The browser could not open the board: another program has it, or the system blocks it.", next: "Close CCStudio, UniFlash, terminals and other tabs that use it, replug the board and press Connect. On Linux, install the two udev rules (hidraw and usb) first.", fold: "busy" };
  if (name === "SecurityError" || name === "NotAllowedError")
    return { kind: "permission", message: `The browser blocked ${api} on this page.`, next: "Open the page over https or from localhost, and check chrome://policy for a work or school rule that turns it off.", fold: "policy" };
  if (name === "NotSupportedError") return { kind: "browser", message: `This browser does not support ${api}.`, next: "Open the page in Chrome or Edge on a computer.", fold: "browser" };
  if (name === "NetworkError" || name === "InvalidStateError" || name === "AbortError") {
    if (/access denied|permission|insufficient/i.test(text))
      return { kind: "denied", message: "The system did not let the browser open the board.", next: "Linux: install the two udev rules (hidraw and usb), replug the board and try again. Windows: unplug and replug it.", fold: "linux" };
    return { kind: "busy", message: ctx.api === "serial" ? "The serial port is busy." : "Another program has the board open.", next: "Close CCStudio, UniFlash, any terminal and other tabs that use it, replug the board and press Connect.", fold: "busy" };
  }
  return null;
}

/** An Error for the page's existing error areas (message plus next), keeping the original when nothing matches. */
export function asDeviceError(e, ctx, make = (kind, message, next) => Object.assign(new Error(message), { kind, next })) {
  const d = deviceError(e, ctx);
  if (!d) return e;
  const out = make(d.kind, d.message, d.next); out.fold = d.fold; return out;
}
