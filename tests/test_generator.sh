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

# Restyle repairs duplicated step numbers even when numeric colours are left unchanged.
printf '%s\n' \
    '#!/bin/sh' \
    'hum style --foreground 41 --bold "==> Step 1: Update Code"' \
    'hum style --foreground 42 --bold "==> Step 1: Make Clean"' \
    'hum style --foreground 43 --bold "==> Step 2: Make"' \
    'hum style --foreground 44 --bold "==> Step 3: Package / Install"' \
    'hum style --foreground 45 --bold "==> Step 4: Run / Test"' \
    > "$test_dir/renumber-steps.sh"
printf 'Restyle an existing script\n%s/renumber-steps.sh\nKizuku\ny\n' "$test_dir" \
    | ./kizuku >/dev/null
sh -n "$test_dir/renumber-steps.sh"
grep -Fq -- '--foreground 41 --bold "==> Step 1: Update Code"' "$test_dir/renumber-steps.sh"
grep -Fq -- '--foreground 42 --bold "==> Step 2: Make Clean"' "$test_dir/renumber-steps.sh"
grep -Fq -- '--foreground 43 --bold "==> Step 3: Make"' "$test_dir/renumber-steps.sh"
grep -Fq -- '--foreground 44 --bold "==> Step 4: Package / Install"' "$test_dir/renumber-steps.sh"
grep -Fq -- '--foreground 45 --bold "==> Step 5: Run / Test"' "$test_dir/renumber-steps.sh"

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

check_added_theme() {
    theme_name=$1
    file_name=$2
    shift 2
    printf '\n%s App\n1.0\n\n%s\ny\ny\n\ny\n\ny\n\ny\n\n%s/%s.sh\nn\n' \
        "$theme_name" "$theme_name" "$test_dir" "$file_name" | ./kizuku >/dev/null
    for colour in "$@"; do
        grep -Fq -- "--foreground \"$colour\"" "$test_dir/$file_name.sh"
    done
    bash -n "$test_dir/$file_name.sh"
}

check_added_theme Desert desert '#e9c46a' '#f4a261' '#e76f51' '#bc6c25' '#6f4e37'
check_added_theme Neon neon '#39ff14' '#00f5d4' '#00bbf9' '#9b5de5' '#f15bb5'
check_added_theme Rosewood rosewood '#ffcad4' '#f4acb7' '#9d8189' '#7d4e57' '#5c374c'
check_added_theme Arctic arctic '#caf0f8' '#90e0ef' '#48cae4' '#0077b6' '#023e8a'
check_added_theme Citrus citrus '#f9c74f' '#90be6d' '#43aa8b' '#577590' '#f94144'

check_four_colour_theme() {
    theme_name=$1
    file_name=$2
    first_colour=$3
    second_colour=$4
    third_colour=$5
    fourth_colour=$6
    printf '\n%s App\n1.0\n\n%s\ny\ny\n\ny\n\ny\n\ny\n\n%s/%s.sh\nn\n' \
        "$theme_name" "$theme_name" "$test_dir" "$file_name" | ./kizuku >/dev/null
    test "$(grep -Fc -- "--foreground \"$first_colour\"" "$test_dir/$file_name.sh")" -eq 2
    grep -Fq -- "--foreground \"$second_colour\"" "$test_dir/$file_name.sh"
    grep -Fq -- "--foreground \"$third_colour\"" "$test_dir/$file_name.sh"
    grep -Fq -- "--foreground \"$fourth_colour\"" "$test_dir/$file_name.sh"
    bash -n "$test_dir/$file_name.sh"
}

check_four_colour_theme 'Meadow Sunflower Glow' meadow '#d4e09b' '#f6f4d2' '#cbdfbd' '#f19c79'
check_four_colour_theme 'Candy Floss Dreams' candy '#ffa69e' '#faf3dd' '#b8f2e6' '#aed9e0'
check_four_colour_theme 'Fiery Ice Cream Delight' fiery '#003049' '#d62828' '#f77f00' '#fcbf49'
check_four_colour_theme 'Golden Autumn Twilight' golden '#219ebc' '#023047' '#ffb703' '#fb8500'
check_four_colour_theme 'Midnight Sun Dance' midnight '#001524' '#15616d' '#ffecd1' '#ff7d00'
check_four_colour_theme 'Seaside Serenity' seaside '#e7ecef' '#274c77' '#6096ba' '#a3cef1'
check_four_colour_theme 'Cherry Blossom Sky' cherry-sky '#2b2d42' '#8d99ae' '#edf2f4' '#ef233c'
check_four_colour_theme 'Cotton Candy Skies' cotton '#70d6ff' '#ff70a6' '#ff9770' '#ffd670'
check_four_colour_theme 'Ocean Sunset Vibes' ocean '#26547c' '#ef476f' '#ffd166' '#06d6a0'
check_four_colour_theme 'Cherry Blossom Dream' cherry-dream '#880d1e' '#dd2d4a' '#f26a8d' '#f49cbb'

