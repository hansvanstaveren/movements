#include "balans.h"
// balans as a subprogram

int main(int argc, char *argv[])
{
  QUAL Qual;
  FILE *File;
  int filenr=1;
  int filenr2;
  int retval, mretval=0;

  // Skip initial options in dumb way, not allowing space after option letter
  // (but be wise enough to know that a lone '-' isn't an option but like a file):
  while(filenr < argc
        && argv[filenr][0] == '-'
        && argv[filenr][1] != '\0') ++filenr;

  if(filenr >= argc) // Only options; at least one filename (including '-') required
  {
    printf("call: %s [BALANS OPTION]... FILENAME...\n", argv[0]);
    return 1;
  }
  filenr2=filenr;

  while(filenr2<argc)
  {
    argv[filenr]=argv[filenr2];
    printf("%s\t",argv[filenr]);
    memset(&Qual, 0, sizeof(QUAL));
    retval= balans_main(filenr+1, argv, &Qual, &File); // balans_main must only see 1 file arg
    if(mretval<retval)mretval=retval;
    printf("x=%d ",retval);
    if(retval<2)
      printf("p=%2d r=%2d Qf=%5.2f  Qf1av=%5.2f",
      Qual.P, Qual.r, Qual.Qf, Qual.Qf1av);
    printf("\n");
    fflush(stdout);
    fflush(stderr);
    ++filenr2;
  }
  return mretval;
}
