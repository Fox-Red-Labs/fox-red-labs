#!/bin/bash
# Runs inside ros:noetic-ros-base (arm64). /clone: radar-lab/ti_mmwave_rospkg (read-only, unedited);
# /in: stream.bin; /out: results. Builds the driver, feeds the stream into a virtual serial port at
# 4 frames a second, records every point published on /ti_mmwave/radar_scan_pcl.
set -e
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq >/dev/null
apt-get install -y -qq ros-noetic-serial ros-noetic-pcl-ros ros-noetic-pcl-conversions libpcl-dev socat python3-catkin-tools >/dev/null 2>&1
source /opt/ros/noetic/setup.bash
mkdir -p /ws/src && cp -r /clone /ws/src/ti_mmwave_rospkg
# The one edit: Noetic's PCL 1.10 needs C++14; the package's build file asks for C++11 in two lines.
sed -i 's/-std=c++11/-std=c++14/; s/CMAKE_CXX_STANDARD 11/CMAKE_CXX_STANDARD 14/' /ws/src/ti_mmwave_rospkg/CMakeLists.txt
cd /ws && catkin_make -j4 > /out/build.log 2>&1 || { tail -30 /out/build.log; exit 1; }
echo "build OK"
source /ws/devel/setup.bash
roscore > /out/roscore.log 2>&1 &
sleep 5
socat pty,raw,echo=0,link=/tmp/radar pty,raw,echo=0,link=/tmp/feed &
socat pty,raw,echo=0,link=/tmp/cmd pty,raw,echo=0,link=/tmp/cmdfeed &
sleep 1
for ns in ti_mmwave mmWaveDataHdl mmWaveCommSrv; do
  rosparam set /$ns/data_port /tmp/radar; rosparam set /$ns/data_rate 115200
  rosparam set /$ns/command_port /tmp/cmd; rosparam set /$ns/command_rate 115200; rosparam set /$ns/frame_id ti_mmwave
done
# Glue: the driver busy-waits for these until its config node has parsed profileCfg/frameCfg, which an
# MMWAVE-L-SDK cfg does not have. Values from people-count-250ms.cfg (chirpComnCfg 16 0 0 128 4 28 0,
# chirpTimingCfg 6 32 0 40 60.5, frameCfg 2 0 200 64 250 0, channelCfg 7 3 0). On the SDK 3 point path
# only t_fr is used (marker lifetime); the rest are printed.
P=/ti_mmwave
rosparam set $P/numAdcSamples 128; rosparam set $P/numLoops 64; rosparam set $P/num_TX 2
rosparam set $P/f_s 6.25e6; rosparam set $P/f_c 60.5e9; rosparam set $P/BW 819.2e6
rosparam set $P/PRI 200e-6; rosparam set $P/t_fr 0.25; rosparam set $P/max_range 23.4
rosparam set $P/range_resolution 0.183; rosparam set $P/max_doppler_vel 12.39
rosparam set $P/doppler_vel_resolution 0.194
python3 /tools/record_points.py /out/points.json 120 > /out/record.log 2>&1 &
REC=$!
ROSCONSOLE_STDOUT_LINE_BUFFERED=1 rosrun ti_mmwave_rospkg ti_mmwave_rospkg __name:=ti_mmwave > /out/driver.log 2>&1 &
sleep 4
FEED_PERIOD=${FEED_PERIOD:-0.25} python3 /tools/feed.py /in/stream.bin /tmp/feed
sleep 3
kill -INT $REC; wait $REC || true
tail -3 /out/record.log; cp -r /root/.ros/log /out/roslog 2>/dev/null || true
grep -c . /out/driver.log | sed 's/^/driver log lines: /'
