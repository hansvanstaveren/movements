#include<stdio.h>
#include "raninit.h"

int main()
{ 
  double s1, s2, s3, s4, s5;
  unsigned int k1, k2, k3, k4, k5;
  long t1,t2,t3,t4,t5, t6;
  int i;

  printf("ticks\nSee if ticks value changes fast enough\n");
  for(i=0; i<60; i++)
  {
    t1=ticks();
    t2=ticks();
    t3=ticks();
    t4=ticks();
    t5=ticks();
    t6=ticks();
    printf("%10ld %10ld %10ld %10ld %10ld %10ld\n",
      t1, t2, t3, t4, t5, t6);
  }

  printf("\ngenrand\nDefault values without raninit\n");
  for(i=0; i<10; i++)
  {
    k1=genrand();
    k2=genrand();
    k3=genrand();
    k4=genrand();
    k5=genrand();
    printf("%10u %10u %10u %10u %10u\n", k1, k2, k3, k4, k5);
  }

  printf("\ndgenrand\nCheck if subsequent calls raninit produce different sequence\n");
  for(i=0; i<60; i++)
  {
    raninit(); s1=dgenrand();
    raninit(); s2=dgenrand();
    raninit(); s3=dgenrand();
    raninit(); s4=dgenrand();
    raninit(); s5=dgenrand();
    printf("%7.3f %7.3f %7.3f %7.3f %7.3f\n", s1, s2, s3, s4, s5);
  }
 
  printf("\ngenrand\nCheck if subsequent calls raninit produce different sequence\n");
  for(i=0; i<60; i++)
  {
    raninit(); k1=genrand();
    raninit(); k2=genrand();
    raninit(); k3=genrand();
    raninit(); k4=genrand();
    raninit(); k5=genrand();
    printf("%10u %10u %10u %10u %10u\n", k1, k2, k3, k4, k5);
  }
}
