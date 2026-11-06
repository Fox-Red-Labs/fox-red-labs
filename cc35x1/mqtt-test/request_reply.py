import sys,time,threading,statistics,paho.mqtt.client as mqtt
BROKER_HOST,BROKER_PORT='<broker address>',1883   # set to your broker
n=int(sys.argv[1]); gap=float(sys.argv[2]); topic_cmd=sys.argv[3]; topic_rep=sys.argv[4]
ev=threading.Event(); got=[]; t_sent=[0.0]; lat=[]; unsol=[0]
def on_msg(c,u,m):
    t=time.time()
    if t_sent[0] and not ev.is_set():
        lat.append((t-t_sent[0])*1000); ev.set(); sample.append(m.payload[:200])
    else: unsol[0]+=1
sample=[]
try: c=mqtt.Client(mqtt.CallbackAPIVersion.VERSION1)
except Exception: c=mqtt.Client()
c.on_message=on_msg; c.connect(BROKER_HOST,BROKER_PORT,60); c.subscribe(topic_rep,qos=0); c.loop_start(); time.sleep(1)
ok=0; start=time.time()
for i in range(n):
    ev.clear(); t_sent[0]=time.time(); c.publish(topic_cmd,'x',qos=0)
    if ev.wait(min(gap,4.0)): ok+=1
    rest=gap-(time.time()-t_sent[0])
    if rest>0: time.sleep(rest)
c.loop_stop()
print('sent',n,'replies',ok,'loss_pct',round(100*(n-ok)/n,2),'unsolicited',unsol[0],'secs',round(time.time()-start))
if lat: print('lat_ms min',round(min(lat),1),'median',round(statistics.median(lat),1),'mean',round(statistics.mean(lat),1),'max',round(max(lat),1))
print(sample[:1])
