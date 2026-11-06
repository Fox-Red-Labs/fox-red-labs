#!/usr/bin/env python3
"""Build the IAR (.ewp) projects of the TI SDKs with IAR Build Tools for Arm (CXARM) on the Mac mini.

Runs on the Mac mini (python3 and docker there). Nothing big lives on the MacBook.
The image is frl-iar:10.10.2 (Dockerfile beside this file, linux/amd64 under OrbStack).
SDK trees live on the build host at $FRL_TI_ROOT/<sdk>; results land in data/toolchains/iar/<sdk>.csv
next to this repo's copy (or --out).

  iar_harness.py list  --sdk mspm0_sdk_2_11_00_07 [--board LP_MSPM0L1306] [--filter empty] [--limit N]
  iar_harness.py build --sdk ...  same filters; add --dry to print the docker command and stop
  iar_harness.py status            licence state inside the image (iarlogin login-status)

Licence (never stored in this repo): either a bearer token saved on the build host in ~/.config/iar/token
(exported as IAR_LMS_BEARER_TOKEN for the container), or a device-code sign-in stored in the docker
volume frl-iar-home:  docker run -it --platform linux/amd64 -v frl-iar-home:/root frl-iar:10.10.2 \\
  iarlogin login --use-device-code
Dry mode and list need neither.

Device support: the CXARM .deb has no TI device files (iarbuild says 'Unknown device name'). IAR publishes them
as a public tarball (github.com/iarsystems/arm releases 10.10.2, cxarm-device-support-ti-*.tar.bz2). Extract it on the
mini into $FRL_TI_ROOT/iar-devsup; batch.sh symlinks it into the install at run time. Do not bake it into the image:
the rebuild needs about 3 GB of Docker disk and filled the build host's boot disk on 2026-10-21.

Note: the SDK .ewp files point at SysConfig output that ships nowhere in the SDK. Generate it first
with sysconfig_gen.py (on the Mac, small files) and push it to the build host; without it a project fails
on the missing ti_msp_dl_config.c, which the CSV records as status "fail" with that text.
"""
import argparse, csv, glob, os, re, shlex, subprocess, sys, xml.etree.ElementTree as ET

HERE = os.path.dirname(os.path.abspath(__file__))
SDK_ROOT = os.environ.get("FRL_TI_ROOT", "/opt/ti")
IMAGE = "frl-iar:10.10.2"
OUTDIR = os.path.join(HERE, "..", "..", "..", "data", "toolchains", "iar")
TOKEN_FILE = os.path.expanduser("~/.config/iar/token")
FIELDS = ["sdk", "board", "project", "config", "status", "exit_code", "seconds",
          "code_bytes", "rodata_bytes", "data_bytes", "error", "tool"]
BOARD_RE = re.compile(r"(LP_[A-Z0-9_]+|[a-z0-9]+_lp\b|LP_EM_[A-Z0-9]+)")


def configs(ewp):
    """Configuration names defined in a project file."""
    try:
        return [c.findtext("name") for c in ET.parse(ewp).getroot().iter("configuration")] or ["Debug"]
    except ET.ParseError:
        return ["Debug"]


def projects(sdk, board=None, filt=None):
    root = os.path.join(SDK_ROOT, sdk)
    out = []
    for ewp in sorted(glob.glob(root + "/**/*.ewp", recursive=True)):
        rel = os.path.relpath(ewp, root)
        if board and board not in rel:
            continue
        if filt and filt not in rel:
            continue
        m = BOARD_RE.search(rel)
        out.append((ewp, rel, m.group(1) if m else ""))
    return out


def write_varfile(sdk, path):
    """Argument-variable file for iarbuild -varfile: the SDK's own tools/iar/*.custom_argvars with every
    path that points into the SDK (its Windows or Linux install folder) rewritten to the build host's copy."""
    root = os.path.join(SDK_ROOT, sdk)
    files = sorted(glob.glob(root + "/tools/iar/*.custom_argvars"))
    files = [f for f in files if "WINDOWS" not in f.upper()] or files
    if not files:
        return None
    text = open(files[0], errors="replace").read()
    text = re.sub(r"<value>[^<]*?/(%s[^/<]*)(/[^<]*)?</value>" % re.escape(sdk.split("_sdk")[0]),
                  lambda m: "<value>" + root + (m.group(2) or "") + "</value>", text)
    text = re.sub(r"(<name>\w*SYSCONFIG_ROOT</name>\s*<value>)[^<]*", r"\1/harness/shim", text)
    text = re.sub(r"(<name>\w*SYSCONFIG_CLI_CMD</name>\s*<value>)[^<]*", r"\1/harness/shim/sysconfig_cli.sh", text)
    open(path, "w").write(text)
    return os.path.basename(files[0])


