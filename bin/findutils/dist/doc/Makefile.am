# Copyright (C) 1996-2021 Free Software Foundation, Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.
AM_CFLAGS = $(WARN_CFLAGS)

info_TEXINFOS = find.texi find-maint.texi
find_TEXINFOS = perm.texi parse-datetime.texi regexprops.texi fdl.texi
BUILT_SOURCES = dblocation.texi
nodist_find_TEXINFOS = dblocation.texi
find_maint_TEXINFOS = fdl.texi
MOSTLYCLEANFILES = find.cps
CLEANFILES = find.txt find_mono.html findutils.texi_html_node.tar.gz dblocation.texi \
	find_mono.html findutils.texi_html_node.tar.gz \
	find-info.tar.gz find.texi.tar.gz \
	find.txt  tmp-doc-install find_mono.html.gz
MAKEINFOTXT = $(MAKEINFO) --plaintext

find.txt: find.texi $(srcdir)/version.texi $(find_TEXINFOS)

# find.txt is a file which we need to know how to build
# because it gets put on the www.gnu.org website.
# This rule is derived from the .texi.html rule.
.texi.txt:
	rm -rf $(@:.txt=.tmp)
	if $(MAKEINFOTXT) $(AM_MAKEINFOTXTFLAGS) $(MAKEINFOFLAGS) -I $(srcdir) \
	 -o $(@:.txt=.tmp) $<; \
	then \
	  rm -rf $@; \
	  if test ! -d $(@:.txt=.tmp) && test -d $(@:.txt=); then \
	    mv $(@:.txt=) $@; else mv $(@:.txt=.tmp) $@; fi; \
	else \
	  if test ! -d $(@:.txt=.tmp) && test -d $(@:.txt=); then \
	    rm -rf $(@:.txt=); else rm -Rf $(@:.txt=.tmp) $@; fi; \
	  exit 1; \
	fi


# find_mono.html is a file which we need to know how to build
# because it gets put on the www.gnu.org website.
# This rule is derived from the generic .texi.html rule.
find_mono.html: find.texi
	rm -rf $(@:.html=.htp)
	if $(MAKEINFOHTML) --no-split $(AM_MAKEINFOHTMLFLAGS) $(MAKEINFOFLAGS) -I $(srcdir) \
	 -o $(@:.html=.htp) find.texi; \
	then \
	  rm -rf $@; \
	  if test ! -d $(@:.html=.htp) && test -d $(@:.html=); then \
	    mv $(@:.html=) $@; else mv $(@:.html=.htp) $@; fi; \
	else \
	  if test ! -d $(@:.html=.htp) && test -d $(@:.html=); then \
	    rm -rf $(@:.html=); else rm -Rf $(@:.html=.htp) $@; fi; \
	  exit 1; \
	fi


# findutils.texi_html_node.tar.gz is a file which we need to know
# how to build because it gets put on the www.gnu.org website.
# This rule depends on GNU tar, but it's principally used
# by the maintainer, and we don't need to build the file
# for "make all" or "make install" (or even "make check").
findutils.texi_html_node.tar.gz: find.html
	tar zcf $@ find.html


dblocation.texi: ../locate/dblocation.texi
	$(LN_S) ../locate/dblocation.texi $@

find-info.tar.gz:
	$(MKDIR_P) tmp-doc-install/info
	$(MAKE) $(AM_MAKEFLAGS) \
	  top_distdir="tmp-doc-install" distdir="tmp-doc-install/info" \
	  dist-info
	( cd tmp-doc-install && tar -c -f - info ) | GZIP=$(GZIP_ENV) gzip -c >| $@
	rm -rf tmp-doc-install/info
	rmdir tmp-doc-install

find.texi.tar.gz: $(TEXINFOS) $(find_TEXINFOS) $(nodist_find_TEXINFOS) $(info_TEXINFOS) $(find_maint_TEXINFOS)  $(srcdir)/version.texi $(srcdir)/versionmaint.texi Makefile
	$(MKDIR_P) tmp-doc-install/texi
	for f in $(TEXINFOS) $(find_TEXINFOS) $(info_TEXINFOS) $(find_maint_TEXINFOS)  version.texi versionmaint.texi ; \
	do cp $(srcdir)/"$$f" tmp-doc-install/texi/ || break; done && cp dblocation.texi tmp-doc-install/texi/
	( cd tmp-doc-install/texi/ && tar -c -f - *.texi ) | GZIP=$(GZIP_ENV) gzip -c >| $@
	rm -rf tmp-doc-install/texi
	rmdir tmp-doc-install
