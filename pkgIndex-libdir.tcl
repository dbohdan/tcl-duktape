package ifneeded "duktape" 0.3.1 [format "%s\n%s" \
    [list load libtclduktape[info sharedlibextension]] \
    [list source [file join $dir utils.tcl]] \
]
package ifneeded "duktape::oo" 0.3.1  \
    [list source [file join $dir oo.tcl]]