# Random Theme selects and reports one complete built-in palette.
random_output=$(printf '\nRandom App\n1.0\n\nRandom Theme\ny\nn\ny\n\nn\nn\n%s/random.sh\nn\n' \
    "$test_dir" | ./kizuku)
printf '%s\n' "$random_output" | grep -Fq 'Random theme selected:'
test -x "$test_dir/random.sh"
bash -n "$test_dir/random.sh"
grep -Eq -- '--foreground "#[0-9a-f]{6}"' "$test_dir/random.sh"

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
printf 'Edit an existing script\n%s/edit-me.sh\nHum Confirm\nDeploy?\nNo\necho "deploying"\n\ny\nn\n' \
    "$test_dir" | ./kizuku >/dev/null
test -x "$test_dir/edit-me.sh"
sh -n "$test_dir/edit-me.sh"
grep -Fq 'if hum confirm "Deploy?" --default=no; then' "$test_dir/edit-me.sh"
grep -Fq '    echo "deploying"' "$test_dir/edit-me.sh"
grep -Fq 'fi' "$test_dir/edit-me.sh"

# The Hum Confirm wizard can also generate an affirmative default.
printf '#!/bin/sh\n' > "$test_dir/edit-confirm-yes.sh"
printf 'Edit an existing script\n%s/edit-confirm-yes.sh\nHum Confirm\nContinue?\nYes\necho "continuing"\n\ny\nn\n' \
    "$test_dir" | ./kizuku >/dev/null
sh -n "$test_dir/edit-confirm-yes.sh"
grep -Fq 'if hum confirm "Continue?" --default=yes; then' "$test_dir/edit-confirm-yes.sh"

# Add the ready-made basic Git pull step at a selected source position.
printf '#!/bin/sh\n\necho "existing"\n' > "$test_dir/edit-git-pull.sh"
printf 'Edit an existing script\n%s/edit-git-pull.sh\nGit Pull Basic\nNo\n3\ny\nn\n' \
    "$test_dir" | ./kizuku >/dev/null
sh -n "$test_dir/edit-git-pull.sh"
grep -Fq 'hum style --foreground "#d4e09b" --bold "==> Step 1: Update Code"' \
    "$test_dir/edit-git-pull.sh"
grep -Fq 'if hum confirm "Git Pull?" --default=no; then' "$test_dir/edit-git-pull.sh"
grep -Fq '    git pull' "$test_dir/edit-git-pull.sh"

# Add a Git pull spinner and last-commit summary with the default title.
printf '#!/bin/sh\nif true; then\n    git pull\nfi\n' > "$test_dir/edit-fancy-git-pull.sh"
printf 'Edit an existing script\n%s/edit-fancy-git-pull.sh\nFancy Git Pull\n1\n\ny\nn\n' \
    "$test_dir" | ./kizuku >/dev/null
sh -n "$test_dir/edit-fancy-git-pull.sh"
grep -Fq '    hum spin --title "Updating Git" -- git pull' \
    "$test_dir/edit-fancy-git-pull.sh"
grep -Fq '    git show --name-status --oneline' "$test_dir/edit-fancy-git-pull.sh"
test "$(grep -Fc 'git pull' "$test_dir/edit-fancy-git-pull.sh")" -eq 1

# The Fancy Git Pull spinner also accepts a custom title.
printf '#!/bin/sh\ngit pull\n' > "$test_dir/edit-fancy-git-pull-custom.sh"
printf 'Fancy Git Pull\n1\nFetching changes\ny\nn\n' \
    | ./kizuku "$test_dir/edit-fancy-git-pull-custom.sh" >/dev/null
sh -n "$test_dir/edit-fancy-git-pull-custom.sh"
grep -Fq 'hum spin --title "Fetching changes" -- git pull' \
    "$test_dir/edit-fancy-git-pull-custom.sh"

