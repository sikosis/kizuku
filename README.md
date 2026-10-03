# Kizuku

kizuku (japanese: "to notice/to build") is a small native C++ terminal application for Haiku. It creates executable,
interactive Bash build files powered by `hum`.

## Requirement

hum 

see: https://github.com/sikosis/hum/

## Build on Haiku

```sh
make
./kizuku
```

The wizard asks for a project name, working directory, output filename and the
clean, build, test, and run commands to include. The generated file uses `hum`
for its styled headings and confirmation prompts, so `hum` must be available in
`PATH` when a generated build file is run.

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

- Add an optional Git workflow block to generated build files.
- Random or suite of colours for the steps and title heading

## Author

- Designed by Sikosis
- Creation Date: 3rd October, 2026


