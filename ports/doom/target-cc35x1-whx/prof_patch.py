#!/usr/bin/env python3
"""Profiling build only (PROF=1 in build.sh): adds cycle timers around the big steps of pd_end_frame in the pinned upstream copy.
Usage: prof_patch.py <path to pd_render.cpp>   (edits that copy, never the pinned source)"""
import sys
p = sys.argv[1]
s = open(p).read()
head = '''extern "C" uint64_t hal_cyc(void);
uint64_t fox_pt[8];
#define FOXT(i, stmt) do { uint64_t _t = hal_cyc(); stmt; fox_pt[i] += hal_cyc() - _t; } while (0)
'''
i = s.index('void pd_end_frame(int wipe_start)')
s = s[:i] + head + s[i:]
def sub(a, b):
    global s
    assert a in s, a
    s = s.replace(a, b, 1)
sub('    reclip_fuzz_columns();', '    FOXT(0, reclip_fuzz_columns());')
sub('    int16_t fr_list = predraw_visplanes();', '    int16_t fr_list; FOXT(1, fr_list = predraw_visplanes());')
sub('    draw_visplanes(fr_list);', '    FOXT(2, draw_visplanes(fr_list));')
sub('    re_sort_regular_columns_by_fd_num();', '    FOXT(3, re_sort_regular_columns_by_fd_num());')
sub('    draw_regular_columns(0);', '    FOXT(4, draw_regular_columns(0));')
sub('    draw_fuzz_columns();', '    FOXT(5, draw_fuzz_columns());')
open(p, 'w').write(s)
