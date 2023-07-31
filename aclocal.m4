dnl Tcl M4 Routines

dnl Find a runnable Tcl
AC_DEFUN([TCLEXT_FIND_TCLSH_PROG], [
	AC_CACHE_CHECK([for runnable tclsh], [tcl_cv_tclsh_native_path], [
		dnl Try to find a runnable tclsh
		if test -z "$TCLCONFIGPATH"; then
			TCLCONFIGPATH=/dev/null/null
		fi

		for try_tclsh in "$TCLSH_NATIVE" "$TCLCONFIGPATH/../bin/tclsh" \
		                 "$TCLCONFIGPATH/../bin/tclsh8.6" \
		                 "$TCLCONFIGPATH/../bin/tclsh8.5" \
		                 "$TCLCONFIGPATH/../bin/tclsh8.4" \
		                 `which tclsh 2>/dev/null` \
		                 `which tclsh8.6 2>/dev/null` \
		                 `which tclsh8.5 2>/dev/null` \
		                 `which tclsh8.4 2>/dev/null` \
		                 tclsh; do
			if test -z "$try_tclsh"; then
				continue
			fi
			if test -x "$try_tclsh"; then
				if echo 'exit 0' | "$try_tclsh" 2>/dev/null >/dev/null; then
					tcl_cv_tclsh_native_path="$try_tclsh"

					break
				fi
			fi
		done

		if test "$TCLCONFIGPATH" = '/dev/null/null'; then
			unset TCLCONFIGPATH
		fi
	])

	TCLSH_PROG="${tcl_cv_tclsh_native_path}"
	AC_SUBST(TCLSH_PROG)
])


dnl Must call AC_CANONICAL_HOST  before calling us
AC_DEFUN([TCLEXT_FIND_TCLCONFIG], [

	TCLCONFIGPATH=""
	AC_ARG_WITH([tcl], AS_HELP_STRING([--with-tcl], [directory containing tcl configuration (tclConfig.sh)]), [
		if test "x$withval" = "xno"; then
			AC_MSG_ERROR([cant build without tcl])
		fi

		TCLCONFIGPATH="$withval"
	], [
		if test "$cross_compiling" = 'no'; then
			TCLEXT_FIND_TCLSH_PROG
			tclConfigCheckDir0="`echo 'puts [[tcl::pkgconfig get libdir,runtime]]' | "$TCLSH_PROG" 2>/dev/null`"
			tclConfigCheckDir1="`echo 'puts [[tcl::pkgconfig get scriptdir,runtime]]' | "$TCLSH_PROG" 2>/dev/null`"
		else
			tclConfigCheckDir0=/dev/null/null
			tclConfigCheckDir1=/dev/null/null
		fi

		if test "$cross_compiling" = 'no'; then
			dirs="/usr/$host_alias/lib /usr/lib /usr/lib64 /usr/local/lib /usr/local/lib64"
		else
			dirs=''
		fi

		for dir in "$tclConfigCheckDir0" "$tclConfigCheckDir1" $dirs; do
			if test -f "$dir/tclConfig.sh"; then
				TCLCONFIGPATH="$dir"

				break
			fi
		done
	])

	AC_MSG_CHECKING([for path to tclConfig.sh])

	if test -z "$TCLCONFIGPATH"; then
		AC_MSG_ERROR([unable to locate tclConfig.sh.  Try --with-tcl.])
	fi

	AC_SUBST(TCLCONFIGPATH)

	AC_MSG_RESULT([$TCLCONFIGPATH])

	dnl Find Tcl if we haven't already
	if test -z "$TCLSH_PROG"; then
		TCLEXT_FIND_TCLSH_PROG
	fi
])

