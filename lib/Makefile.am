lib_LTLIBRARIES = libtcptools.la
include_HEADERS = bwest.h reordering.h rtthandshake.h hashtable.h \
		rttnsequence.h rtttimestamp.h sessionmanager.h tcpsession.h \
		queue.h rttmodule.h


libtcptools_la_SOURCES = bwest.c hashtable.c queue.c reordering.c \
			rtthandshake.c rttnsequence.c rtttimestamp.c \
			sessionmanager.c tcpsession.c
INCLUDES = @ADD_INCLS@
libtcptools_la_LIBADD = @ADD_LIBS@ @LTLIBOBJS@
libtcptools_la_LDFLAGS = -version-info 1:1:0 @ADD_LDFLAGS@

