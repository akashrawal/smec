lib_LTLIBRARIES = libsme.la

#TODO: Adapt these
#Generated source files
#BUILT_SOURCES = simple_router_declares.h simple_router_defines.h
#CLEANFILES = simple_router_declares.h simple_router_defines.h 

#simple_router_declares.h simple_router_defines.h: simple_router.mdl
#	$(MDLC) $(srcdir)/simple_router.mdl
#
#EXTRA_DIST = simple_router.mdl

sme_c = channel.c

sme_h = incl.h \
		channel.h


libsme_la_SOURCES = $(sme_c) $(sme_h)
#nodist_libsme_la_SOURCES = 
                          
libsme_la_CFLAGS = -Wall -I$(top_builddir)
libsme_la_LIBADD = $(MMC_LIBS) $(SSC_LIBS) -levent_core

smeincludedir = $(includedir)/sme
smeinclude_HEADERS = $(sme_h)
