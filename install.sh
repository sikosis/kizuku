#!/bin/sh

clear
hum style --border rounded --foreground 67 --padding "1 2" --margin "1 0" --bold "Kizuku - Install"

if hum confirm "Continue?"; then
	cp kizuku /boot/home/config/non-packaged/bin/kizuku

	/boot/home/config/non-packaged/bin/kizuku --help
fi
