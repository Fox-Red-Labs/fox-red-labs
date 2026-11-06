# Sourced by the MSPM0 build scripts:  . "<repo>/tools/frl/toolpaths.sh"
# Sets SDK (MSPM0 SDK folder) and CC (TI Arm Clang folder) and nothing else, found the way frl finds them
# (TI install folders, or FRL_MSPM0_SDK and FRL_TICLANG). Exits 2 with toolenv's message when it cannot.
# TOOLPATHS_FAMILY (default mspm0-cc; mspm33-cc for the MSPM33 scripts, SDK then holds the MSPM33 SDK) picks the toolenv family.
# A script that has its own override sets TOOLPATHS_KEEP=1 first: SDK and CC already set stay as they are.
_tp_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [ -z "${TOOLPATHS_KEEP:-}" ] || [ -z "${SDK:-}" ] || [ -z "${CC:-}" ]; then
  SDK=""; CC=""
  while IFS= read -r -d '' _tp_a; do
    case "$_tp_a" in MSPM0_SDK_INSTALL_DIR=*|MSPM33_SDK_INSTALL_DIR=*) SDK=${_tp_a#*=};; TICLANG_ARMCOMPILER=*) CC=${_tp_a#*=};; esac
  done < <(python3 "$_tp_dir/toolenv.py" "${TOOLPATHS_FAMILY:-mspm0-cc}")
  [ -n "$SDK" ] && [ -n "$CC" ] || exit 2
fi
unset _tp_dir _tp_a
