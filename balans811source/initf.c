/* @(#)initf.c             june 2017 */
/* =================== initf  ===================== */

/* is mingw sufficiently unix-like? */

/* ^C trapping works in windows cmd,
 * but mingw shell traps the ^C already and kills the program:
 * signed integer overflow works in both shells
 * but not in combination with -O option.
 * compile with gcc -ftrapv <filename>
 */

/* keyboard interrupt signal (^C), timer, and integer overflow handling.
 * call initf() (once only).
 * ^C sets global variable INTERRUPT
 * so does time expiration
 * integer overflow exits program
 */

#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <time.h>
#include "optim.h"
#define true 1

int INTERRUPT=0;

static void sigint()                    /* come here by ^C */
{
  INTERRUPT= true;
  /* fprintf(3) is not on the signal-safety(7) list but write(2) is so use that although a brutal thing.
   * The "(void) !" silences gcc in case we compile with -D_FORTIFY_SOURCE=2 with glibc (Linux), see
   * https://gcc.gnu.org/bugzilla/show_bug.cgi?id=66425#c34
   */
  (void) !write(2," ***quit***\007\015\012",14);
  if(ferror(stdout)) clearerr(stdout); /* signal unsafe but usually works and we'll finish soon anyway */
  signal(SIGINT, sigint);
}

__attribute__ ((noreturn)) static void sigabort() /* come here on signed integer overflow */
{
  (void) !write(2,"***integer overflow***\007\015\012",25);
  exit(1);
}

int initf()
{
  signal(SIGINT , sigint );
  signal(SIGABRT, sigabort);
  return(0);
}

static int maxtime;
static time_t tim0;

int setmaxtime(int max)
{
  maxtime=max;
  return (tim0= (int)time(NULL));
}

int qinterrupt()
{
  /* User interrrupt (Ctrl-C) sets global variable INTERRUPT */
  /* time expiration also sets INTERRUPT */
  if( difftime(time(NULL), tim0) >= maxtime)
  {
    INTERRUPT=1;
  }
  return INTERRUPT;
}
/* --------------  test program ----------------------------- */
#ifdef TEST
#include <signal.h>
#include <stdio.h>
#include <limits.h>

int main() {

  initf();
  int largeInt = INT_MAX;
  int normalInt = 42;
  int overflowInt = largeInt + normalInt;  /* should cause overflow */

  /* if compiling with -ftrapv, we shouldn't get here */
  printf("%12d + %2d = %12d\n", largeInt, normalInt, overflowInt);
  return 0;
}
#endif
