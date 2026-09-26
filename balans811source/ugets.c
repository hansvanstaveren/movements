/* @(#)ugets.c                            11/08/2020 */
/*============================= ugets ===============================*/
/* substitute for fgets, all of CR LF CRLF LFCR serve as end-of-line */
/* compared to fgets, there is an extra parameter *lastEOL  */
/* pointer to integer variable with value 0 on first call   */
/* on return *lastEOL is the actual EOL character, CR or LF */

#include <stdio.h>
#include <string.h>

#ifdef unix
#include <errno.h>
#endif /* unix */

// This prototype should ideally be in a ugets.h, but this is enough for 'clang -Weverything'
char *ugets(char *ptr, int cnt, FILE *fp, int *lastEOL);

#define EOL0 10       /* LF, end-of-line character   */
#define EOL1 13       /* CR, to be converted to EOL0 */

char *ugets(char *ptr, int cnt, FILE *fp, int *lastEOL)
{ int c=0;
  char *pt;
  int firstbyte=1;
  pt=ptr;

  while (--cnt)
  {
skip:
    c=getc(fp);
    if((c==EOL0) || (c==EOL1))
    {
      if(firstbyte)
      {
        firstbyte=0;
        /* ignore second byte from previous CRLF or LFCR */
        if((*lastEOL) && (*lastEOL != c)) goto skip;
      }
      *lastEOL=c;      /* save actual EOL */
      *pt++ = EOL0;    /* store EOL0 in buffer */
      break;           /* terminate this ugets call */ 
    }
    if(c < 0 || c==EOF)
    {
      *pt++ =0;
      *lastEOL=0;      /* reset lastEOL */
      ptr=NULL;        /* return value of ugets */
      clearerr(fp);
      break;
    }
    *pt++ = (char)c;   /* store character in buffer */
    firstbyte=0;
  }
  if(cnt >= 0) *pt = 0;

  return(ptr);
}
