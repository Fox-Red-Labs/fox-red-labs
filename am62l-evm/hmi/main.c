/* Fixed HMI scene: 12 animated bars + 12 live counters + one big counter, 1920x1080.
   usage: scene fbdev|drm SECONDS ; prints "frames N" per second to stderr via flush count */
#include "lvgl/lvgl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
static uint32_t flushes;
static uint32_t tick_ms(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec*1000u+t.tv_nsec/1000000u;}
static void rr(lv_event_t*e){(void)e;flushes++;}
static lv_obj_t *bars[12], *lbl[12], *big;
static void set_w(void*o,int32_t v){lv_obj_set_width(o,v);}
static unsigned cnt;
static void tm(lv_timer_t*t){ (void)t; cnt++; char b[24];
  for(int i=0;i<12;i++){snprintf(b,sizeof b,"%u",(cnt*(i+3))%100000);lv_label_set_text(lbl[i],b);}
  snprintf(b,sizeof b,"%u",cnt);lv_label_set_text(big,b);}
int main(int argc,char**argv){
  int drm=argc>1&&!strcmp(argv[1],"drm"); int secs=argc>2?atoi(argv[2]):30;
  lv_init(); lv_tick_set_cb(tick_ms);
  lv_display_t*d;
  if(drm){d=lv_linux_drm_create(); lv_linux_drm_set_file(d,"/dev/dri/card0",-1);}
  else {d=lv_linux_fbdev_create(); lv_linux_fbdev_set_file(d,"/dev/fb0");}
  if(!d){fprintf(stderr,"no display\n");return 1;}
  lv_display_add_event_cb(d,rr,LV_EVENT_REFR_READY,NULL);
  lv_timer_set_period(lv_display_get_refr_timer(d),16);
  lv_obj_t*s=lv_screen_active(); lv_obj_set_style_bg_color(s,lv_color_hex(0x101820),0);
  int W=lv_display_get_horizontal_resolution(d),H=lv_display_get_vertical_resolution(d);
  fprintf(stderr,"res %dx%d\n",W,H);
  for(int i=0;i<12;i++){
    bars[i]=lv_obj_create(s); lv_obj_remove_style_all(bars[i]);
    lv_obj_set_pos(bars[i],40,40+i*70); lv_obj_set_size(bars[i],200,56);
    lv_obj_set_style_bg_opa(bars[i],LV_OPA_COVER,0);
    lv_obj_set_style_bg_color(bars[i],lv_color_hex(0x2080e0+i*0x0a0a00),0);
    lv_obj_set_style_radius(bars[i],14,0);
    lv_anim_t a; lv_anim_init(&a); lv_anim_set_var(&a,bars[i]); lv_anim_set_exec_cb(&a,set_w);
    lv_anim_set_values(&a,120,W-480); lv_anim_set_duration(&a,1500+i*180);
    lv_anim_set_playback_duration(&a,1500+i*180); lv_anim_set_repeat_count(&a,LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a,lv_anim_path_ease_in_out); lv_anim_start(&a);
    lbl[i]=lv_label_create(s); lv_obj_set_style_text_color(lbl[i],lv_color_white(),0);
    lv_obj_set_style_text_font(lbl[i],&lv_font_montserrat_28,0);
    lv_obj_set_pos(lbl[i],W-400,52+i*70); lv_label_set_text(lbl[i],"0");
  }
  big=lv_label_create(s); lv_obj_set_style_text_font(big,&lv_font_montserrat_48,0);
  lv_obj_set_style_text_color(big,lv_color_hex(0xffd040),0); lv_obj_set_pos(big,W-300,H-120);
  lv_timer_create(tm,16,NULL);
  uint32_t t0=tick_ms(),last=t0,lf=0;
  while(tick_ms()-t0<(uint32_t)secs*1000){
    uint32_t w=lv_timer_handler(); usleep(w>2?2000:500);
    uint32_t n=tick_ms(); if(n-last>=1000){fprintf(stderr,"fps %u\n",flushes-lf);lf=flushes;last=n;}
  }
  fprintf(stderr,"total_flushes %u over %u ms\n",flushes,tick_ms()-t0);
  return 0;}
