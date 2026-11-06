# IAR CXARM 10.10.2 build tools on Ubuntu 24.04 (linux/amd64, Rosetta on the Mac mini).
# Build context must hold dl/iar-lmsc-tools_1.14_amd64.deb and dl/cxarm-10.10.2.27058.deb.
# TI device files are NOT in this image (see iar_harness.py): they are mounted from $FRL_TI_ROOT/iar-devsup.
# Do not rebuild casually: it needs about 3 GB of Docker disk.
# The debs are bind-mounted, not copied, so no layer carries them (the build host has little disk).
# Evaluation licence: nothing built here leaves the lab (see README.md).
FROM --platform=linux/amd64 ubuntu:24.04
ENV DEBIAN_FRONTEND=noninteractive
RUN --mount=type=bind,source=dl,target=/dl \
    apt-get update \
 && apt-get install -y --no-install-recommends ca-certificates make python3 \
 && apt-get install -y --no-install-recommends /dl/iar-lmsc-tools_1.14_amd64.deb /dl/cxarm-10.10.2.27058.deb \
 && rm -rf /var/lib/apt/lists/*
# Debs install under /opt/iar (checked in the build log); put every bin dir on PATH.
RUN set -e; ls -d /opt/iar/* 2>/dev/null || true; \
    for d in $(find /opt/iar -type d -name bin 2>/dev/null); do echo "$d" >> /etc/iar-path; done; \
    echo "PATH=$(tr '\n' ':' </etc/iar-path)\$PATH" > /etc/profile.d/iar.sh
WORKDIR /work
CMD ["bash","-l"]
