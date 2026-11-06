# Regenerate mspm0l1306/memory-breakdown.json from the maps: run from mspm0l1306/ as python3 tools/memory/gen.py
import sys,json,re,collections
import os; sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import tiparse,gnuparse
NAMES=["RTOS","Radio stack","TI drivers and device library","C library and compiler runtime","Your app (the example's own code)","Heap and stacks","Other"]
def full(d): return {n:d.get(n,0) for n in NAMES}
builds=[]
def ti(id,label,sdk,source,path,notes=None):
    fu,ru,fl,ra,det=tiparse.parse(path)
    txt=open(path).read()
    for reg in('BCR_CONFIG','BSL_CONFIG'):
        assert int(re.search(reg+r'\s+\w+\s+\w+\s+(\w+)',txt).group(1),16)==0
    assert sum(fl.values())==fu and sum(ra.values())==ru,(id)
    tables=sum(v for (c,o),v in det.items() if c=='Other' and o!='hole')
    holes_f=sum(v for (c,o),v in det.items() if c=='Other' and o=='hole')
    b=dict(id=id,label=label,sdk=sdk,source=source,flash=full(fl),ram=full(ra),
      check=dict(flash_used_in_map=fu,flash_sum=sum(fl.values()),ram_used_in_map=ru,ram_sum=sum(ra.values()),
        flash_region=dict(origin="0x0",length=0xFFF8),nonmain_used=0,
        other_flash_startup_tables=tables,other_flash_padding=holes_f,other_ram_padding=0,unexplained_flash=0,unexplained_ram=0))
    builds.append(b)
SDK="MSPM0 SDK 2.11.00.07, no RTOS, TI Arm Clang 5.1.1"
ti('blink',"TI's blink example (no OS)",SDK+" (examples/nortos/LP_MSPM0L1306/driverlib/gpio_toggle_output)","data/maps/sdk-gpio_toggle_output.map","data/maps/sdk-gpio_toggle_output.map")
ti('out_of_box',"TI's out-of-box demo",SDK+" (demos/out_of_box)","data/maps/sdk-out_of_box.map","data/maps/sdk-out_of_box.map")
ti('coremark',"CoreMark, bare metal",SDK+", -O3, stack 3,072 bytes","bench/coremark/build-O3-32/coremark.map","bench/coremark/build-O3-32/coremark.map")
ti('occ_8k',"8k-parameter AI model",SDK+", -O3, stack 1,024 bytes, room-occupancy model","bench/edgeai/build-occ_cnn_8k-O3/occupancy.map","bench/edgeai/build-occ_cnn_8k-O3/occupancy.map")
ti('largest_sdk',"Largest TI example (battery monitor demo)",SDK+" (demos/bq7690x_control_i2c)","data/maps/sdk-bq7690x_control_i2c.map","data/maps/sdk-bq7690x_control_i2c.map")
def z(id,label,src,path,f,r,sdk):
    fl,ra,gf,gr,of,orr,fo,ro=gnuparse.account(path,f,r)
    assert of==0 and orr==0 and sum(fl.values())==f and sum(ra.values())==r
    b=dict(id=id,label=label,sdk=sdk,source=src,flash=full(fl),ram=full(ra),
      check=dict(flash_used_in_map=f,flash_sum=sum(fl.values()),ram_used_in_map=r,ram_sum=sum(ra.values()),
        flash_region=dict(origin="0x0",length=65536),nonmain_used=0,
        other_flash_startup_tables=0,other_flash_padding=fl[gnuparse.OT],other_ram_padding=ra[gnuparse.OT],unexplained_flash=0,unexplained_ram=0))
    builds.append(b)
ZS="Zephyr 4.4.2, GCC 12.2.0 (Zephyr SDK 0.17.4), out-of-tree lp_mspm0l1306 board"
z('zephyr_hello',"Zephyr hello_world","data/maps/zephyr-hello_world.map","data/maps/zephyr-hello_world.map",9424,3832,ZS+", samples/hello_world")
z('zephyr_blinky',"Zephyr blinky","data/maps/zephyr-blinky.map","data/maps/zephyr-blinky.map",11364,3888,ZS+", samples/basic/blinky")
z('zephyr_coremark',"Zephyr CoreMark","data/maps/zephyr-coremark2k-O3.map","data/maps/zephyr-coremark2k-O3.map",35760,4080,ZS+", mcu-bench-l1306 CoreMark alone at 2,000 bytes, -O3 (zephyr/build.sh coremark2k-O3)")
for b in builds: b['source']=b['source']
out=dict(board="LP-MSPM0L1306",chip="MSPM0L1306",flash_total=65536,ram_total=4096,units="bytes",generated="2026-10-06",
 method="Each object and library in the linker map was put in one component. TI maps: SECTION ALLOCATION MAP entries placed by address in FLASH or SRAM, grouped by library or object (driverlib.a, the SysConfig-generated ti_msp_dl_config and the startup file go to TI drivers and device library; libc.a, libsysbm.a and libclang_rt.builtins.a to C library and compiler runtime; the example's own objects, its .common data and the AI model archive to Your app), the .stack and .sysmem sections to Heap and stacks, and the compressed start-up tables (.cinit) and alignment holes to Other. Zephyr GNU maps: every input section was added up by address (run address in RAM; initialized data also counted at its load address in flash). Zephyr kernel, arch, drivers (timer, serial, pinctrl, clock, console and the board's GPIO driver), libzephyr and the C-library glue went to RTOS; picolibc and libgcc to C library and compiler runtime; app/libapp.a to Your app; TI's hal_ti driverlib to TI drivers and device library; the interrupt stack, main stack and idle thread stack to Heap and stacks; padding between sections to Other. Every build's components add up exactly to the used FLASH and RAM the linker or west reported. The chip has no radio, so no build has a radio stack. None of these builds writes the chip's boot configuration area (NONMAIN).",
 flash_total_note="64 KB main flash. TI's linker script gives FLASH a length of 0xFFF8 (65,528 bytes), 8 bytes short of 64 KB; the Zephyr board uses the full 0x10000 (65,536).",
 builds=builds)
json.dump(out,open('memory-breakdown.json','w'),indent=2); open('memory-breakdown.json','a').write('\n')
for b in builds:
    print(b['id'],b['check']['flash_used_in_map'],b['check']['ram_used_in_map'],b['check']['flash_sum']==b['check']['flash_used_in_map'],b['check']['ram_sum']==b['check']['ram_used_in_map'],b['check']['other_flash_startup_tables'],b['check']['other_flash_padding'])