dnl Must define TCLCONFIGPATH before calling us (i.e., by TCLEXT_FIND_TCLCONFIG)
AC_DEFUN([TCLEXT_LOAD_TCLCONFIG], [
	AC_MSG_CHECKING([for working tclConfig.sh])

	if test -f "$TCLCONFIGPATH/tclConfig.sh"; then
		. "$TCLCONFIGPATH/tclConfig.sh"
	else
		AC_MSG_ERROR([unable to load tclConfig.sh])
	fi


	AC_MSG_RESULT([found])
])

AC_DEFUN([TCLEXT_INIT], [
	AC_CANONICAL_HOST

	TCLEXT_FIND_TCLCONFIG
	TCLEXT_LOAD_TCLCONFIG

	AC_DEFINE_UNQUOTED([MODULE_SCOPE], [static], [Define how to declare a function should only be visible to the current module])

	TCLEXT_BUILD='shared'
	AC_ARG_ENABLE([shared], AS_HELP_STRING([--disable-shared], [disable the shared build (same as --enable-static)]), [
		if test "$enableval" = "no"; then
			TCLEXT_BUILD='static'
			TCL_SUPPORTS_STUBS=0
		fi
	])

	AC_ARG_ENABLE([static], AS_HELP_STRING([--enable-static], [enable a static build]), [
		if test "$enableval" = "yes"; then
			TCLEXT_BUILD='static'
			TCL_SUPPORTS_STUBS=0
		fi
	])

	AC_ARG_ENABLE([stubs], AS_HELP_STRING([--disable-stubs], [disable use of Tcl stubs]), [
		if test "$enableval" = "no"; then
			TCL_SUPPORTS_STUBS=0
		else
			TCL_SUPPORTS_STUBS=1
		fi
	])

	if test "$TCL_SUPPORTS_STUBS" = "1"; then
		AC_DEFINE([USE_TCL_STUBS], [1], [Define if you are using the Tcl Stubs Mechanism])

		TCL_STUB_LIB_SPEC="`eval echo "${TCL_STUB_LIB_SPEC}"`"
		LIBS="${LIBS} ${TCL_STUB_LIB_SPEC}"
	else
		TCL_LIB_SPEC="`eval echo "${TCL_LIB_SPEC}"`"
		LIBS="${LIBS} ${TCL_LIB_SPEC}"
	fi

	TCL_INCLUDE_SPEC="`eval echo "${TCL_INCLUDE_SPEC}"`"

	CFLAGS="${CFLAGS} ${TCL_INCLUDE_SPEC}"
	CPPFLAGS="${CPPFLAGS} ${TCL_INCLUDE_SPEC}"
	TCL_DEFS_TCL_ONLY=`(
		eval "set -- ${TCL_DEFS}"
		for flag in "[$]@"; do
			case "${flag}" in
				-DTCL_*)
					echo "${flag}" | sed "s/'/'\\''/g" | sed "s@^@'@;s@"'[$]'"@'@" | tr $'\n' ' '
					;;
			esac
		done
	)`
	TCL_DEFS="${TCL_DEFS_TCL_ONLY}"
	AC_SUBST(TCL_DEFS)

	dnl Needed for package installation
	if test "$prefix" = 'NONE' -a "$exec_prefix" = 'NONE' -a "${libdir}" = '${exec_prefix}/lib'; then
		TCL_PACKAGE_PATH="`echo "${TCL_PACKAGE_PATH}" | sed 's@  *$''@@' | awk '{ print [$]1 }'`"
	else
		TCL_PACKAGE_PATH='${libdir}'
	fi
	AC_SUBST(TCL_PACKAGE_PATH)

	AC_SUBST(LIBS)
])
dnl Usage:
dnl    DC_TEST_SHOBJFLAGS(shobjflags, shobjldflags, action-if-not-found)
dnl
AC_DEFUN([DC_TEST_SHOBJFLAGS], [
  AC_SUBST(SHOBJFLAGS)
  AC_SUBST(SHOBJCPPFLAGS)
  AC_SUBST(SHOBJLDFLAGS)

  OLD_LDFLAGS="$LDFLAGS"
  OLD_CFLAGS="$CFLAGS"
  OLD_CPPFLAGS="$CPPFLAGS"

  SHOBJFLAGS=""
  SHOBJCPPFLAGS=""
  SHOBJLDFLAGS=""

  CFLAGS="$OLD_CFLAGS $1"
  CPPFLAGS="$OLD_CPPFLAGS $2"
  LDFLAGS="$OLD_LDFLAGS $3"

  AC_TRY_LINK([#include <stdio.h>
int unrestst(void);], [ printf("okay\n"); unrestst(); return(0); ], [ SHOBJFLAGS="$1"; SHOBJCPPFLAGS="$2"; SHOBJLDFLAGS="$3" ], [
    LDFLAGS="$OLD_LDFLAGS"
    CFLAGS="$OLD_CFLAGS"
    CPPFLAGS="$OLD_CPPFLAGS"
    $4
  ])

  LDFLAGS="$OLD_LDFLAGS"
  CFLAGS="$OLD_CFLAGS"
  CPPFLAGS="$OLD_CPPFLAGS"
])

