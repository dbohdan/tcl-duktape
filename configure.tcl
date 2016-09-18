#!/usr/bin/env tclsh
namespace eval ::buildsys {
    namespace export build generate-makefile install uninstall
    namespace ensemble create

    variable cc cc
    variable packageDir tcl-duktape
    variable libraryFilename libtclduktape[info sharedlibextension]
    variable tclsh [info nameofexecutable]

    variable makefile {}
}

proc ::buildsys::generate-makefile args {
    variable libraryFilename
    variable makefile
    variable tclsh

    set makefile {}

    if {[llength $args] % 2 == 1} {
        error "the arguments should be a dictionary (\"$args\" given)"
    }
    if {[dict exists $args -destdir]} {
        set customInstallPath [dict get $args -destdir]
        dict unset args -destdir
    } else {
        set customInstallPath {}
    }
    if {[dict size $args] > 0} {
        error "unknown options: $args; should be\
                \"[dict get [info frame 0] proc] ?-destdir PATH?\""
    }

    target all test
    target $libraryFilename [list \
            tcl-duktape.c \
            external/duktape.c \
            external/duktape.h \
            external/duk_config.h \
    ]
    build
    target test $libraryFilename
    command $tclsh tests.tcl
    target install $libraryFilename
    install $customInstallPath
    target uninstall
    uninstall $customInstallPath
    target clean
    command -rm $libraryFilename
    target .PHONY {all clean install test uninstall}
    return $makefile
}

proc ::buildsys::build {} {
    variable cc
    variable libraryFilename

    set tclFlags [list \
            -I[::tcl::pkgconfig get includedir,runtime] \
            -L[::tcl::pkgconfig get libdir,runtime] \
    ]
    command $cc -Wall external/duktape.c -shared -o $libraryFilename \
            -fPIC {*}$tclFlags tcl-duktape.c
}

proc ::buildsys::install {{customInstallPath {}}} {
    set-install-paths

    command mkdir -p $packageInstallPath
    copy-to $libInstallPath $libraryFilename
    if {$customInstallPath eq {}} {
        set pkgFile pkgIndex-libdir.tcl
    } else {
        set pkgFile pkgIndex.tcl
    }
    copy-to $packageInstallPath \
            [list $pkgFile pkgIndex.tcl] \
            oo.tcl \
            utils.tcl
}

proc ::buildsys::uninstall {{customInstallPath {}}} {
    set-install-paths

    delete-in $libInstallPath $libraryFilename
    delete-in $packageInstallPath pkgIndex.tcl oo.tcl utils.tcl
    command rmdir $packageInstallPath
}

# Functionality common to both the install and the uninstall procedure.
proc ::buildsys::set-install-paths {} {
    uplevel 1 {
        variable libraryFilename
        variable packageDir

        if {$customInstallPath ne {}} {
            set packageInstallPath $customInstallPath
            set libInstallPath $customInstallPath
        } else {
            set packageInstallPath \
                    [file join [::tcl::pkgconfig get scriptdir,runtime] \
                            $packageDir]
            set libInstallPath [::tcl::pkgconfig get libdir,runtime]
        }
    }
}

proc ::buildsys::main {argv0 argv} {
    variable makefile

    cd [file dirname [file dirname [file normalize $argv0/___]]]

    set makefile [generate-makefile {*}$argv]

    set ch [open Makefile w]
    puts -nonewline $ch $makefile
    close $ch
}

# Utility procs.

proc ::buildsys::target {name {deps {}}} {
    variable makefile
    append makefile "$name: [join $deps]\n"
}

proc ::buildsys::quote s {
    if {[regexp {[^a-zA-Z0-9_./-]} $s]} {
        # Quote $arg and escape any single quotes in it.
        set result '[string map {' '"'"' $ $$} $s]'
    } else {
        set result $s
    }
    return $result
}

proc ::buildsys::command args {
    variable makefile
    set quotedArgs {}
    foreach arg $args {
        lappend quotedArgs [quote $arg]
    }
    command-raw {*}$quotedArgs
}

proc ::buildsys::command-raw args {
    variable makefile
    append makefile \t[join $args]\n
}

# Emit the commands to copy the files in the list $args to $destDir. Each
# element in $args can be either a filename or a list of the format
# {sourceFilename destFilename}.
proc ::buildsys::copy-to {destDir args} {
    foreach file $args {
        lassign $file sourceFilename destFilename
        command cp $sourceFilename [file join $destDir $destFilename]
    }
}

# Emit the command(s) to delete the given file(s) under $path.
proc ::buildsys::delete-in {path args} {
    foreach file $args {
        command -rm [file join $path $file]
    }
}

# If this is the main script...
if {[info exists argv0] && ([file tail [info script]] eq [file tail $argv0])} {
    ::buildsys::main $argv0 $argv
}
