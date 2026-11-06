# Security

This file says what counts as a security problem in the Fox Red Labs scripts and how to report one.

## What is in scope

Scripts here erase and program microcontroller flash, drive debug probes over USB, join radios to home networks (Zigbee, Thread, Bluetooth LE) and read from Home Assistant. If you find one that can damage a board, leak a network key, or run something it did not say it would, tell us.

## How to report

Open a private security advisory on the repository (Security tab, "Report a vulnerability"), or email hello@foxredlabs.dev. Say which file, what it does, and how you found it. You will get an answer within a week.

## What we do

- A privacy scanner runs on every export, and the export stops if it finds a hit. It reads text files, the text inside `.gz` and `.zip` archives, the printable strings of binary files, PDF and Office documents, the metadata of images (GPS, owner, camera serial) and the text inside images. It looks for network keys, passwords and tokens, private addresses, personal email and phone numbers, home folder paths, device addresses and a list of private names. If you find something it missed, that is a report we want.
- The scanner cannot read video, audio, fonts and 3D meshes. A file of one of those types only ships when the export's manifest lists its type with a reason and a count, so a new one is a deliberate decision. Git history of the private source repository is not scanned; this repository starts with a fresh history.
- Every script that can erase flash says so in its first lines and in its folder's README.
- This repository's tree holds no built firmware images. Prebuilt images, when they are published, go out as release assets with a SHA-256 checksum. The private preview copy that includes the browser flasher (the Digital Lab) carries the images it flashes inside its site folder, and is served only to a named list of readers.

## What is out of scope

Vulnerabilities in the vendor SDKs, Zephyr, OpenThread, Home Assistant or other upstream projects: report those upstream. We will link the upstream advisory from the affected page.
