# Runs ON the board: sh run-lan.sh <Mac LAN address>. Frigate with the C7x detector, input = the Mac's webcam over RTSP (start beagley-ai/bench/camera-lan/mac_stream.sh on the Mac first).
cd /root/frigate
(SOC=j722s FRL_LABELS=/root/frigate/labelmap.txt setsid nohup timeout 3000 python3 deepstack_tidl.py > server.log 2>&1 < /dev/null &)
sleep 25
python3 probe.py http://localhost:5100/v1/vision/detection 5 /opt/edgeai-test-data/images/0000.jpg 2>&1 | tail -3
sed "s#<mac-address>#$1#" config-lan-camera.yml > cfg/config.yml
docker rm -f frigate >/dev/null 2>&1
docker run -d --name frigate --network host --shm-size 256m -v /root/frigate/cfg:/config -v /root/frigate/media:/media ghcr.io/blakeblackshear/frigate:0.18.2-standard-arm64 | cut -c1-12
sleep 80; uptime
sh sample.sh lan-camera-c7x 130 netcam 2>&1
