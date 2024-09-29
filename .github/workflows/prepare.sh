#! /bin/sh
set -eu

printf '\n%s\n\n' "$(uname -a)"

if [ "$(uname)" = Linux ]; then
    apt-get install -y tcl-dev
fi

if [ "$(uname)" = Darwin ]; then
    brew install tcl-tk
fi


if [ "$(uname)" = FreeBSD ]; then
    pkg install -y tcl86
    ln -s /usr/local/bin/tclsh8.6 /usr/local/bin/tclsh
fi

if [ "$(uname)" = NetBSD ]; then
    pkgin -y install tcl
fi

if [ "$(uname)" = OpenBSD ]; then
    pkg_add -I tcl%8.6
    ln -s /usr/local/bin/tclsh8.6 /usr/local/bin/tclsh
fi
