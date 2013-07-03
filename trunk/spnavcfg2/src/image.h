#ifndef IMAGE_H_
#define IMAGE_H_

struct image {
	int width, height;
	unsigned char *pixels;
};

#ifdef __cplusplus
extern "C" {
#endif

struct image *load_image(const char *fname);
void free_image(struct image *img);

#ifdef __cplusplus
}
#endif

#endif	/* IMAGE_H_ */