# Refuse Fancy Git Pull when there is no standalone command to replace.
printf '#!/bin/sh\necho "git pull"\n# git pull\n' > "$test_dir/edit-fancy-git-pull-empty.sh"
cp "$test_dir/edit-fancy-git-pull-empty.sh" \
    "$test_dir/edit-fancy-git-pull-empty.expected"
if printf 'Fancy Git Pull\n' \
    | ./kizuku "$test_dir/edit-fancy-git-pull-empty.sh" >/dev/null 2>&1; then
    echo "Fancy Git Pull unexpectedly accepted a script without a standalone git pull" >&2
    exit 1
fi
cmp "$test_dir/edit-fancy-git-pull-empty.expected" \
    "$test_dir/edit-fancy-git-pull-empty.sh"

# Append valid combined output and error logging to a selected command.
printf '#!/bin/sh\n# make should stay untouched\nmake -j4   \n' > "$test_dir/edit-output-log.sh"
chmod 740 "$test_dir/edit-output-log.sh"
printf 'Output to Log\n1\ny\nn\n' \
    | ./kizuku "$test_dir/edit-output-log.sh" >/dev/null
test -x "$test_dir/edit-output-log.sh"
sh -n "$test_dir/edit-output-log.sh"
grep -Fq 'make -j4 > output.log 2>&1' "$test_dir/edit-output-log.sh"
grep -Fq '# make should stay untouched' "$test_dir/edit-output-log.sh"

# Refuse Output to Log when a script contains only blank lines and comments.
printf '#!/bin/sh\n\n# Nothing to run\n' > "$test_dir/edit-output-log-empty.sh"
cp "$test_dir/edit-output-log-empty.sh" "$test_dir/edit-output-log-empty.expected"
if printf 'Output to Log\n' \
    | ./kizuku "$test_dir/edit-output-log-empty.sh" >/dev/null 2>&1; then
    echo "Output to Log unexpectedly accepted a script without command lines" >&2
    exit 1
fi
cmp "$test_dir/edit-output-log-empty.expected" "$test_dir/edit-output-log-empty.sh"

# Insert the ready-made command for displaying the last Git commit.
printf '#!/bin/sh\n' > "$test_dir/edit-git-show.sh"
chmod 750 "$test_dir/edit-git-show.sh"
printf 'Edit an existing script\n%s/edit-git-show.sh\nGit Last Commit Show\n\ny\nn\n' \
    "$test_dir" | ./kizuku >/dev/null
grep -Fq 'git show --name-status --oneline' "$test_dir/edit-git-show.sh"
test -x "$test_dir/edit-git-show.sh"
sh -n "$test_dir/edit-git-show.sh"

# A positional script argument opens that file directly and supports editing it.
printf '#!/bin/sh\n' > "$test_dir/direct-edit.sh"
chmod 740 "$test_dir/direct-edit.sh"
printf 'Git Last Commit Show\n\ny\nn\n' | ./kizuku "$test_dir/direct-edit.sh" >/dev/null
grep -Fq 'git show --name-status --oneline' "$test_dir/direct-edit.sh"
test -x "$test_dir/direct-edit.sh"
sh -n "$test_dir/direct-edit.sh"

# Collect every generated step command into one all-or-nothing confirmation.
cp "$test_dir/build.sh" "$test_dir/edit-all-or-nothing.sh"
printf 'Edit an existing script\n%s/edit-all-or-nothing.sh\nAll or Nothing\n\nNo\n\ny\nn\n' \
    "$test_dir" | ./kizuku >/dev/null
bash -n "$test_dir/edit-all-or-nothing.sh"
grep -Fq 'if hum confirm "Run all steps?" --default=no; then' \
    "$test_dir/edit-all-or-nothing.sh"
test "$(grep -Fc '    git pull' "$test_dir/edit-all-or-nothing.sh")" -eq 2
test "$(grep -Fc '    make clean' "$test_dir/edit-all-or-nothing.sh")" -eq 2
test "$(grep -Fc '    make -j4' "$test_dir/edit-all-or-nothing.sh")" -eq 2
test "$(grep -Fc '    make test' "$test_dir/edit-all-or-nothing.sh")" -eq 2
test "$(grep -Fc '    ./my-project' "$test_dir/edit-all-or-nothing.sh")" -eq 2

