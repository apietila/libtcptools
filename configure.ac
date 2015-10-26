

AC_INIT([libtcptools],[1.0.1],[contact@wand.net.nz],[libtcptools])

AC_CONFIG_SRCDIR(lib/sessionmanager.c)
AM_INIT_AUTOMAKE

m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

AC_CONFIG_FILES([Makefile lib/Makefile])

AC_GNU_SOURCE
AM_CONFIG_HEADER([config.h])

AC_CHECK_LIB([trace], [trace_create_packet],,trace_found=0)

AC_PROG_CC
AC_PROG_INSTALL

AC_CONFIG_LIBOBJ_DIR(lib)

AC_C_CONST
AC_C_INLINE
AC_TYPE_SIZE_T
AC_HEADER_TIME
AC_SYS_LARGEFILE

if test "$GCC" = "yes"; then
        CFLAGS="$CFLAGS -Wall -W"
fi

AC_PROG_LIBTOOL
AC_PROG_GCC_TRADITIONAL

AC_CHECK_SIZEOF([long int])

AC_HEADER_STDC
AC_CHECK_HEADERS(inttypes.h)

AC_CHECK_HEADERS(netinet/in.h)

if test "$trace_found" = 0; then
	AC_MSG_ERROR(Required library libtrace not found; use LDFLAGS to specify library location)
fi

AC_SUBST([ADD_LIBS])
AC_SUBST([ADD_LDFLAGS])
AC_SUBST([ADD_INCLS])

AC_OUTPUT

