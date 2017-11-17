package ifneeded "duktape" 0.3.3 [format "%s\n%s" \
    [list load libtclduktape[info sharedlibextension]] \
    [list source [file join $dir utils.tcl]] \
]
package ifneeded "duktape::oo" 0.3.3  \
    [list source [file join $dir oo.tcl]]
