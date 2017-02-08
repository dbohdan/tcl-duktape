# tcl-duktape

[![Build Status](https://travis-ci.org/dbohdan/tcl-duktape.svg)](https://travis-ci.org/dbohdan/tcl-duktape)

This Tcl extension provides bindings for [Duktape](http://duktape.org/),
a JavaScript interpreter library.

## Installation

You will need Tcl 8.5 or 8.6 installed on your system and available as `tclsh`
to build tcl-duktape. You will also need the header files for Tcl. Duktape
itself is bundled in the repository. To use the object-oriented API wrapper
and the JSON object system [TclOO](http://tcl.wiki/18152) is required.

```sh
# Build and test.
tclsh configure
make test
# Install the package.
sudo make install
```

By default the shared library is installed to
[`libdir,runtime`](http://tcl.wiki/11825), e.g., `/usr/lib64`, and the
package is installed to the subdirectory `tcl-duktape` in `scriptdir,runtime`,
e.g., `/usr/share/tcl8.6/tcl-duktape`. To install both the shared library and
the package to `/custom/path/` instead use the command

```sh
tclsh configure --destdir /custom/path/
sudo make install # Or just `make install` depending on the destination.
```

## API

### Procedures

* `::duktape::init` -> token
* `::duktape::close token` -> (nothing)
* `::duktape::eval token code` -> (evaluation result)
* `::duktape::call-method token method this ?{arg ?type?}?` -> (evaluation result)
* `::duktape::call-method-(str|num) token method this ?arg?` -> (evaluation result)
* `::duktape::call token function ?{arg ?type?}?` -> (evaluation result)
* `::duktape::call-(str|num) token function ?arg?` -> (evaluation result)
* `::duktape::jsproc token name arguments body` -> (nothing)

### TclOO wrapper

* `::duktape::oo::Duktape new` -> (objName)
* `$objName destroy` -> (nothing)
* `$objName eval code` -> (evaluation result)
* `$objName call-method method this ?{arg ?type?}?` -> (evaluation result)
* `$objName call-method-(str|num) method this ?arg?` -> (evaluation result)
* `$objName call function ?{arg ?type?}?` -> (evaluation result)
* `$objName call-(str|num) function ?arg?` -> (evaluation result)
* `$objName jsproc name arguments body` -> (nothing)
* `$objName jsmethod name arguments body` -> (nothing)

`jsmethod` defines a new method in JavaScript on the Duktape object instance
`$objName`.

### JSON objects

* `::duktape::oo::JSON new` -> (objName)
* `$objName destroy` -> (nothing)
* `$objName get key ?key ...?` -> (value)
* `$objName get-json ?key ...?` -> (JSON string)
* `$objName set key ?key ...? value` -> (nothing)
* `$objName set-json ?key ...? value` -> (nothing)
* `$objName stringify` -> (JSON string)
* `$objName parse value` -> (nothing)

Note that `get` returns objects to Tcl as the string "[object Object]" or
similar. Use `stringify` to get their JSON representation instead.

## License

MIT.

Duktape 2.0 is copyright (c) 2013-2017 by Duktape authors and is distributed
under the MIT license. See `external/LICENSE.txt`.
