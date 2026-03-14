#ifndef IMAGE_H
#define IMAGE_H

#include "escpos.h"

/* load image, convert to 1-bit raster, emit GS v 0 */
int image_print(buf_t *b, const char *path, int max_dots);

#endif
