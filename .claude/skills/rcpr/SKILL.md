---
name: rcpr
description: Print text or images to the ESC/POS receipt printer
argument-hint: [text or options]
allowed-tools: Bash(rcpr *), Bash(echo * | rcpr *), Bash(cat * | rcpr *)
---

# rcpr - Receipt Printer Skill

Print text or images to the receipt printer using `rcpr`. It sends ESC/POS directly to the printer
over the network, wraps text, and cuts the paper on its own.

## Usage

```
rcpr [OPTIONS] [TEXT]
```

**Output:**
- `-H HOST[:PORT]` — Network printer (default: compiled-in host, port 9100)
- `-d DEVICE` — Output device or `-` for stdout
- `-P PRINTER` — Print via a CUPS queue instead of the network

**Text:**
- `-s SIZE` — Font size 1-8 (default: 1)
- `-S FONT` — Font select: 0=12x24 (default), 1=9x17
- `-a ALIGN` — left (default), center, right, justify
- `-b` — Bold
- `-u` — Underline
- `-w WIDTH` — Override chars-per-line (auto-detected from font/size)
- `-W` — Disable word wrapping

**Image:**
- `-i FILE` — Print image (PNG, JPG, GIF, BMP)

**Control:**
- `-C` — Do NOT cut the paper (cutting is the default)
- `-n N` — Feed N lines after print (default: 4)
- `-r` — Reset printer before printing

**Input:**
- `-f FILE` — Read text from file (`-` for stdin). Text is also read from stdin when piped.

## Printer Setup

The printer's address is compiled into the binary, so no setup is normally needed. Override it per
command with `-H 192.168.1.50` or `-H 192.168.1.50:9100`. To change the default permanently, edit
`DEFAULT_HOST` in `src/rcpr.c` and rebuild.

## Examples

Print simple text (wraps and cuts automatically):
```bash
rcpr "Hello World"
```

Print bold centered text:
```bash
rcpr -b -a center "Receipt Header"
```

Print a file or piped text:
```bash
cat notes.txt | rcpr
rcpr -f notes.txt -a justify
```

Print an image:
```bash
rcpr -i logo.png
```

Several prints on one uncut strip:
```bash
rcpr -C -s 3 -b "HEADER"
rcpr -C "body text"
rcpr "last part, cuts here"
```

## Tips

- Cutting is the default. Use `-C` only when the user wants the paper left attached, or when
  building one receipt from several commands — put `-C` on every command except the last.
- `-c` is still accepted and does nothing; it is left over from older versions.
- Word wrapping is on by default, is UTF-8 aware, and preserves leading indentation. Use `-W` only
  when the user explicitly wants long lines left unwrapped.
- Font 0 (default) has 48 chars per line at size 1. Font 1 has 64.
- Chars per line = base / size. At size 2, font 0 gives 24 chars per line.
- To check output without wasting paper, use `-d -` to dump the raw bytes to stdout.
- The user will tell you what to print. Interpret their request and pick the right options.

## When invoked as `/rcpr`

The user's `$ARGUMENTS` are a description of what they want printed. Translate their request into
the appropriate rcpr command(s) and run them.
