#!/usr/bin/env tclsh
namespace eval ::buildsys {
    namespace export build install uninstall
    namespace ensemble create

    variable cc cc
    variable packageDir tcl-duktape
    variable libraryFilename libtclduktape[info sharedlibextension]
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

    file mkdir $packageInstallPath
    copy-to $libInstallPath $libraryFilename
    if { $customInstallPath eq "" } {
        set pkgFile pkgIndex-libdir.tcl
    } else {
        set pkgFile pkgIndex.tcl
    }
    copy-to $packageInstallPath [list $pkgFile pkgIndex.tcl] utils.tcl
}

proc ::buildsys::uninstall {{customInstallPath {}}} {
    set-install-paths

    delete-in $libInstallPath $libraryFilename
    delete-in $packageInstallPath pkgIndex.tcl utils.tcl
    delete $packageInstallPath
}

# Functionality common to both the installation and the uninstallation
# operation.
proc ::buildsys::set-install-paths {} {
    uplevel 1 {
        variable libraryFilename
        variable packageDir

        if { $customInstallPath ne "" } {
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
    cd [file dirname [file dirname [file normalize $argv0/___]]]
    buildsys {*}$argv
}

# Utility procs.

proc ::buildsys::cc args {
    variable cc
    puts "$cc $args"
    exec -- $cc {*}$args
}

# Copy files $args to $destDir. Each element in $args can be either a filename
# or a list of the format {sourceFilename destFilename}.
proc ::buildsys::copy-to {destDir args} {
    foreach file $args {
        lassign $file sourceFilename destFilename
        set message "copying file $sourceFilename to $destDir"
        if { ($destFilename ne "") && ($destFilename ne $sourceFilename) } {
            append message " as $destFilename"
        } else {
            set destFilename $sourceFilename
        }
        puts $message
        file copy $sourceFilename [file join $destDir $destFilename]
    }
}

# Delete file or directory $path.
proc ::buildsys::delete path {
    if { [file exists $path] } {
        puts "deleting $path"
        file delete $path
    } else {
        puts "cannot delete nonexistent path $path"
    }
}

# Delete file or directory $path.
proc ::buildsys::delete-in {path args} {
    foreach file $args {
        delete [file join $path $file]
    }
}

# If this is the main script...
if {[info exists argv0] && ([file tail [info script]] eq [file tail $argv0])} {
    ::buildsys::main $argv0 $argv
}
