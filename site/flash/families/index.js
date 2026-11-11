// One module per chip family. familyOf(board) picks the module from the board's data: an explicit "family", the board
// id for the AM13, else the flash controller (a 16-byte program word means the Cortex-M33 family).
import { family as mspm0 } from "./mspm0.js";
import { family as mspm33, am13 } from "./mspm33.js";
export const FAMILIES = { mspm0, mspm33, am13 };
export function familyOf(board) {
  if (board.family && FAMILIES[board.family]) return FAMILIES[board.family];
  if (board.id === "am13e230") return am13;
  return board.fc && board.fc.word === 16 ? mspm33 : mspm0;
}
