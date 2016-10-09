#!/usr/bin/env tclsh
# Tcl bindings for Duktape.
# Copyright (c) 2015, 2016 dbohdan.
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

    tcltest::testConstraint tcloo [expr {
        ![catch { package require TclOO }]
    }]

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

    tcltest::test test3 {jsproc} \
            -setup $setup \
            -body {
        set result {}
        set id [::duktape::init]
        ::duktape::jsproc $id foo {{a 1 num} {b 2 num}} {
            return Math.sin(a) + b;
        }
        catch {
            ::duktape::jsproc $id foo {} {
                return -1;
            }
        }
        lappend result [foo 0 0]
        lappend result [foo 1 2]
        ::duktape::close $id
        return $result
    } -result {0 2.8414709848078967}

    tcltest::test test4 {oo} \
            -setup $setup \
            -constraints tcloo \
            -body {
        package require duktape::oo

        set result {}
        set duktapeInterp [::duktape::oo::Duktape new]
        $duktapeInterp jsproc foo {{a 1 num} {b 2 num}} {
            return Math.sin(a) + b;
        }
        catch {
            $duktapeInterp jsproc foo {} {
                return -1;
            }
        }
        lappend result [foo 0 0]
        lappend result [foo 1 2]

        $duktapeInterp jsmethod sin {{deg 0 number}} {
            return Math.sin(deg * Math.PI / 180);
        }
        lappend result [$duktapeInterp sin 90]

        $duktapeInterp destroy
        return $result
    } -result {0 2.8414709848078967 1}

    tcltest::test test4 {JSON object} \
            -setup $setup \
            -constraints tcloo \
            -body {
        package require duktape::oo

        set result {}
        set duktapeInterp [::duktape::oo::Duktape new]
        set json1 [::duktape::oo::JSON new $duktapeInterp {{"a":[1,2,3]}}]
        set json2 [::duktape::oo::JSON new $duktapeInterp {{}}]
        lappend result [$json1 get a 2]
        $json1 set b "Hello, world!\"'"
        lappend result [$json1 get b]
        $json1 set-json c {["foo", {"bar": "baz"}]}
        lappend result [$json1 get-json c]
        $json1 set-json {"test1"}
        lappend result [$json1 get-json]
        $json1 parse {"test2"}
        lappend result [$json1 stringify]

        $json1 destroy
        $json2 destroy
        $duktapeInterp destroy
        return $result
    } -result [list \
            3 \
            "Hello, world!\"'" \
            {["foo",{"bar":"baz"}]} \
            {"test1"} \
            {"test2"} \
    ]

    # Exit with nonzero status if there are failed tests.
    if {$::tcltest::numTests(Failed) > 0} {
        exit 1
    }
}
