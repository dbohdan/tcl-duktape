DESTDIR?=

all: test
libduktcl:
	tclsh build.tcl
test: libduktcl
	tclsh tests.tcl
clean:
	rm libduktcl.so
.PHONY: clean
