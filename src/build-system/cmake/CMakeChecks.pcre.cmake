############################################################################
#
# PCRE additions
#
# Perl-Compatible Regular Expressions (PCRE)

find_package(PCRE)
set(PCRE_INCLUDE ${PCRE_INCLUDE_DIR})
set(PCRE_LIBS ${PCRE_LIBRARIES})
if (PCRE_FOUND)
    set(HAVE_LIBPCRE 1)
endif()

if (WIN32 AND HAVE_LIBPCRE)
    set(USE_LOCAL_PCRE TRUE)
endif()

if (HAVE_LIBPCRE AND NOT USE_LOCAL_PCRE)
    set(PCRE_LIBS -lpcre)
else(HAVE_LIBPCRE AND NOT USE_LOCAL_PCRE)
    set(USE_LOCAL_PCRE 1 CACHE INTERNAL "Using local PCRE due to system library absence")
    set(PCRE_LIBS regexp)
endif(HAVE_LIBPCRE AND NOT USE_LOCAL_PCRE)

