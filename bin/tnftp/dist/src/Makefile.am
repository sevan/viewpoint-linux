## $NetBSD: Makefile.am,v 1.5 2020/07/05 09:37:53 lukem Exp $

bin_PROGRAMS = tnftp

tnftp_SOURCES = \
	cmds.c \
	cmdtab.c \
	complete.c \
	domacro.c \
	fetch.c \
	ftp.c \
	main.c \
	progressbar.c \
	ruserpass.c \
	util.c

tnftp_CPPFLAGS = \
	-DHAVE_TNFTPD_H=1 \
	-D_DEFAULT_CONFDIR=\"${sysconfdir}\" \
	-I$(srcdir) \
	-I$(top_srcdir)/libnetbsd \
	-I$(top_srcdir) \
	-I$(top_builddir)

tnftp_LDADD = \
	../libnetbsd/libnetbsd.la 


if OPT_EDITCOMPLETE
if WITH_LOCAL_LIBEDIT
tnftp_CPPFLAGS += \
	-I$(top_srcdir)/libedit

tnftp_LDADD += \
	../libedit/libedit.la
else	# !WITH_LOCAL_LIBEDIT
tnftp_LDADD += -ledit
endif	# !WITH_LOCAL_LIBEDIT
endif	# !OPT_EDITCOMPLETE


if WITH_SSL
tnftp_SOURCES += \
	ssl.c

tnftp_CPPFLAGS += \
	$(OPENSSL_INCLUDES)

tnftp_LDADD += \
	$(OPENSSL_LDFLAGS) \
	$(OPENSSL_LIBS)
endif


man1_MANS = \
	tnftp.1

tnftp.1: ftp.1
	cp $(srcdir)/ftp.1 tnftp.1

CLEANFILES = \
	tnftp.1

EXTRA_DIST = \
	extern.h \
	ftp.1 \
	ftp_var.h \
	progressbar.h \
	ssl.h \
	version.h
