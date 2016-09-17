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

proc ::buildsys::generate-makefile options {
    variable libraryFilename
    variable makefile
    variable tclsh

    set makefile {}

    if {[dict exists $options -destdir]} {
        set customInstallPath [dict get $options -destdir]
    } else {
        set customInstallPath {}
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
    command "'$tclsh' tests.tcl"
    target install $libraryFilename
    install $customInstallPath
    target uninstall
    uninstall $customInstallPath
    target clean
    command "-rm '$libraryFilename'"
    target .PHONY {clean all install test uninstall}
    return $makefile
}

proc ::buildsys::build {} {
    variable libraryFilename

    set tclFlags [list \
            -I[::tcl::pkgconfig get includedir,runtime] \
            -L[::tcl::pkgconfig get libdir,runtime] \
    ]
    cc -Wall external/duktape.c -shared -o $libraryFilename \
            -fPIC {*}$tclFlags tcl-duktape.c
}

proc ::buildsys::install {{customInstallPath {}}} {
    set-install-paths

    command "mkdir -p '$packageInstallPath'"
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
    command "rmdir '$packageInstallPath'"
}

# Functionality common to both the installation and the uninstallation
# operation.
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

    set makefile [generate-makefile $argv]

    set ch [open Makefile w]
    puts $ch $makefile
    close $ch
}

# Utility procs.

proc ::buildsys::target {name {deps {}}} {
    variable makefile
    append makefile "$name: [join $deps { }]\n"
}

proc ::buildsys::command line {
    variable makefile
    append makefile \t$line\n
}

proc ::buildsys::cc args {
    variable cc
    command "'$cc' [join $args { }]"
}

# Copy files $args to $destDir. Each element in $args can be either a filename
# or a list of the format {sourceFilename destFilename}.
proc ::buildsys::copy-to {destDir args} {
    foreach file $args {
        lassign $file sourceFilename destFilename
        command "cp '$sourceFilename' '[file join $destDir $destFilename]'"
    }
}

# Delete file(s) in $path.
proc ::buildsys::delete-in {path args} {
    foreach file $args {
        command "-rm '[file join $path $file]'"
    }
}

# If this is the main script...
if {[info exists argv0] && ([file tail [info script]] eq [file tail $argv0])} {
    ::buildsys::main $argv0 $argv
}