# Refuse All or Nothing without recognised steps and preserve the source file.
printf '#!/bin/sh\necho "no styled steps"\n' > "$test_dir/edit-all-or-nothing-empty.sh"
cp "$test_dir/edit-all-or-nothing-empty.sh" "$test_dir/edit-all-or-nothing-empty.expected"
if printf 'Edit an existing script\n%s/edit-all-or-nothing-empty.sh\nAll or Nothing\n' \
    "$test_dir" | ./kizuku >/dev/null 2>&1; then
    echo "All or Nothing unexpectedly accepted a script without styled steps" >&2
    exit 1
fi
cmp "$test_dir/edit-all-or-nothing-empty.expected" "$test_dir/edit-all-or-nothing-empty.sh"

# Select a source insertion point and add a Hum Choose block after the shebang.
printf '#!/bin/sh\n\necho "existing"\n' > "$test_dir/edit-choose.sh"
chmod 700 "$test_dir/edit-choose.sh"
printf 'Edit an existing script\n%s/edit-choose.sh\nHum Choose\nselection\nFirst item, Second item\n2\ny\nn\n' \
    "$test_dir" | ./kizuku >/dev/null
test -x "$test_dir/edit-choose.sh"
sh -n "$test_dir/edit-choose.sh"
grep -Fq "selection=\$(hum choose 'First item' 'Second item')" "$test_dir/edit-choose.sh"

# Exercise numbered placement by inserting input immediately before line 3.
printf '#!/bin/sh\n\necho "existing"\n' > "$test_dir/edit-before.sh"
printf 'Edit an existing script\n%s/edit-before.sh\nHum Input\nanswer\nYour answer\n3\ny\nn\n' \
    "$test_dir" | ./kizuku >/dev/null
sh -n "$test_dir/edit-before.sh"
test "$(sed -n '3p' "$test_dir/edit-before.sh")" = 'answer=$(hum input --placeholder "Your answer")'

# Cancelling from the source insertion picker must leave the script untouched.
printf '#!/bin/sh\necho "keep me"\n' > "$test_dir/edit-cancel.sh"
cp "$test_dir/edit-cancel.sh" "$test_dir/edit-cancel.expected"
printf 'Edit an existing script\n%s/edit-cancel.sh\nHum Spin\n\n\n0\n' \
    "$test_dir" | ./kizuku >/dev/null
cmp "$test_dir/edit-cancel.expected" "$test_dir/edit-cancel.sh"

# A generated Hum file picker starts from the script/project directory rather
# than whichever directory the user happens to launch the script from.
printf '#!/bin/sh\n' > "$test_dir/edit-file-picker.sh"
printf 'Edit an existing script\n%s/edit-file-picker.sh\nHum File Picker\nselected_directory\nDirectory\n\n\ny\nn\n' \
    "$test_dir" | ./kizuku >/dev/null
sh -n "$test_dir/edit-file-picker.sh"
grep -Fq 'project_directory=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)' \
    "$test_dir/edit-file-picker.sh"
grep -Fq 'selected_directory=$(hum file --no-file --directory --header "Select a directory — Right arrow opens folders; Enter selects" "$project_directory")' \
    "$test_dir/edit-file-picker.sh"

# List Hum confirmations, add a missing default, and replace an existing one.
printf '%s\n' \
    '#!/bin/sh' \
    '# hum confirm "Comment only?"' \
    'echo "hum confirm is text here"' \
    'if hum confirm "First?"; then' \
    '    echo first' \
    'fi' \
    'if hum confirm "Second?" --default=no; then' \
    '    echo second' \
    'fi' \
    > "$test_dir/edit-confirm-status.sh"
chmod 751 "$test_dir/edit-confirm-status.sh"
confirm_status_output=$(printf 'Edit an existing script\n%s/edit-confirm-status.sh\nCheck Hum Confirm Status\n1\nNo\ny\ny\nCheck Hum Confirm Status\n2\nYes\ny\nn\n' \
    "$test_dir" | ./kizuku)
