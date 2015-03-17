DESTDIR?=

all: test
libtclduktape:
	tclsh build.tcl build
test: libtclduktape
	tclsh tests.tcl
install:
	tclsh build.tcl install $(DESTDIR)
uninstall:
	tclsh build.tcl uninstall $(DESTDIR)
clean:
	rm libtclduktape.so
.PHONY: clean
.POSIX:
