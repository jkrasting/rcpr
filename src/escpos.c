#include "escpos.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* --- buffer --- */

void buf_init(buf_t *b)
{
	b->data = NULL;
	b->len = 0;
	b->cap = 0;
}

void buf_free(buf_t *b)
{
	free(b->data);
	b->data = NULL;
	b->len = b->cap = 0;
}

void buf_append(buf_t *b, const void *data, size_t len)
{
	if (b->len + len > b->cap) {
		size_t nc = b->cap ? b->cap * 2 : 256;
		while (nc < b->len + len)
			nc *= 2;
		b->data = realloc(b->data, nc);
		b->cap = nc;
	}
	memcpy(b->data + b->len, data, len);
	b->len += len;
}

void buf_byte(buf_t *b, uint8_t c)
{
	buf_append(b, &c, 1);
}

/* --- ESC/POS commands --- */

void esc_init(buf_t *b)
{
	buf_byte(b, 0x1B); /* ESC @ */
	buf_byte(b, '@');
}

void esc_font(buf_t *b, int n)
{
	buf_byte(b, 0x1B); /* ESC M n */
	buf_byte(b, 'M');
	buf_byte(b, (uint8_t)(n & 1));
}

void esc_size(buf_t *b, int sz)
{
	if (sz < 1) sz = 1;
	if (sz > 8) sz = 8;
	int n = ((sz - 1) << 4) | (sz - 1);
	buf_byte(b, 0x1D); /* GS ! n */
	buf_byte(b, '!');
	buf_byte(b, (uint8_t)n);
}

void esc_bold(buf_t *b, int on)
{
	buf_byte(b, 0x1B); /* ESC E n */
	buf_byte(b, 'E');
	buf_byte(b, on ? 1 : 0);
}

void esc_underline(buf_t *b, int on)
{
	buf_byte(b, 0x1B); /* ESC - n */
	buf_byte(b, '-');
	buf_byte(b, on ? 1 : 0);
}

void esc_align(buf_t *b, int a)
{
	/* only left/center/right are native ESC/POS */
	int v = (a >= 0 && a <= 2) ? a : 0;
	buf_byte(b, 0x1B); /* ESC a n */
	buf_byte(b, 'a');
	buf_byte(b, (uint8_t)v);
}

void esc_feed(buf_t *b, int n)
{
	if (n <= 0) return;
	buf_byte(b, 0x1B); /* ESC d n */
	buf_byte(b, 'd');
	buf_byte(b, (uint8_t)(n > 255 ? 255 : n));
}

void esc_cut(buf_t *b)
{
	buf_byte(b, 0x1D); /* GS V 1 (partial cut) */
	buf_byte(b, 'V');
	buf_byte(b, 1);
}

/* --- word wrap + justify --- */

/* emit a single line, applying justify spacing if needed */
static void emit_line(buf_t *b, const char *line, int len, int width,
		       int align, int last)
{
	if (align == ALIGN_JUSTIFY && !last) {
		/* count words */
		int wc = 0, i = 0;
		while (i < len) {
			while (i < len && line[i] == ' ') i++;
			if (i < len) { wc++; while (i < len && line[i] != ' ') i++; }
		}
		if (wc > 1) {
			/* collect words */
			int gaps = wc - 1;
			int text_len = 0;
			const char *words[512];
			int wlens[512];
			int wi = 0;
			i = 0;
			while (i < len && wi < 512) {
				while (i < len && line[i] == ' ') i++;
				if (i < len) {
					words[wi] = &line[i];
					int start = i;
					while (i < len && line[i] != ' ') i++;
					wlens[wi] = i - start;
					text_len += wlens[wi];
					wi++;
				}
			}
			int total_spaces = width - text_len;
			if (total_spaces < gaps) total_spaces = gaps;
			int base = total_spaces / gaps;
			int extra = total_spaces % gaps;
			esc_align(b, ALIGN_LEFT);
			for (int w = 0; w < wi; w++) {
				buf_append(b, words[w], (size_t)wlens[w]);
				if (w < wi - 1) {
					int sp = base + (w < extra ? 1 : 0);
					for (int s = 0; s < sp; s++)
						buf_byte(b, ' ');
				}
			}
			buf_byte(b, '\n');
			return;
		}
	}

	/* non-justify: let printer handle alignment */
	if (align == ALIGN_JUSTIFY)
		esc_align(b, ALIGN_LEFT);
	else
		esc_align(b, align);
	buf_append(b, line, (size_t)len);
	buf_byte(b, '\n');
}

void esc_text(buf_t *b, const char *text, int width, int align)
{
	if (!text || !*text) return;
	if (width <= 0) width = 48;

	const char *p = text;
	while (*p) {
		/* skip leading spaces at line start */
		while (*p == ' ') p++;
		if (!*p) break;

		/* check for explicit newline */
		const char *nl = strchr(p, '\n');
		int avail = nl ? (int)(nl - p) : (int)strlen(p);

		if (avail <= width) {
			/* fits on one line */
			int is_last = (!nl || !*(nl + 1));
			emit_line(b, p, avail, width, align, is_last);
			p += avail;
			if (*p == '\n') p++;
			continue;
		}

		/* need to wrap: find last space within width */
		int brk = width;
		while (brk > 0 && p[brk] != ' ')
			brk--;

		if (brk == 0) {
			/* no space found, hard break */
			emit_line(b, p, width, width, align, 0);
			p += width;
		} else {
			emit_line(b, p, brk, width, align, 0);
			p += brk + 1; /* skip the space */
		}
	}
}

void esc_raster(buf_t *b, const uint8_t *bits, int w, int h)
{
	/* GS v 0 m xL xH yL yH d... */
	int bw = (w + 7) / 8; /* bytes per row */
	buf_byte(b, 0x1D);
	buf_byte(b, 'v');
	buf_byte(b, '0');
	buf_byte(b, 0);   /* m = normal */
	buf_byte(b, (uint8_t)(bw & 0xFF));
	buf_byte(b, (uint8_t)((bw >> 8) & 0xFF));
	buf_byte(b, (uint8_t)(h & 0xFF));
	buf_byte(b, (uint8_t)((h >> 8) & 0xFF));
	buf_append(b, bits, (size_t)(bw * h));
}