AC_DEFUN([DC_GET_SHOBJFLAGS], [
  AC_SUBST(SHOBJFLAGS)
  AC_SUBST(SHOBJCPPFLAGS)
  AC_SUBST(SHOBJLDFLAGS)

  DC_CHK_OS_INFO

  AC_MSG_CHECKING(how to create shared objects)

  if test -z "$SHOBJFLAGS" -a -z "$SHOBJLDFLAGS" -a -z "$SHOBJCPPFLAGS"; then
    DC_TEST_SHOBJFLAGS([-fPIC], [-DPIC], [-shared], [
      DC_TEST_SHOBJFLAGS([-fPIC], [-DPIC], [-shared -mimpure-text], [
        DC_TEST_SHOBJFLAGS([-fPIC], [-DPIC], [-shared -rdynamic -Wl,-G,-z,textoff], [
          DC_TEST_SHOBJFLAGS([-fPIC], [-DPIC], [-shared -Wl,-G], [
            DC_TEST_SHOBJFLAGS([-fPIC], [-DPIC], [-shared -dynamiclib -flat_namespace -undefined suppress -bind_at_load], [
              DC_TEST_SHOBJFLAGS([-fPIC], [-DPIC], [-dynamiclib -flat_namespace -undefined suppress -bind_at_load], [
                DC_TEST_SHOBJFLAGS([-fPIC], [-DPIC], [-Wl,-dynamiclib -Wl,-flat_namespace -Wl,-undefined,suppress -Wl,-bind_at_load], [
                  DC_TEST_SHOBJFLAGS([-fPIC], [-DPIC], [-dynamiclib -flat_namespace -undefined suppress], [
                    DC_TEST_SHOBJFLAGS([-fPIC], [-DPIC], [-dynamiclib], [
                      AC_MSG_RESULT(cant)
                      AC_MSG_ERROR([We are unable to make shared objects.])
                    ])
                  ])
                ])
              ])
            ])
          ])
        ])
      ])
    ])
  fi

  AC_MSG_RESULT($SHOBJCPPFLAGS $SHOBJFLAGS $SHOBJLDFLAGS)

  DC_SYNC_SHLIBOBJS
])

AC_DEFUN([DC_SYNC_SHLIBOBJS], [
  AC_SUBST(SHLIBOBJS)
  SHLIBOBJS=""
  for obj in $LIB@&t@OBJS; do
    SHLIBOBJS="$SHLIBOBJS `echo $obj | sed 's/\.o$/_shr.o/g'`"
  done
])

