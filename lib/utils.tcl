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
	# Special handling for "__args_json"
	set argsJSONIdx [lsearch -exact $argInfo "__args_json"]

	# Convert the code into a JavaScript string
	set codeJS [list apply [list $argInfo $code]]
	set codeJS [lmap item $codeJS {
		set item [string map [list \n "' + \"\\n\" + '" \\ \\\\ ' \\'] $item]
		set item "'$item'"
		set item
	}]

	# Convert the arguments into a JavaScript way
	## Needs to accomodate the callsite and function declaration
	## Duktape does not support variadic arguments so we must
	## emulate them using arrays :-(
	set argInfoJSFunction $argInfo
	if {$argsJSONIdx != -1} {
		set argInfoJSFunction [lreplace $argInfoJSFunction $argsJSONIdx $argsJSONIdx]
	}
	set argInfoJSFunction [join $argInfoJSFunction {, }]
	set usesVarArgs false
	foreach arg $argInfo {
		if {$arg eq "args"} {
			# Enable workaround for variadic arguments
			set usesVarArgs true
			break
		}
	}

	set jsFunction ""
	append jsFunction "function ${name}($argInfoJSFunction) \{\n"
	append jsFunction "\tvar ret;\n"

	if {$usesVarArgs} {
		append jsFunction "\tvar evalArray = \[\], evalString, argIdx;\n"
		foreach codeJSItem $codeJS {
			append jsFunction "\tevalArray.push($codeJSItem);\n"
		}

		if {$argsJSONIdx != -1} {
			set startIdx -1
		} else {
			set startIdx 0
		}

		append jsFunction "\tfor (argIdx = $startIdx; argIdx < arguments.length; argIdx++) \{\n"
		if {$argsJSONIdx != -1} {
			append jsFunction "\t\tif (argIdx != -1) \{\n"
		}
		append jsFunction "\t\tevalArray.push(arguments\[argIdx\]);\n"
		if {$argsJSONIdx != -1} {
			append jsFunction "\t\t\}\n"
			append jsFunction "\t\tif ((argIdx + 1) == $argsJSONIdx) \{\n"
			append jsFunction "\t\t\tevalArray.push(JSON.stringify(arguments));\n"
			append jsFunction "\t\t\}\n"
		}
		append jsFunction "\t\}\n"
		append jsFunction "\tevalString = evalArray.map(function (element) \{\n"
		append jsFunction "\t\treturn(JSON.stringify(element));\n"
		append jsFunction "\t\}).join(', ');\n"
		append jsFunction "\tret = eval('Duktape.tcl.eval(' + evalString + ');');\n"
	} else {
		set argInfoJSCallSite [lmap arg $argInfo {
			switch -exact -- $arg {
				"__args_json" {
					set arg "JSON.stringify(arguments)"
				}
			}
			return -level 0 $arg
		}]
		set argInfoJSCallSite [join $argInfoJSCallSite {, }]

		set codeJS [join $codeJS {, }]
		append jsFunction "\tret = Duktape.tcl.eval($codeJS, $argInfoJSCallSite);\n"
	}

	append jsFunction "\treturn(ret);\n"
	append jsFunction "\}"

	::duktape::eval $token $jsFunction
}
