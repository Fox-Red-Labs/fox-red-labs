#!/bin/bash
# Build the KVM/binder kernel for the AM62L EVM. Runs inside an arm64 debian:trixie container on the Mac mini
# (native arm64, no cross prefix). Layout on the build host: $KBUILD is a case-sensitive volume (APFS sparsebundle
# on a Mac) holding:
#   src/               git archive HEAD of the SDK kernel tree (the SDK checkout is case-collided on APFS)
#   running-config     zcat of the board's /proc/config.gz
#   kvm-binder.config  the fragment in this folder
# Start: docker run --rm --network host --platform linux/arm64 -v "$KBUILD":/kb debian:trixie bash /kb/build.sh
set -e
export DEBIAN_FRONTEND=noninteractive
# Build tools: from /kb/debs if present (fetched on another machine when the build host had no outbound TCP),
# else from apt.
if ls /kb/debs/*.deb >/dev/null 2>&1; then
  dpkg -i /kb/debs/*.deb
else
  echo 'Acquire::ForceIPv4 "true";' > /etc/apt/apt.conf.d/99v4
  apt-get update
  apt-get install -y make gcc bc bison flex libssl-dev libelf-dev perl kmod
fi
cd /kb/src
O=/kb/out; mkdir -p $O
cp /kb/running-config $O/.config
scripts/kconfig/merge_config.sh -O $O -m $O/.config /kb/kvm-binder.config
make O=$O ARCH=arm64 olddefconfig
cp $O/.config /kb/final.config
date +%s > /kb/t0
make O=$O ARCH=arm64 -j8 Image dtbs modules > /kb/build.log 2>&1
date +%s > /kb/t1
rm -rf /kb/modroot
make O=$O ARCH=arm64 INSTALL_MOD_PATH=/kb/modroot INSTALL_MOD_STRIP=1 modules_install > /kb/modinstall.log 2>&1
rel=$(cat $O/include/config/kernel.release)
B=/kb/bundle; rm -rf $B; mkdir -p $B
cp $O/arch/arm64/boot/Image $B/
cp $O/arch/arm64/boot/dts/ti/k3-am62l3-evm*.dtb* $B/ 2>/dev/null || true
rm -f /kb/modroot/lib/modules/$rel/build /kb/modroot/lib/modules/$rel/source
tar -C /kb/modroot/lib/modules -czf $B/modules-$rel.tar.gz $rel
cp /kb/final.config $B/config-$rel
cp /kb/kvm-binder.config $B/
cp /kb/kernel-swap.sh $B/
(cd $B && sha256sum * > SHA256SUMS)
echo "$rel" > /kb/RELEASE; echo done > /kb/DONE
