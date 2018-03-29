package ifneeded "duktape" 0.3.3 [list apply {dir {
    uplevel 1 [list load [file join $dir libtclduktape[info sharedlibextension]]]
    uplevel 1 [list source [file join $dir utils.tcl]]
}} $dir]
package ifneeded "duktape::oo" 0.3.3 [list apply {dir {
    uplevel 1 [list source [file join $dir oo.tcl]]
}} $dir]
