## $NetBSD: Makefile.am,v 1.3 2020/07/05 11:11:10 lukem Exp $

noinst_LTLIBRARIES = libnetbsd.la

libnetbsd_la_SOURCES = libnetbsd.c

CPPFLAGS = \
	-I$(srcdir) \
	-I$(top_srcdir) \
	-I$(top_builddir)

libnetbsd_la_LIBADD = $(LTLIBOBJS)

EXTRA_DIST = \
	fseeko.c \
	ftpglob.h \
	ftpvis.h
