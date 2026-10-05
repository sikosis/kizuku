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

## TODO

- Random or suite of colours for the steps and title heading

## Author

- Designed by Sikosis
- Creation Date: 3rd October, 2026
