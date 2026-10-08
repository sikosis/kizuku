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
printf '\nDemo App\n2.0\n\n\ny\ny\n\ny\nmake -j4\ny\n\ny\n\n%s/build.sh\nn\n' "$test_dir" \
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

# Apply a new theme to an existing generated script. The numeric title colour
# remains untouched while semantic step colours move to the selected palette.
cp "$test_dir/build.sh" "$test_dir/restyled.sh"
printf 'Restyle an existing script\n%s/restyled.sh\nForest\ny\n' "$test_dir" \
    | ./kizuku >/dev/null
test -x "$test_dir/restyled.sh"
bash -n "$test_dir/restyled.sh"
check_title_colour "$test_dir/restyled.sh"
grep -Fq -- '--foreground "#a7c957"' "$test_dir/restyled.sh"
grep -Fq -- '--foreground "#6a994e"' "$test_dir/restyled.sh"
grep -Fq -- '--foreground "#386641"' "$test_dir/restyled.sh"
grep -Fq -- '--foreground "#588157"' "$test_dir/restyled.sh"
grep -Fq -- '--foreground "#344e41"' "$test_dir/restyled.sh"
if grep -Fq -- '--foreground "#f2cc60"' "$test_dir/restyled.sh"; then
    echo "old theme colour remains after restyling" >&2
    exit 1
fi

# Repeated custom colours map consistently, while unrelated hex text is left alone.
printf '#!/bin/sh\nhum style --foreground "#abcdef" "One"\nhum style \\\n    --foreground '\''#ABCDEF'\'' "Two"\necho "#123456"\n' \
    > "$test_dir/custom-colours.sh"
printf 'Restyle an existing script\n%s/custom-colours.sh\nSunset\ny\n' "$test_dir" \
    | ./kizuku >/dev/null
sh -n "$test_dir/custom-colours.sh"
test "$(grep -Fc -- '--foreground "#ffb703"' "$test_dir/custom-colours.sh")" -eq 2
grep -Fq 'echo "#123456"' "$test_dir/custom-colours.sh"

# Verify both additional palettes across all five step roles.
printf '\nCoast App\n1.0\n\nCoast\ny\ny\n\ny\n\ny\n\ny\n\n%s/coast.sh\nn\n' "$test_dir" \
    | ./kizuku >/dev/null
grep -Fq -- '--foreground "#73d2de"' "$test_dir/coast.sh"
grep -Fq -- '--foreground "#52b2cf"' "$test_dir/coast.sh"
grep -Fq -- '--foreground "#2e86ab"' "$test_dir/coast.sh"
grep -Fq -- '--foreground "#33658a"' "$test_dir/coast.sh"
grep -Fq -- '--foreground "#7b6dba"' "$test_dir/coast.sh"
bash -n "$test_dir/coast.sh"

printf '\nSakura App\n1.0\n\nSakura\ny\ny\n\ny\n\ny\n\ny\n\n%s/sakura.sh\nn\n' "$test_dir" \
    | ./kizuku >/dev/null
grep -Fq -- '--foreground "#ffb3c6"' "$test_dir/sakura.sh"
grep -Fq -- '--foreground "#ff8fab"' "$test_dir/sakura.sh"
grep -Fq -- '--foreground "#fb6f92"' "$test_dir/sakura.sh"
grep -Fq -- '--foreground "#c77dff"' "$test_dir/sakura.sh"
grep -Fq -- '--foreground "#7b2cbf"' "$test_dir/sakura.sh"
bash -n "$test_dir/sakura.sh"

# Generate only the standalone Git helper.
printf '\nGit Demo\n1.0\n/boot/home/git-demo/\n\nn\nn\nn\nn\nn\ny\n%s/git.sh\n' \
    "$test_dir" | ./kizuku >/dev/null

test -x "$test_dir/git.sh"
sh -n "$test_dir/git.sh"
grep -Fq "Updating Git Demo's Git repository" "$test_dir/git.sh"
grep -Fq 'git -C "$script_directory" add -A' "$test_dir/git.sh"
grep -Fq 'hum confirm --default no' "$test_dir/git.sh"
grep -Fq "rev-parse --abbrev-ref '@{upstream}'" "$test_dir/git.sh"
check_numeric_colours "$test_dir/git.sh"

# Edit an existing executable script and append a configured Hum Confirm block.
printf '#!/bin/sh\n\necho "start"\n' > "$test_dir/edit-me.sh"
chmod 754 "$test_dir/edit-me.sh"
printf 'Edit an existing script\n%s/edit-me.sh\nHum Confirm\nDeploy?\necho "deploying"\nEnd of script\ny\n' \
    "$test_dir" | ./kizuku >/dev/null
test -x "$test_dir/edit-me.sh"
sh -n "$test_dir/edit-me.sh"
grep -Fq 'if hum confirm "Deploy?"; then' "$test_dir/edit-me.sh"
grep -Fq '    echo "deploying"' "$test_dir/edit-me.sh"
grep -Fq 'fi' "$test_dir/edit-me.sh"

# Insert a Hum Choose block after the shebang using shell-safe item quoting.
printf '#!/bin/sh\n\necho "existing"\n' > "$test_dir/edit-choose.sh"
chmod 700 "$test_dir/edit-choose.sh"
printf 'Edit an existing script\n%s/edit-choose.sh\nHum Choose\nselection\nFirst item, Second item\nAfter shebang\ny\n' \
    "$test_dir" | ./kizuku >/dev/null
test -x "$test_dir/edit-choose.sh"
sh -n "$test_dir/edit-choose.sh"
grep -Fq "selection=\$(hum choose 'First item' 'Second item')" "$test_dir/edit-choose.sh"

# Exercise numbered placement by inserting input immediately before line 3.
printf '#!/bin/sh\n\necho "existing"\n' > "$test_dir/edit-before.sh"
printf 'Edit an existing script\n%s/edit-before.sh\nHum Input\nanswer\nYour answer\nBefore a line\n3\ny\n' \
    "$test_dir" | ./kizuku >/dev/null
sh -n "$test_dir/edit-before.sh"
test "$(sed -n '3p' "$test_dir/edit-before.sh")" = 'answer=$(hum input --placeholder "Your answer")'

version=$(tr -d '\r\n' < VERSION)
test "$(./kizuku --version)" = "Kizuku v$version"
./kizuku --help | grep -Fq "Kizuku v$version"
./kizuku --help | grep -Fq 'Forest, Sunset, Lavender, and Slate'

echo "generator test passed"
