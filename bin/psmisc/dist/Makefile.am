
AM_CPPFLAGS = \
	      -Wall \
	      -DLOCALEDIR=\"$(localedir)\" \
	      -I$(top_builddir)/src \
	      @HARDEN_CFLAGS@

AM_LDFLAGS = @HARDEN_LDFLAGS@

ACLOCAL_AMFLAGS = -I m4

SUBDIRS = \
	  doc \
	  po \
	  icons \
	  man-po \
	  testsuite

bin_PROGRAMS = \
	       src/killall \
	       src/pslog \
	       src/prtstat \
	       src/pstree

if WANT_FUSER
  bin_PROGRAMS += src/fuser
endif
if WANT_PEEKFD_I386
  bin_PROGRAMS += src/peekfd
  AM_CPPFLAGS += -DI386
endif
if WANT_PEEKFD_X86_64
  bin_PROGRAMS += src/peekfd
  AM_CPPFLAGS += -DX86_64
endif
if WANT_PEEKFD_PPC
  bin_PROGRAMS += src/peekfd
  AM_CPPFLAGS += -DPPC
endif
if WANT_PEEKFD_ARM
  bin_PROGRAMS += src/peekfd
  AM_CPPFLAGS += -DARM
endif

if WANT_PEEKFD_ARM64
  bin_PROGRAMS += src/peekfd
  AM_CPPFLAGS += -DARM64
endif

if WANT_PEEKFD_MIPS
  bin_PROGRAMS += src/peekfd
  AM_CPPFLAGS += -DMIPS
endif
if WANT_PEEKFD_M68K
  bin_PROGRAMS += src/peekfd
  AM_CPPFLAGS += -DM68K
endif

src_fuser_SOURCES = \
		    src/fuser.c \
		    src/comm.h \
		    src/signals.c \
		    src/signals.h \
		    src/i18n.h \
		    src/fuser.h \
		    src/lists.h

if WANT_TIMEOUT_STAT
src_fuser_SOURCES += src/timeout.c src/timeout.h
endif
src_fuser_LDADD = @LIBINTL@
src_killall_SOURCES = src/killall.c src/comm.h src/signals.c src/signals.h src/i18n.h
src_killall_LDADD = @LIBINTL@ @DL_LIB@
src_peekfd_SOURCES = src/peekfd.c
src_peekfd_LDADD = @LIBINTL@
src_pslog_SOURCES = src/pslog.c
src_pstree_SOURCES = src/pstree.c src/comm.h src/i18n.h
src_pstree_LDADD = @LIBINTL@ @TERMCAP_LIB@
src_prtstat_SOURCES = src/prtstat.c src/prtstat.h
src_prtstat_LDADD = @LIBINTL@
nodist_src_killall_SOURCES = signames.h

BUILT_SOURCES = src/signames.h .version

EXTRA_DIST = src/signames.c README.md misc/git-version-gen

CLEANFILES = src/signames.h .version

src/signames.h: src/signames.c Makefile src/$(am__dirstamp)
		export LC_ALL=C ; \
		@CPP@ -dM $< |\
		tr -s '\t ' ' ' | sort -n -k 3 | sed \
	's:#define SIG\([A-Z][A-Z]*[0-9]*\) \([0-9][0-9]*\).*$\:{\ \2,"\1" },:p;d' | \
		  grep -v '[0-9][0-9][0-9]' >$@ || \
		  { rm -f $@; exit 1; }
		grep '^{ 1,"HUP" },$$' $@ >/dev/null || \
		  { rm -f $@; exit 1; }

src/signals.c: src/signames.h

.version:
	echo $(VERSION) > $@-t && mv $@-t $@

install-exec-hook:
		cd $(DESTDIR)$(bindir) && \
			( [ -h pstree.x11 ]  || $(LN_S) pstree pstree.x11)
dist-hook:
	echo $(VERSION) > $(distdir)/.tarball-version

get-trans:
	rsync -Lrtvz  translationproject.org::tp/latest/psmisc/  po
	rsync -Lrtvz  translationproject.org::tp/latest/psmisc-man/  man-po
