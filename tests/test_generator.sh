#!/bin/bash

set -eu

test_dir="${TMPDIR:-/tmp}/kizuku-test-$$"
mkdir -p "$test_dir"
trap 'rm -rf "$test_dir"' EXIT

check_numeric_colours() {
    awk '
        /--foreground/ {
            for (i = 1; i <= NF; ++i) {
                if ($i == "--foreground") {
                    value = $(i + 1)
                    gsub(/[\047\042]/, "", value)
                    if (value !~ /^[0-9]+$/ || value < 1 || value > 255) {
                        print "invalid foreground colour: " value > "/dev/stderr"
                        exit 1
                    }
                    found = 1
                }
            }
        }
        END { if (!found) exit 1 }
    ' "$1"
}

check_title_colour() {
    awk '
        /hum style --border rounded/ {
            for (i = 1; i <= NF; ++i) {
                if ($i == "--foreground") {
                    value = $(i + 1)
                    if (value !~ /^[0-9]+$/ || value < 1 || value > 255) {
                        print "invalid title colour: " value > "/dev/stderr"
                        exit 1
                    }
                    found = 1
                }
            }
        }
        END { if (!found) exit 1 }
    ' "$1"
}

# Project, version, accept the derived directory, add every themed step using
# the default Kizuku theme and default commands except for build, output, then
# skip the Git script.
printf 'Demo App\n2.0\n\n\ny\ny\n\ny\nmake -j4\ny\n\ny\n\n%s/build.sh\nn\n' "$test_dir" \
    | ./kizuku >/dev/null

test -x "$test_dir/build.sh"
bash -n "$test_dir/build.sh"
grep -Fq 'Demo App Builder v2.0' "$test_dir/build.sh"
grep -Fq "cd '/boot/home/demo-app/' || exit 1" "$test_dir/build.sh"
grep -Fq '==> Step 1: Update Code' "$test_dir/build.sh"
grep -Fq 'git pull' "$test_dir/build.sh"
grep -Fq '==> Step 2: Make Clean' "$test_dir/build.sh"
grep -Fq '==> Step 3: Make' "$test_dir/build.sh"
grep -Fq '==> Step 4: Make Test' "$test_dir/build.sh"
grep -Fq '==> Step 5: Run / Test' "$test_dir/build.sh"
grep -Fq 'make -j4' "$test_dir/build.sh"
grep -Fq -- '--foreground "#f2cc60"' "$test_dir/build.sh"
grep -Fq -- '--foreground "#e8b76a"' "$test_dir/build.sh"
grep -Fq -- '--foreground "#d98a4e"' "$test_dir/build.sh"
grep -Fq -- '--foreground "#d98a3e"' "$test_dir/build.sh"
grep -Fq -- '--foreground "#6f3a63"' "$test_dir/build.sh"
check_title_colour "$test_dir/build.sh"

# Verify both additional palettes across all five step roles.
printf 'Coast App\n1.0\n\nCoast\ny\ny\n\ny\n\ny\n\ny\n\n%s/coast.sh\nn\n' "$test_dir" \
    | ./kizuku >/dev/null
grep -Fq -- '--foreground "#73d2de"' "$test_dir/coast.sh"
grep -Fq -- '--foreground "#52b2cf"' "$test_dir/coast.sh"
grep -Fq -- '--foreground "#2e86ab"' "$test_dir/coast.sh"
grep -Fq -- '--foreground "#33658a"' "$test_dir/coast.sh"
grep -Fq -- '--foreground "#7b6dba"' "$test_dir/coast.sh"
bash -n "$test_dir/coast.sh"

printf 'Sakura App\n1.0\n\nSakura\ny\ny\n\ny\n\ny\n\ny\n\n%s/sakura.sh\nn\n' "$test_dir" \
    | ./kizuku >/dev/null
grep -Fq -- '--foreground "#ffb3c6"' "$test_dir/sakura.sh"
grep -Fq -- '--foreground "#ff8fab"' "$test_dir/sakura.sh"
grep -Fq -- '--foreground "#fb6f92"' "$test_dir/sakura.sh"
grep -Fq -- '--foreground "#c77dff"' "$test_dir/sakura.sh"
grep -Fq -- '--foreground "#7b2cbf"' "$test_dir/sakura.sh"
bash -n "$test_dir/sakura.sh"

# Generate only the standalone Git helper.
printf 'Git Demo\n1.0\n/boot/home/git-demo/\n\nn\nn\nn\nn\nn\ny\n%s/git.sh\n' \
    "$test_dir" | ./kizuku >/dev/null

test -x "$test_dir/git.sh"
sh -n "$test_dir/git.sh"
grep -Fq "Updating Git Demo's Git repository" "$test_dir/git.sh"
grep -Fq 'git -C "$script_directory" add -A' "$test_dir/git.sh"
grep -Fq 'hum confirm --default no' "$test_dir/git.sh"
grep -Fq "rev-parse --abbrev-ref '@{upstream}'" "$test_dir/git.sh"
check_numeric_colours "$test_dir/git.sh"

version=$(tr -d '\r\n' < VERSION)
test "$(./kizuku --version)" = "Kizuku v$version"
./kizuku --help | grep -Fq "Kizuku v$version"

echo "generator test passed"
