# rcpr

ESC/POS receipt printer CLI — print text and images from the command line via CUPS.

## Features

- Prints straight to a network printer over TCP — no CUPS, no drivers, no spooler
- Print text with configurable font size, bold, underline, and alignment
- Print images (PNG, JPG, GIF, BMP) using ESC/POS raster graphics
- UTF-8 aware word wrapping that keeps words whole and preserves indentation
- Text justification support (left, center, right, justify)
- Paper cut control
- Pipe-friendly: `cat notes.txt | rcpr` just works
- Can also target a raw device or a CUPS queue

## Installation

### Prerequisites

- C compiler (gcc or clang)
- autotools (autoconf, automake)

No external libraries are required. CUPS is optional and only needed if you use `-P`, which shells
out to `lp`.

### Build

```bash
autoreconf -fi
./configure
make
sudo make install
```

This installs `rcpr` to `/usr/local/bin`.

## Printer Setup

`rcpr` talks to the printer directly over TCP on port 9100, the raw ESC/POS port that essentially
every network thermal printer exposes. All you need is its IP address — find it from your router's
lease table or by printing the printer's self-test page.

Point `rcpr` at it with `-H`:

```bash
rcpr -H 192.168.1.50 "hello"
rcpr -H 192.168.1.50:9100 "hello"   # explicit port
```

The default host is compiled in as `10.70.1.20:9100` (see `DEFAULT_HOST` in `src/rcpr.c`). Change it
there and rebuild if you want a bare `rcpr "text"` to reach your own printer.

Confirm the printer is listening before troubleshooting anything else:

```bash
nc -vz 192.168.1.50 9100
```

## Usage

```
rcpr [OPTIONS] [TEXT]

Output:
  -H HOST[:PORT]  Network printer (default: 10.70.1.20:9100)
  -d DEVICE    Output device or "-" for stdout
  -P PRINTER   Print via CUPS queue instead of the network

Text:
  -s SIZE      Font size 1-8 (default: 1)
  -S FONT      Font select: 0=12x24 (default), 1=9x17
  -a ALIGN     left (default), center, right, justify
  -b           Bold
  -u           Underline
  -w WIDTH     Override chars-per-line (auto from font/size)
  -W           Disable word wrapping

Image:
  -i FILE      Print image (PNG, JPG, GIF, BMP)

Control:
  -C           Do not cut the paper (cutting is the default)
  -B           Do not add a blank line before the cut
  -n N         Feed N lines after print (default: 4)
  -r           Reset printer before printing

Input:
  -f FILE      Read text from file ("-" for stdin)
               Text is also read from stdin when piped
```

## Examples

Print some text — it wraps, feeds, and cuts on its own:

```bash
rcpr "Hello World"
```

Pipe a file in:

```bash
cat notes.txt | rcpr
rcpr < notes.txt
rcpr -f notes.txt
```

Bold centered header:

```bash
rcpr -b -a center "Receipt Header"
```

Justified prose:

```bash
fortune | rcpr -a justify
```

Print an image:

```bash
rcpr -i logo.png
```

Keep the paper attached so several prints come out on one strip:

```bash
rcpr -C "first part"
rcpr "second part, cut here"
```

Send to a different printer, a raw device, or a CUPS queue:

```bash
rcpr -H 192.168.1.50 "other printer"
rcpr -d /dev/usb/lp0 "usb printer"
rcpr -P ticket "via CUPS"
```

Inspect the ESC/POS bytes without printing:

```bash
rcpr -d - "test" | od -An -tx1
```

## Claude Code Integration

This repo includes a bundled Claude Code skill. Use `/rcpr` followed by a description of what you want printed, and Claude will translate it into the appropriate command(s).

## License

MIT — see [LICENSE](LICENSE).

`stb_image.h` is public domain (Unlicense / MIT), included for image loading.
