#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([SME],[0.0.0],[],[sme],[https://github.com/akashrawal/smec])
AC_CONFIG_SRCDIR([sme/incl.h])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([-Wall -Werror silent-rules])
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])
AM_PROG_AR
LT_INIT

# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O
AC_PATH_PROG([SIDC], [sidc])

# Checks for libraries.
AC_CHECK_LIB([event_core], [event_base_new], [], [AC_MSG_ERROR(["could not find required library libevent_core"])])
PKG_CHECK_MODULES([MMC], [mmc >= 0.0.0])
PKG_CHECK_MODULES([SSC], [ssc >= 0.0.0])

# Checks for header files.
AC_CHECK_HEADERS([stddef.h stdint.h stdlib.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T
AC_TYPE_UINT32_T

# Checks for library functions.
AC_FUNC_ERROR_AT_LINE
AC_FUNC_MALLOC
AC_FUNC_REALLOC

AC_CONFIG_FILES([Makefile
                 data/Makefile
                 data/sme.pc
                 sme/Makefile
				 tests/Makefile
				 tests/logcc.sh
                 ])
AC_OUTPUT
