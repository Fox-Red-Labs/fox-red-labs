#!/usr/bin/env python3
"""Adds TI's Wi-Fi CSI presence pipeline to the generated thermostat example (called by make_example.py).

Files under thermostat/csi/ti/ are TI's, byte for byte, from the EdgeAI plugin 1.20 live-preview example; csi/cmsis/ is the
CMSIS-DSP subset they need, from the same plugin. Every edit below is made on the copy in the generated example, never on
the originals.  FRL_CSI=1 adds the pipeline; FRL_CSI_TRIM=1 applies three RAM trims; FRL_HEAP=0x... sets the FreeRTOS heap.

RAM placement: TI's Matter linker script sends every .bss to the 128 kB TCM, which the thermostat image already fills
(127.4 kB used), while the main 509 kB RAM has the room. TI's own PRES_TCM attribute (section .bss.presence) is turned on
and extended to the two raw-store arrays; the model's workspace keeps its own section name (.bss.noinit.tvm); a copy of the
linker script in the example routes both to DRAM with one rule, and a GN build argument (frl_cc35xx_ldscript, added to the
executable template once, guarded) selects that copy. build-thermostat.sh passes the argument when FRL_CSI=1.

  python3 csi_patch.py --check      # dry run on a temp copy: proves every patch anchor still matches TI's files
"""
import os, re, shutil, sys

LDS_SRC = "src/platform/ti/cc35xx/cc35xx_freertos.lds"               # relative to TI's matter checkout, where make_example.py runs
GNI = "third_party/ti_simplelink_sdk/ti_simplelink_executable.gni"

LDS_RULE = ("    /* Fox Red Labs: the Wi-Fi CSI presence pipeline's window buffers and the model workspace, in DRAM.\n"
            "     * Both are written before they are read, so they need no zeroing at start (startup zeroes .bss and .psram_bss only). */\n"
            "    .bss.presence : {\n        *(.bss.presence)\n        *(.bss.noinit.tvm)\n        . = ALIGN(4);\n    } > DRAM_NON_SECURE\n")
GNI_OLD = ('    } else if (ti_simplelink_device_family == "cc35xx") {\n'
           '      ldscript = "${chip_root}/src/platform/ti/cc35xx/cc35xx_freertos.lds"\n')
GNI_NEW = ('    } else if (ti_simplelink_device_family == "cc35xx") {\n'
           '      if (frl_cc35xx_ldscript != "") {\n        ldscript = frl_cc35xx_ldscript\n      } else {\n'
           '        ldscript = "${chip_root}/src/platform/ti/cc35xx/cc35xx_freertos.lds"\n      }\n')
GNI_ARGS = ('declare_args() {\n  # Fox Red Labs: an alternative linker script for the cc35xx application (empty: TI\'s own).\n'
            '  frl_cc35xx_ldscript = ""\n}\n\ntemplate("ti_simplelink_executable") {')


