"""Record every PointCloud2 on /ti_mmwave/radar_scan_pcl: one list of [x, y, z, ...fields] per message."""
import json, signal, sys, rospy
from sensor_msgs.msg import PointCloud2
import sensor_msgs.point_cloud2 as pc2
msgs, fields = [], None
def cb(m):
    global fields
    fields = [f.name for f in m.fields]
    msgs.append([list(p) for p in pc2.read_points(m)])
def done(*_):
    json.dump({"fields": fields, "messages": msgs}, open(sys.argv[1], "w"))
    print(f"messages {len(msgs)}, points {sum(len(m) for m in msgs)}, fields {fields}"); sys.exit(0)
rospy.init_node("record_points"); rospy.Subscriber("/ti_mmwave/radar_scan_pcl", PointCloud2, cb)
signal.signal(signal.SIGINT, done); rospy.sleep(float(sys.argv[2])); done()
