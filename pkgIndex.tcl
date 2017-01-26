package ifneeded "duktape" 0.3.0 [format "%s\n%s" \
    [list load [file join $dir libtclduktape[info sharedlibextension]]] \
    [list source [file join $dir utils.tcl]] \
]
package ifneeded "duktape::oo" 0.3.0 \
    [list source [file join $dir oo.tcl]]
