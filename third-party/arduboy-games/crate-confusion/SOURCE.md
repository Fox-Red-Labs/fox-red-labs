# Crate Confusion

- Upstream: https://github.com/phoboslab/arduboy-games, folder `crate-confusion`, commit `de5e8589f20aeb48179b459172e0971f136dbc57` (2019-03-15).
- Copied unmodified: `*.cpp`, `*.h`, `crate-confusion.ino`, `data/` (the source images). Left out: the `.hex` files and screenshots.
- Licence: MIT. The repository README says "All games are licensed under MIT-License" and its `LICENSE` file is the MIT text, kept here as `LICENSE`. That file names OBONO as the copyright holder, while the game's credits screen names Dominic Szablewski (phoboslab.org, 2019). The grant is MIT either way, with the notice kept; the mismatch is a question for the repository owner if this ever ships outside the private repository.
- Build notes: defines its own type `mode_t`, which the system also defines; the host and board builds pass `-DFRL_RENAME_MODE_T` (see `ports/arduboy/tools/ino_prelude.h`).
