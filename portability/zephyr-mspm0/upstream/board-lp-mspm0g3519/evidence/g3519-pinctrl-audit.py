# Compares every pin function in Zephyr's shared MSPM0G1x0x/G3x0x pin file (hal_ti
# dts/ti/mspm0/g/mspm0g1x0x_g3x0x-pinctrl.dtsi, the file lp_mspm0g3519.dts includes) with SysConfig 1.28.0's
# MSPM0G351X device data. Usage: SYSCONFIG_DEVICEDATA=<sysconfig>/dist/deviceData HAL_TI=<hal_ti> python3 this.py
import os
import json,re
d=json.load(open(os.environ['SYSCONFIG_DEVICEDATA']+'/MSPM0G351X/MSPM0G351X.json'))
pp=d['peripheralPins']; dp=d['devicePins']
sc={}
for m in d['muxes']:
    p=dp[str(m['devicePinID'])]; pin=p['name']
    c=p['attributes'].get('iomux_pincm'); 
    sc.setdefault(pin,{})['pincm']=int(c) if c and c.isdigit() else None
    for s in m.get('muxSetting',[]):
        sc[pin][int(s['mode'])]=pp.get(str(s['peripheralPinID']),{}).get('name')
t=open(os.environ['HAL_TI']+'/dts/ti/mspm0/g/mspm0g1x0x_g3x0x-pinctrl.dtsi').read()
bad=0;tot=0
for name,cm,fn in re.findall(r'(\w+): \w+ \{\s*pinmux = <MSP_PINMUX\((\d+), MSPM0_PIN_FUNCTION_(\w+)\)>',t):
    pin=name.split('_')[-1].upper()
    if not re.match(r'P[ABC]\d+$',pin): continue
    tot+=1
    if pin not in sc: print('NOPIN',name); bad+=1; continue
    if int(cm)!=sc[pin]['pincm']: print('PINCM',name,cm,'sysconfig',sc[pin]['pincm']); bad+=1; continue
    if fn in ('GPIO','ANALOG'): continue
    got=sc[pin].get(int(fn))
    if got is None: print('NOFUNC',name,'f',fn); bad+=1; continue
    per=name.rsplit('_',1)[0].split('_')[0].upper()   # e.g. TIMA1, SPI0, I2C1, UART0
    if per not in got.replace('.','_').upper(): print('DIFF',name,'f',fn,'sysconfig says',got); bad+=1
print('checked',tot,'pin functions; mismatch',bad)
