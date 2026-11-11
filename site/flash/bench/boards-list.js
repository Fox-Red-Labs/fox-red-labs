// One row of "Boards this page can see": what the page knows about one XDS110 it may already open. Pure: no page, no board.
// k is what identify() returned for it ({serial, board, what}); statusOf(board) names what a run here has shown.

export function boardRow(k, inUse, statusOf = () => "") {
  const info = k || {};
  const named = !!info.board, checked = !!info.what;
  return {
    kit: named ? info.board.kit : checked ? "A board this page does not know" : "XDS110, not checked yet",
    serial: info.serial || "",
    tag: inUse ? "In use" : named ? statusOf(info.board) : "",
    note: named ? "" : checked ? info.what : "Press Check to read which board is behind it.",
    canUse: named && !inUse,
    canCheck: !checked,
  };
}

/** The line above the list: how many the page sees, and what to do when there are none or one. */
export function listSummary(count, connected) {
  if (!count) return "None yet. Plug a board in with its USB cable, press Connect and choose its XDS110 in the list Chrome shows.";
  if (count === 1 && connected) return "One board. Press Connect another to add a second one to this list.";
  return `${count} boards. Use this one switches the page to that board; nothing is written by switching.`;
}

/** The unattended path (?auto=1): which allowed device Connect takes with no chooser. Where the browser shows serials (WebUSB) it takes the
 *  probe the link names (&probe=<serial>); otherwise, or with no name, only a single allowed device. Several with no way to tell them
 *  apart pick nothing, so a link never chooses a board by luck. */
export function autoPick(devs, want) {
  if (want && devs.some((d) => d.serialNumber)) return devs.filter((d) => d.serialNumber === want);
  return devs.length === 1 ? devs : [];
}
