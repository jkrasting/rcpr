# rcpr

ESC/POS receipt printer CLI — print text and images from the command line via CUPS.

## Features

- Print text with configurable font size, bold, underline, and alignment
- Print images (PNG, JPG, GIF, BMP) using ESC/POS raster graphics
- Word wrapping with proper character-per-line calculation
- Text justification support (left, center, right, justify)
- Paper cut control
- Direct device output or CUPS printer queue
- Pipe-friendly: combine multiple style runs into a single print job

## Installation

### Prerequisites

- C compiler (gcc or clang)
- autotools (autoconf, automake)
- CUPS development headers (`libcups2-dev` on Debian/Ubuntu)

### Build

```bash
autoreconf -fi
./configure
make
sudo make install
```

This installs `rcpr` to `/usr/local/bin`.

## Printer Setup

Find your printer name:

```bash
lpstat -p
```

Set it as your system default:

```bash
lpoptions -d YOUR_PRINTER
```

Or specify it per-command with `-P YOUR_PRINTER`.

## Usage

```
rcpr [OPTIONS] [TEXT]

Output:
  -d DEVICE    Output device or "-" for stdout (default: CUPS printer)
  -P PRINTER   CUPS printer name (default: system default)

Text:
  -s SIZE      Font size 1-8 (default: 1)
  -S FONT      Font select: 0=12x24 (default), 1=9x17
  -a ALIGN     left (default), center, right, justify
  -b           Bold
  -u           Underline
  -w WIDTH     Override chars-per-line (auto from font/size)

Image:
  -i FILE      Print image (PNG, JPG, GIF, BMP)

Control:
  -c           Cut paper after printing
  -n N         Feed N lines after print (default: 4)
  -r           Reset printer before printing

Input:
  -f FILE      Read text from file ("-" for stdin)
```

## Examples

Print text with a paper cut:

```bash
rcpr -c "Hello World"
```

Bold centered header:

```bash
rcpr -b -a center -c "Receipt Header"
```

Justified text from stdin:

```bash
echo "long text here" | rcpr -f - -a justify -c
```

Print an image:

```bash
rcpr -i logo.png -c
```

Multiple styles in one print job:

```bash
(rcpr -d - -s 3 -b "HEADER"; rcpr -d - "body text"; rcpr -d - -n 4 -c "") | lp -o raw
```

## Claude Code Integration

This repo includes a bundled Claude Code skill. Use `/rcpr` followed by a description of what you want printed, and Claude will translate it into the appropriate command(s).

## License

MIT — see [LICENSE](LICENSE).

`stb_image.h` is public domain (Unlicense / MIT), included for image loading.
