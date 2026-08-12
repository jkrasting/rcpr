#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <sys/socket.h>
#include <netdb.h>
#include "escpos.h"
#include "image.h"

#ifndef VERSION /* normally supplied by the build */
#define VERSION "0.2.0"
#endif
#define MAX_DOTS 576
#define DEFAULT_HOST "10.70.1.20"
#define DEFAULT_PORT "9100"

static void usage(void)
{
	printf(
"Usage: rcpr [OPTIONS] [TEXT]\n"
"\n"
"Output:\n"
"  -H HOST[:PORT]  Network printer (default: " DEFAULT_HOST ":" DEFAULT_PORT ")\n"
"  -d DEVICE    Output device or \"-\" for stdout\n"
"  -P PRINTER   Print via CUPS queue instead of the network\n"
"\n"
"Text:\n"
"  -s SIZE      Font size 1-8 (default: 1)\n"
"  -S FONT      Font select: 0=12x24 (default), 1=9x17\n"
"  -a ALIGN     left (default), center, right, justify\n"
"  -b           Bold\n"
"  -u           Underline\n"
"  -w WIDTH     Override chars-per-line (auto from font/size)\n"
"  -W           Disable word wrapping\n"
"\n"
"Image:\n"
"  -i FILE      Print image (PNG, JPG, GIF, BMP)\n"
"\n"
"Control:\n"
"  -C           Do not cut the paper (cutting is the default)\n"
"  -B           Do not add a blank line before the cut\n"
"  -n N         Feed N lines after print (default: 4)\n"
"  -r           Reset printer before printing\n"
"\n"
"Input:\n"
"  -f FILE      Read text from file (\"-\" for stdin)\n"
"               Text is also read from stdin when piped\n"
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

/* split HOST or HOST:PORT (a bare IPv6 literal keeps the default port) */
static void parse_host(char *arg, const char **host, const char **port)
{
	char *c = strchr(arg, ':');
	if (c && !strchr(c + 1, ':')) {
		*c = '\0';
		*port = c + 1;
	}
	*host = arg;
}

/* send buffer to a network printer (raw ESC/POS over TCP) */
static int send_net(buf_t *b, const char *host, const char *port)
{
	struct addrinfo hints, *res, *ai;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	int e = getaddrinfo(host, port, &hints, &res);
	if (e) {
		fprintf(stderr, "rcpr: %s:%s: %s\n", host, port, gai_strerror(e));
		return 1;
	}

	int fd = -1;
	for (ai = res; ai; ai = ai->ai_next) {
		fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if (fd < 0) continue;
		if (connect(fd, ai->ai_addr, ai->ai_addrlen) == 0) break;
		close(fd);
		fd = -1;
	}
	freeaddrinfo(res);
	if (fd < 0) {
		fprintf(stderr, "rcpr: cannot connect to %s:%s: %s\n",
			host, port, strerror(errno));
		return 1;
	}

	/* socket writes can be short: loop until the whole job is out */
	size_t off = 0;
	while (off < b->len) {
		ssize_t n = write(fd, b->data + off, b->len - off);
		if (n <= 0) {
			perror("rcpr: write");
			close(fd);
			return 1;
		}
		off += (size_t)n;
	}
	close(fd);
	return 0;
}

/* flush buffer to output target */
static int flush_output(buf_t *b, const char *device, const char *printer,
			const char *host, const char *port)
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

	if (printer) {
		/* CUPS via lp */
		char cmd[512];
		snprintf(cmd, sizeof(cmd), "lp -d '%s' -o raw", printer);
		FILE *p = popen(cmd, "w");
		if (!p) { perror("lp"); return 1; }
		fwrite(b->data, 1, b->len, p);
		return pclose(p) ? 1 : 0;
	}

	/* network socket */
	return send_net(b, host, port);
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
	const char *host = DEFAULT_HOST, *port = DEFAULT_PORT;
	int size = 1, font = 0, align = ALIGN_LEFT;
	int bold = 0, underline = 0, cut = 1, reset = 0;
	int feed = 4, width = 0, nowrap = 0, blank = 1;
	int opt;

	while ((opt = getopt(argc, argv, "d:P:H:s:S:a:buw:Wi:cCBn:rf:hv")) != -1) {
		switch (opt) {
		case 'd': device = optarg; break;
		case 'P': printer = optarg; break;
		case 'H': parse_host(optarg, &host, &port); break;
		case 's': size = atoi(optarg); break;
		case 'S': font = atoi(optarg); break;
		case 'a': align = parse_align(optarg); break;
		case 'b': bold = 1; break;
		case 'u': underline = 1; break;
		case 'w': width = atoi(optarg); break;
		case 'W': nowrap = 1; break;
		case 'i': image = optarg; break;
		case 'c': break; /* accepted: cutting is the default */
		case 'C': cut = 0; break;
		case 'B': blank = 0; break;
		case 'n': feed = atoi(optarg); break;
		case 'r': reset = 1; break;
		case 'f': textfile = optarg; break;
		case 'v': printf("rcpr %s\n", VERSION); return 0;
		case 'h': usage(); return 0;
		default:  usage(); return 1;
		}
	}

	/* auto-detect chars per line (-W turns wrapping off entirely) */
	if (nowrap) {
		width = 0;
	} else if (!width) {
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

	/* nothing given: read stdin when it is a pipe or a redirect */
	if (!text && !image && !isatty(STDIN_FILENO)) {
		text = read_file("-");
		/* empty input is not a print job: do not waste paper on it */
		if (text && !*text) { free(text); text = NULL; }
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

	/* blank line to close the receipt off, then feed + cut */
	if (cut && blank) buf_byte(&b, '\n');
	esc_feed(&b, feed);
	if (cut) esc_cut(&b);

	/* reset formatting */
	if (bold) esc_bold(&b, 0);
	if (underline) esc_underline(&b, 0);

	int ret = flush_output(&b, device, printer, host, port);
	buf_free(&b);
	return ret;
}
