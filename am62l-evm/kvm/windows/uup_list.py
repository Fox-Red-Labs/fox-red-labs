"""Turn a UUP dump API file list into an aria2 input file, leaving out the cumulative updates.

Usage: python3 -I uup_list.py uup.json aria2-base.txt
uup.json comes from (GET, no account):
  https://api.uupdump.net/get.php?id=<build uuid>&lang=en-us&edition=professional
The URLs point at Microsoft's download servers; each file carries its SHA-1.
The Linux converter cannot integrate .psf/.msu updates, so SSU and KB packages are skipped:
the result is the base 22621.1 image (Windows 11 22H2 RTM), not 22631.
"""
import json, re, sys
d = json.load(open(sys.argv[1]))["response"]
with open(sys.argv[2], "w") as o:
    for name, f in d["files"].items():
        if re.match(r"(?i)(windows11\.0-kb|ssu-)", name):
            continue
        o.write("%s\n  out=%s\n  checksum=sha-1=%s\n" % (f["url"], name, f["sha1"]))
print(d["updateName"], d["build"])
