#ifndef SPEKTRO_FFT_PGM_H_
#define SPEKTRO_FFT_PGM_H_

#include <stdint.h>

int create_rdft_image(float alpha, uint32_t fft_nbits, uint32_t use_logscale, char *fname_in, int fd_out);

#endif
