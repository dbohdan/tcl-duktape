# tcl-duktape

This Tcl extension provides bindings for [Duktape](http://duktape.org/),
a JavaScript interpreter library.

## Installation

You will need Tcl 8.5, 8.6 or 9 installed on your system and available as
`tclsh` to build tcl-duktape. You will also need the header files for Tcl.
Duktape itself is bundled in the repository. To use the object-oriented API
wrapper and the JSON object system [TclOO](https://wiki.tcl-lang.org/18152)
is required.

```sh
# Build and test.
./configure
make test
# Install the package.
sudo make install
```

## API

### Procedures

* `::duktape::init ?-safe <boolean>?` -> token
* `::duktape::close token` -> (nothing)
* `::duktape::eval token code` -> (evaluation result)
* `::duktape::call-method token method this ?{arg ?type?}?` -> (evaluation result)
* `::duktape::call-method-(str|num) token method this ?arg?` -> (evaluation result)
* `::duktape::call token function ?{arg ?type?}?` -> (evaluation result)
* `::duktape::call-(str|num) token function ?arg?` -> (evaluation result)
* `::duktape::js-proc token name arguments body` -> (nothing)
* `::duktape::tcl-function token name ?returnType? arguments body` -> (nothing)
* `::duktape::make-safe token` -> (nothing)`
* `::duktape::make-unsafe token` -> (nothing)`

`make-safe` and `make-unsafe` control whether a new JavaScript function named
`Duktape.tcl.eval()` is created that allows for evaluation of arbitrary Tcl
scripts.

The optional `returnType` argument to `tcl-function` may be one of:
  * `boolean` — results in a boolean
  * `bytearray` — results in a Duktape [buffer](https://duktape.org/guide.html#bufferobjects)
  * `string` — default; results in a string
  * `undefined` — return value ignored
  * `null` — results in a JavaScript null regardless of the actual data
  * `double` — results in a number
  * `integer` — same as `double`
  * `bigint` — results in a JavaScript string representation of an integer
  * `json` — expects a JSON string; the result is the string decoded as JSON
  * `array itemType` — for each item in the Tcl list, encode as `itemType`;
                       the result is an array

### TclOO wrapper

* `::duktape::oo::Duktape new` -> (objName)
* `$objName destroy` -> (nothing)
* `$objName eval code` -> (evaluation result)
* `$objName call-method method this ?{arg ?type?}?` -> (evaluation result)
* `$objName call-method-(str|num) method this ?arg?` -> (evaluation result)
* `$objName call function ?{arg ?type?}?` -> (evaluation result)
* `$objName call-(str|num) function ?arg?` -> (evaluation result)
* `$objName js-proc name arguments body` -> (nothing)
* `$objName js-method name arguments body` -> (nothing)
* `$objname tcl-function name ?returnType? arguments body -> (nothing)
* `$objName token` -> token (for the procedural API)

`js-method` defines a new method in JavaScript on the Duktape object instance
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

Duktape 2.7 is copyright (c) 2013-2022 by Duktape authors and is distributed
under the MIT license. See `vendor/duktape/LICENSE.txt`.
