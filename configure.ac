# -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.
AC_PREREQ([2.69])
AC_INIT([eventbus], [0.0.1], [marco.pensallorto@gmail.com])

# where to look for install.sh and other helpers
AC_CONFIG_SRCDIR([eventbus/eventbus.c])
AC_CONFIG_AUX_DIR([helpers])
AC_CONFIG_MACRO_DIR([m4])
AM_CONFIG_HEADER([config.h])

AM_INIT_AUTOMAKE([-Wall -Werror foreign])

# Checks for programs. These may set default variables, such as CFLAGS
AC_PROG_CPP
AC_PROG_CC_C99
AM_PROG_AR
LT_INIT

# Use the C language and compiler for the following checks
AC_LANG([C])

# (optional) Doxygen
AC_CHECK_PROGS([DOXYGEN], [doxygen])
if test -z "$DOXYGEN";
then AC_MSG_WARN([Doxygen not found - continuing without Doxygen support])
fi

AC_DEFINE_UNQUOTED(PACKAGE_BUILD_DATE, "`date -u`", Build date)

# Library informations
dnl AC_DEFINE_UNQUOTED(PROGRAM_NAME, "yasmv", Program Name)
dnl AC_DEFINE_UNQUOTED(PROGRAM_VERSION, "0.0.5", Program Version)
dnl AC_DEFINE_UNQUOTED(PROGRAM_BUILD_DATE, "`date -u`", Program build date)
dnl AC_DEFINE_UNQUOTED(PROGRAM_WEBSITE, "https://github.com/mwolf76", Program Website)
dnl AC_DEFINE_UNQUOTED(PROGRAM_EMAIL, "marco.pensallorto@gmail.com", Program Email)
dnl AC_DEFINE_UNQUOTED(PROGRAM_BUGREPORT, "Please report bugs to <marco.pensallorto@gmail.com>", Program bug message)

# Checks for programs.
dnl AC_PROG_INSTALL
dnl AC_PROG_LN_S
dnl AC_PROG_MAKE_SET
dnl AC_PROG_LIBTOOL
dnl AC_PROG_CC
dnl AM_PROG_CC_C_O
dnl AC_PROG_CPP
dnl AC_PROG_CXX
dnl AC_PROG_CXXCPP

if test "x$ac_cv_prog_CPP" != "x"; then
AC_DEFINE(HAVE_CPP, 1, Have preprocessor)
AC_DEFINE_UNQUOTED(PROG_CPP, "$ac_cv_prog_CPP -x c", Preprocessor call string)
else
AC_DEFINE(HAVE_CPP, 0, Have preprocessor)
fi

# Checks for libraries.
AC_CHECK_LIB([m], [pthread], [jansson])

AM_CONDITIONAL([HAVE_DOXYGEN],
[test -n "$DOXYGEN"])AM_COND_IF([HAVE_DOXYGEN], [AC_CONFIG_FILES([doc/Doxyfile])])

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([float.h limits.h memory.h stddef.h stdlib.h string.h \
  sys/ioctl.h sys/param.h sys/time.h sys/resource.h unistd.h signal.h sys/signal.h \
  errno.h regex.h inttypes.h])

# This is for malloc:
AC_CHECK_HEADER(sys/types.h)
AC_CHECK_HEADER(malloc.h,
        [ AC_DEFINE(HAVE_MALLOC_H, 1, Defined to 1 if the system provides malloc.h)],
        [ AC_CHECK_HEADER(sys/malloc.h,
            [AC_DEFINE(HAVE_SYS_MALLOC_H, 1, Defined to 1 if the system provides sys/malloc.h)],
            [],
            [
              #if HAVE_SYS_TYPES_H
              # include <sys/types.h>
              #endif
            ]
            )],
        [
         #if HAVE_SYS_TYPES_H
         # include <sys/types.h>
         #endif
        ])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_SIZE_T

# Checks for program functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([floor memmove memset pow strcasecmp strchr \
                strrchr strstr strtol, random srandom getpid \
                mkstemp mktemp tmpnam getenv setvbuf system popen isatty])

# Add debug support
AC_ARG_ENABLE(debug,
  AS_HELP_STRING(
    [--enable-debug],
    [enable debugging, default: no]),
    [case "${enableval}" in
      yes) debug=true ;;
      no)  debug=false ;;
      *)   AC_MSG_ERROR([bad value ${enableval} for --enable-debug]) ;;
    esac],
    [debug=false])
AM_CONDITIONAL(DEBUG, test x"$debug" = x"true")
AM_COND_IF(DEBUG,
    AC_DEFINE(DEBUG, 1, [Define to 0 if this is a release build]),
    AC_DEFINE(DEBUG, 0, [Define to 1 or higher if this is a debug build]))

AC_CONFIG_FILES([Makefile
		 eventbus/Makefile
		 doc/Makefile
		 test/Makefile])
AC_OUTPUT
