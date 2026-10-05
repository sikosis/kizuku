#!/bin/bash

set -eu

test_dir="${TMPDIR:-/tmp}/kizuku-test-$$"
mkdir -p "$test_dir"
trap 'rm -rf "$test_dir"' EXIT

# Project, version, directory, skip clean, add build, build command,
# skip test, skip run, output, skip Git script.
printf 'Demo App\n2.0\n/boot/home/demo-app/\nn\ny\nmake -j4\nn\nn\n%s/build.sh\nn\n' "$test_dir" \
    | ./kizuku >/dev/null

test -x "$test_dir/build.sh"
bash -n "$test_dir/build.sh"
grep -Fq 'Demo App Builder v2.0' "$test_dir/build.sh"
grep -Fq "cd '/boot/home/demo-app/' || exit 1" "$test_dir/build.sh"
grep -Fq 'make -j4' "$test_dir/build.sh"

# Generate only the standalone Git helper.
printf 'Git Demo\n1.0\n/boot/home/git-demo/\nn\nn\nn\nn\ny\n%s/git.sh\n' \
    "$test_dir" | ./kizuku >/dev/null

test -x "$test_dir/git.sh"
sh -n "$test_dir/git.sh"
grep -Fq "Updating Git Demo's Git repository" "$test_dir/git.sh"
grep -Fq 'git -C "$script_directory" add -A' "$test_dir/git.sh"
grep -Fq 'gum confirm --default=false' "$test_dir/git.sh"
grep -Fq "rev-parse --abbrev-ref '@{upstream}'" "$test_dir/git.sh"

echo "generator test passed"
