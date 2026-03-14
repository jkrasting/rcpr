#ifndef ESCPOS_H
#define ESCPOS_H

#include <stddef.h>
#include <stdint.h>

/* dynamic byte buffer */
typedef struct {
	uint8_t *data;
	size_t len;
	size_t cap;
} buf_t;

void buf_init(buf_t *b);
void buf_free(buf_t *b);
void buf_append(buf_t *b, const void *data, size_t len);
void buf_byte(buf_t *b, uint8_t c);

/* alignment */
enum { ALIGN_LEFT = 0, ALIGN_CENTER = 1, ALIGN_RIGHT = 2, ALIGN_JUSTIFY = 3 };

/* ESC/POS commands */
void esc_init(buf_t *b);
void esc_font(buf_t *b, int n);
void esc_size(buf_t *b, int sz);
void esc_bold(buf_t *b, int on);
void esc_underline(buf_t *b, int on);
void esc_align(buf_t *b, int a);
void esc_feed(buf_t *b, int n);
void esc_cut(buf_t *b);
void esc_text(buf_t *b, const char *text, int width, int align);

/* raster image: raw 1-bit packed data */
void esc_raster(buf_t *b, const uint8_t *bits, int w, int h);

#endif
