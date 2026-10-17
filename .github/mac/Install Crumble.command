#!/bin/bash
# Crumble installer for macOS - Primal Rage Lab
cd "$(dirname "$0")" || exit 1

SRC="$(pwd)/Crumble.vst3"
DEST="/Library/Audio/Plug-Ins/VST3"

echo ""
echo "=============================================="
echo "   CRUMBLE - Primal Rage Lab"
echo "=============================================="
echo ""

if [ ! -d "$SRC" ]; then
    echo "Crumble.vst3 not found next to this installer."
    echo "Не нашёл Crumble.vst3 рядом с установщиком."
    echo ""
    read -n 1 -s -r -p "Press any key / Нажми любую клавишу..."
    exit 1
fi

echo "Your Mac password will be asked. Letters don't show while typing - that's normal."
echo "Сейчас спросит пароль от мака. Буквы при вводе не видно - так и надо."
echo ""

sudo mkdir -p "$DEST" || exit 1
sudo rm -rf "$DEST/Crumble.vst3"
sudo cp -R "$SRC" "$DEST/"
sudo chmod -R 755 "$DEST/Crumble.vst3"
sudo xattr -cr "$DEST/Crumble.vst3"

# Remove an old copy from the user folder, so the DAW doesn't see two Crumbles.
rm -rf "$HOME/Library/Audio/Plug-Ins/VST3/Crumble.vst3" 2>/dev/null

if [ -f "$DEST/Crumble.vst3/Contents/MacOS/Crumble" ]; then
    echo ""
    echo "DONE! Crumble is installed to $DEST"
    echo "ГОТОВО! Crumble установлен."
    echo ""
    echo "Open your DAW and rescan plugins."
    echo "Открой DAW и пересканируй плагины."
else
    echo ""
    echo "Something went wrong. Try the Terminal way from HOW TO INSTALL.txt"
    echo "Что-то пошло не так. Попробуй способ через Терминал из HOW TO INSTALL.txt"
fi
echo ""
read -n 1 -s -r -p "Press any key to close / Нажми любую клавишу, чтобы закрыть..."
echo ""
