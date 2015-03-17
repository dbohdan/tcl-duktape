#!/usr/bin/env tclsh
proc main {} {
    set ::cc cc
    set tclFlags [list \
            -I[::tcl::pkgconfig get includedir,runtime] \
            -L[::tcl::pkgconfig get libdir,runtime] \
    ]
    cc -Wall external/duktape.c -shared -o libduktcl.so \
            -fPIC {*}$tclFlags duktcl.c
}

proc cc args {
    puts "$::cc $args"
    exec -- $::cc {*}$args
}

main