def apply(DST, HERE, rd, wr, sub):
    if os.environ.get("FRL_CSI") == "1":
        csi = DST + "/src/csi"
        shutil.rmtree(csi, ignore_errors=True)
        shutil.copytree(HERE + "/csi", csi)
        c = rd(csi + "/ti/csi_presence.c")
        c = sub(c, '#include "uart_term.h"\n', '#include "csi_glue.h"\n')
        c = sub(c, '#include <ti/ai/edge_ai/dap/DAP.h>\n', '')
        c = c.replace("UART_PRINT(", "Report(")
        c = sub(c, "static void run_inference_and_send(void)\n{\n",
                   "static void run_inference_and_send(void)\n{\n    uint32_t t_start = csi_glue_cycles();\n")
        i = c.index("    if (DAP_isReadyToStartStreaming())"); j = c.index("    /* Wi-Fi keepalive")
        c = c[:i] + "    csi_glue_result(&s_last, csi_glue_ms_since(t_start));\n" + c[j:]
        # The example keeps the station alive with a 1-byte UDP broadcast through lwIP's socket API, which the Matter build
        # of lwIP does not compile (LWIP_SOCKET off). Matter's own traffic, mDNS and subscriptions, plays that part here.
        c = sub(c, '#include "lwip/sockets.h"\n', '')
        i = c.index("    /* Wi-Fi keepalive"); j = c.index("/* Inference task */"); k = c.rindex("}\n", 0, j)
        c = c[:i] + c[k:]
        i = c.index("    /* Open the UDP keepalive socket once."); j = c.index("    s_infer_sem = xSemaphoreCreateBinary();")
        c = c[:i] + c[j:]
        assert "lwip_" not in c, "a socket call survived"
        if os.environ.get("FRL_CSI_TRIM") == "1":
            c = trim(c, sub)
        c = sub(c, "#ifdef CC35XX\n/* .bss.presence",
                   "#if 1 /* Fox Red Labs: .bss.presence is routed to DRAM by cc35xx_freertos.lds in this example */\n/* .bss.presence")
        c = re.sub(r"^static (uint32_t|uint16_t|float)(\s+)(s_raw_ts_us|s_raw_mag)\[", r"PRES_TCM static \1\2\3[", c, flags=re.M)
        assert c.count("PRES_TCM static") >= 6, c.count("PRES_TCM static")
        wr(csi + "/ti/csi_presence.c", c)
        wr(DST + "/cc35xx_freertos.lds", sub(rd(LDS_SRC), "    /* COMMON symbols are in DRAM. */", LDS_RULE + "    /* COMMON symbols are in DRAM. */"))
        g = rd(GNI)
        if "frl_cc35xx_ldscript" not in g:
            g = sub(g, 'template("ti_simplelink_executable") {', GNI_ARGS)
            g = sub(g, GNI_OLD, GNI_NEW)
            wr(GNI, g)
            print("gni: frl_cc35xx_ldscript build argument added (once)")
        srcs = ["csi/csi_glue.c", "csi/ti/csi_presence.c", "csi/ti/feature_extract.c", "csi/ti/feature_extract_fft_cmsis.c"] + \
               ["csi/cmsis/Source/" + f for f in sorted(os.listdir(csi + "/cmsis/Source"))]
        b = rd(DST + "/BUILD.gn")
        b = sub(b, '    "${project_dir}/src/ThermostatManager.cpp",\n',
                   '    "${project_dir}/src/ThermostatManager.cpp",\n' + "".join('    "${project_dir}/src/%s",\n' % f for f in srcs))
        b = sub(b, '  include_dirs = [\n    "${project_dir}",\n',
                   '  include_dirs = [\n    "${project_dir}",\n    "${project_dir}/src/csi",\n    "${project_dir}/src/csi/ti",\n'
                   '    "${project_dir}/src/csi/cmsis/Include",\n    "${project_dir}/src/csi/cmsis/PrivateInclude",\n'
                   '    "${chip_root}/third_party/ti_simplelink_sdk/repo_cc35xx/source/ti/devices/cc35xx/cmsis/core",\n')
        b = sub(b, '  defines = []\n', '  defines = [ "FRL_CSI=1" ]\n  libs = [ "${project_dir}/src/csi/ti/mod.a" ]\n  cflags_c = [ "-w" ]\n')
        wr(DST + "/BUILD.gn", b)
        print("csi pipeline added:", len(srcs), "sources", "(trimmed)" if os.environ.get("FRL_CSI_TRIM") == "1" else "")
    heap = os.environ.get("FRL_HEAP")
    if heap:
        s = rd(DST + "/chip_thermostat.syscfg")
        s = sub(s, "FreeRTOS.heapSize       = 0x00051000;", "FreeRTOS.heapSize       = %s;" % heap)
        wr(DST + "/chip_thermostat.syscfg", s)
        print("heap size set to", heap)


