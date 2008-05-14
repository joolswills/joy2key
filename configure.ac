dnl Process this file with autoconf to produce a configure script.
AC_INIT(joy2key.c)
AM_INIT_AUTOMAKE(joy2key, 1.6.1)
AM_CONFIG_HEADER(config.h)
AC_PROG_MAKE_SET

dnl Checks for programs.
AC_PROG_CC
AC_ISC_POSIX

dnl Checks for library functions.
AC_PATH_X

dnl Checks for header files.
AC_STDC_HEADERS

dnl Check for site-specific configuration
AC_ARG_ENABLE(console, [  --disable-console       do not compile in console features],
              if test $enableval != no; then AC_DEFINE(ENABLE_CONSOLE) fi,
              AC_DEFINE(ENABLE_CONSOLE))

AC_ARG_ENABLE(X, [  --disable-X             do not compile in X features],
              if test $enableval != no; then AC_DEFINE(ENABLE_X) export xstuff="-L$x_libraries -lX11"; fi,
              AC_DEFINE(ENABLE_X) export X_Stuff="-L$x_libraries -lX11")

AC_SUBST(X_Stuff)
AC_SUBST(x_headers)
AC_OUTPUT(Makefile)
