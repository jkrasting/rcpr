#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include "escpos.h"
#include "image.h"

#define VERSION "0.1.0"
#define MAX_DOTS 576

static void usage(void)
{
	printf(
"Usage: rcpr [OPTIONS] [TEXT]\n"
"\n"
"Output:\n"
"  -d DEVICE    Output device or \"-\" for stdout (default: CUPS printer)\n"
"  -P PRINTER   CUPS printer name (default: system default)\n"
"\n"
"Text:\n"
"  -s SIZE      Font size 1-8 (default: 1)\n"
"  -S FONT      Font select: 0=12x24 (default), 1=9x17\n"
"  -a ALIGN     left (default), center, right, justify\n"
"  -b           Bold\n"
"  -u           Underline\n"
"  -w WIDTH     Override chars-per-line (auto from font/size)\n"
"\n"
"Image:\n"
"  -i FILE      Print image (PNG, JPG, GIF, BMP)\n"
"\n"
"Control:\n"
"  -c           Cut paper after printing\n"
"  -n N         Feed N lines after print (default: 4)\n"
"  -r           Reset printer before printing\n"
"\n"
"Input:\n"
"  -f FILE      Read text from file (\"-\" for stdin)\n"
"\n"
"Info:\n"
"  -h           Help\n"
"  -v           Version\n"
	);
}

/* read entire file or stdin into malloc'd string */
static char *read_file(const char *path)
{
	FILE *f = strcmp(path, "-") == 0 ? stdin : fopen(path, "r");
	if (!f) { perror(path); return NULL; }

	size_t cap = 4096, len = 0;
	char *buf = malloc(cap);
	size_t n;
	while ((n = fread(buf + len, 1, cap - len, f)) > 0) {
		len += n;
		if (len == cap) { cap *= 2; buf = realloc(buf, cap); }
	}
	if (f != stdin) fclose(f);
	buf[len] = '\0';
	return buf;
}

/* get default CUPS printer name */
static char *default_printer(void)
{
	FILE *p = popen("lpstat -d 2>/dev/null", "r");
	if (!p) return NULL;
	char line[256];
	char *name = NULL;
	if (fgets(line, sizeof(line), p)) {
		/* "system default destination: PrinterName" */
		char *colon = strchr(line, ':');
		if (colon) {
			colon++;
			while (*colon == ' ') colon++;
			char *end = colon + strlen(colon) - 1;
			while (end > colon && (*end == '\n' || *end == ' ')) *end-- = '\0';
			name = strdup(colon);
		}
	}
	pclose(p);
	return name;
}

/* flush buffer to output target */
static int flush_output(buf_t *b, const char *device, const char *printer)
{
	if (device && strcmp(device, "-") == 0) {
		/* stdout */
		fwrite(b->data, 1, b->len, stdout);
		return 0;
	}

	if (device) {
		/* direct device file */
		FILE *f = fopen(device, "wb");
		if (!f) { perror(device); return 1; }
		fwrite(b->data, 1, b->len, f);
		fclose(f);
		return 0;
	}

	/* CUPS via lp */
	char *pr = printer ? strdup(printer) : default_printer();
	char cmd[512];
	if (pr)
		snprintf(cmd, sizeof(cmd), "lp -d '%s' -o raw", pr);
	else
		snprintf(cmd, sizeof(cmd), "lp -o raw");
	free(pr);

	FILE *p = popen(cmd, "w");
	if (!p) { perror("lp"); return 1; }
	fwrite(b->data, 1, b->len, p);
	return pclose(p) ? 1 : 0;
}

static int parse_align(const char *s)
{
	if (strcmp(s, "left") == 0) return ALIGN_LEFT;
	if (strcmp(s, "center") == 0) return ALIGN_CENTER;
	if (strcmp(s, "right") == 0) return ALIGN_RIGHT;
	if (strcmp(s, "justify") == 0) return ALIGN_JUSTIFY;
	fprintf(stderr, "rcpr: unknown alignment: %s\n", s);
	return ALIGN_LEFT;
}

int main(int argc, char **argv)
{
	char *device = NULL, *printer = NULL, *image = NULL, *textfile = NULL;
	int size = 1, font = 0, align = ALIGN_LEFT;
	int bold = 0, underline = 0, cut = 0, reset = 0;
	int feed = 4, width = 0;
	int opt;

	while ((opt = getopt(argc, argv, "d:P:s:S:a:buw:i:cn:rf:hv")) != -1) {
		switch (opt) {
		case 'd': device = optarg; break;
		case 'P': printer = optarg; break;
		case 's': size = atoi(optarg); break;
		case 'S': font = atoi(optarg); break;
		case 'a': align = parse_align(optarg); break;
		case 'b': bold = 1; break;
		case 'u': underline = 1; break;
		case 'w': width = atoi(optarg); break;
		case 'i': image = optarg; break;
		case 'c': cut = 1; break;
		case 'n': feed = atoi(optarg); break;
		case 'r': reset = 1; break;
		case 'f': textfile = optarg; break;
		case 'v': printf("rcpr %s\n", VERSION); return 0;
		case 'h': usage(); return 0;
		default:  usage(); return 1;
		}
	}

	/* auto-detect chars per line */
	if (!width) {
		int base_cpl = (font == 1) ? 64 : 48;
		width = base_cpl / (size > 0 ? size : 1);
		if (width < 1) width = 1;
	}

	/* gather text from remaining args */
	char *text = NULL;
	if (textfile) {
		text = read_file(textfile);
	} else if (optind < argc) {
		/* join remaining args with spaces */
		size_t len = 0;
		for (int i = optind; i < argc; i++)
			len += strlen(argv[i]) + 1;
		text = malloc(len);
		text[0] = '\0';
		for (int i = optind; i < argc; i++) {
			if (i > optind) strcat(text, " ");
			strcat(text, argv[i]);
		}
	}

	/* need something to print */
	if (!text && !image) {
		fprintf(stderr, "rcpr: no text or image specified\n");
		usage();
		return 1;
	}

	buf_t b;
	buf_init(&b);

	if (reset) esc_init(&b);

	/* set formatting */
	esc_font(&b, font);
	esc_size(&b, size);
	if (bold) esc_bold(&b, 1);
	if (underline) esc_underline(&b, 1);

	/* image */
	if (image) {
		if (image_print(&b, image, MAX_DOTS) != 0) {
			buf_free(&b);
			free(text);
			return 1;
		}
	}

	/* text */
	if (text) {
		esc_text(&b, text, width, align);
		free(text);
	}

	/* trailing feed + cut */
	esc_feed(&b, feed);
	if (cut) esc_cut(&b);

	/* reset formatting */
	if (bold) esc_bold(&b, 0);
	if (underline) esc_underline(&b, 0);

	int ret = flush_output(&b, device, printer);
	buf_free(&b);
	return ret;
}
