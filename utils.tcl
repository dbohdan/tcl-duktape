namespace eval ::duktape {}

proc ::duktape::slugify {text} {
    string trim [regsub -all {[^[:alnum:]]+} [string tolower $text] "-"] "-"
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
