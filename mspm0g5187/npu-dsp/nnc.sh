#!/bin/bash
# Compile the DSP graphs with TI's NNC on GitHub's Linux machines (NNC has no Intel Mac build) and fetch the result.
#   nnc.sh <dir with <name>/model_tinpu_int8.onnx for each model>   (the output dir of host/make_models.py)
# Uses the private mspm0g3507 repository (same GitHub owner as this repo's origin, or FRL_GH_OWNER), branch nnc-dsp, whose
# workflow nnc-compile-dsp.yml compiles every dsp-models/<name>/model_tinpu_int8.onnx on push, for the G5187 NPU and the
# Cortex-M0+ CPU, each with float and with integer input and output (skip_normalize=true output_int=true).
# Result: out/npu-dsp/nnc/<name>/nnc_g5187_{npu,cpu}_{float,int}/ (mod.a, tvmgen_default.h, compile.log)
set -e
SRC=$1; HERE="$(cd "$(dirname "$0")" && pwd)"
OWNER="${FRL_GH_OWNER:-$(git -C "$HERE" remote get-url origin | sed -E 's#^.*[:/]([^/]+)/[^/]+$#\1#')}"; REPO="$OWNER/mspm0g3507"
[ "$(gh repo view $REPO --json visibility -q .visibility)" = PRIVATE ] || { echo "repo not private, stop"; exit 1; }
W=$PWD/out/npu-dsp; C=$W/mfb
[ -d "$C/.git" ] || git clone -q --depth 1 -b nnc-dsp https://github.com/$REPO.git "$C"
cd "$C" && git checkout -q nnc-dsp && git pull -q
rm -rf dsp-models && mkdir dsp-models
for d in "$SRC"/*/; do n=$(basename "$d"); [ -f "$d/model_tinpu_int8.onnx" ] && mkdir -p "dsp-models/$n" && cp "$d/model_tinpu_int8.onnx" "dsp-models/$n/"; done
git add -A && git diff --cached --quiet || CODEX_SKIP_REASON="Fox Red Labs: no Codex review (project rule); model files only" git commit -qm "dsp models" && git push -q
sleep 10
ID=$(gh run list -R $REPO -b nnc-dsp -L 1 --json databaseId -q '.[0].databaseId')
echo "run $ID"; gh run watch "$ID" -R $REPO --exit-status > /dev/null
rm -rf "$W/nnc_dl" && gh run download "$ID" -R $REPO -n nnc-dsp -D "$W/nnc_dl"
rm -rf "$W/nnc" && mv "$W/nnc_dl" "$W/nnc" && ls "$W/nnc"