def docker_cmd(listfile, logdir, extra_env, sdk=None):
    envs = [x for k in extra_env for x in ("-e", k)]
    # SimpleLink F3 projects call macOS-only post-build tools; mount Linux stand-ins over them (see each file's header).
    standins = [("tools/common/crc_tool/crc_tool", "crc_tool_linux.py"), ("tools/common/mcuboot/imgtool", "imgtool_linux.sh"),
                ("tools/ble/oad/commands_runner", "pyscript_linux.sh"), ("tools/ble/oad/msoad_prebuild", "pyscript_linux.sh"),
                ("tools/ble/oad/msoad_postbuild", "pyscript_linux.sh")]
    crcmount = []
    for rel, src in standins:
        dst = os.path.join(SDK_ROOT, sdk or "", rel)
        if sdk and os.path.isfile(dst):
            crcmount += ["-v", os.path.join(HERE, src) + ":" + dst + ":ro"]
    return (["docker", "run", "--rm", "--network", "host", "--platform", "linux/amd64", "-v", SDK_ROOT + ":" + SDK_ROOT, "-e", "FRL_TI_ROOT=" + SDK_ROOT] + crcmount + [
             "-v", HERE + ":/harness:ro", "-v", os.path.dirname(listfile) + ":/run",
             "-v", "frl-iar-home:/root", "-v", SDK_ROOT + "/iar-devsup:/devsup:ro"] + envs +
            [IMAGE, "/harness/batch.sh", "/run/" + os.path.basename(listfile), "/run/" + os.path.basename(logdir), "/run/argvars.custom_argvars"])


def parse_log(text):
    """(status, error line, code, rodata, data) from an iarbuild log."""
    num = lambda s: int(re.sub(r"[^\d]", "", s) or 0)
    sizes = {}
    for m in re.finditer(r"([\d ,.]+)\s+bytes of\s+(readonly\s+code|readonly\s+data|readwrite\s+data)\s+memory", text):
        sizes[re.sub(r"\s+", " ", m.group(2))] = num(m.group(1))
    err = ""
    for pat in (r"(?:Fatal )?[Ee]rror\[LMSC\w*\][^\n]*", r"(?:Fatal )?[Ee]rror\[\w+\][^\n]*", r"fatal error:[^\n]*", r"sysconfig shim: no pre-generated[^\n]*", r"Unknown device name[^\n]*", r"No such file[^\n]*", r"ninja: error[^\n]*", r"[Cc]annot open[^\n]*", r"Total number of errors: [1-9]\d*", r"cannot make progress[^\n]*"):
        m = re.search(pat, text)
        if m:
            err = re.sub(r"\s+", " ", m.group(0)).strip()[:300]
            break
    return err, sizes.get("readonly code", ""), sizes.get("readonly data", ""), sizes.get("readwrite data", "")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("cmd", choices=["list", "build", "status", "report"])
    ap.add_argument("--sdk"); ap.add_argument("--board"); ap.add_argument("--filter")
    ap.add_argument("--config", help="one configuration name (default: every one the project defines)")
    ap.add_argument("--limit", type=int, default=0); ap.add_argument("--offset", type=int, default=0, help="skip the first N projects"); ap.add_argument("--shard", help="K/N: projects whose index mod N is K"); ap.add_argument("--stride", type=int, default=1, help="every Nth project (sampling)"); ap.add_argument("--from-list", help="file of '<ewp path>TAB<config>' lines (as in a run's list.tsv): build exactly these"); ap.add_argument("--dry", action="store_true")
    ap.add_argument("--out", default=OUTDIR); ap.add_argument("--work", default=os.path.expanduser("~/iar-build/runs"))
    a = ap.parse_args()

    env = dict(os.environ)
    if os.path.exists(TOKEN_FILE):
        env["IAR_LMS_BEARER_TOKEN"] = open(TOKEN_FILE).read().strip()
    extra = ["IAR_LMS_BEARER_TOKEN"] if "IAR_LMS_BEARER_TOKEN" in env else []

    if a.cmd == "status":
        cmd = ["docker", "run", "--rm", "--platform", "linux/amd64", "-v", "frl-iar-home:/root"] + \
              [x for k in extra for x in ("-e", k)] + [IMAGE, "bash", "-lc", "iarlogin login-status; iarlogin version"]
        print(shlex.join(cmd)) if a.dry else subprocess.call(cmd, env=env)
        return
    if not a.sdk:
        ap.error("--sdk is required")
    if a.cmd == "report":
        write_csv(a.sdk, a.work, {(l.split("\t")[0], l.split("\t")[1]): (None, BOARD_RE.search(l).group(1) if BOARD_RE.search(l) else "") for l in open(os.path.join(a.work, "list.tsv")).read().splitlines() if l}, a.out)
        return
    if a.cmd == "build" and not a.dry:
        sys.path.insert(0, HERE); import sdk_fixes; sdk_fixes.apply(os.path.join(SDK_ROOT, a.sdk))
    plist = projects(a.sdk, a.board, a.filter)
    if a.offset:
        plist = plist[a.offset:]
    if a.shard:
        k, n = map(int, a.shard.split("/")); plist = plist[k::n]
    if a.stride > 1:
        plist = plist[::a.stride]
    if a.limit:
        plist = plist[:a.limit]
    rows = [(e, r, b, c) for e, r, b in plist for c in ([a.config] if a.config else configs(e))]
    if a.from_list:
        want = {tuple(l.rstrip("\n").split("\t")[:2]) for l in open(a.from_list) if l.strip()}
        rows = [x for x in rows if (x[0], x[3]) in want]
    if a.cmd == "list" or a.dry:
        for e, r, b, c in rows:
            print(f"{b or '-':22} {c:10} iarbuild {shlex.quote(e)} -build {c}")
        print(f"# {len(plist)} projects, {len(rows)} builds", file=sys.stderr)
        if a.cmd == "build":
            os.makedirs(a.work, exist_ok=True)
            print(shlex.join(docker_cmd(os.path.join(a.work, a.sdk, "list.tsv"), os.path.join(a.work, a.sdk, "logs"), extra, a.sdk)))
        return

    run = os.path.join(a.work, a.sdk + (("-shard" + a.shard.replace("/", "of") if a.shard else "") + ("-off%d" % a.offset if a.offset else ""))); os.makedirs(run, exist_ok=True)
    listfile = os.path.join(run, "list.tsv")
    print("argument variable file:", write_varfile(a.sdk, os.path.join(run, "argvars.custom_argvars")))
    open(listfile, "w").write("".join(f"{e}\t{c}\n" for e, r, b, c in rows))
    logdir = os.path.join(run, "logs")
    subprocess.call(docker_cmd(listfile, logdir, extra, a.sdk), env=env)
    meta = {(e, c): (r, b) for e, r, b, c in rows}
    write_csv(a.sdk, run, meta, a.out)


