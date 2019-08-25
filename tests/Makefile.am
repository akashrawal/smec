
#Common
AM_CFLAGS = -I$(top_srcdir) $(SSC_CFLAGS)
LDADD = ../sme/libsme.la -lm $(SSC_LIBS) 

#Log compiler
nodist_check_SCRIPTS = logcc.sh
DISTCLEANFILES = logcc.sh
EXTRA_DIST = logcc.sh.in
LOG_COMPILER = sh $(builddir)/logcc.sh

#Unit tests
check_PROGRAMS = test_channel \
				 test_msg

#All tests
TESTS = $(check_PROGRAMS)
