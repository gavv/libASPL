#! /bin/bash
set -euo pipefail

msg() {
    echo "-- $*"
}

cmd() {
    echo "$*"
    "$@"
}

if [ "${EUID}" -ne 0 ]
then
    echo "This script must be run as root"
    exit 1
fi

cd "$(dirname "$0")"/..

drivers=($(find examples \
    -type d -not -name '_*' -mindepth 1 -maxdepth 1 -exec basename {} ';'))

install_dir="/Library/Audio/Plug-Ins/HAL"
build_dir="$(pwd)/build/Examples"

mode="install"
while getopts ":u" opt
do
    case "${opt}" in
        u)
            mode="uninstall"
            ;;
    esac
done

case "${mode}" in
    install)
        for driver in "${drivers[@]}"
        do
            msg "Installing ${driver}.driver"
            cmd rm -rf "${install_dir}/${driver}.driver"
            cmd cp -fr "${build_dir}/${driver}/${driver}.driver" "${install_dir}"
        done

        msg "Restarting audio server"
        cmd launchctl kickstart -k system/com.apple.audio.coreaudiod
        ;;

    uninstall)
        for driver in "${drivers[@]}"
        do
            msg "Uninstalling ${driver}.driver"
            cmd rm -rf "${install_dir}/${driver}.driver"
        done

        msg "Restarting audio server"
        cmd launchctl kickstart -k system/com.apple.audio.coreaudiod
        ;;
esac

msg "All done"
