// Include orderb.h before this one (defines mov_info_t)

int read_movement(mov_info_t *pmv, const char *inputfile);
void free_movement(mov_info_t *pmv);
void print_movement(const mov_info_t *pmv, FILE *f, int base, int offodd, int offeven);
