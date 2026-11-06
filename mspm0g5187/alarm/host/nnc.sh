#!/bin/bash
# Compile a trained alarm model with TI's NNC on GitHub and put the NPU build in ../model/.
#   nnc.sh <train_alarm.py OUTDIR> <name>
# Uses a clone of the private mspm0g3507 repository (same GitHub owner as this repo's origin, or FRL_GH_OWNER), branch nnc-alarm, whose
# workflow nnc-compile-alarm.yml compiles every alarm-models/<name>/model_tinpu_int8.onnx on push.
set -e
SRC=$1; NAME=$2; HERE="$(cd "$(dirname "$0")" && pwd)"
OWNER="${FRL_GH_OWNER:-$(git -C "$HERE" remote get-url origin | sed -E 's#^.*[:/]([^/]+)/[^/]+$#\1#')}"; REPO="$OWNER/mspm0g3507"; R=gh_repo=$REPO
C=$PWD/out/alarm/mfb
[ "$(gh repo view $REPO --json visibility -q .visibility)" = PRIVATE ] || { echo "repo not private, stop"; exit 1; }
if [ ! -d "$C/.git" ]; then git clone -q --depth 1 -b nnc-alarm https://github.com/$REPO.git "$C"; fi
cd "$C" && git pull -q
rm -rf alarm-models/* && mkdir -p "alarm-models/$NAME" && cp "$SRC/model_tinpu_int8.onnx" "alarm-models/$NAME/"
git add -A && CODEX_SKIP_REASON="Fox Red Labs: no Codex review (project rule); model file only" git commit -qm "alarm model $NAME" && git push -q
sleep 10
ID=$(gh run list -R $REPO -b nnc-alarm -L 1 --json databaseId -q '.[0].databaseId')
echo "run $ID"; gh run watch "$ID" -R $REPO --exit-status > /dev/null
D=$PWD/out/alarm/nnc_$NAME; rm -rf "$D"; gh run download "$ID" -R $REPO -n nnc-alarm -D "$D"
grep -il "error" "$D/$NAME/nnc_g5187_npu/compile.log" && { echo "compile log has errors"; }
rm -rf "$HERE/../model/nnc_g5187_npu" && mkdir -p "$HERE/../model" && cp -R "$D/$NAME/nnc_g5187_npu" "$HERE/../model/"
cp "$SRC/test_vectors.h" "$HERE/../model/" && cp "$SRC/model_tinpu_int8.onnx" "$SRC/result.json" "$HERE/../model/"
grep "Tensor" "$HERE/../model/nnc_g5187_npu/tvmgen_default.h"; echo "model $NAME in mspm0g5187/alarm/model/"
