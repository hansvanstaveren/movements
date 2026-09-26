//bits of Verbose
//                     octal      hex
#define V_inp_move     0000010 /* 0x0008 input movement     */
#define V_inp_mat      0000020 /* 0x0010 input matrices     */
#define V_inp_swit     0000040 /* 0x0020 input fixed tables */
#define V_out_move     0000100 /* 0x0040 output movement    */
#define V_out_mat      0000200 /* 0x0080 output matrices    */
#define V_out_swit     0000400 /* 0x0100 fixed and switched tables */
#define V_out_mini     0000400 /* 0x0100 minimal output */
// alias: in balans en vernum use V_out_swit, in score2 use V_out_mini
#define V_misc         0001000 /* 0x0200 miscellaneous output */
#define V_progress     0002000 /* 0x0400 show progress text */
#define V_progress_ext 0004000 /* 0x0800 show progress more often */
#define V_Qf1          0010000 /* 0x1000 Qf1 table          */
#define V_Qf           0020000 /* 0x2000 Results: Qf, etc   */
#define V_summary      0040000 /* 0x4000 Results: as output by '-Q;' */
#define V_expert       0100000 /* 0x8000 experts */
#define V_warn         0200000 /* 0x10000 warnings (e.g. pairs meeting twice) */

#define P_inp_move  (Verbose & V_inp_move)
#define P_inp_mat   (Verbose & V_inp_mat)
#define P_inp_swit  (Verbose & V_inp_swit)
#define P_out_move  (Verbose & V_out_move)
#define P_out_mat   (Verbose & V_out_mat)
#define P_out_swit  (Verbose & V_out_swit)
#define P_out_mini  (Verbose & V_out_swit)
#define P_misc      (Verbose & V_misc)
#define P_progress  (Verbose & V_progress)
#define P_progress_ext  (Verbose & V_progress_ext)
#define P_Qf1       (Verbose & V_Qf1)
#define P_Qf        (Verbose & V_Qf)
#define P_summary   (Verbose & V_summary)
#define P_expert    (Verbose & V_expert)
#define P_warn      (Verbose & V_warn)

#define P_null      (Verbose == 0)
#define P_quiet     (Verbose == 1)
#define P_mini      (Verbose == 2)
#define P_regular   (Verbose == 3)
#define P_all       (Verbose == 7)

#define V_null      (0)
#define V_quiet     (V_summary)
#define V_mini      (V_out_mini | V_summary)
#define V_regular   (V_misc | V_inp_move | V_inp_mat | V_inp_swit | V_out_move | V_out_mat | V_out_swit | V_Qf1 | V_Qf | V_progress | V_warn)
#define V_all       (0x7ffffff8)