AC_DEFUN([DC_SYNC_RPATH], [
	AC_ARG_ENABLE([rpath], AS_HELP_STRING([--disable-rpath], [disable setting of rpath]), [
		if test "$enableval" = 'no'; then
			set_rpath='no'
		else
			set_rpath='yes'
		fi
	], [
		if test "$cross_compiling" = 'yes'; then
			set_rpath='no'
		else
			ifelse($1, [], [
				set_rpath='yes'
			], [
				set_rpath='$1'
			])
		fi
	])

	if test "$set_rpath" = 'yes'; then
		OLD_LDFLAGS="$LDFLAGS"

		AC_CACHE_CHECK([how to set rpath], [rsk_cv_link_set_rpath], [
			AC_LANG_PUSH(C)
			for tryrpath in "-Wl,-rpath" "-Wl,--rpath" "-Wl,-R"; do
				LDFLAGS="$OLD_LDFLAGS $tryrpath -Wl,/tmp"
				AC_LINK_IFELSE([AC_LANG_PROGRAM([], [ return(0); ])], [
					rsk_cv_link_set_rpath="$tryrpath"
					break
				])
			done
			AC_LANG_POP(C)
			unset tryrpath
		])

		LDFLAGS="$OLD_LDFLAGS"
		unset OLD_LDFLAGS

		if test -n "$rsk_cv_link_set_rpath"; then
			ADDLDFLAGS=""
			for opt in $LDFLAGS $LIBS; do
				if echo "$opt" | grep '^-L' >/dev/null; then
					rpathdir="`echo "$opt" | sed 's@^-L *@@'`"
					ADDLDFLAGS="$ADDLDFLAGS $rsk_cv_link_set_rpath -Wl,$rpathdir"
				fi
			done
			unset opt

			LDFLAGS="$LDFLAGS $ADDLDFLAGS"

			unset ADDLDFLAGS
		fi
	fi
])

AC_DEFUN([DC_CHK_OS_INFO], [
	AC_CANONICAL_HOST
	AC_SUBST(SHOBJEXT)
	AC_SUBST(SHOBJFLAGS)
	AC_SUBST(SHOBJCPPFLAGS)
	AC_SUBST(SHOBJLDFLAGS)
	AC_SUBST(CFLAGS)
	AC_SUBST(CPPFLAGS)
	AC_SUBST(AREXT)

	if test "$dc_cv_dc_chk_os_info_called" != '1'; then
		dc_cv_dc_chk_os_info_called='1'

		AC_MSG_CHECKING(host operating system)
		AC_MSG_RESULT($host_os)

		SHOBJEXT="so"
		AREXT="a"

		case $host_os in
			darwin*)
				SHOBJEXT="dylib"
				;;
			hpux*)
				case "$host_cpu" in
					ia64)
						SHOBJEXT="so"
						;;
					*)
						SHOBJEXT="sl"
						;;
				esac
				;;
			mingw32|mingw32msvc*)
				SHOBJEXT="dll"
				CFLAGS="$CFLAGS -mms-bitfields"
				CPPFLAGS="$CPPFLAGS -mms-bitfields"
				SHOBJCPPFLAGS="-DPIC"
				SHOBJLDFLAGS='-shared -Wl,--dll -Wl,--enable-auto-image-base -Wl,--output-def,$[@].def,--out-implib,$[@].a'
				;;
			msvc)
				SHOBJEXT="dll"
				AREXT='lib'
				CFLAGS="$CFLAGS -nologo"
				SHOBJCPPFLAGS='-DPIC'
				SHOBJLDFLAGS='/LD /LINK /NODEFAULTLIB:MSVCRT'
				;;
			cygwin*)
				SHOBJEXT="dll"
				SHOBJFLAGS="-fPIC"
				SHOBJCPPFLAGS="-DPIC"
				CFLAGS="$CFLAGS -mms-bitfields"
				CPPFLAGS="$CPPFLAGS -mms-bitfields"
				SHOBJLDFLAGS='-shared -Wl,--enable-auto-image-base -Wl,--output-def,$[@].def,--out-implib,$[@].a'
				;;
		esac
	fi
])

