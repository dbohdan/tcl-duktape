#! /bin/sh
set -eu

if [ "$(uname)" = Darwin ]; then
    ./configure --with-tcl="$(brew --prefix tcl-tk)"/lib
else
    ./configure
fi

make test
