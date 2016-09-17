all: test
libtclduktape.so: tcl-duktape.c external/duktape.c external/duktape.h external/duk_config.h
	'cc' -Wall external/duktape.c -shared -o libtclduktape.so -fPIC -I/usr/include -L/usr/lib64 tcl-duktape.c
test: libtclduktape.so
	'/usr/bin/tclsh' tests.tcl
install: libtclduktape.so
	mkdir -p '/tmp/yyy'
	cp 'libtclduktape.so' '/tmp/yyy'
	cp 'pkgIndex.tcl' '/tmp/yyy/pkgIndex.tcl'
	cp 'oo.tcl' '/tmp/yyy'
	cp 'utils.tcl' '/tmp/yyy'
uninstall: 
	-rm '/tmp/yyy/libtclduktape.so'
	-rm '/tmp/yyy/pkgIndex.tcl'
	-rm '/tmp/yyy/oo.tcl'
	-rm '/tmp/yyy/utils.tcl'
	rmdir '/tmp/yyy'
clean: 
	-rm 'libtclduktape.so'
.PHONY: clean all install test uninstall

