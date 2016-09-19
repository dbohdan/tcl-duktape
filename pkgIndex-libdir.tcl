package ifneeded "duktape" 0.2.0 [format "%s\n%s" \
    [list load libtclduktape[info sharedlibextension]] \
    [list source [file join $dir utils.tcl]] \
]
package ifneeded "duktape::oo" 0.2.0 \
    [list source [file join $dir oo.tcl]]
