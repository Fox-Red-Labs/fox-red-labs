// Link and log helpers for phone_console.mjs: the token never reaches a log, and a log file is owner-only.
import { appendFileSync, writeFileSync, chmodSync, existsSync } from "fs";
export const mask = (token) => token.slice(0, 4) + "..." + token.slice(-2);
export const maskedLink = (host, port, token) => `http://${host}:${port}/?t=${mask(token)}`;
export const fullLink = (host, port, token) => `http://${host}:${port}/?t=${token}`;
// the full link goes to a terminal only; stdout redirected to a file or pipe gets the masked one
export const startLines = (host, port, token, isTTY) => isTTY
  ? [`open on the phone (on the private network): ${fullLink(host, port, token)}`]
  : [`open on the phone (on the private network): ${maskedLink(host, port, token)} (token masked: run from a terminal to see the full link, or read the owner-only token file)`];
export function openLog(path) {
  if (!existsSync(path)) writeFileSync(path, "", { mode: 0o600 });
  chmodSync(path, 0o600);
  return (line) => appendFileSync(path, line + "\n");
}
