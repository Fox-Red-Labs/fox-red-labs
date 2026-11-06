/* Host harness for tt_proto.c, driven by test_proto.py over stdin. One command per line:
 *   L <uart line>                         parse; prints "ok <type> <n> <ev> <v> <cmd> <duty>" or "err <code>", then "S ..."
 *   G <seq> <link> <ext>                  legacy advertising data as hex
 *   X <seq> <name|-> <events|-> <tag|->   extended data as hex; events "id:type:t:v,..." tag "up:link:ok:bad:fw:mv"
 *   D <hex>                               decode; prints "F ..." or "none"
 *   Z                                     clear rpmx (a new snapshot)
 */
#include "tt_proto.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static tt_tool_t tool;

static void print_state(void)
{
    printf("S %s %u %u %u %u %u %u %u %u %d %u %u %u %u %u %d\n", tool.id[0] ? tool.id : "-", tool.trig, tool.run, tool.oc,
           tool.flt, tool.rpm, tool.rpmx, tool.ipk, tool.vb, tool.tc, tool.st, tool.has_bat, tool.soc, tool.cyc, tool.bvb,
           tool.btc);
}

static void hex(const uint8_t *b, size_t n)
{
    size_t i;
    for (i = 0; i < n; i++) printf("%02x", b[i]);
    printf("\n");
}

int main(void)
{
    char buf[1024];
    memset(&tool, 0, sizeof(tool));
    tool.soc = 0xFF;
    while (fgets(buf, sizeof(buf), stdin)) {
        size_t n = strlen(buf);
        while (n && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) buf[--n] = 0;
        if (buf[0] == 'L' && buf[1] == ' ') {
            tt_line_t l;
            int r = tt_parse_line(buf + 2, n - 2, &l, &tool);
            if (r) printf("err %d\n", r);
            else printf("ok %s %d %u %u %s %u P %u %u %u %u %u %u %d %u\n", l.type, l.n, l.ev, l.v, l.cmd[0] ? l.cmd : "-",
                        l.duty, l.pull.dur10, l.pull.ravg, l.pull.rmax, l.pull.iavg, l.pull.ipk, l.pull.vmin, l.pull.tc,
                        l.pull.cls);
            print_state();
        } else if (buf[0] == 'G') {
            unsigned seq, link, ext; uint8_t out[31];
            sscanf(buf + 2, "%u %u %u", &seq, &link, &ext);
            hex(out, tt_build_legacy(out, (uint8_t)seq, &tool, tt_flags(&tool, (int)link, (int)ext)));
        } else if (buf[0] == 'X') {
            unsigned seq, i; char name[40], evs[512], tags[64], pls[400];
            tt_pull_t pl[TT_MAX_PULLS]; uint8_t npl = 0;
            tt_event_t ev[TT_MAX_EVENTS]; uint8_t nev = 0; tt_tagstat_t tag; int has_tag = 0;
            uint8_t out[TT_EXT_MAX];
            sscanf(buf + 2, "%u %39s %511s %63s %399s", &seq, name, evs, tags, pls);
            for (i = 0; name[i]; i++) if (name[i] == '_') name[i] = ' ';
            if (strcmp(evs, "-")) {
                char *tok = strtok(evs, ",");
                while (tok && nev < TT_MAX_EVENTS) {
                    unsigned a, b, c, d;
                    sscanf(tok, "%u:%u:%u:%u", &a, &b, &c, &d);
                    ev[nev].id = (uint16_t)a; ev[nev].type = (uint8_t)b; ev[nev].t = c; ev[nev].v = (uint16_t)d; nev++;
                    tok = strtok(NULL, ",");
                }
            }
            if (strcmp(tags, "-")) {
                unsigned a, b, c, d, e, g = 0;
                sscanf(tags, "%u:%u:%u:%u:%u:%u", &a, &b, &c, &d, &e, &g);
                tag.up = a; tag.link = (uint16_t)b; tag.rx_ok = (uint16_t)c; tag.rx_bad = (uint8_t)d; tag.fw = (uint8_t)e; tag.mv = (uint16_t)g;
                has_tag = 1;
            }
            if (strcmp(pls, "-")) {
                char *tok = strtok(pls, ",");
                while (tok && npl < TT_MAX_PULLS) {
                    unsigned a[9]; int c;
                    sscanf(tok, "%u:%u:%u:%u:%u:%u:%u:%d:%u", &a[0], &a[1], &a[2], &a[3], &a[4], &a[5], &a[6], &c, &a[8]);
                    pl[npl].id = (uint16_t)a[0]; pl[npl].dur10 = (uint16_t)a[1]; pl[npl].ravg = (uint16_t)a[2];
                    pl[npl].rmax = (uint16_t)a[3]; pl[npl].iavg = (uint16_t)a[4]; pl[npl].ipk = (uint16_t)a[5];
                    pl[npl].vmin = (uint16_t)a[6]; pl[npl].tc = (int8_t)c; pl[npl].cls = (uint8_t)a[8]; npl++;
                    tok = strtok(NULL, ",");
                }
            }
            hex(out, tt_build_ext(out, sizeof(out), (uint8_t)seq, &tool, ev, nev, pl, npl, has_tag ? &tag : NULL,
                                  strcmp(name, "-") ? name : NULL));
        } else if (buf[0] == 'D') {
            uint8_t d[300]; size_t dl = 0, i; tt_frame_t f;
            for (i = 2; i + 1 < n && dl < sizeof(d); i += 2) { unsigned x; sscanf(buf + i, "%2x", &x); d[dl++] = (uint8_t)x; }
            if (tt_decode_adv(d, dl, &f)) { printf("none\n"); continue; }
            printf("F %u %u %s %s %u %u %u %u", f.vt, f.seq, f.name[0] ? f.name : "-", f.tool.id[0] ? f.tool.id : "-",
                   f.tool.trig, f.tool.oc, f.tool.flt, f.tool.run);
            if (f.vt == TT_VT_LEGACY) printf(" flags=%u soc=%u tc=%d\n", f.flags, f.tool.soc, f.tool.tc);
            else {
                printf(" live=%u %u %u %u %u %d %u bat=%u", f.has_live, f.tool.rpm, f.tool.rpmx, f.tool.ipk, f.tool.vb,
                       f.tool.tc, f.tool.st, f.has_bat);
                if (f.has_bat) printf(" %u %u %u %d", f.tool.soc, f.tool.cyc, f.tool.bvb, f.tool.btc);
                printf(" ev=%u", f.nev);
                for (i = 0; i < f.nev; i++) printf(" %u:%u:%u:%u", f.ev[i].id, f.ev[i].type, f.ev[i].t, f.ev[i].v);
                if (f.has_tag) printf(" tag=%u:%u:%u:%u:%u:%u", f.tag.up, f.tag.link, f.tag.rx_ok, f.tag.rx_bad, f.tag.fw, f.tag.mv);
                printf(" pulls=%u", f.npull);
                for (i = 0; i < f.npull; i++) printf(" %u:%u:%u:%u:%u:%u:%u:%d:%u", f.pull[i].id, f.pull[i].dur10, f.pull[i].ravg,
                                                    f.pull[i].rmax, f.pull[i].iavg, f.pull[i].ipk, f.pull[i].vmin, f.pull[i].tc, f.pull[i].cls);
                printf("\n");
            }
        } else if (buf[0] == 'Z') {
            tool.rpmx = tool.rpm;
        }
        fflush(stdout);
    }
    return 0;
}
