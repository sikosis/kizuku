# Kizuku

kizuku (japanese: "to notice/to build") is a small native C++ terminal helper
for Haiku. It creates executable interactive build files and optional Git helper
scripts, using `hum` throughout when it is available.

## Requirement

- `hum` for enhanced Kizuku prompts and generated scripts
- `git` for Git update and commit/push operations

see: https://github.com/sikosis/hum/

## Build on Haiku

```sh
make
./kizuku
```

The wizard asks for a project name, working directory, output filename and the
update-code, clean, build, test, and run commands to include. When `hum` is in
`PATH`, Kizuku uses its styling, input, confirmation, and directory-picker
commands. Without `hum`, Kizuku falls back to ordinary terminal prompts. The
default project directory is derived from the project name under `/boot/home/`.
Each generated project receives a numeric title colour selected from the full
1–255 terminal palette. Step headings use one of seven grouped themes selected
during generation:

- **Kizuku** — the original amber, orange, and plum palette
- **Coast** — cool cyan, blue, and violet tones
- **Sakura** — pink, magenta, and purple tones
- **Forest** — leaf, moss, and deep evergreen tones
- **Sunset** — gold, orange, red, and violet tones
- **Lavender** — pale lilac through deep purple
- **Slate** — restrained grey-green and blue-grey tones

When `hum` is available the theme is selected with `hum choose`; the terminal
fallback accepts the theme name. The palettes are kept together as theme data so
more choices can be added without changing the step generator.

## Edit an existing script

Choose **Edit an existing script** from Kizuku's opening menu. Kizuku can insert
configured `hum confirm`, `hum choose`, `hum input`, `hum style`, `hum write`,
or `hum spin` blocks into an existing script. The insertion wizard supports:

- immediately after the shebang;
- before or after a selected line; or
- at the end of the script.

Kizuku previews the block and asks for confirmation before editing. It writes
through a temporary file, atomically replaces the original, and preserves the
original file permissions.

## Restyle an existing script

Choose **Restyle an existing script**, select a shell script and then choose any
built-in theme. Kizuku updates hexadecimal colours attached to Hum
`--foreground` options while leaving numeric colours, unrelated hexadecimal
text, and the rest of the script unchanged. Known generated steps keep their
semantic theme roles; other repeated colours are mapped consistently through
the selected palette. Kizuku reports the replacement count and asks before
atomically replacing the original file.

Kizuku can also generate a standalone POSIX shell script that stages all Git
changes, gathers a commit title and optional details with `hum`, commits them,
and optionally pushes the current branch. The Git script is located relative to
itself, so place it in the repository it should manage.

Install it for the current Haiku installation with:

```sh
make install
```

Use `DESTDIR` when creating a package staging tree.

## Test

```sh
make test
```

The test drives the wizard, checks that its generated build file is executable,
and validates its Bash syntax.

## Author

- Designed by Sikosis
- Creation Date: 3rd October, 2026
