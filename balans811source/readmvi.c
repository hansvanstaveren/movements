#include "readmove.h"
#include "orderb.h"
#include "readmvi.h"

/* ======================= read_movement ==============================
 * Wrapper around readmove() using the enhanced movement info structure.
 * At input the given structure may be a local variable on the stack with
 * completely indeterminate content; we only write to it, don't read it.
 *
 * When you are done using the current movement, before reusing the struct
 * to read another movement, call free_movement() to free the current one.
 *
 * Returns: 0 on success, -1 on error
 */
int read_movement(mov_info_t *pmv, const char *inputfile)
{
  pmv->movement = NULL;
  pmv->Nb = 0;
  pmv->Vacant = 0;
  pmv->Status = 0;

  if(!(pmv->movement = readmove(NULL, inputfile, &pmv->P1, &pmv->r, &pmv->b, &pmv->t1, &pmv->Getallen)))
  {
    fprintf(stderr,"error in movement file %s\n",inputfile);
    pmv->Status |= Invaliddat; // Includes the case 'b <= 0' since readmove() already checked that
    return -1;
  }
  pmv->G = pmv->r * pmv->t1;
  return 0; // Success
}

void free_movement(mov_info_t *pmv)
{
  if(!pmv->movement) return;
  free(pmv->movement);
  pmv->movement = NULL;
  pmv->Nb = 0;  // Extra safety against programming errors
}

// Convenience wrapper around printmove()
void print_movement(const mov_info_t *pmv, FILE *f, int base, int offodd, int offeven)
{
  printmove(f, pmv->movement, pmv->P1, pmv->r, pmv->b, pmv->t1, pmv->Getallen, base, offodd, offeven);
}

