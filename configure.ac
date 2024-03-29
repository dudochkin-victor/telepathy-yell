AC_PREREQ([2.59])

# Making releases:
#   set the new version number:
#     odd minor -> development series
#     even minor -> stable series
#     increment micro for each release within a series
#   set nano_version to 0
#   make the release, tag it
#   set nano_version to 1

m4_define([tp_yell_major_version], [0])
m4_define([tp_yell_minor_version], [0])
m4_define([tp_yell_micro_version], [1])
m4_define([tp_yell_nano_version], [0])

# If library source has changed since last release, increment revision
# If interfaces have been added, removed or changed since last release,
#  increment current and set revision to 0
# If interfaces have been added since last release, increment age
# If interfaces have been removed since last release, set age to 0

m4_define([tp_yell_lt_current], [0])
m4_define([tp_yell_lt_revision], [0])
m4_define([tp_yell_lt_age], [0])

# Some magic
m4_define([tp_yell_base_version],
          [tp_yell_major_version.tp_yell_minor_version.tp_yell_micro_version])
m4_define([tp_yell_version],
          [m4_if(tp_yell_nano_version, 0, [tp_yell_base_version],
              [tp_yell_base_version].[tp_yell_nano_version])])

AC_INIT([telepathy-yell], [tp_yell_version],
        [mailto:telepathy@lists.freedesktop.org])

AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([1.11 -Wall -Wno-portability foreign])
AM_CONFIG_HEADER(config.h)

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES])

dnl check for tools
AC_PROG_CC
AC_PROG_CC_STDC
AC_PROG_INSTALL
AC_PROG_LIBTOOL
AM_PROG_LIBTOOL
AM_PROG_MKDIR_P

LT_CURRENT=tp_yell_lt_current
LT_REVISION=tp_yell_lt_revision
LT_AGE=tp_yell_lt_age
AC_SUBST([LT_CURRENT])
AC_SUBST([LT_REVISION])
AC_SUBST([LT_AGE])

dnl optimizations, etc.
COMPILER_OPTIMISATIONS
COMPILER_COVERAGE
LINKER_OPTIMISATIONS
LINKER_VERSION_SCRIPT

dnl decide error flags
ifelse(tp_yell_nano_version, 0,
    [ official_release=yes ],
    [ official_release=no ])

TP_COMPILER_WARNINGS([ERROR_CFLAGS], [test "x$official_release" = xno],
  [all \
   extra \
   declaration-after-statement \
   shadow \
   strict-prototypes \
   missing-prototypes \
   sign-compare \
   nested-externs \
   pointer-arith \
   format-security \
   init-self],
  [missing-field-initializers \
   unused-parameter])
AC_SUBST([ERROR_CFLAGS])

ifelse(tp_yell_nano_version, 0,
    [ # version x.y.z - disable coding style checks by default
AC_ARG_ENABLE(coding-style-checks,
  AC_HELP_STRING([--enable-coding-style-checks],
                 [check coding style using grep]),
    [ENABLE_CODING_STYLE_CHECKS=$enableval], [ENABLE_CODING_STYLE_CHECKS=no] )
    ],
    [ # version x.y.z.1 - enable coding style checks by default
AC_ARG_ENABLE(coding-style-checks,
  AC_HELP_STRING([--disable-coding-style-checks],
                 [don't check coding style using grep]),
    [ENABLE_CODING_STYLE_CHECKS=$enableval], [ENABLE_CODING_STYLE_CHECKS=yes])
    ])

AC_SUBST([ENABLE_CODING_STYLE_CHECKS])

dnl shared library
AC_ARG_ENABLE(shared-library,
  AC_HELP_STRING([--disable-shared-library],
                 [disable producing a shared library to install]),
    [ENABLE_SHARED_LIBRARY=$enableval], [ENABLE_SHARED_LIBRARY=yes])

AM_CONDITIONAL(ENABLE_SHARED_LIBRARY, test "x$ENABLE_SHARED_LIBRARY" != "xno")

dnl Check for Glib
PKG_CHECK_MODULES(GLIB, [glib-2.0 >= 2.28, gobject-2.0 >= 2.26])

AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)

GLIB_GENMARSHAL=`$PKG_CONFIG --variable=glib_genmarshal glib-2.0`
AC_SUBST(GLIB_GENMARSHAL)

GLIB_MKENUMS=`$PKG_CONFIG --variable=glib_mkenums glib-2.0`
AC_SUBST(GLIB_MKENUMS)

dnl Check for D-Bus
PKG_CHECK_MODULES(DBUS, [dbus-1 >= 0.95, dbus-glib-1 >= 0.82])

AC_SUBST(DBUS_CFLAGS)
AC_SUBST(DBUS_LIBS)

dnl Check for D-Bus
PKG_CHECK_MODULES(TP_GLIB, [telepathy-glib >= 0.13.10])

AC_SUBST(TP_GLIB_CFLAGS)
AC_SUBST(TP_GLIB_LIBS)

dnl Check for code generation tools
XSLTPROC=
AC_CHECK_PROGS([XSLTPROC], [xsltproc])
if test -z "$XSLTPROC"; then
  AC_MSG_ERROR([xsltproc (from the libxslt source package) is required])
fi
AM_PATH_PYTHON([2.5])

AC_OUTPUT( Makefile \
	   telepathy-yell/Makefile \
	   telepathy-yell/telepathy-yell.pc \
	   telepathy-yell/telepathy-yell-uninstalled.pc \
	   tools/Makefile \
	   spec/Makefile \
	   m4/Makefile
)
