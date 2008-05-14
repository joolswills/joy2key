dnl Process this file with autoconf to produce a configure script.
AC_INIT(joy2key.c)
AM_INIT_AUTOMAKE(joy2key, 1.6.3)
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

AC_ARG_ENABLE( X, [ AC_HELP_STRING([--enable-X], [Compile in X features default=enabled]) ], Xbuild=$enableval )
if test "$Xbuild" != "no"
then
	AC_DEFINE(ENABLE_X)
	if test x$x_libraries != x
	then
		export X_Stuff="-L$x_libraries -lX11"
	else
		export X_Stuff="-lX11"
	fi
fi

AC_SUBST(X_Stuff)
AC_SUBST(x_headers)
AC_OUTPUT(Makefile)
