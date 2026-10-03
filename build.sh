#!/bin/bash

clear
cd '/boot/home/kizuku' || exit 1

if ! command -v hum >/dev/null 2>&1; then
    echo "This build file requires hum." >&2
    exit 127
fi

hum style --border rounded --foreground 41 --padding "1 2" --margin "1 0" --bold "kizuku Builder v0.1"

if hum confirm "Continue?"; then

hum style --foreground "#e8b76a" --bold "==> Step 1: Make Clean"
if hum confirm "Make Clean?"; then
    make clean
fi

hum style --foreground "#d98a4e" --bold "==> Step 2: Make"
if hum confirm "Make?"; then
    make
fi

hum style --foreground "#d98a3e" --bold "==> Step 3: Make Test"
if hum confirm "Make Tests?"; then
    make test
fi

hum style --foreground "#6f3a63" --bold "==> Step 4: Run / Test"
if hum confirm "Run?"; then
    ./kizuku
fi

fi
