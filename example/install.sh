#! /bin/bash
set -euo pipefail

cd "$(dirname "$0")"/..

MODE="install"
while getopts ":u" opt
do
    case "${opt}" in
        u)
            MODE="uninstall"
            ;;
    esac
done

DRIVER_NAME="ASPL_Example"
DRIVER_BUNDLE="${DRIVER_NAME}.driver"

INSTALL_DIR="/Library/Audio/Plug-Ins/HAL"
BUILD_DIR="$(pwd)/build/Example"

case "${MODE}" in
    install)
        echo "Installing driver..."
        rm -rf "${INSTALL_DIR}/${DRIVER_BUNDLE}"
        cp -fr "${BUILD_DIR}/${DRIVER_BUNDLE}" "${INSTALL_DIR}/${DRIVER_BUNDLE}"

        echo "Restarting audio server..."
        launchctl kickstart -k system/com.apple.audio.coreaudiod
        ;;
    uninstall)
        echo "Uninstalling driver..."
        rm -rf "${INSTALL_DIR}/${DRIVER_BUNDLE}"

        echo "Restarting audio server..."
        launchctl kickstart -k system/com.apple.audio.coreaudiod
        ;;
esac

echo "Done."
