#include <string.h>
#include <alloca.h>
#include "fblur.h"

/* some color packing/unpacking macros */
#ifdef FBLUR_BIG_ENDIAN
#define RSHIFT		24
#define GSHIFT		16
#define BSHIFT		8
#else	/* little endian */
#define RSHIFT		0
#define GSHIFT		8
#define BSHIFT		16
#endif

#define RED(p)			(((p) >> RSHIFT) & 0xff)
#define GREEN(p)		(((p) >> GSHIFT) & 0xff)
#define BLUE(p)			(((p) >> BSHIFT) & 0xff)
#define RGB(r, g, b)	\
	((((r) & 0xff) << RSHIFT) | \
	 (((g) & 0xff) << GSHIFT) | \
	 (((b) & 0xff) << BSHIFT))

#define MIN(a, b)	((a) < (b) ? (a) : (b))
#define MAX(a, b)	((a) > (b) ? (a) : (b))


void fast_blur(int dir, int amount, uint32_t *buf, int x, int y)
{
	int i, j, half;
	uint32_t *dptr, *sptr, *tmp_buf;

	int blur_len = dir == BLUR_HORIZ ? x : y;
	int blur_times = dir == BLUR_HORIZ ? y : x;

	if(amount <= 1) return;

	dptr = buf;
	half = amount / 2;

	tmp_buf = alloca(blur_len * sizeof(uint32_t));

	for(i=0; i<blur_times; i++) {
		int ar = 0, ag = 0, ab = 0;
		int divisor = 0;

		if(dir == BLUR_HORIZ) {
			sptr = tmp_buf;
			memcpy(sptr, dptr, x * sizeof(uint32_t));
		} else {
			dptr = buf + i;

			sptr = tmp_buf;
			for(j=0; j<y; j++) {
				*sptr++ = *dptr;
				dptr += x;
			}
			dptr = buf + i;
			sptr = tmp_buf;
		}


		for(j=0; j<half; j++) {
			uint32_t pixel = tmp_buf[j];
			ar += RED(pixel);
			ag += GREEN(pixel);
			ab += BLUE(pixel);
			divisor++;
		}

		for(j=0; j<blur_len; j++) {
			int r, g, b;

			if(j > half) {
				uint32_t out = *(sptr - half - 1);
				ar -= RED(out);
				ag -= GREEN(out);
				ab -= BLUE(out);
				divisor--;
			}

			if(j < blur_len - half) {
				uint32_t in = *(sptr + half);
				ar += RED(in);
				ag += GREEN(in);
				ab += BLUE(in);
				divisor++;
			}

			r = ar / divisor;
			g = ag / divisor;
			b = ab / divisor;

			r = MAX(MIN(r, 255), 0);
			g = MAX(MIN(g, 255), 0);
			b = MAX(MIN(b, 255), 0);

			*dptr = RGB(r, g, b);
			dptr += dir == BLUR_HORIZ ? 1 : x;
			sptr++;
		}

	}

	if(dir == BLUR_BOTH) {
		fast_blur(BLUR_HORIZ, amount, buf, x, y);
	}
}