AC_DEFUN([SHOBJ_SET_SONAME], [
	SAVE_LDFLAGS="$LDFLAGS"

	AC_MSG_CHECKING([how to specify soname])

	for try in "-Wl,--soname,$1" "Wl,-install_name,$1" '__fail__'; do
		LDFLAGS="$SAVE_LDFLAGS"

		if test "${try}" = '__fail__'; then
			AC_MSG_RESULT([can't])

			break
		fi

		LDFLAGS="${LDFLAGS} ${try}"
		AC_TRY_LINK([void TestTest(void) { return; }], [], [
			LDFLAGS="${SAVE_LDFLAGS}"
			SHOBJLDFLAGS="${SHOBJLDFLAGS} ${try}"

			AC_MSG_RESULT([$try])

			break
		])
	done

	AC_SUBST(SHOBJLDFLAGS)
])

dnl $1 = Description to show user
dnl $2 = Libraries to link to
dnl $3 = Variable to update (optional; default LIBS)
dnl $4 = Action to run if found
dnl $5 = Action to run if not found
AC_DEFUN([SHOBJ_DO_STATIC_LINK_LIB], [
        ifelse($3, [], [
                define([VAR_TO_UPDATE], [LIBS])
        ], [
                define([VAR_TO_UPDATE], [$3])
        ])  


	AC_MSG_CHECKING([for how to statically link to $1])

	trylink_ADD_LDFLAGS=''
	for arg in $VAR_TO_UPDATE; do
		case "${arg}" in
			-L*)
				trylink_ADD_LDFLAGS="${arg}"
				;;
		esac
	done

	SAVELIBS="$LIBS"
	staticlib=""
	found="0"
	dnl HP/UX uses -Wl,-a,archive ... -Wl,-a,shared_archive
	dnl Linux and Solaris us -Wl,-Bstatic ... -Wl,-Bdynamic
	AC_LANG_PUSH([C])
	for trylink in "-Wl,-a,archive $2 -Wl,-a,shared_archive" "-Wl,-Bstatic $2 -Wl,-Bdynamic" "$2"; do
		if echo " ${LDFLAGS} " | grep ' -static ' >/dev/null; then
			if test "${trylink}" != "$2"; then
				continue
			fi
		fi

		LIBS="${SAVELIBS} ${trylink_ADD_LDFLAGS} ${trylink}"

		AC_LINK_IFELSE([AC_LANG_PROGRAM([], [])], [
			staticlib="${trylink}"
			found="1"

			break
		])
	done
	AC_LANG_POP([C])
	LIBS="${SAVELIBS}"

	if test "${found}" = "1"; then
		new_RESULT=''
		SAVERESULT="$VAR_TO_UPDATE"
		for lib in ${SAVERESULT}; do
			addlib='1'
			for removelib in $2; do
				if test "${lib}" = "${removelib}"; then
					addlib='0'
					break
				fi
			done

			if test "$addlib" = '1'; then
				new_RESULT="${new_RESULT} ${lib}"
			fi
		done
		VAR_TO_UPDATE="${new_RESULT} ${staticlib}"

		AC_MSG_RESULT([${staticlib}])

		$4
	else
		AC_MSG_RESULT([cant])

		$5
	fi
])

AC_DEFUN([DC_SETUP_STABLE_API], [
	VERSIONSCRIPT="$1"
	SYMFILE="$2"

	DC_FIND_STRIP_AND_REMOVESYMS([$SYMFILE])
	DC_SETVERSIONSCRIPT([$VERSIONSCRIPT], [$SYMFILE])
])


AC_DEFUN([DC_SETVERSIONSCRIPT], [
	VERSIONSCRIPT="$1"
	SYMFILE="$2"
	TMPSYMFILE="${SYMFILE}.tmp"
	TMPVERSIONSCRIPT="${VERSIONSCRIPT}.tmp"

	echo "${SYMPREFIX}Test_Symbol" > "${TMPSYMFILE}"

	echo '{' > "${TMPVERSIONSCRIPT}"
	echo '	local:' >> "${TMPVERSIONSCRIPT}"
	echo "		${SYMPREFIX}Test_Symbol;" >> "${TMPVERSIONSCRIPT}"
	echo '};' >> "${TMPVERSIONSCRIPT}"

	SAVE_LDFLAGS="${LDFLAGS}"

	AC_MSG_CHECKING([for how to set version script])

	for tryaddldflags in "-Wl,--version-script,${TMPVERSIONSCRIPT}" "-Wl,-exported_symbols_list,${TMPSYMFILE}"; do
		LDFLAGS="${SAVE_LDFLAGS} ${tryaddldflags}"
		AC_TRY_LINK([void Test_Symbol(void) { return; }], [], [
			addldflags="`echo "${tryaddldflags}" | sed 's/\.tmp$//'`"

			break
		])
	done

	rm -f "${TMPSYMFILE}"
	rm -f "${TMPVERSIONSCRIPT}"

	LDFLAGS="${SAVE_LDFLAGS}"

	if test -n "${addldflags}"; then
		SHOBJLDFLAGS="${SHOBJLDFLAGS} ${addldflags}"

		AC_MSG_RESULT($addldflags)
	else
		AC_MSG_RESULT([don't know])
	fi

	AC_SUBST(SHOBJLDFLAGS)
])

AC_DEFUN([DC_FIND_STRIP_AND_REMOVESYMS], [
	SYMFILE="$1"

	dnl Determine how to strip executables
	AC_CHECK_TOOLS(OBJCOPY, objcopy gobjcopy, [false])
	AC_CHECK_TOOLS(STRIP, strip gstrip, [false])

	if test "x${STRIP}" = "xfalse"; then
		STRIP="${OBJCOPY}"
	fi

	WEAKENSYMS='true'
	REMOVESYMS='true'
	SYMPREFIX=''

	case $host_os in
		darwin*)
			SYMPREFIX="_"
			REMOVESYMS="${STRIP} -u -x"
			;;
		*)
			if test "x${OBJCOPY}" != "xfalse"; then
				WEAKENSYMS="${OBJCOPY} --keep-global-symbols=${SYMFILE}"
				REMOVESYMS="${OBJCOPY} --discard-all"
			elif test "x${STRIP}" != "xfalse"; then
				REMOVESYMS="${STRIP} -x"
			fi
			;;
	esac

	AC_SUBST(WEAKENSYMS)
	AC_SUBST(REMOVESYMS)
	AC_SUBST(SYMPREFIX)
])
# ===========================================================================
#  https://www.gnu.org/software/autoconf-archive/ax_check_compile_flag.html
# ===========================================================================
#
# SYNOPSIS
#
#   AX_CHECK_COMPILE_FLAG(FLAG, [ACTION-SUCCESS], [ACTION-FAILURE], [EXTRA-FLAGS], [INPUT])
#
# DESCRIPTION
#
#   Check whether the given FLAG works with the current language's compiler
#   or gives an error.  (Warnings, however, are ignored)
#
#   ACTION-SUCCESS/ACTION-FAILURE are shell commands to execute on
#   success/failure.
#
#   If EXTRA-FLAGS is defined, it is added to the current language's default
#   flags (e.g. CFLAGS) when the check is done.  The check is thus made with
#   the flags: "CFLAGS EXTRA-FLAGS FLAG".  This can for example be used to
#   force the compiler to issue an error when a bad flag is given.
#
#   INPUT gives an alternative input source to AC_COMPILE_IFELSE.
#
#   NOTE: Implementation based on AX_CFLAGS_GCC_OPTION. Please keep this
#   macro in sync with AX_CHECK_{PREPROC,LINK}_FLAG.
#
# LICENSE
#
#   Copyright (c) 2008 Guido U. Draheim <guidod@gmx.de>
#   Copyright (c) 2011 Maarten Bosmans <mkbosmans@gmail.com>
#
#   Copying and distribution of this file, with or without modification, are
#   permitted in any medium without royalty provided the copyright notice
#   and this notice are preserved.  This file is offered as-is, without any
#   warranty.

#serial 6

AC_DEFUN([AX_CHECK_COMPILE_FLAG],
[AC_PREREQ(2.64)dnl for _AC_LANG_PREFIX and AS_VAR_IF
AS_VAR_PUSHDEF([CACHEVAR],[ax_cv_check_[]_AC_LANG_ABBREV[]flags_$4_$1])dnl
AC_CACHE_CHECK([whether _AC_LANG compiler accepts $1], CACHEVAR, [
  ax_check_save_flags=$[]_AC_LANG_PREFIX[]FLAGS
  _AC_LANG_PREFIX[]FLAGS="$[]_AC_LANG_PREFIX[]FLAGS $4 $1"
  AC_COMPILE_IFELSE([m4_default([$5],[AC_LANG_PROGRAM()])],
    [AS_VAR_SET(CACHEVAR,[yes])],
    [AS_VAR_SET(CACHEVAR,[no])])
  _AC_LANG_PREFIX[]FLAGS=$ax_check_save_flags])
AS_VAR_IF(CACHEVAR,yes,
  [m4_default([$2], :)],
  [m4_default([$3], :)])
AS_VAR_POPDEF([CACHEVAR])dnl
])dnl AX_CHECK_COMPILE_FLAGS
# ===========================================================================
#    https://www.gnu.org/software/autoconf-archive/ax_check_link_flag.html
# ===========================================================================
#
# SYNOPSIS
#
#   AX_CHECK_LINK_FLAG(FLAG, [ACTION-SUCCESS], [ACTION-FAILURE], [EXTRA-FLAGS], [INPUT])
#
# DESCRIPTION
#
#   Check whether the given FLAG works with the linker or gives an error.
#   (Warnings, however, are ignored)
#
#   ACTION-SUCCESS/ACTION-FAILURE are shell commands to execute on
#   success/failure.
#
#   If EXTRA-FLAGS is defined, it is added to the linker's default flags
#   when the check is done.  The check is thus made with the flags: "LDFLAGS
#   EXTRA-FLAGS FLAG".  This can for example be used to force the linker to
#   issue an error when a bad flag is given.
#
#   INPUT gives an alternative input source to AC_LINK_IFELSE.
#
#   NOTE: Implementation based on AX_CFLAGS_GCC_OPTION. Please keep this
#   macro in sync with AX_CHECK_{PREPROC,COMPILE}_FLAG.
#
# LICENSE
#
#   Copyright (c) 2008 Guido U. Draheim <guidod@gmx.de>
#   Copyright (c) 2011 Maarten Bosmans <mkbosmans@gmail.com>
#
#   Copying and distribution of this file, with or without modification, are
#   permitted in any medium without royalty provided the copyright notice
#   and this notice are preserved.  This file is offered as-is, without any
#   warranty.

#serial 6

AC_DEFUN([AX_CHECK_LINK_FLAG],
[AC_PREREQ(2.64)dnl for _AC_LANG_PREFIX and AS_VAR_IF
AS_VAR_PUSHDEF([CACHEVAR],[ax_cv_check_ldflags_$4_$1])dnl
AC_CACHE_CHECK([whether the linker accepts $1], CACHEVAR, [
  ax_check_save_flags=$LDFLAGS
  LDFLAGS="$LDFLAGS $4 $1"
  AC_LINK_IFELSE([m4_default([$5],[AC_LANG_PROGRAM()])],
    [AS_VAR_SET(CACHEVAR,[yes])],
    [AS_VAR_SET(CACHEVAR,[no])])
  LDFLAGS=$ax_check_save_flags])
AS_VAR_IF(CACHEVAR,yes,
  [m4_default([$2], :)],
  [m4_default([$3], :)])
AS_VAR_POPDEF([CACHEVAR])dnl
])dnl AX_CHECK_LINK_FLAGS
