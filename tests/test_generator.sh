#!/bin/bash

set -eu

test_dir="${TMPDIR:-/tmp}/kizuku-test-$$"
mkdir -p "$test_dir"
trap 'rm -rf "$test_dir"' EXIT

# Project, version, directory, output, skip clean, add build, build command,
# skip test, skip run.
printf 'Demo App\n2.0\n/boot/home/demo-app/\n%s/build.sh\nn\ny\nmake -j4\nn\nn\n' "$test_dir" \
    | ./kizuku >/dev/null

test -x "$test_dir/build.sh"
bash -n "$test_dir/build.sh"
grep -Fq 'Demo App Builder v2.0' "$test_dir/build.sh"
grep -Fq "cd '/boot/home/demo-app/' || exit 1" "$test_dir/build.sh"
grep -Fq 'make -j4' "$test_dir/build.sh"

echo "generator test passed"
