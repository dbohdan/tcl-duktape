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

# OO wrapper.
namespace eval ::duktape::oo {}

::oo::class create ::duktape::oo::Duktape {
    constructor {{_debug 0}} {
        my variable id
        my variable debug
        set id [::duktape::init]
        set debug $_debug
    }

    destructor {
        my variable id
        ::duktape::close $id
    }

    method eval {code} {
        my variable id
        my variable debug
        if {$debug} {
            puts "evaluating $code"
        }
        ::duktape::eval $id $code
    }

    method call-method args {
        my variable id
        ::duktape::call-method $id {*}$args
    }

    method call args {
        my variable id
        ::duktape::call $id {*}$args
    }
    foreach type $::duktape::types {
        method call-method-$type args [format {
            my variable id
            ::duktape::call-method-%s $id {*}$args
        } $type]

        method call-$type args [format {
            my variable id
            ::duktape::call-%s $id {*}$args
        } $type]
    }

    method jsproc {name arguments body} {
        my variable id
        ::duktape::jsproc $id $name $arguments $body
    }
}

# JSON object.
::oo::class create ::duktape::oo::JSON {
    constructor {duktapeInterp json} {
        my variable d
        my variable jsonId
        set d $duktapeInterp
        $d eval {
            if (typeof jsonStorage !== 'object') {
                jsonStorage = {};
                jsonStorage.create = function() {
                    that = {};
                    that.count = 0;
                    that.data = {};

                    that.init = function(literal) {
                        that.count++;
                        var id = that.count;
                        that.data[id] = JSON.parse(literal);
                        return id;
                    };

                    that._manipulatePath = function(id, pathList, from, to,
                            set, newValue) {
                        var currentPath = that.data[id];
                        var set = set || false;
                        for (var i = from; i < to - 1; i++) {
                            if (set && (typeof currentPath[pathList[i]] !==
                                    'object')) {
                                currentPath[[pathList[i]]] = {};
                            };
                            currentPath = currentPath[pathList[i]];
                        };
                        if (set) {
                            currentPath[pathList[to - 1]] = newValue;
                        };
                        return currentPath[pathList[to - 1]];
                    };
                    that.set = function(id) {
                        var value = arguments[arguments.length - 1];
                        return that._manipulatePath(id, arguments, 1,
                                arguments.length - 1, true, value);
                    };
                    that.get = function(id) {
                        return that._manipulatePath(id, arguments, 1,
                                arguments.length, false);
                    };
                    that.stringify = function(id) {
                        return JSON.stringify(that.data[id]);
                    };
                    that.destroy = function(id) {
                        delete that.data[id];
                        // Do not decrement count.
                        return;
                    };

                    return that;
                };

                json_data = jsonStorage.create();

                if (0) {
                    var tempid = json_data.init('{"a": "b"}');
                    var tempid2 = json_data.init('{}');
                    json_data._manipulatePath(tempid, ["hello"], 0, 1, false);
                    json_data.set(tempid, "hello", 5);
                    json_data.set(tempid, "a", "b", "c", 42, 5);
                    json_data.get(tempid, "hello");
                    json_data.destroy(tempid);
                    json_data.destroy(tempid2);
                    delete tempid;
                    delete tempid2;
                };
            };
        }
        set jsonId [$d call json_data.init [list $json str]]
    }

    method set args {
        my variable d
        my variable jsonId
        $d call-str json_data.set $jsonId {*}$args
    }

    method get args {
        my variable d
        my variable jsonId
        $d call-str json_data.get $jsonId {*}$args
    }

    method stringify {} {
        my variable d
        my variable jsonId
        $d call-str json_data.stringify $jsonId
    }
}
