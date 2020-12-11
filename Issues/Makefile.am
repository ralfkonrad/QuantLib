
AM_CPPFLAGS = -I${top_builddir} -I${top_srcdir}

ISSUES = \
    930CallableBond/930CallableBond

if AUTO_ISSUES
bin_PROGRAMS = $(ISSUES)
TESTS = $(ISSUES)
else
noinst_PROGRAMS = $(ISSUES)
endif

LDADDS = ../ql/libQuantLib.la ${BOOST_THREAD_LIB}

930CallableBond_930CallableBond_SOURCES = 930CallableBond/930CallableBond.cpp
930CallableBond_930CallableBond_LDADD = $(LDADDS)

EXTRA_DIST = CMakeLists.txt

dist-hook:
	for i in $(notdir $(ISSUES)); do \
	    cp $(srcdir)/$$i/$$i.vcxproj $(distdir)/$$i/ && \
	    cp $(srcdir)/$$i/$$i.vcxproj.filters $(distdir)/$$i/ && \
	    cp $(srcdir)/$$i/CMakeLists.txt $(distdir)/$$i/ \
	; done

.PHONY: check-issues

check-issues: $(ISSUES)
	for i in $(ISSUES); do echo "========" && echo $$i && $$i || break -1 ; done
