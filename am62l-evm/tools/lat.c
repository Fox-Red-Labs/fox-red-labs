#include <stdio.h>
#include <stdlib.h>
#include <time.h>
static double now(){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec*1e9+t.tv_nsec;}
int main(){
 size_t sizes[]={8192,16384,32768,65536,131072,262144,524288,1048576,4194304,16777216,67108864,0};
 for(int s=0;sizes[s];s++){size_t n=sizes[s]/sizeof(void*);void**a=malloc(sizes[s]);
  size_t stride=64/sizeof(void*);size_t cnt=n/stride;size_t*idx=malloc(cnt*sizeof(size_t));
  for(size_t i=0;i<cnt;i++)idx[i]=i;srand(1);
  for(size_t i=cnt-1;i>0;i--){size_t j=rand()%(i+1);size_t t=idx[i];idx[i]=idx[j];idx[j]=t;}
  for(size_t i=0;i<cnt;i++)a[idx[i]*stride]=&a[idx[(i+1)%cnt]*stride];
  void**p=&a[idx[0]*stride];long it=20000000;for(long i=0;i<1000000;i++)p=*p;
  double t0=now();for(long i=0;i<it;i++)p=*p;double t1=now();
  printf("%8zu KB %7.2f ns %p\n",sizes[s]/1024,(t1-t0)/it,(void*)p);free(a);free(idx);}
 return 0;}
