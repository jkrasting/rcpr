#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_GIF
#define STBI_ONLY_BMP
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#include "stb_image.h"
#include "image.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* nearest-neighbor scale grayscale image to fit max_dots width */
static uint8_t *scale_gray(const uint8_t *src, int sw, int sh,
			    int dw, int *dh_out)
{
	float ratio = (float)dw / (float)sw;
	int dh = (int)(sh * ratio);
	uint8_t *dst = malloc((size_t)(dw * dh));
	if (!dst) return NULL;
	for (int y = 0; y < dh; y++) {
		int sy = (int)(y / ratio);
		if (sy >= sh) sy = sh - 1;
		for (int x = 0; x < dw; x++) {
			int sx = (int)(x / ratio);
			if (sx >= sw) sx = sw - 1;
			dst[y * dw + x] = src[sy * sw + sx];
		}
	}
	*dh_out = dh;
	return dst;
}

/* convert grayscale to 1-bit packed, MSB first. black = 1 */
static uint8_t *threshold_pack(const uint8_t *gray, int w, int h)
{
	int bw = (w + 7) / 8;
	uint8_t *bits = calloc((size_t)(bw * h), 1);
	if (!bits) return NULL;
	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
			if (gray[y * w + x] < 128) /* black */
				bits[y * bw + x / 8] |= (uint8_t)(0x80 >> (x % 8));
		}
	}
	return bits;
}

int image_print(buf_t *b, const char *path, int max_dots)
{
	int w, h, ch;
	uint8_t *img = stbi_load(path, &w, &h, &ch, 1); /* force grayscale */
	if (!img) {
		fprintf(stderr, "rcpr: cannot load image: %s\n", path);
		return -1;
	}

	uint8_t *gray = img;
	int fw = w, fh = h;

	/* scale down if wider than max_dots */
	if (w > max_dots) {
		gray = scale_gray(img, w, h, max_dots, &fh);
		stbi_image_free(img);
		if (!gray) return -1;
		fw = max_dots;
	}

	uint8_t *bits = threshold_pack(gray, fw, fh);
	if (gray != img) free(gray);
	else stbi_image_free(img);

	if (!bits) return -1;

	esc_align(b, ALIGN_LEFT);
	esc_raster(b, bits, fw, fh);
	free(bits);
	return 0;
}
