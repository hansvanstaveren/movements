#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

typedef struct
{
  int noordzuid[2];
  unsigned int bord;
} SPUL;


#define errstat    0x80000000L   /* set if error status */
#define notinteger 0x00000001L   /* number is non-integer */
#define nonumber   0x00000002L   /* no number decoded   */
#define EOString   0x00000004L   /* end of string encountered */
#define alpha      0x00000008L   /* terminator is alphanumeric */
#define numerr     0x00000020L   /* numerical error */
#define syntaxerr  0x00000040L
#define filerr     0x00000800L   /* file error */
#define EOFstat    0x00004000L   /* end of file encountered */
#define readerr    0x00040000L   /* read error */

int intgetb(char *line,int *len,int *ipt,int *ianswer,int* term);

SPUL *readmove(SPUL *move, const char *inputfile, int *pP,int *pr, int *pb, int *pt, int *pGetallen);

void printmove(FILE *f, const SPUL *move, int P1, int r, int b, int t1,
               int Getallen, int base, int offodd, int offeven);
