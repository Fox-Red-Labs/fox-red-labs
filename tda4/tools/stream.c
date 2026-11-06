/* STREAM-style copy/scale/add/triad on 64 MB arrays, single thread, wall clock. Not the official STREAM output format. */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#define N (8*1024*1024)
static double now(){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
int main(){double *a=malloc(N*8),*b=malloc(N*8),*c=malloc(N*8);double s=3.0;for(long i=0;i<N;i++){a[i]=1;b[i]=2;c[i]=0;}
double best[4]={1e9,1e9,1e9,1e9};for(int r=0;r<5;r++){double t;
t=now();for(long i=0;i<N;i++)c[i]=a[i];t=now()-t;if(t<best[0])best[0]=t;
t=now();for(long i=0;i<N;i++)b[i]=s*c[i];t=now()-t;if(t<best[1])best[1]=t;
t=now();for(long i=0;i<N;i++)c[i]=a[i]+b[i];t=now()-t;if(t<best[2])best[2]=t;
t=now();for(long i=0;i<N;i++)a[i]=b[i]+s*c[i];t=now()-t;if(t<best[3])best[3]=t;}
double by[4]={2.0*8*N,2.0*8*N,3.0*8*N,3.0*8*N};const char*nm[4]={"copy","scale","add","triad"};
for(int k=0;k<4;k++)printf("%s %.0f MB/s\n",nm[k],by[k]/best[k]/1e6);printf("check %f\n",a[N/2]);return 0;}
