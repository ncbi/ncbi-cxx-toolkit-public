
############################################################################
#
# OpenSSL, GnuTLS: headers and libs; TLS_* points to GNUTLS_* by preference.
# FIXME: replace with native CMake check
#
set(GCRYPT_INCLUDE  ${NCBI_TOOLS_ROOT}/libgcrypt-1.4.3/include 
                    ${NCBI_TOOLS_ROOT}/libgpg-error-1.6/include)
set(GCRYPT_LIBS     -L${NCBI_TOOLS_ROOT}/libgcrypt-1.4.3/GCC401-ReleaseMT64/lib -Wl,-rpath,/opt/ncbi/64/libgcrypt-1.4.3/GCC401-ReleaseMT64/lib:${NCBI_TOOLS_ROOT}/libgcrypt-1.4.3/GCC401-ReleaseMT64/lib -lgcrypt -L${NCBI_TOOLS_ROOT}/libgpg-error-1.6/GCC401-ReleaseMT64/lib -Wl,-rpath,/opt/ncbi/64/libgpg-error-1.6/GCC401-ReleaseMT64/lib:${NCBI_TOOLS_ROOT}/libgpg-error-1.6/GCC401-ReleaseMT64/lib -lgpg-error -lz)
set(OPENSSL_INCLUDE)
set(OPENSSL_LIBS         -lssl -lcrypto)
set(OPENSSL_STATIC_LIBS  -lssl -lcrypto)
set(TLS_INCLUDE     ${NCBI_TOOLS_ROOT}/libgcrypt-1.4.3/include 
                    ${NCBI_TOOLS_ROOT}/libgpg-error-1.6/include 
					${NCBI_TOOLS_ROOT}/gnutls-2.4.2/include)
set(TLS_LIBS        -L${NCBI_TOOLS_ROOT}/gnutls-2.4.2/GCC401-ReleaseMT64/lib -Wl,-rpath,/opt/ncbi/64/gnutls-2.4.2/GCC401-ReleaseMT64/lib:${NCBI_TOOLS_ROOT}/gnutls-2.4.2/GCC401-ReleaseMT64/lib -lgnutls -L${NCBI_TOOLS_ROOT}/libgcrypt-1.4.3/GCC401-ReleaseMT64/lib -Wl,-rpath,/opt/ncbi/64/libgcrypt-1.4.3/GCC401-ReleaseMT64/lib:${NCBI_TOOLS_ROOT}/libgcrypt-1.4.3/GCC401-ReleaseMT64/lib -lgcrypt -L${NCBI_TOOLS_ROOT}/libgpg-error-1.6/GCC401-ReleaseMT64/lib -Wl,-rpath,/opt/ncbi/64/libgpg-error-1.6/GCC401-ReleaseMT64/lib:${NCBI_TOOLS_ROOT}/libgpg-error-1.6/GCC401-ReleaseMT64/lib -lgpg-error -lz)

#include(FindGnuTLS)
find_package(GnuTLS)
set(GNUTLS_INCLUDE ${GNUTLS_INCLUDE_DIR})
set(GNUTLS_LIBS ${GNUTLS_LIBRARIES})


