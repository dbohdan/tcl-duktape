#!/usr/bin/env tclsh
# tcl-augeas, Tcl bindings for Augeas.
# Copyright (C) 2015 Danyil Bohdan.
# This code is released under the terms of the MIT license. See the file
# LICENSE for details.

package require tcltest

namespace eval ::duktape::tests {
    variable path [file dirname [file dirname [file normalize $argv0/___]]]
    variable setup [list apply {{path} {
        lappend ::auto_path $path
        package require duktape
        cd $path
    }} $path]

    tcltest::test test1 {init, eval and close} \
            -setup $setup \
            -body {
        set id [::duktape::init]
        set value [::duktape::eval $id {1 + 2 * 3}]
        ::duktape::close $id
        return $value
    } -result 7

    tcltest::test test2 {call, types} \
            -setup $setup \
            -body {
        set result {}
        set id [::duktape::init]
        lappend result [::duktape::call $id Math.abs -5]
        lappend result [::duktape::call $id Math.abs {-5 boolean}]
        lappend result [::duktape::call $id Math.abs {-5 nan}]
        lappend result [::duktape::call $id Math.abs {-5 null}]
        lappend result [::duktape::call $id Math.abs {-5 number}]
        lappend result [::duktape::call $id Math.abs {-5 undefined}]
        lappend result [::duktape::call $id Math.abs {-5 string}]
        catch {
            lappend result [::duktape::call $id Math.abs {-5 foo}]
        }
        catch {
            lappend result [::duktape::call $id Math.abs {-5 hello world}]
        }
        ::duktape::close $id
        return $result
    } -result [list \
        5         1       NaN 0    5      NaN       5]
    #   (no type) boolean nan null number undefined string

    # Exit with nonzero status if there are failed tests.
    if {$::tcltest::numTests(Failed) > 0} {
        exit 1
    }
}
