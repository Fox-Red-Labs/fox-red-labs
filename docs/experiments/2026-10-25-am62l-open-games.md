<!-- experiment
id: E286
board: TMDS62LEVM (AM62L evaluation module)
date: 2026-10-25
status: line found
reached: run
line: With no GPU, a 3D game on the AM62L draws in Mesa llvmpipe on two A53 cores: SuperTuxKart 1.5 (arm64 build) averages 4.5 fps at 640x480 and 2.4 fps at 1280x720, and its x86_64 build under Box64 0.3.4 crashes in OpenGL setup. Steam's download path works anonymously and natively: 2.1 GB of a dedicated server in 371 s.
stop: x86_64 SuperTuxKart crashes under Box64 0.3.4; a Box64 git build (for it and for steamcmd) was started and stopped unfinished
-->
# Do open 3D games run on the AM62L, and does Steam's download path work without an account?

**Short answer.** 3D games run but are not playable: the chip has no GPU, so Mesa's llvmpipe draws every frame on the two A53 cores. SuperTuxKart 1.5, native arm64 build, AI race with 4 karts: 4.5 fps at 640x480 and 2.4 fps at 1280x720. Doom drawn in software is the other end: 186 to 193 fps straight to the framebuffer (E275), 27.2 fps at 640x480 and 10.5 fps at 1280x720 through dsda-doom on X. Steam's own download path works with an anonymous login, natively on arm64: Valheim's dedicated server, 2,088 MB on disk, in 371 s (5.6 MB/s).

Terms: [vocab](../vocab.md). **llvmpipe** is Mesa's software OpenGL. **Box64** runs x86-64 Linux programs on 64-bit Arm. **steamcmd** is Valve's command-line Steam client, 32-bit x86 only. **DepotDownloader** is an open-source Steam download client (SteamKit2) with an arm64 Linux build. Scripts: `am62l-evm/steam/games/`; rows in `data/results/am62l-evm.csv`.

**Question.** Without a Steam account, how close does the AM62L get to "Steam games": can it fetch Steam content, and what frame rate does a real open 3D game reach, natively and as an x86 binary?

**Expectation.** Single-digit fps for any 3D game (the HMI tests drew a fixed LVGL scene at 54 fps and Chocolate Doom through llvmpipe at 6.5 fps); an x86 build under Box64 slower again.

**Setup.** TI SDK 12.01 Linux (kernel 6.18.38), both A53 cores at 1.25 GHz, 2 GB RAM; Debian trixie arm64 container; Xvfb, Mesa 25.0.7 llvmpipe (LLVM 19.1.7). The framebuffer Doom demo kept running on HDMI during every run, so these numbers share the CPU with it (it uses about a third of one core).

**Result.**

| Workload | Resolution | Result | Notes |
|---|---|--:|---|
| SuperTuxKart 1.5 arm64, `--profile-time` race, lighthouse, 4 karts | 640x480 | 4.48 fps | 3,005 frames in 671 s; renderer OpenGL ES 3.2 on llvmpipe |
| Same | 1280x720 | 2.42 fps | 3,005 frames in 1,242 s |
| SuperTuxKart 1.5 x86_64 under Box64 0.3.4 (Debian) | 640x480 and 1280x720 | crash | segmentation fault after the OpenGL 4.5 feature checks, 15 to 18 s after start |
| dsda-doom 0.29.0 (installed by Debian's prboom-plus package), `-timedemo` DEMO1, shareware WAD | 640x480 | 27.2 fps | 5,026 game tics in 6,460 real tics; default settings |
| Same | 1280x720 | 10.5 fps | 16,823 real tics |
| Steam download, anonymous, DepotDownloader 3.4.0 arm64 | app 1007 (Steamworks SDK Redist) | 107 MB in 28.9 s | 36.7 MB transferred |
| Same | app 896660 (Valheim dedicated server) | 2,088 MB in 371 s | 1,912 MB transferred, 5.2 MB/s on the wire; the server is an x86_64 program and was not started |
| steamcmd (x86) under Box64 0.3.4 BOX32, `+login anonymous` | app 1007 | stops | updates itself, then `PltResolver32: Symbol __sched_cpualloc ... not found` and hangs |

Box64 git (420eda6, 2026-10-23) wraps `__sched_cpualloc`; its build on the board was paused for the timed runs and then stopped at about 1 percent (a full build on two A53 cores is over an hour). So steamcmd and the x86_64 game were not retried with it.

**What it does not show.** Any frame rate on the HDMI screen (the Doom demo owned it; all runs drew to Xvfb), sound, input latency, a Steam game (needs an account), OpenArena or Quake (no timedemo data shipped with the Debian packages), RetroArch cores, three runs per row (one each), and the cause of the Box64 crash (desktop GL path; the arm64 build uses GLES).

**Verdict for games.** A 3D game is a slide show at 2 to 5 fps on this chip, natively; x86 translation would cut that further. 2D and software-rendered games are the class that fits: Doom written straight to the framebuffer runs at 186 to 193 fps, and through X at 10 to 27 fps.

**If you try it.** If SuperTuxKart's x86_64 build segfaults under Box64 0.3.4 after the GL feature list, run the arm64 build instead (the release ships one). If dsda-doom says `Unable to find required file "demo1"`, extract the DEMO1 lump to a `.lmp` file and pass its path. If steamcmd under Box64 stops on `__sched_cpualloc`, use DepotDownloader's arm64 build for anonymous downloads, or a Box64 built after 2026-09-13.
