// Note: You must include readmove.h before including orderb.h

// errors and warnings indicated in Status
// =======================================
// 1  playing twice against same opponents.
// 2  movement not suitable for balans
//      not all boards played equally often
//      different number of tables in different rounds
// 4  pairs playing more than once in the same round
// 8  pairs playing the same board twice
// 16 illegal movement, other than the above
// 32 invalid data

#define Dubbelopp   1
#define Unsuitable  2
#define Dubbelpair  4
#define Dubbelbord  8
#define Illegalmov 16
#define Invaliddat 32

typedef struct
{
  // Basic movement info as read by readmove():
  SPUL *movement; // Array of G=r*t1 elements of {{NS pair#, EW pair#}, board group}
  int P1;         // Number of pairs in the movement (including vacant pair, if any)
  int r;          // Number of rounds
  int b;          // Number of board groups according to header of movement file
  int t1;         // Number of tables nominally (some may be unoccupied)
  int Getallen;   // 1 if board groups numeric, 0 if they should be seen as chars
  int G;          // = r*t1, redundant, but added for convenience (# of elems in array)
  // Extra movement info found by inspecting the full movement, plus choice of seeing it:
  int Nb;                    // Actual number of board groups found (0 = not yet inspected)
  unsigned char letter[256]; // letter[0..Nb-1] = each different board group letter found
  int order_b[256];          // order_b[c] = index of c in letter[] (cache avoiding linear search)
  unsigned int Bias;         // Smallest board group value (~ letter before conv to uchar)
  int Vacant;     // Regard this pair number as vacant (0 = no vacancy)
  int Status;     // Error flags from inspection of movement, see balans.h for values
} mov_info_t;

int orderb(mov_info_t *pmv, unsigned int L);
int Letter(mov_info_t *pmv, int k);

// order_b[] should be kept in sync with letter[]; using this func for updates ensures that
static inline void set_letter(mov_info_t *pmv, int k, unsigned int L)
{
  pmv->letter[k]  = (unsigned char)L;
  pmv->order_b[L] = k;
}
