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
1–255 terminal palette. Step headings use one of twenty-two grouped themes selected
during generation:

- **Kizuku** — the original amber, orange, and plum palette
- **Coast** — cool cyan, blue, and violet tones
- **Sakura** — pink, magenta, and purple tones
- **Forest** — leaf, moss, and deep evergreen tones
- **Sunset** — gold, orange, red, and violet tones
- **Lavender** — pale lilac through deep purple
- **Slate** — restrained grey-green and blue-grey tones
- **Desert** — sand, terracotta, burnt orange, and brown
- **Neon** — vivid green, aqua, blue, violet, and pink
- **Rosewood** — soft rose through deep muted burgundy
- **Arctic** — ice blue through deep ocean blue
- **Citrus** — lemon, lime, teal, blue-grey, and grapefruit

Kizuku also includes ten popular four-colour palettes from
[Coolors](https://coolors.co/palettes/popular/4%20colors):

- **Meadow Sunflower Glow**
- **Candy Floss Dreams**
- **Fiery Ice Cream Delight**
- **Golden Autumn Twilight**
- **Midnight Sun Dance**
- **Seaside Serenity**
- **Cherry Blossom Sky**
- **Cotton Candy Skies**
- **Ocean Sunset Vibes**
- **Cherry Blossom Dream**

Four-colour themes cycle their first colour back onto the fifth build role;
five-colour themes continue to assign one distinct colour to every role.

When `hum` is available the theme is selected with `hum choose`; the terminal
fallback accepts the theme name. Selecting **Random Theme** chooses one of the
built-in palettes and displays its name before continuing. The palettes are kept
together as theme data so more choices can be added without changing the step
generator.

## Edit an existing script

Choose **Edit an existing script** from Kizuku's opening menu. Kizuku can insert
configured `hum confirm`, `hum choose`, `hum input`, `hum file`, `hum style`,
`hum write`, or `hum spin` blocks into an existing script. The ready-made
**Git Pull Basic** template adds a styled Update Code heading, a configurable
default response, and a guarded `git pull` command. **Git Last Commit Show** adds
`git show --name-status --oneline` at the selected source position. Generated
Hum file pickers resolve the script's own directory and open there, so they
continue to work when the project is moved. The **All or Nothing** template
collects the commands from existing styled `==> Step` blocks and copies them
into one confirmation block, allowing every command to run together or none to
run. In directory pickers, the right-arrow key opens the highlighted folder and
Enter selects it.
Hum input defaults are displayed as placeholders, so they do not need to be
deleted before typing. Hum confirm blocks ask whether **Yes** or **No** should
be the default response, with **No** selected by default. After the block is
configured, the insertion picker shows the current script as a navigable list.
Selecting a source line inserts the block immediately before that line; **End
of script** appends it, and **Cancel** leaves the file alone. Without Hum, the
same insertion points are shown as a numbered list.

Pass a script directly on the command line to skip the opening menu and file
picker:

```sh
kizuku ./build.sh
```

After each successful change, Kizuku asks whether to make another edit to the
same script. Choosing Yes reloads the changed file before showing the Edit menu
again.

Kizuku previews the block and asks for confirmation before editing. It writes
through a temporary file, atomically replaces the original, and preserves the
original file permissions.

Choose **Check Hum Confirm Status** to list each source line containing a
`hum confirm`, including its current default status. Select a line and choose
**Yes** or **No** to add `--default=yes` or `--default=no`, or replace the
existing default option. Kizuku previews the exact before-and-after lines before
changing the script.

Choose **Update Script Version** to list version text such as `v0.2` with its
source line and column. Select an occurrence, then increment or decrement it by
exactly `0.1`. Decimal arithmetic is performed without floating-point rounding,
the selected line is previewed before writing, and versions cannot be reduced
below zero.

## Restyle an existing script

Choose **Restyle an existing script**, select a shell script and then choose any
built-in theme. Kizuku updates hexadecimal colours attached to Hum
`--foreground` options while leaving numeric colours, unrelated hexadecimal
text, and the rest of the script unchanged. It also finds `==> Step N:`
headings and renumbers them sequentially from one in source order. This
renumbering still works when a script uses numeric colours and has no theme
colours to replace. Known generated steps keep their semantic theme roles;
other repeated colours are mapped consistently through the selected palette.
Kizuku reports both update counts and asks before atomically replacing the
original file.

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