def trim(c, sub):
    """Three RAM trims, each a few lines, none changing the arithmetic:
    1. raw store as 16-bit fixed point (1/256 of a magnitude step; the max magnitude 180.3 fits) instead of float: 62 kB to 31 kB
    2. interpolate straight into the transposed grid the feature extraction reads; the untransposed grid and the copy loop go: 27 kB
    3. the model reads the feature grid in place instead of a copy: 7 kB"""
    c = sub(c, "static float    s_raw_mag[PRES_RAW_CAP][PRES_N_SUB];",
               "static uint16_t s_raw_mag[PRES_RAW_CAP][PRES_N_SUB];   /* trim 1: magnitude * 256 */\n"
               "static void raw_row_f(uint32_t i, float out[PRES_N_SUB]) { for (uint32_t s = 0; s < PRES_N_SUB; s++) out[s] = (float) s_raw_mag[i][s] * (1.0f / 256.0f); }")
    c = sub(c, "        memcpy(s_raw_mag[s_raw_head], mags, sizeof(mags));",
               "        for (uint32_t s = 0; s < PRES_N_SUB; s++) s_raw_mag[s_raw_head][s] = (uint16_t) (mags[s] * 256.0f + 0.5f);")
    c = sub(c, "PRES_TCM static float s_dec[PRES_WIN_MODEL][PRES_N_SUB];\n", "")
    assert c.count("    if (n < 2u)\n") == 1
    c = sub(c, "    if (n < 2u)\n", "    float ra[PRES_N_SUB], rb[PRES_N_SUB], row[PRES_N_SUB];\n    if (n < 2u)\n")
    c = sub(c, "        memset(s_dec, 0, sizeof(s_dec));", "        memset(s_dec_T, 0, sizeof(s_dec_T));")
    c = sub(c, """        if (a == b || dt_ab == 0u)
        {
            memcpy(s_dec[k], s_raw_mag[a], sizeof(s_dec[k]));
        }
        else
        {
            uint32_t dt_aq = tq_us - s_raw_ts_us[a];
            float    frac  = (float)dt_aq / (float)dt_ab;
            lerp52(s_raw_mag[a], s_raw_mag[b], frac, s_dec[k]);
        }""", """        raw_row_f(a, ra);
        if (a == b || dt_ab == 0u)
        {
            memcpy(row, ra, sizeof(row));
        }
        else
        {
            uint32_t dt_aq = tq_us - s_raw_ts_us[a];
            float    frac  = (float)dt_aq / (float)dt_ab;
            raw_row_f(b, rb);
            lerp52(ra, rb, frac, row);
        }
        for (uint32_t sc = 0; sc < PRES_N_SUB; sc++) s_dec_T[sc][k] = row[sc];""")
    c = sub(c, "    for (uint32_t t = 0; t < PRES_WIN_MODEL; t++)\n        for (uint32_t sc = 0; sc < PRES_N_SUB; sc++)\n            s_dec_T[sc][t] = s_dec[t][sc];\n", "")
    c = sub(c, "PRES_TCM static float s_model_in[1][PRES_MODEL_SC][1][PRES_MODEL_T];\n", "")
    c = sub(c, "static struct tvmgen_default_inputs  s_tvm_in   = { .onnx__Add_0 = (void *)s_model_in };",
               "static struct tvmgen_default_inputs  s_tvm_in   = { .onnx__Add_0 = (void *)s_freq_feat };   /* trim 3: in place */")
    c = sub(c, "    memcpy(s_model_in, s_freq_feat, sizeof(s_model_in));\n", "")
    assert "s_dec[" not in c and "s_model_in" not in c
    return c


if __name__ == "__main__" and "--check" in sys.argv:
    import tempfile
    HERE = os.path.dirname(os.path.abspath(__file__)); DST = tempfile.mkdtemp()
    os.makedirs(DST + "/src"); os.makedirs(DST + "/" + os.path.dirname(LDS_SRC)); os.makedirs(DST + "/" + os.path.dirname(GNI))
    os.chdir(DST)
    def rd(p): return open(p).read()
    def wr(p, s): open(p, "w").write(s)
    def sub(s, old, new, count=1):
        assert old in s, "missing: " + old[:60]
        return s.replace(old, new, count)
    for trim_flag in ("0", "1"):
        wr("BUILD.gn", '    "${project_dir}/src/ThermostatManager.cpp",\n  include_dirs = [\n    "${project_dir}",\n  defines = []\n')
        wr("chip_thermostat.syscfg", "FreeRTOS.heapSize       = 0x00051000;\n")
        wr(LDS_SRC, "MEMORY {}\n    /* COMMON symbols are in DRAM. */\n")
        wr(GNI, 'template("ti_simplelink_executable") {\n' + GNI_OLD)
        os.environ["FRL_CSI"] = "1"; os.environ["FRL_CSI_TRIM"] = trim_flag; os.environ["FRL_HEAP"] = "0x00049000"
        apply(DST, HERE, rd, wr, sub)
    c = rd("src/csi/ti/csi_presence.c")
    print("placed:", [l.split("[")[0].split()[-1] for l in c.split("\n") if l.startswith("PRES_TCM static")])
    print("check ok")
