#ifndef FBLUR_H_
#define FBLUR_H_

#include <stdint.h>

enum {
	BLUR_BOTH,	/* blur in X and Y */
	BLUR_HORIZ,	/* blur in X */
	BLUR_VERT	/* blur in Y */
};

void fast_blur(int dir, int amount, uint32_t *buf, int x, int y);

#endif	/* FBLUR_H_ */
