# Tcl bindings for Duktape.
# Copyright (c) 2015, 2016, 2017, 2018
# dbohdan and contributors listed in AUTHORS
# This code is released under the terms of the MIT license. See the file
# LICENSE for details.

package ifneeded "duktape" 0.5.0 [list apply {dir {
    uplevel 1 [list load libtclduktape[info sharedlibextension]]
    uplevel 1 [list source [file join $dir utils.tcl]]
}} $dir]
package ifneeded "duktape::oo" 0.5.0 [list apply {dir {
    uplevel 1 [list source [file join $dir oo.tcl]]
}} $dir]
