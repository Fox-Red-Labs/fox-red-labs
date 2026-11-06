#include <stdio.h>
#include <time.h>
#include "gp_core.h"
static void hex(const char *t, const uint8_t *p, int n){ printf("%s len=%d ", t, n); for(int i=0;i<n;i++) printf("%02x", p[i]); printf("\n"); }
static void run(const char *name, int startsel){
  gp_config_t c; gp_config_default(&c); if(startsel){c.special[GP_START]=1;c.special[GP_SELECT]=1;}
  gp_t g; gp_begin(&g,&c); uint8_t m[64]; int n;
  printf("== %s\nFRL-GP1 begin\n", name);
  hex("MAP", g.desc, g.desc_len);
  n=gp_pack(&g,m); hex("RPT1",m,n);
  gp_press(&g,5); gp_press(&g,16); gp_press_start(&g);
  gp_set_axes(&g,32767,32767,32767,32767,32767,32767,32767,32767); gp_set_hat1(&g,GP_HAT_DOWN_RIGHT);
  n=gp_pack(&g,m); hex("RPT2",m,n);
  gp_release(&g,5); gp_release_start(&g); gp_set_hat1(&g,GP_HAT_CENTERED); gp_set_axes(&g,0,0,0,0,0,0,0,0);
  n=gp_pack(&g,m); hex("RPT3",m,n);
  gp_press(&g,1); gp_press(&g,8); gp_press(&g,9); gp_press_select(&g);
  gp_set_axes(&g,-32768,-1,1,255,256,-256,12345,-12345); gp_set_hat1(&g,GP_HAT_UP_LEFT);
  n=gp_pack(&g,m); hex("RPT4",m,n);
  struct timespec a,b; volatile int sink=0;
  clock_gettime(CLOCK_MONOTONIC,&a); for(int i=0;i<200;i++){g.desc_len=0;gp_build_descriptor(&g);sink+=g.desc_len;} clock_gettime(CLOCK_MONOTONIC,&b);
  double d=((b.tv_sec-a.tv_sec)*1e9+(b.tv_nsec-a.tv_nsec))/200/1000.0;
  clock_gettime(CLOCK_MONOTONIC,&a); for(int i=0;i<1000;i++) sink+=gp_pack(&g,m); clock_gettime(CLOCK_MONOTONIC,&b);
  double p=((b.tv_sec-a.tv_sec)*1e9+(b.tv_nsec-a.tv_nsec))/1000/1000.0;
  printf("TIME (host, not target) desc_us=%.3f pack_us=%.3f\nFRL-GP1 end\n", d, p);
}
int main(void){ run("code-derived default (no special buttons)",0); run("JOBSPEC wording (start+select enabled)",1); return 0; }
