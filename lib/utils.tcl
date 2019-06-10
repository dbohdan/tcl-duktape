# Utility procs for tcl-duktape.
# Copyright (c) 2015, 2016, 2017
# dbohdan and contributors listed in AUTHORS
# This code is released under the terms of the MIT license. See the file
# LICENSE for details.

namespace eval ::duktape {}

set ::duktape::types [list num str]

proc ::duktape::call {id function args} {
    ::duktape::call-method $id $function $function {*}$args
}

# Generate procs call-num, call-str.
foreach type $::duktape::types {
    proc ::duktape::call-method-$type {id function this args} [format {
        set callArgs {}
        foreach arg $args {
            lappend callArgs [list $arg %1$s]
        }
        ::duktape::call-method $id $function $this {*}$callArgs
    } $type]

    proc ::duktape::call-$type {id function args} [format {
        set callArgs {}
        foreach arg $args {
            lappend callArgs [list $arg %1$s]
        }
        ::duktape::call $id $function {*}$callArgs
    } $type]
}

# This is used by ::duktape::jsproc.
proc ::duktape::slugify {text} {
    string trim [regsub -all {[^[:alnum:]]+} [string tolower $text] _] _
}

# Create a Tcl proc with JavaScript code. $arguments follows the format of
# {{arg ?default? ?type} ...}. JavaScript argument names will be the same as
# the Tcl argument names.
proc ::duktape::jsproc {id name arguments body} {
    set tclArgList {}
    set jsArgList {}
    set jsName [slugify $name]
    foreach arg $arguments {
        lassign $arg argName default type
        lappend tclArgList [list $argName $default]
        lappend jsArgList $argName
    }

    # Abort if the JS function exists.
    set exists [::duktape::eval $id "typeof $jsName === 'function'"]
    if {$exists} {
        error "Duktape function \"$jsName\" exists"
    }

    # Create the JS function.
    ::duktape::eval $id [format {
        function %1$s (%2$s) {
            %3$s
        }
    } $jsName [join $jsArgList ,] $body]

    # Create the corresponding Tcl wrapper at uplevel 1.
    uplevel 1 [list proc $name $tclArgList [list apply {{id jsName arguments} {
        set callArgs {}
        foreach arg $arguments {
            lassign $arg argName _ type
            upvar 1 $argName argValue
            lappend callArgs [list $argValue $type]
        }
        ::duktape::call $id $jsName {*}$callArgs
    }} $id $jsName $arguments]]
}

# Create a JavaScript function with Tcl code
proc ::duktape::tclfunction {token name argInfo code} {
	set argInfoJS [join $argInfo {, }]

	set codeJS [list apply [list $argInfo $code]]
	set codeJS [lmap item $codeJS {
		set item [string map [list \n "' + \"\\n\" + '" \\ \\\\ ' \\'] $item]
		set item "'$item'"
		set item
	}]
	set codeJS [join $codeJS {, }]

	set jsFunction ""
	append jsFunction "function ${name}($argInfoJS) \{\n"
	append jsFunction "\tvar ret;\n"
	append jsFunction "\tret = Duktape.tcl.eval($codeJS, $argInfoJS);\n"
	append jsFunction "\treturn(ret);\n"
	append jsFunction "\}"

	::duktape::eval $token $jsFunction
}
