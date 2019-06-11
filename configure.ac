dnl Define ourselves
AC_INIT(tcl-duktape, 0.7.0)

dnl Checks for programs.
AC_PROG_CC
AC_PROG_CC_C99
AC_PROG_MAKE_SET
AC_PROG_INSTALL
AC_GNU_SOURCE
AC_PROG_LN_S

dnl Monocypher requires C99
if test "$ac_cv_prog_cc_c99" = no; then
	AC_ERROR([C99 compiler required])
fi

dnl Determine system information
DC_CHK_OS_INFO

dnl Look for appropriate headers
AC_CHECK_HEADERS(stdint.h limits.h unistd.h stdlib.h string.h sys/stat.h sys/types.h fcntl.h sys/random.h)

dnl Perform Tcl Extension required stuff
TCLEXT_INIT

if test "$TCLEXT_BUILD" != 'static'; then
	dnl Determine how to make shared objects
	DC_GET_SHOBJFLAGS

	EXTENSION_TARGET="libtclduktape.${SHOBJEXT}"
else
	AC_CHECK_TOOL([AR], [ar], [false])
	AC_CHECK_TOOL([RANLIB], [ranlib], [:])

	EXTENSION_TARGET="libtclduktape.${AREXT}"
fi
AC_SUBST(EXTENSION_TARGET)
AC_SUBST(TCLEXT_BUILD)

dnl Locate libraries needed for math functions used
for checkFunction in acos asin atan atan2 ceil cos exp floor fmod log pow sin sqrt tan; do
	AC_SEARCH_LIBS($checkFunction, m,, [
		AC_ERROR([Unable to find $checkFunction])
	])
done

dnl Enable support for a debugging build
tcl_duktape_debug='false'
AC_ARG_ENABLE([debug], AS_HELP_STRING([--enable-debug], [enable debugging parameters]), [
	if test "$enableval" = "yes"; then
		tcl_duktape_debug='true'
	fi
])

dnl If we are building a debug release, enable debugging flags
if test "$tcl_duktape_debug" = 'true'; then
	AC_DEFINE(TCLEXT_TCL_DUKTAPE_DEBUG, [1], [Enable debugging build])
	AX_CHECK_COMPILE_FLAG([-mmpx -fcheck-pointer-bounds], [
		AX_CHECK_LINK_FLAG([-mmpx -fcheck-pointer-bounds], [
			CFLAGS="$CFLAGS -mmpx -fcheck-pointer-bounds"
		])
	])
	AX_CHECK_COMPILE_FLAG([-g3], [CFLAGS="$CFLAGS -g3"])
	AX_CHECK_COMPILE_FLAG([-ggdb3], [CFLAGS="$CFLAGS -ggdb3"])
else
	dnl If we are not doing debugging disable some of the more annoying warnings
	AX_CHECK_COMPILE_FLAG([-Wno-unused-value], [CFLAGS="$CFLAGS -Wno-unused-value"])
	AX_CHECK_COMPILE_FLAG([-Wno-unused-parameter], [CFLAGS="$CFLAGS -Wno-unused-parameter"])
	AX_CHECK_COMPILE_FLAG([-Wno-deprecated-declarations], [CFLAGS="$CFLAGS -Wno-deprecated-declarations"])
fi
AX_CHECK_COMPILE_FLAG([-Wno-sign-compare], [CFLAGS="$CFLAGS -Wno-sign-compare"])

dnl Enable compiler warnings
AX_CHECK_COMPILE_FLAG([-Wall], [CFLAGS="-Wall $CFLAGS"])
AX_CHECK_COMPILE_FLAG([-W], [
	CFLAGS="-W $CFLAGS"
	AX_CHECK_COMPILE_FLAG([-Wno-self-assign], [CFLAGS="$CFLAGS -Wno-self-assign"])
	AX_CHECK_COMPILE_FLAG([-Wno-tautological-constant-out-of-range-compare], [CFLAGS="$CFLAGS -Wno-tautological-constant-out-of-range-compare"])
])

dnl Sync the RPATH if requested
if test "$TCLEXT_BUILD" != 'static'; then
	DC_SYNC_RPATH([yes])
fi

dnl Setup a stable ABI
DC_SETUP_STABLE_API([${srcdir}/libtclduktape.vers], libtclduktape.syms)
if test "$tcl_duktape_debug" = 'true'; then
	WEAKENSYMS=':'
	REMOVESYMS=':'
fi

dnl Default to using the amalgamation for static, not using it for shared
if test "$TCLEXT_BUILD" != 'static'; then
	tcl_duktape_amalgamation='false'
else
	tcl_duktape_amalgamation='true'
fi

AC_ARG_ENABLE([amalgamation], AS_HELP_STRING([--enable-amalgamation], [enable compiling the whole program as a single translation unit (default: disabled for shared, enabled for static)]), [
	if test "$enableval" = 'yes'; then
		tcl_duktape_amalgamation='true'
	elif test "$enableval" = 'no'; then
		tcl_duktape_amalgamation='false'
	else
		AC_ERROR([Unknown value for --enable-amalgamation])
	fi
])
if test "$tcl_duktape_amalgamation" = 'true'; then
	TCL_DUKTAPE_AMALGAMATION='1'
else
	TCL_DUKTAPE_AMALGAMATION='0'
fi
AC_SUBST(TCL_DUKTAPE_AMALGAMATION)

dnl Produce output
AC_CONFIG_FILES([pkgIndex.tcl:${srcdir}/lib/pkgIndex.tcl.in Makefile libtclduktape.syms])
AC_OUTPUT()