printf '%s\n' "$confirm_status_output" | grep -Fq '[default: not set]'
grep -Fq 'if hum confirm "First?" --default=no; then' "$test_dir/edit-confirm-status.sh"
grep -Fq 'if hum confirm "Second?" --default=yes; then' "$test_dir/edit-confirm-status.sh"
test "$(grep -Fc -- '--default=' "$test_dir/edit-confirm-status.sh")" -eq 2
grep -Fq '# hum confirm "Comment only?"' "$test_dir/edit-confirm-status.sh"
grep -Fq 'echo "hum confirm is text here"' "$test_dir/edit-confirm-status.sh"
test -x "$test_dir/edit-confirm-status.sh"
sh -n "$test_dir/edit-confirm-status.sh"

# A status check with no Hum confirmations fails without changing the file.
printf '#!/bin/sh\necho "nothing to configure"\n' > "$test_dir/edit-confirm-status-empty.sh"
cp "$test_dir/edit-confirm-status-empty.sh" "$test_dir/edit-confirm-status-empty.expected"
if printf 'Edit an existing script\n%s/edit-confirm-status-empty.sh\nCheck Hum Confirm Status\n' \
    "$test_dir" | ./kizuku >/dev/null 2>&1; then
    echo "confirm status unexpectedly accepted a script without Hum confirmations" >&2
    exit 1
fi
cmp "$test_dir/edit-confirm-status-empty.expected" "$test_dir/edit-confirm-status-empty.sh"

# Select exact version occurrences and adjust them by 0.1 in either direction.
printf '%s\n' \
    '#!/bin/sh' \
    'hum style --bold "Builder v0.2"' \
    'echo "Release v1.9"' \
    'echo "Precise v0.26"' \
    > "$test_dir/edit-version.sh"
chmod 751 "$test_dir/edit-version.sh"
printf 'Edit an existing script\n%s/edit-version.sh\nUpdate Script Version\n1\nIncrement (+0.1)\ny\nn\n' \
    "$test_dir" | ./kizuku >/dev/null
grep -Fq 'hum style --bold "Builder v0.3"' "$test_dir/edit-version.sh"
printf 'Edit an existing script\n%s/edit-version.sh\nUpdate Script Version\n2\nDecrement (-0.1)\ny\nn\n' \
    "$test_dir" | ./kizuku >/dev/null
grep -Fq 'echo "Release v1.8"' "$test_dir/edit-version.sh"
printf 'Edit an existing script\n%s/edit-version.sh\nUpdate Script Version\n3\nIncrement (+0.1)\ny\nn\n' \
    "$test_dir" | ./kizuku >/dev/null
grep -Fq 'echo "Precise v0.36"' "$test_dir/edit-version.sh"
test -x "$test_dir/edit-version.sh"
sh -n "$test_dir/edit-version.sh"

# Reject a decrement below zero and preserve the original script.
printf '#!/bin/sh\necho "Version v0.0"\n' > "$test_dir/edit-version-zero.sh"
cp "$test_dir/edit-version-zero.sh" "$test_dir/edit-version-zero.expected"
if printf 'Edit an existing script\n%s/edit-version-zero.sh\nUpdate Script Version\n1\nDecrement (-0.1)\n' \
    "$test_dir" | ./kizuku >/dev/null 2>&1; then
    echo "version updater decremented below zero" >&2
    exit 1
fi
cmp "$test_dir/edit-version-zero.expected" "$test_dir/edit-version-zero.sh"

# A version update with no recognised version text leaves the script unchanged.
printf '#!/bin/sh\necho "no version"\n' > "$test_dir/edit-version-empty.sh"
cp "$test_dir/edit-version-empty.sh" "$test_dir/edit-version-empty.expected"
if printf 'Edit an existing script\n%s/edit-version-empty.sh\nUpdate Script Version\n' \
    "$test_dir" | ./kizuku >/dev/null 2>&1; then
    echo "version updater unexpectedly accepted a script without versions" >&2
    exit 1
fi
cmp "$test_dir/edit-version-empty.expected" "$test_dir/edit-version-empty.sh"

version=$(tr -d '\r\n' < VERSION)
test "$(./kizuku --version)" = "Kizuku v$version"
./kizuku --help | grep -Fq "Kizuku v$version"
./kizuku --help | grep -Fq 'SCRIPT           Open this file directly in the Edit workflow'
./kizuku --help | grep -Fq 'Desert, Neon, Rosewood, Arctic, Citrus'
./kizuku --help | grep -Fq 'ten popular four-colour palettes'
./kizuku --help | grep -Fq 'Quit'
printf 'Quit\n' | ./kizuku >/dev/null

echo "generator test passed"
