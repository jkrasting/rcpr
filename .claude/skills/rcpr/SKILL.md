---
name: rcpr
description: Print text or images to the ESC/POS receipt printer
argument-hint: [text or options]
allowed-tools: Bash(rcpr *), Bash(echo * | rcpr *), Bash(lp *), Bash(lpstat *)
---

# rcpr - Receipt Printer Skill

Print text or images to the receipt printer using `rcpr`.

## Usage

```
rcpr [OPTIONS] [TEXT]
```

**Output:**
- `-d DEVICE` — Output device or `-` for stdout (default: CUPS printer)
- `-P PRINTER` — CUPS printer name (default: system default via `lpstat -d`)

**Text:**
- `-s SIZE` — Font size 1-8 (default: 1)
- `-S FONT` — Font select: 0=12x24 (default), 1=9x17
- `-a ALIGN` — left (default), center, right, justify
- `-b` — Bold
- `-u` — Underline
- `-w WIDTH` — Override chars-per-line (auto-detected from font/size)

**Image:**
- `-i FILE` — Print image (PNG, JPG, GIF, BMP)

**Control:**
- `-c` — Cut paper after printing
- `-n N` — Feed N lines after print (default: 4)
- `-r` — Reset printer before printing

**Input:**
- `-f FILE` — Read text from file (`-` for stdin)

## Printer Setup

Find your printer with `lpstat -p`. Either set it as the system default (`lpoptions -d YOUR_PRINTER`) or pass `-P YOUR_PRINTER` to each command.

## Examples

Print simple text with cut:
```bash
rcpr -c "Hello World"
```

Print bold centered text with cut:
```bash
rcpr -b -a center -c "Receipt Header"
```

Print justified text from stdin with cut:
```bash
echo "long text here" | rcpr -f - -a justify -c
```

Print an image:
```bash
rcpr -i logo.png -c
```

Multiple styles in one job (pipe through lp):
```bash
(rcpr -d - -s 3 -b "HEADER"; rcpr -d - "body text"; rcpr -d - -n 4 -c "") | lp -o raw
```

## Tips

- Always use `-c` (cut) unless the user says not to.
- When combining multiple styles in one print job, use `-d -` to output raw bytes and pipe through `lp -o raw`.
- Font 0 (default) has 48 chars per line at size 1. Font 1 has 64.
- Chars per line = base / size. At size 2, font 0 gives 24 chars per line.
- The user will tell you what to print. Interpret their request and pick the right options.

## When invoked as `/rcpr`

The user's `$ARGUMENTS` are a description of what they want printed. Translate their request into the appropriate rcpr command(s) and run them.
