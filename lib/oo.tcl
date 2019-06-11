# tcl-duktape OO wrapper.
# Copyright (c) 2015, 2016, 2017, 2019
# dbohdan and contributors listed in AUTHORS
# This code is released under the terms of the MIT license. See the file
# LICENSE for details.

package require TclOO

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
            set printedCode $code
            if {[string length $printedCode] > 80} {
                set printedCode [string range $printedCode 0 79]...
            }
            puts "evaluating code {$printedCode\n}"
        }
        ::duktape::eval $id $code
    }

    method call-method args {
        my variable id
        my variable debug
        if {$debug} {
            puts "calling method $args"
        }
        ::duktape::call-method $id {*}$args
    }

    method call args {
        my variable id
        my variable debug
        if {$debug} {
            puts "calling function $args"
        }
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
        uplevel 1 ::duktape::jsproc [list $id] [list $name] [list $arguments] \
                [list $body]
    }

    method jsmethod {name arguments body} {
        my variable id
        namespace eval [self namespace]::jsmethods {}
        set procName [self namespace]::jsmethods::$name
        ::duktape::jsproc $id $procName $arguments $body
        ::oo::objdefine [self object] forward $name $procName
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

                    that._keyError = function(pathList, from, to, i) {
                        throw "key " + pathList[i] + " in sequence {" +
                                Array.prototype.join.apply(
                                        Array.prototype.slice.apply(pathList,
                                                [from, to]),
                                                [' ']) + "} isn't an object";
                    }

                    that._manipulatePath = function(id, pathList, from, to,
                            set, newValue) {
                        var currentPath = that.data[id];
                        var set = set || false;
                        for (var i = from; i < to - 1; i++) {
                            if (typeof currentPath[pathList[i]] !== 'object') {
                                if (set) {
                                    currentPath[[pathList[i]]] = {};
                                } else {
                                    that._keyError(pathList, from, to, i);
                                };
                            };
                            currentPath = currentPath[pathList[i]];
                        };
                        if (set) {
                            currentPath[pathList[to - 1]] = newValue;
                        };
                        return currentPath[pathList[to - 1]];
                    };

                    that.get = function(id) {
                        return that._manipulatePath(id, arguments, 1,
                                arguments.length, false);
                    };

                    that.getJson = function(id) {
                        if (arguments.length > 1) {
                            var result = that.get.apply(that, arguments);
                        } else {
                            var result = that.data[id];
                        };
                        return JSON.stringify(result);
                    };

                    that.set = function(id) {
                        var value = arguments[arguments.length - 1];
                        that._manipulatePath(id, arguments, 1,
                                arguments.length - 1, true, value);
                        return "";
                    };

                    that.setJson = function(id) {
                        var value = JSON.parse(arguments[arguments.length - 1]);
                        if (arguments.length > 2) {
                            that._manipulatePath(id, arguments, 1,
                                    arguments.length - 1, true, value);
                        } else {
                            that.data[id] = value;
                        };
                        return "";
                    };

                    that.destroy = function(id) {
                        delete that.data[id];
                        // Do not decrement count.
                        return;
                    };

                    return that;
                };

                json_data = jsonStorage.create();
            };
        }
        set jsonId [$d call json_data.init [list $json str]]
    }

    method get args {
        my variable d
        my variable jsonId
        set result [$d call-str json_data.get $jsonId {*}$args]
        if {$result eq "undefined"} {
            error "can't access key $args"
        }
        return $result
    }

    method get-json args {
        my variable d
        my variable jsonId
        $d call-str json_data.getJson $jsonId {*}$args
    }

    method set args {
        my variable d
        my variable jsonId
        $d call-str json_data.set $jsonId {*}$args
    }

    method set-json args {
        my variable d
        my variable jsonId
        $d call-str json_data.setJson $jsonId {*}$args
    }

    method stringify {} {
        my get-json
    }

    method parse value {
        my set-json $value
    }
}

package provide duktape::oo 0.6.1
