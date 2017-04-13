############################################################################
#
# GnuTLS: headers and libs; TLS_* points to GNUTLS_* by preference.
#

#find_package doesn't find static libraries, so, the path to static gnutls is hardcoded
if (NOT GNUTLS_DISABLED)
  if (EXISTS "${NCBI_TOOLS_ROOT}/gnutls-3.4.0/")
    set(GNUTLS_FOUND true)
    set(GNUTLS_INCLUDE "${NCBI_TOOLS_ROOT}/gnutls-3.4.0/include")
    set(GNUTLS_LIBS "-L${NCBI_TOOLS_ROOT}/gnutls-3.4.0/lib -lgnutls -lz -lidn -lrt -L/netopt/ncbi_tools64/nettle-3.1.1/lib64 -lhogweed -lnettle -L/netopt/ncbi_tools64/gmp-6.0.0a/lib64 -lgmp")
  else()
    find_package(GnuTLS)
    set(GNUTLS_INCLUDE ${GNUTLS_INCLUDE_DIR})
    set(GNUTLS_LIBS ${GNUTLS_LIBRARIES})
  endif()
  if (GNUTLS_FOUND)
    set(GNUTLS_COMMENT ${GNUTLS_LIBS})
  else()
    set(GNUTLS_COMMENT "Not found")
  endif()
else()
  set(GNUTLS_COMMENT "disabled by configuration")
endif()