def classify(text, rc):
    """Status word for one build: pass, licence, link-ok (only the macOS post-build tool failed) or fail."""
    if rc == 0:
        return "pass"
    if re.search(r"(Fatal )?[Ee]rror\[LMSC|LMSC2\d\d\d\]|set IAR_LMS_BEARER_TOKEN", text):
        return "licence"
    if "Cannot run macOS (Mach-O)" in text and not re.search(r"Error\[", text) and "Unknown device name" not in text:
        return "link-ok"
    return "fail"


def map_sizes(text):
    """Code, rodata, data bytes from the linker map named on the ilinkarm command line, when it was written."""
    m = re.search(r'--map\s+("[^"]+"|\S+)', text)
    if not m:
        return "", "", ""
    path = m.group(1).strip('"')
    try:
        mt = open(path, errors="replace").read()
    except OSError:
        return "", "", ""
    num = lambda x: int(re.sub(r"[^\d]", "", x) or 0)
    got = {}
    for mm in re.finditer(r"([\d' ,.]+)\s+bytes of\s+(readonly\s+code|readonly\s+data|readwrite\s+data)\s+memory", mt):
        got[re.sub(r"\s+", " ", mm.group(2))] = num(mm.group(1))
    return got.get("readonly code", ""), got.get("readonly data", ""), got.get("readwrite data", "")


def write_csv(sdk, run, meta, out):
    """One CSV row per build, from the run folder's logs/results.tsv. meta maps (ewp, config) to (relative path, board)."""
    os.makedirs(out, exist_ok=True)
    logdir = os.path.join(run, "logs")
    csvp = os.path.join(out, sdk + ".csv")
    with open(csvp, "w", newline="") as f:
        w = csv.writer(f); w.writerow(FIELDS)
        for line in open(os.path.join(logdir, "results.tsv")):
            e, c, rc, secs, log = line.rstrip("\n").split("\t")
            text = open(os.path.join(logdir, os.path.basename(log)), errors="replace").read()
            err, code, ro, data = parse_log(text)
            if not code:
                code, ro, data = map_sizes(text)
            status = classify(text, int(rc))
            if status == "link-ok":
                m = re.findall(r'"?/[^ "]*/tools/[^ "]*?/([A-Za-z_]+)"? [^\n]*\n+Cannot run macOS', text)
                err = "post-build %s is a macOS binary" % (m[-1] if m else "tool")
            r, b = meta.get((e, c), (None, ""))
            r = r or os.path.relpath(e, os.path.join(SDK_ROOT, sdk))
            w.writerow([sdk, b, r, c, status, rc, secs, code, ro, data, err, "iar-cxarm-10.10.2"])
    print("wrote", csvp)


if __name__ == "__main__":
    main()
