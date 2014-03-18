if (${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    add_definitions(-D_DEBUG)
ELSE()
    add_definitions(-DNDEBUG)
ENDIF()
add_definitions(-D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64)
#set(XYY -I)
set(XYY )

include(CheckIncludeFile)
check_include_file(arpa/inet.h HAVE_ARPA_INET_H)
check_include_file(atomic.h HAVE_ATOMIC_H)
check_include_file(dlfcn.h HAVE_DLFCN_H)
check_include_file(errno.h HAVE_ERRNO_H)
check_include_file(ieeefp.h HAVE_IEEEFP_H)
check_include_file(inttypes.h HAVE_INTTYPES_H)
check_include_file(malloc.h HAVE_MALLOC_H)
check_include_file(memory.h HAVE_MEMORY_H)
check_include_file(netdb.h HAVE_NETDB_H)
check_include_file(netinet/in.h HAVE_NETINET_IN_H)
check_include_file(netinet/tcp.h HAVE_NETINET_TCP_H)
check_include_file(odbcss.h HAVE_ODBCSS_H)
check_include_file(paths.h HAVE_PATHS_H)
check_include_file(poll.h HAVE_POLL_H)
check_include_file(signal.h HAVE_SIGNAL_H)
check_include_file(sqlite3async.h HAVE_SQLITE3ASYNC_H)
check_include_file(stdint.h HAVE_STDINT_H)
check_include_file(stdlib.h HAVE_STDLIB_H)
check_include_file(string.h HAVE_STRING_H)
check_include_file(strings.h HAVE_STRINGS_H)
check_include_file(sys/ioctl.h HAVE_SYS_IOCTL_H)
check_include_file(sys/mount.h HAVE_SYS_MOUNT_H)
check_include_file(sys/select.h HAVE_SYS_SELECT_H)
check_include_file(sys/socket.h HAVE_SYS_SOCKET_H)
check_include_file(sys/sockio.h HAVE_SYS_SOCKIO_H)
check_include_file(sys/stat.h HAVE_SYS_STAT_H)
check_include_file(sys/statvfs.h HAVE_SYS_STATVFS_H)
check_include_file(sys/sysinfo.h HAVE_SYS_SYSINFO_H)
check_include_file(sys/time.h HAVE_SYS_TIME_H)
check_include_file(sys/types.h HAVE_SYS_TYPES_H)
check_include_file(sys/vfs.h HAVE_SYS_VFS_H)
check_include_file(unistd.h HAVE_UNISTD_H)
check_include_file(wchar.h HAVE_WCHAR_H)
check_include_file(windows.h HAVE_WINDOWS_H)

include(CheckIncludeFileCXX)
check_include_file_cxx(fstream HAVE_FSTREAM)
check_include_file_cxx(fstream.h HAVE_FSTREAM_H)
check_include_file_cxx(iostream HAVE_IOSTREAM)
check_include_file_cxx(iostream.h HAVE_IOSTREAM_H)
check_include_file_cxx(limits HAVE_LIMITS)
check_include_file_cxx(strstrea.h HAVE_STRSTREA_H)
check_include_file_cxx(strstream HAVE_STRSTREAM_H)

include(CheckFunctionExists)
check_function_exists(asprintf HAVE_ASPRINTF)
check_function_exists(atoll HAVE_ATOLL)
check_function_exists(basename HAVE_BASENAME)
check_function_exists(erf HAVE_ERF)
check_function_exists(euidaccess HAVE_EUIDACCESS)
check_function_exists(freehostent HAVE_FREEHOSTENT)
check_function_exists(fseeko HAVE_FSEEKO)
check_function_exists(getaddrinfo HAVE_ASPRINTF)
check_function_exists(gethostent_r HAVE_GETHOSTENT_R)
check_function_exists(getipnodebyaddr HAVE_GETIPNODEBYADDR)
check_function_exists(getipnodebyname HAVE_GETIPNODEBYNAME)
check_function_exists(getloadavg HAVE_GETLOADAVG)
check_function_exists(getlogin_r HAVE_GETLOGIN_R)
check_function_exists(getnameinfo HAVE_GETNAMEINFO)
check_function_exists(getpagesize HAVE_GETPAGESIZE)
check_function_exists(readpassphrase HAVE_READPASSPHRASE)
check_function_exists(getpassphrase HAVE_GETPASSPHRASE)
check_function_exists(getpass HAVE_GETPASS)
check_function_exists(getpwuid HAVE_GETPWUID)
check_function_exists(getrusage HAVE_GETRUSAGE)
check_function_exists(gettimeofday HAVE_GETTIMEOFDAY)
check_function_exists(getuid HAVE_GETUID)
check_function_exists(inet_ntoa_r HAVE_INET_NTOA_R)
check_function_exists(inet_ntop HAVE_INET_NTOP)
check_function_exists(lchown HAVE_LCHOWN)
check_function_exists(localtime_r HAVE_LOCALTIME_R)
check_function_exists(lutimes HAVE_LUTIMES)
check_function_exists(nanosleep HAVE_NANOSLEEP)
check_function_exists(pthread_atfork HAVE_PTHREAD_ATFORK)
check_function_exists(pthread_setconcurrency HAVE_PTHREAD_SETCONCURRENCY)
check_function_exists(readpassphrase HAVE_READPASSPHRASE)
check_function_exists(readv HAVE_READV)
check_function_exists(sched_yield HAVE_SCHED_YIELD)
check_function_exists(select HAVE_SELECT)
check_function_exists(sqlite3_unlock_notify HAVE_SQLITE3_UNLOCK_NOTIFY)
check_function_exists(statfs HAVE_STATFS)
check_function_exists(statvfs HAVE_STATVFS)
check_function_exists(strdup HAVE_STRDUP)
check_function_exists(strlcat HAVE_STRLCAT)
check_function_exists(strlcpy HAVE_STRLCPY)
check_function_exists(strndup HAVE_STRNDUP)
check_function_exists(strtok_r HAVE_STRTOK_R)
check_function_exists(utimes HAVE_UTIMES)
check_function_exists(vasprintf HAVE_VASPRINTF)
check_function_exists(vprintf HAVE_VPRINTF)
check_function_exists(vsnprintf HAVE_VSNPRINTF)
check_function_exists(writev HAVE_WRITEV)

include(CheckLibraryExists)
check_library_exists(dl dlopen "" HAVE_LIBDL)
check_library_exists(pcre pcre_study "" HAVE_LIBPCRE)

include(CheckTypeSize)
check_type_size(char SIZEOF_CHAR)
check_type_size(double SIZEOF_DOUBLE)
check_type_size(float SIZEOF_FLOAT)
check_type_size(int SIZEOF_INT)
check_type_size(long SIZEOF_LONG)
check_type_size("long double" SIZEOF_LONG_DOUBLE)
check_type_size("long long" SIZEOF_LONG_LONG)
check_type_size(short SIZEOF_SHORT)
check_type_size(size_t SIZEOF_SIZE_T)
check_type_size(void* SIZEOF_VOIDP)
check_type_size(__int64 SIZEOF___INT64)

option(USE_LOCAL_BZLIB "Use a local copy of libbz2")
option(USE_LOCAL_PCRE "Use a local copy of libpcre")

if (HAVE_WCHAR_H)
    set(HAVE_WSTRING 1)
endif (HAVE_WCHAR_H)

#
# Threading libraries
find_package(Threads)
if (CMAKE_USE_PTHREADS_INIT)
    add_definitions(-D_MT -D_REENTRANT -D_THREAD_SAFE)
    set(NCBI_POSIX_THREADS 1)
endif (CMAKE_USE_PTHREADS_INIT)


if(HAVE_LIBDL)
    set(DL_LIBS -ldl)
endif(HAVE_LIBDL)

set(ORIG_LIBS   -lrt -lm  -lpthread)
set(ORIG_C_LIBS      -lm  -lpthread)
set(C_LIBS      ${ORIG_C_LIBS})

if (HAVE_LIBPCRE AND NOT USE_LOCAL_PCRE)
    set(PCRE_LIBS -lpcre)
else(HAVE_LIBPCRE AND NOT USE_LOCAL_PCRE)
    set(USE_LOCAL_PCRE 1 CACHE INTERNAL "Using local PCRE due to system library absence")
endif(HAVE_LIBPCRE AND NOT USE_LOCAL_PCRE)


if (UNIX)
    SET(NCBI_OS_UNIX 1 CACHE INTERNAL "Is Unix")
    SET(NCBI_OS \"UNIX\" CACHE INTERNAL "Is Unix")
    if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
        SET(NCBI_OS_LINUX 1 CACHE INTERNAL "Is Linux")
    endif(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
endif(UNIX)

if (WIN32)
    SET(NCBI_OS_MSWIN 1 CACHE INTERNAL "Is Windows")
    SET(NCBI_OS \"WINDOWS\" CACHE INTERNAL "Is Windows")
endif(WIN32)

if (CYGWIN)
    SET(NCBI_OS_CYGWIN 1 CACHE INTERNAL "Is CygWin")
    SET(NCBI_OS \"CYGWIN\" CACHE INTERNAL "Is Cygwin")
endif(CYGWIN)

if (APPLE)
    if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
        SET(NCBI_OS_DARWIN 1 CACHE INTERNAL "Is Mac OS X")
    endif(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
endif(APPLE)



# ############################################################
# Specialized libs settings
# Mostly, from Makefile.mk
# ############################################################
#

set(LIBS ${LIBS} ${DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})


set(LOCAL_LBSM ncbi_lbsm ncbi_lbsm_ipc ncbi_lbsmd)
set(NCBI_CRYPT ncbi_crypt)


# non-public (X)CONNECT extensions
set(CONNEXT connext)
set(XCONNEXT xconnext)

# NCBI C++ API for BerkeleyDB
set(BDB_LIB bdb)
set(BDB_CACHE_LIB ncbi_xcache_bdb)

# Possibly absent DBAPI drivers (depending on whether the relevant
# 3rd-party libraries are present, and whether DBAPI was disabled altogether)
set(DBAPI_DRIVER  dbapi_driver)
set(DBAPI_CTLIB   ncbi_xdbapi_ctlib)
set(DBAPI_DBLIB   ncbi_xdbapi_dblib)
set(DBAPI_MYSQL   ncbi_xdbapi_mysql)
#DBAPI_ODBC 

# Compression libraries
include(FindZLIB)
find_package(ZLIB)
include(FindBZip2)
find_package(BZip2)
FIND_PATH(LZO_INCLUDE_DIR lzo/lzoconf.h
          ${LZO_ROOT}/include/lzo/
          ${NCBI_TOOLS_ROOT}/lzo-2.05/include/
          /usr/include/lzo/
          /usr/local/include/lzo/
          /sw/lib/lzo/
          /sw/local/lib/lzo/
         )
FIND_LIBRARY(LZO_LIBRARIES NAMES lzo2
             PATHS
             ${LZO_ROOT}/lib
             ${NCBI_TOOLS_ROOT}/lzo-2.05/lib64/
             ${NCBI_TOOLS_ROOT}/lzo-2.05/lib/
             /sw/lib
             /sw/local/lib
             /usr/lib
             /usr/local/lib
            )

set(Z_INCLUDE ${ZLIB_INCLUDE_DIRS})
set(Z_LIBS ${ZLIB_LIBRARIES})
set(Z_LIB)
set(BZ2_INCLUDE ${BZIP2_INCLUDE_DIRS})
set(BZ2_LIBS ${BZIP2_LIBRARIES})
set(BZ2_LIB)
set(LZO_INCLUDE ${LZO_INCLUDE_DIR})
set(LZO_LIBS ${LZO_LIBRARIES})


set(CMPRS_INCLUDE ${Z_INCLUDE} ${BZ2_INCLUDE} ${LZO_INCLUDE})
set(CMPRS_LIBS ${Z_LIBS} ${BZ2_LIBS} ${LZO_LIBS})
set(COMPRESS_LIBS xcompress ${CMPRS_LIBS})


# Perl-Compatible Regular Expressions (PCRE)
include(FindPCRE)
find_package(PCRE)
set(PCRE_INCLUDE ${PCRE_INCLUDE_DIR})
set(PCRE_LIBS ${PCRE_LIBRARIES})
set(PCRE_LIB)
set(PCREPOSIX_LIBS -lpcreposix -lpcre)
set(PCRE_LIB)

# OpenSSL, GnuTLS: headers and libs; TLS_* points to GNUTLS_* by preference.
set(GCRYPT_INCLUDE  ${XYY}${NCBI_TOOLS_ROOT}/libgcrypt-1.4.3/include 
                    ${XYY}${NCBI_TOOLS_ROOT}/libgpg-error-1.6/include)
set(GCRYPT_LIBS     -L${NCBI_TOOLS_ROOT}/libgcrypt-1.4.3/GCC401-ReleaseMT64/lib -Wl,-rpath,/opt/ncbi/64/libgcrypt-1.4.3/GCC401-ReleaseMT64/lib:${NCBI_TOOLS_ROOT}/libgcrypt-1.4.3/GCC401-ReleaseMT64/lib -lgcrypt -L${NCBI_TOOLS_ROOT}/libgpg-error-1.6/GCC401-ReleaseMT64/lib -Wl,-rpath,/opt/ncbi/64/libgpg-error-1.6/GCC401-ReleaseMT64/lib:${NCBI_TOOLS_ROOT}/libgpg-error-1.6/GCC401-ReleaseMT64/lib -lgpg-error -lz)
set(OPENSSL_INCLUDE)
set(OPENSSL_LIBS         -lssl -lcrypto)
set(OPENSSL_STATIC_LIBS  -lssl -lcrypto)
set(TLS_INCLUDE     ${XYY}${NCBI_TOOLS_ROOT}/libgcrypt-1.4.3/include 
                    ${XYY}${NCBI_TOOLS_ROOT}/libgpg-error-1.6/include 
					${XYY}${NCBI_TOOLS_ROOT}/gnutls-2.4.2/include)
set(TLS_LIBS        -L${NCBI_TOOLS_ROOT}/gnutls-2.4.2/GCC401-ReleaseMT64/lib -Wl,-rpath,/opt/ncbi/64/gnutls-2.4.2/GCC401-ReleaseMT64/lib:${NCBI_TOOLS_ROOT}/gnutls-2.4.2/GCC401-ReleaseMT64/lib -lgnutls -L${NCBI_TOOLS_ROOT}/libgcrypt-1.4.3/GCC401-ReleaseMT64/lib -Wl,-rpath,/opt/ncbi/64/libgcrypt-1.4.3/GCC401-ReleaseMT64/lib:${NCBI_TOOLS_ROOT}/libgcrypt-1.4.3/GCC401-ReleaseMT64/lib -lgcrypt -L${NCBI_TOOLS_ROOT}/libgpg-error-1.6/GCC401-ReleaseMT64/lib -Wl,-rpath,/opt/ncbi/64/libgpg-error-1.6/GCC401-ReleaseMT64/lib:${NCBI_TOOLS_ROOT}/libgpg-error-1.6/GCC401-ReleaseMT64/lib -lgpg-error -lz)

#include(FindGnuTLS)
find_package(GnuTLS)
set(GNUTLS_INCLUDE ${GNUTLS_INCLUDE_DIR})
set(GNUTLS_LIBS ${GNUTLS_LIBRARIES})

# Kerberos 5 (via GSSAPI)
set(KRB5_INCLUDE)
set(KRB5_LIBS   -lgssapi_krb5 -lkrb5 -lk5crypto -lcom_err)

#
# Sybase stuff
set(SYBASE_INCLUDE ${XYY}/export/home/sybase/clients/12.5-64bit/include)
set(SYBASE_LIBS   -L/export/home/sybase/clients/12.5-64bit/lib64 -Wl,-rpath,/export/home/sybase/clients/12.5-64bit/lib64 -lblk_r64 -lct_r64 -lcs_r64 -lsybtcl_r64 -lcomn_r64 -lintl_r64)
set(SYBASE_DLLS)
set(SYBASE_DBLIBS  -L/export/home/sybase/clients/12.5-64bit/lib64 -Wl,-rpath,/export/home/sybase/clients/12.5-64bit/lib64 -lsybdb64)

#
# FreeTDS
set(ftds64   ftds)
set(FTDS64_CTLIB_LIBS  ${ICONV_LIBS} ${KRB5_LIBS})
set(FTDS64_CTLIB_LIB   ct_ftds64 tds_ftds64)
set(FTDS64_CTLIB_INCLUDE ${XYY}${includedir}/dbapi/driver/ftds64/freetds)
set(FTDS64_LIBS        ${FTDS64_CTLIB_LIBS})
set(FTDS64_LIB        ${FTDS64_CTLIB_LIB})
set(FTDS64_INCLUDE    ${FTDS64_CTLIB_INCLUDE})

set(FTDS_LIBS     ${FTDS64_LIBS})
set(FTDS_LIB      ${FTDS64_LIB})
set(FTDS_INCLUDE  ${FTDS64_INCLUDE})

# MySQL: headers and libs
set(MYSQL_INCLUDE ${XYY}/usr/include/mysql)
set(MYSQL_LIBS    -rdynamic -L/usr/lib64/mysql -Wl,-rpath,/usr/lib64/mysql -lmysqlclient_r -lz -lpthread -lcrypt -lnsl -lm -lpthread -lssl -lcrypto)

#
# BerkeleyDB
set(BERKELEYDB_INCLUDE ${XYY}${NCBI_TOOLS_ROOT}/BerkeleyDB/include)
set(BERKELEYDB_LIBS    -L"${NCBI_TOOLS_ROOT}/BerkeleyDB/${CMAKE_BUILD_TYPE}" -ldb)
set(BERKELEYDB_STATIC_LIBS      -L${NCBI_TOOLS_ROOT}/BerkeleyDB-4.6.21.1/${buildconf0} -ldb-static)
set(BERKELEYDB_CXX_STATIC_LIBS  -L${NCBI_TOOLS_ROOT}/BerkeleyDB-4.6.21.1/${buildconf0} -ldb_cxx-static -ldb-static)
set(BERKELEYDB_CXX_LIBS         -L${NCBI_TOOLS_ROOT}/BerkeleyDB-4.6.21.1/${buildconf0} -Wl,-rpath,/opt/ncbi/64/BerkeleyDB-4.6.21.1/${buildconf0}:${NCBI_TOOLS_ROOT}/BerkeleyDB-4.6.21.1/${buildconf0} -ldb_cxx -ldb)

# ODBC
set(ODBC_INCLUDE  ${XYY}${includedir}/dbapi/driver/odbc/unix_odbc 
                  ${XYY}${includedir0}/dbapi/driver/odbc/unix_odbc)
set(ODBC_LIBS)

# PYTHON: headers and libs (default + specific versions)
set(PYTHON_INCLUDE  ${XYY}/opt/python-2.5.1/include/python2.5 
                    ${XYY}/opt/python-2.5.1/include/python2.5)
set(PYTHON_LIBS     -L/opt/python-2.5/lib -L/opt/python-2.5/lib/python2.5/config -Wl,-rpath,/opt/python-2.5/lib:/opt/python-2.5/lib/python2.5/config -lpython2.5 -lpthread -ldl  -lutil -lm)
set(PYTHON23_INCLUDE)
set(PYTHON23_LIBS)
set(PYTHON24_INCLUDE)
set(PYTHON24_LIBS)
set(PYTHON25_INCLUDE  ${XYY}/opt/python-2.5.1/include/python2.5 
                      ${XYY}/opt/python-2.5.1/include/python2.5)
set(PYTHON25_LIBS     -L/opt/python-2.5/lib -L/opt/python-2.5/lib/python2.5/config -Wl,-rpath,/opt/python-2.5/lib:/opt/python-2.5/lib/python2.5/config -lpython2.5 -lpthread -ldl  -lutil -lm)

# Perl: executable, headers and libs
set(PERL          /opt/perl-5.8.8/bin/perl)
set(PERL_INCLUDE  ${XYY}/opt/perl-5.8.8/lib/5.8.8/x86_64-linux/CORE 
                  ${XYY}/usr/include/gdbm)
set(PERL_LIBS     -L/opt/perl-5.8.8/lib/5.8.8/x86_64-linux/CORE -Wl,-rpath,/opt/perl-5.8.8/lib/5.8.8/x86_64-linux/CORE -lperl -lnsl -lgdbm -ldb -ldl -lm -lcrypt -lutil -lc)

# Java
set(JDK_INCLUDE)
set(JDK_PATH)


# Boost: headers and libs [use as ${BOOST_LIBPATH} ${BOOST_*_LIBS} ${RT_LIBS}]
include(FindBoost)
set(CMAKE_INCLUDE_PATH ${NCBI_TOOLS_ROOT}/boost-1.41.0/include)
set(CMAKE_LIBRARY_PATH ${NCBI_TOOLS_ROOT}/boost-1.41.0/lib)
find_package(Boost)
if (NOT Boost_FOUND)
    message(WARNING "BOOST not found...")
endif (NOT Boost_FOUND)
set(BOOST_INCLUDE ${Boost_INCLUDE_DIRS})
set(BOOST_LIBPATH              -L${NCBI_TOOLS_ROOT}/boost-1.53.0-ncbi1/lib -Wl,-rpath,/opt/ncbi/64/boost-1.53.0-ncbi1/lib:${NCBI_TOOLS_ROOT}/boost-1.53.0-ncbi1/lib)
set(BOOST_TAG                  -gcc48-mt-d-1_53-64)
set(BOOST_FILESYSTEM_LIBS      -lboost_filesystem-gcc48-mt-d-1_53-64 -lboost_system-gcc48-mt-d-1_53-64)
set(BOOST_FILESYSTEM_STATIC_LIBS -lboost_filesystem-gcc48-mt-d-1_53-64-static -lboost_system-gcc48-mt-d-1_53-64-static)
set(BOOST_REGEX_LIBS           -lboost_regex-gcc48-mt-d-1_53-64)
set(BOOST_REGEX_STATIC_LIBS    -lboost_regex-gcc48-mt-d-1_53-64-static)
set(BOOST_SYSTEM_LIBS          -lboost_system-gcc48-mt-d-1_53-64)
set(BOOST_SYSTEM_STATIC_LIBS   -lboost_system-gcc48-mt-d-1_53-64-static)
set(BOOST_TEST_PEM_LIBS        -lboost_prg_exec_monitor-gcc48-mt-d-1_53-64-static)
set(BOOST_TEST_PEM_STATIC_LIBS -lboost_prg_exec_monitor-gcc48-mt-d-1_53-64-static)
set(BOOST_TEST_TEM_LIBS        -lboost_test_exec_monitor-gcc48-mt-d-1_53-64-static)
set(BOOST_TEST_TEM_STATIC_LIBS -lboost_test_exec_monitor-gcc48-mt-d-1_53-64-static)
set(BOOST_TEST_UTF_LIBS        -lboost_unit_test_framework-gcc48-mt-d-1_53-64-static)
set(BOOST_TEST_UTF_STATIC_LIBS -lboost_unit_test_framework-gcc48-mt-d-1_53-64-static)
set(BOOST_THREAD_LIBS          -lboost_thread-gcc48-mt-d-1_53-64 -lboost_system-gcc48-mt-d-1_53-64)
set(BOOST_THREAD_STATIC_LIBS   -lboost_thread-gcc48-mt-d-1_53-64-static -lboost_system-gcc48-mt-d-1_53-64-static)
set(BOOST_TEST_LIBS            ${BOOST_LIBPATH} ${BOOST_TEST_UTF_LIBS})
set(BOOST_TEST_STATIC_LIBS     ${BOOST_LIBPATH} ${BOOST_TEST_UTF_STATIC_LIBS})
# Temporary, for backward compatibility, to be removed later:
set(BOOST_LIBS            ${BOOST_TEST_LIBS})
set(BOOST_STATIC_LIBS     ${BOOST_TEST_STATIC_LIBS})

# NCBI C Toolkit:  headers and libs
set(NCBI_C_INCLUDE  ${XYY}${NCBI_TOOLS_ROOT}/include64)
set(NCBI_C_LIBPATH  "-L${NCBI_TOOLS_ROOT}/lib64")
set(NCBI_C_ncbi     ncbi)

# OpenGL: headers and libs (including core X dependencies) for code
# not using other toolkits.  (The wxWidgets variables already include
# these as appropriate.)
set(OPENGL_INCLUDE     ${XYY}${NCBI_TOOLS_ROOT}/Mesa-7.0.2-ncbi2/include)
set(OPENGL_LIBS        -L${NCBI_TOOLS_ROOT}/Mesa-7.0.2-ncbi2/lib64 -Wl,-rpath,/opt/ncbi/64/Mesa-7.0.2-ncbi2/lib64:${NCBI_TOOLS_ROOT}/Mesa-7.0.2-ncbi2/lib64   -lGLU -lGL -lXmu -lXt -lXext  -lSM -lICE -lX11 )
set(OPENGL_STATIC_LIBS -L${NCBI_TOOLS_ROOT}/Mesa-7.0.2-ncbi2/lib64 -Wl,-rpath,/opt/ncbi/64/Mesa-7.0.2-ncbi2/lib64:${NCBI_TOOLS_ROOT}/Mesa-7.0.2-ncbi2/lib64   -lGLU-static -lGL-static -lXmu -lXt -lXext  -lSM -lICE -lX11 )
set(OSMESA_INCLUDE     ${XYY}${NCBI_TOOLS_ROOT}/Mesa-7.0.2-ncbi2/include)
set(OSMESA_LIBS         -L${NCBI_TOOLS_ROOT}/Mesa-7.0.2-ncbi2/lib64 -Wl,-rpath,/opt/ncbi/64/Mesa-7.0.2-ncbi2/lib64:${NCBI_TOOLS_ROOT}/Mesa-7.0.2-ncbi2/lib64   -lOSMesa -lGLU -lGL -lXmu -lXt -lXext  -lSM -lICE -lX11 )
set(OSMESA_STATIC_LIBS  -L${NCBI_TOOLS_ROOT}/Mesa-7.0.2-ncbi2/lib64 -Wl,-rpath,/opt/ncbi/64/Mesa-7.0.2-ncbi2/lib64:${NCBI_TOOLS_ROOT}/Mesa-7.0.2-ncbi2/lib64   -lOSMesa-static -lGLU-static -lGL-static -lXmu -lXt -lXext  -lSM -lICE -lX11 )
set(GLUT_INCLUDE       )
set(GLUT_LIBS          )
set(GLEW_INCLUDE       ${XYY}${NCBI_TOOLS_ROOT}/glew-1.5.8/GCC401-Debug64/include)
set(GLEW_LIBS          -L${NCBI_TOOLS_ROOT}/glew-1.5.8/GCC401-Debug64/lib64 -Wl,-rpath,/opt/ncbi/64/glew-1.5.8/GCC401-Debug64/lib64:${NCBI_TOOLS_ROOT}/glew-1.5.8/GCC401-Debug64/lib64 -lGLEW)
set(GLEW_STATIC_LIBS   ${NCBI_TOOLS_ROOT}/glew-1.5.8/GCC401-Debug64/lib/libGLEW-static.a)


# wxWidgets
set(foo "${CMAKE_PREFIX_PATH}")
#set(CMAKE_PREFIX_PATH "${NCBI_TOOLS_ROOT}/wxwidgets/${CMAKE_BUILD_TYPE}/bin")
set(CMAKE_PREFIX_PATH "${NCBI_TOOLS_ROOT}/wxGTK/DebugMT64/bin")
FIND_PACKAGE(wxWidgets
    COMPONENTS core gl base
    OPTIONAL)
set(WXWIDGETS_INCLUDE "${wxWidgets_INCLUDE_DIRS}")
set(WXWIDGETS_LIBS "${wxWidgets_LIBRARIES}")
set(CMAKE_PREFIX_PATH "${foo}")
set(foo)
set(WXWIDGETS_STATIC_LIBS    -L${NCBI_TOOLS_ROOT}/wxWidgets-2.9.5-ncbi1/GCC442-DebugMT64/lib -pthread   ${NCBI_TOOLS_ROOT}/wxWidgets-2.9.5-ncbi1/GCC442-DebugMT64/lib/libwx_gtk2_xrc-2.9.a ${NCBI_TOOLS_ROOT}/wxWidgets-2.9.5-ncbi1/GCC442-DebugMT64/lib/libwx_gtk2_qa-2.9.a ${NCBI_TOOLS_ROOT}/wxWidgets-2.9.5-ncbi1/GCC442-DebugMT64/lib/libwx_base_net-2.9.a ${NCBI_TOOLS_ROOT}/wxWidgets-2.9.5-ncbi1/GCC442-DebugMT64/lib/libwx_gtk2_html-2.9.a ${NCBI_TOOLS_ROOT}/wxWidgets-2.9.5-ncbi1/GCC442-DebugMT64/lib/libwx_gtk2_adv-2.9.a ${NCBI_TOOLS_ROOT}/wxWidgets-2.9.5-ncbi1/GCC442-DebugMT64/lib/libwx_gtk2_core-2.9.a ${NCBI_TOOLS_ROOT}/wxWidgets-2.9.5-ncbi1/GCC442-DebugMT64/lib/libwx_base_xml-2.9.a ${NCBI_TOOLS_ROOT}/wxWidgets-2.9.5-ncbi1/GCC442-DebugMT64/lib/libwx_base-2.9.a -pthread -lgthread-2.0 -lX11 -lSM -lgtk-x11-2.0 -lgdk-x11-2.0 -latk-1.0 -lgdk_pixbuf-2.0 -lpangocairo-1.0 -lpango-1.0 -lcairo -lgobject-2.0 -lgmodule-2.0 -lglib-2.0 -lpng -ljpeg -ltiff -lexpat -lz -ldl )
set(WXWIDGETS_GL_LIBS        -L${NCBI_TOOLS_ROOT}/wxWidgets-2.9.5-ncbi1/GCC442-DebugMT64/lib -pthread   -lwx_gtk2_gl-2.9 -lwx_base-2.9 )
set(WXWIDGETS_GL_STATIC_LIBS -L${NCBI_TOOLS_ROOT}/wxWidgets-2.9.5-ncbi1/GCC442-DebugMT64/lib -pthread   ${NCBI_TOOLS_ROOT}/wxWidgets-2.9.5-ncbi1/GCC442-DebugMT64/lib/libwx_gtk2_gl-2.9.a ${NCBI_TOOLS_ROOT}/wxWidgets-2.9.5-ncbi1/GCC442-DebugMT64/lib/libwx_base-2.9.a -L${NCBI_TOOLS_ROOT}/Mesa-7.0.2-ncbi2/lib64 -Wl,-rpath,/opt/ncbi/64/Mesa-7.0.2-ncbi2/lib64:${NCBI_TOOLS_ROOT}/Mesa-7.0.2-ncbi2/lib64   -lGLU-static -lGL-static -lXmu -lXt -lXext  -lSM -lICE -lX11  -lz -ldl -lm )

set(WXWIDGETS_INCLUDE     ${XYY}/usr/include/gtk-2.0 
                          ${XYY}/usr/lib64/gtk-2.0/include 
						  ${XYY}/usr/include/atk-1.0 
						  ${XYY}/usr/include/cairo 
						  ${XYY}/usr/include/pango-1.0 
						  ${XYY}/usr/include/glib-2.0 
						  ${XYY}/usr/lib64/glib-2.0/include 
						  ${XYY}/usr/include/pixman-1 
						  ${XYY}/usr/include/freetype2 
						  ${XYY}/usr/include/libpng12   
						  ${XYY}/netopt/ncbi_tools64/wxWidgets-2.9.5-ncbi1/GCC442-DebugMT64/lib/wx/include/gtk2-ansi-2.9 
						  ${XYY}/netopt/ncbi_tools64/wxWidgets-2.9.5-ncbi1/include/wx-2.9)
set(WXWIDGETS_LIBS        -L/netopt/ncbi_tools64/wxWidgets-2.9.5-ncbi1/GCC442-DebugMT64/lib -Wl,-rpath,/opt/ncbi/64/wxWidgets-2.9.5-ncbi1/GCC442-DebugMT64/lib:/netopt/ncbi_tools64/wxWidgets-2.9.5-ncbi1/GCC442-DebugMT64/lib -pthread -lwx_gtk2_richtext-2.9 -lwx_gtk2_aui-2.9 -lwx_gtk2_xrc-2.9 -lwx_gtk2_html-2.9 -lwx_gtk2_qa-2.9 -lwx_gtk2_adv-2.9 -lwx_gtk2_core-2.9 -lwx_base_xml-2.9 -lwx_base_net-2.9 -lwx_base-2.9 -pthread -lgthread-2.0 -lX11 -lSM -lgtk-x11-2.0 -lgdk-x11-2.0 -latk-1.0 -lgdk_pixbuf-2.0 -lpangocairo-1.0 -lpango-1.0 -lcairo -lgobject-2.0 -lgmodule-2.0 -lglib-2.0 -lpng -ljpeg -ltiff -lexpat -lz -ldl)





# Fast-CGI
set(FASTCGI_INCLUDE ${XYY}${NCBI_TOOLS_ROOT}/fcgi-current/include64)
set(FASTCGI_LIBS    -L${NCBI_TOOLS_ROOT}/fcgi-2.4.0/altlib64 -Wl,-rpath,/opt/ncbi/64/fcgi-2.4.0/altlib64:${NCBI_TOOLS_ROOT}/fcgi-2.4.0/altlib64 -lfcgi -lnsl)
# Fast-CGI lib:  (module to add to the "xcgi" library)
set(FASTCGI_OBJS    fcgibuf)


# NCBI SSS:  headers, library path, libraries
set(NCBI_SSS_INCLUDE ${XYY}${incdir}/sss)
set(NCBI_SSS_LIBPATH )
set(LIBSSSUTILS      -lsssutils)
set(LIBSSSDB         -lsssdb -lssssys)
set(sssutils         sssutils)
set(NCBILS2_LIB      ncbils2_cli ncbils2_asn ncbils2_cmn)
set(NCBILS_LIB       ${NCBILS2_LIB})


# SP:  headers, libraries
set(SP_INCLUDE )
set(SP_LIBS    )

# ORBacus CORBA headers, library path, libraries
set(ORBACUS_INCLUDE )
set(ORBACUS_LIBPATH )
set(LIBOB           )
# LIBIMR should be empty for single-threaded builds
set(LIBIMR          )


# IBM's International Components for Unicode
set(ICU_CONFIG      ${NCBI_TOOLS_ROOT}/icu-49.1.1/GCC442-DebugMT/bin/icu-config)
set(ICU_INCLUDE     ${XYY}${NCBI_TOOLS_ROOT}/icu-49.1.1/include )
set(ICU_LIBS        -L${NCBI_TOOLS_ROOT}/icu-49.1.1/GCC442-DebugMT/lib -Wl,-rpath,/opt/ncbi/64/icu-49.1.1/GCC442-DebugMT/lib:${NCBI_TOOLS_ROOT}/icu-49.1.1/GCC442-DebugMT/lib -licui18n -licuuc -licudata )
set(ICU_STATIC_LIBS -L${NCBI_TOOLS_ROOT}/icu-49.1.1/GCC442-DebugMT/lib  -lsicui18n -lsicuuc -lsicudata )


# XML/XSL support:
set(EXPAT_INCLUDE       )
set(EXPAT_LIBS         -L${NCBI_TOOLS_ROOT}/expat-1.95.8/lib64 -Wl,-rpath,/opt/ncbi/64/expat-1.95.8/lib64:${NCBI_TOOLS_ROOT}/expat-1.95.8/lib64 -lexpat )
set(EXPAT_STATIC_LIBS  -L${NCBI_TOOLS_ROOT}/expat-1.95.8/lib64 -Wl,-rpath,/opt/ncbi/64/expat-1.95.8/lib64:${NCBI_TOOLS_ROOT}/expat-1.95.8/lib64 -lexpat )
set(SABLOT_INCLUDE      ${XYY}${NCBI_TOOLS_ROOT}/Sablot-1.0.2/include)
set(SABLOT_LIBS        -L${NCBI_TOOLS_ROOT}/Sablot-1.0.2/lib64 -Wl,-rpath,/opt/ncbi/64/Sablot-1.0.2/lib64:${NCBI_TOOLS_ROOT}/Sablot-1.0.2/lib64 -lsablot -L${NCBI_TOOLS_ROOT}/expat-1.95.8/lib64 -Wl,-rpath,/opt/ncbi/64/expat-1.95.8/lib64:${NCBI_TOOLS_ROOT}/expat-1.95.8/lib64 -lexpat )
set(SABLOT_STATIC_LIBS -L${NCBI_TOOLS_ROOT}/Sablot-1.0.2/lib64 -Wl,-rpath,/opt/ncbi/64/Sablot-1.0.2/lib64:${NCBI_TOOLS_ROOT}/Sablot-1.0.2/lib64 -lsablot -L${NCBI_TOOLS_ROOT}/expat-1.95.8/lib64 -Wl,-rpath,/opt/ncbi/64/expat-1.95.8/lib64:${NCBI_TOOLS_ROOT}/expat-1.95.8/lib64 -lexpat )
set(LIBXML_INCLUDE     ${XYY}${NCBI_TOOLS_ROOT}/libxml-2.7.8/${buildconf}/include/libxml2 
                       ${XYY}${NCBI_TOOLS_ROOT}/libxml-2.7.8/${buildconf}/include)
set(LIBXML_LIBS        -L${NCBI_TOOLS_ROOT}/libxml-2.7.8/${buildconf}/lib -Wl,-rpath,/opt/ncbi/64/libxml-2.7.8/${buildconf}/lib:${NCBI_TOOLS_ROOT}/libxml-2.7.8/${buildconf}/lib -lxml2 )
set(LIBXML_STATIC_LIBS -L${NCBI_TOOLS_ROOT}/libxml-2.7.8/${buildconf}/lib -lxml2-static)
set(LIBXSLT_INCLUDE    ${XYY}${NCBI_TOOLS_ROOT}/libxml-2.7.8/${buildconf}/include/libxml2 
                       ${XYY}${NCBI_TOOLS_ROOT}/libxml-2.7.8/${buildconf}/include 
					   ${XYY}${NCBI_TOOLS_ROOT}/libxml-2.7.8/${buildconf}/include)
set(LIBXSLT_MAIN_LIBS  -L${NCBI_TOOLS_ROOT}/libxml-2.7.8/${buildconf}/lib -Wl,-rpath,/opt/ncbi/64/libxml-2.7.8/${buildconf}/lib:${NCBI_TOOLS_ROOT}/libxml-2.7.8/${buildconf}/lib -lxslt )
set(LIBXSLT_MAIN_STATIC_LIBS -L${NCBI_TOOLS_ROOT}/libxml-2.7.8/${buildconf}/lib -lxslt-static)
set(XSLTPROC           ${NCBI_TOOLS_ROOT}/libxml-2.7.8/${buildconf}/bin/xsltproc)
set(LIBEXSLT_INCLUDE   ${XYY}${NCBI_TOOLS_ROOT}/libxml-2.7.8/${buildconf}/include/libxml2 
                       ${XYY}${NCBI_TOOLS_ROOT}/libxml-2.7.8/${buildconf}/include 
					   ${XYY}${NCBI_TOOLS_ROOT}/libxml-2.7.8/${buildconf}/include 
					   ${XYY}${NCBI_TOOLS_ROOT}/libxml-2.7.8/${buildconf}/include)
set(LIBEXSLT_LIBS      -L${NCBI_TOOLS_ROOT}/libxml-2.7.8/${buildconf}/lib -Wl,-rpath,/opt/ncbi/64/libxml-2.7.8/${buildconf}/lib:${NCBI_TOOLS_ROOT}/libxml-2.7.8/${buildconf}/lib -lexslt )
set(LIBEXSLT_STATIC_LIBS=-L${NCBI_TOOLS_ROOT}/libxml-2.7.8/${buildconf}/lib -lexslt-static)
set(LIBXSLT_LIBS       ${LIBEXSLT_LIBS} ${LIBXSLT_MAIN_LIBS})
set(LIBXSLT_STATIC_LIBS ${LIBEXSLT_STATIC_LIBS} ${LIBXSLT_MAIN_STATIC_LIBS})
set(XERCES_INCLUDE     ${XYY}${NCBI_TOOLS_ROOT}/xerces-3.1.1/GCC442-DebugMT64/include)
set(XERCES_LIBS        -L${NCBI_TOOLS_ROOT}/xerces-3.1.1/GCC442-DebugMT64/lib -Wl,-rpath,/opt/ncbi/64/xerces-3.1.1/GCC442-DebugMT64/lib:${NCBI_TOOLS_ROOT}/xerces-3.1.1/GCC442-DebugMT64/lib -lxerces-c)
set(XERCES_STATIC_LIBS -L${NCBI_TOOLS_ROOT}/xerces-3.1.1/GCC442-DebugMT64/lib -lxerces-c-static -lcurl )
set(XALAN_INCLUDE      ${XYY}${NCBI_TOOLS_ROOT}/xalan-1.11~r1302529/GCC442-DebugMT64/include)
set(XALAN_LIBS         -L${NCBI_TOOLS_ROOT}/xalan-1.11~r1302529/GCC442-DebugMT64/lib -Wl,-rpath,/opt/ncbi/64/xalan-1.11~r1302529/GCC442-DebugMT64/lib:${NCBI_TOOLS_ROOT}/xalan-1.11~r1302529/GCC442-DebugMT64/lib -lxalan-c -lxalanMsg)
set(XALAN_STATIC_LIBS  -L${NCBI_TOOLS_ROOT}/xalan-1.11~r1302529/GCC442-DebugMT64/lib -Wl,-rpath,/opt/ncbi/64/xalan-1.11~r1302529/GCC442-DebugMT64/lib:${NCBI_TOOLS_ROOT}/xalan-1.11~r1302529/GCC442-DebugMT64/lib -lxalan-c -lxalanMsg)
set(ZORBA_INCLUDE      )
set(ZORBA_LIBS         )
set(ZORBA_STATIC_LIBS  )


# OpenEye OEChem library:
set(OECHEM_INCLUDE )
set(OECHEM_LIBS    )

# Sun Grid Engine (libdrmaa):
set(SGE_INCLUDE  ${XYY}/netopt/uge/include)
set(SGE_LIBS    -L/netopt/uge/lib/lx-amd64 -Wl,-rpath,/netopt/uge/lib/lx-amd64 -ldrmaa )

# muParser
set(MUPARSER_INCLUDE  ${XYY}${NCBI_TOOLS_ROOT}/muParser-1.30/include)
set(MUPARSER_LIBS    -L${NCBI_TOOLS_ROOT}/muParser-1.30/GCC-Debug64/lib -lmuparser )

# HDF5
set(HDF5_INCLUDE  ${XYY}${NCBI_TOOLS_ROOT}/hdf5-1.8.3/GCC401-Debug64/include)
set(HDF5_LIBS    -L${NCBI_TOOLS_ROOT}/hdf5-1.8.3/GCC401-Debug64/lib -Wl,-rpath,/opt/ncbi/64/hdf5-1.8.3/GCC401-Debug64/lib:${NCBI_TOOLS_ROOT}/hdf5-1.8.3/GCC401-Debug64/lib -lhdf5_cpp -lhdf5)

# SQLite 3.x
set(SQLITE3_INCLUDE     ${XYY}${NCBI_TOOLS_ROOT}/sqlite-3.7.13-ncbi1/include)
set(SQLITE3_LIBS        -L${NCBI_TOOLS_ROOT}/sqlite-3.7.13-ncbi1/GCC-DebugMT64/lib -Wl,-rpath,/opt/ncbi/64/sqlite-3.7.13-ncbi1/GCC-DebugMT64/lib:${NCBI_TOOLS_ROOT}/sqlite-3.7.13-ncbi1/GCC-DebugMT64/lib -lsqlite3 )
set(SQLITE3_STATIC_LIBS -L${NCBI_TOOLS_ROOT}/sqlite-3.7.13-ncbi1/GCC-DebugMT64/lib -lsqlite3-static)
#set(SQLITE3_INCLUDE    ${XYY}${NCBI_TOOLS_ROOT}/sqlite3/include)
#set(SQLITE3_LIBS       -L"${NCBI_TOOLS_ROOT}/sqlite3/lib" -lsqlite3)


# Various image-format libraries
include(FindJPEG)
find_package(JPEG)

include(FindPNG)
find_package(PNG)

include(FindTIFF)
find_package(TIFF)

include(FindGIF)
find_package(GIF)

set(IMAGE_INCLUDE_DIR ${JPEG_INCLUDE_DIR} ${PNG_INCLUDE_DIR} ${TIFF_INCLUDE_DIR} ${GIF_INCLUDE_DIR})
set(IMAGE_LIBS ${JPEG_LIBRARIES} ${PNG_LIBRARIES} ${TIFF_LIBRARIES} ${GIF_LIBRARIES})

set(JPEG_INCLUDE   )
set(JPEG_LIBS     -ljpeg )
set(PNG_INCLUDE     )
set(PNG_LIBS      -lpng -lz )
set(TIFF_INCLUDE   )
set(TIFF_LIBS     -ltiff -lz )
set(GIF_INCLUDE    )
set(GIF_LIBS      -lungif   -lX11)
set(UNGIF_INCLUDE  )
set(UNGIF_LIBS    -lungif   -lX11)
set(XPM_INCLUDE    )
set(XPM_LIBS      -lXpm   -lX11)

set(IMAGE_LIBS    ${TIFF_LIBS} ${JPEG_LIBS} ${PNG_LIBS} ${GIF_LIBS} ${XPM_LIBS})


# FreeType, FTGL
set(FREETYPE_INCLUDE ${XYY}/usr/include/freetype2)
set(FREETYPE_LIBS    -lfreetype)
set(FTGL_INCLUDE     ${XYY}/usr/include/freetype2 
                     ${XYY}${NCBI_TOOLS_ROOT}/ftgl-2.1.3-rc5/include)
set(FTGL_LIBS        -L${NCBI_TOOLS_ROOT}/ftgl-2.1.3-rc5/GCC401-Debug64/lib -Wl,-rpath,/opt/ncbi/64/ftgl-2.1.3-rc5/GCC401-Debug64/lib:${NCBI_TOOLS_ROOT}/ftgl-2.1.3-rc5/GCC401-Debug64/lib -lftgl -lfreetype)


# libmagic (file-format identification)
set(MAGIC_INCLUDE  )
set(MAGIC_LIBS    -lmagic )

# libcurl (URL retrieval)
set(CURL_INCLUDE  )
set(CURL_LIBS    -lcurl )

# libmimetic (MIME handling)
set(MIMETIC_INCLUDE  ${XYY}${NCBI_TOOLS_ROOT}/mimetic-0.9.7-ncbi1/include)
set(MIMETIC_LIBS    -L${NCBI_TOOLS_ROOT}/mimetic-0.9.7-ncbi1/GCC481-Debug64/lib -Wl,-rpath,/opt/ncbi/64/mimetic-0.9.7-ncbi1/GCC481-Debug64/lib:${NCBI_TOOLS_ROOT}/mimetic-0.9.7-ncbi1/GCC481-Debug64/lib -lmimetic )

# libgSOAP++
set(GSOAP_PATH     ${NCBI_TOOLS_ROOT}/gsoap-2.8.15)
set(GSOAP_INCLUDE  ${XYY}${NCBI_TOOLS_ROOT}/gsoap-2.8.15/include)
set(GSOAP_LIBS     -L${NCBI_TOOLS_ROOT}/gsoap-2.8.15/GCC442-DebugMT64/lib -Wl,-rpath,/opt/ncbi/64/gsoap-2.8.15/GCC442-DebugMT64/lib:${NCBI_TOOLS_ROOT}/gsoap-2.8.15/GCC442-DebugMT64/lib -lgsoapssl++ -lssl -lcrypto -lz )
set(GSOAP_SOAPCPP2 ${NCBI_TOOLS_ROOT}/gsoap-2.8.15/GCC442-DebugMT64/bin/soapcpp2)
set(GSOAP_WSDL2H   ${NCBI_TOOLS_ROOT}/gsoap-2.8.15/GCC442-DebugMT64/bin/wsdl2h)

# MongoDB
set(MONGODB_INCLUDE     ${XYY}${NCBI_TOOLS_ROOT}/boost-1.53.0-ncbi1/include/boost-1_53 -Wno-unused-local-typedefs 
                        ${XYY}${NCBI_TOOLS_ROOT}/mongodb-2.4.6/include)
set(MONGODB_LIBS        -L${NCBI_TOOLS_ROOT}/mongodb-2.4.6/lib -Wl,-rpath,/opt/ncbi/64/mongodb-2.4.6/lib:${NCBI_TOOLS_ROOT}/mongodb-2.4.6/lib -lmongoclient -L${NCBI_TOOLS_ROOT}/boost-1.53.0-ncbi1/lib -Wl,-rpath,/opt/ncbi/64/boost-1.53.0-ncbi1/lib:${NCBI_TOOLS_ROOT}/boost-1.53.0-ncbi1/lib -lboost_filesystem-gcc48-mt-d-1_53-64 -lboost_thread-gcc48-mt-d-1_53-64 -lboost_system-gcc48-mt-d-1_53-64)
set(MONGODB_STATIC_LIBS -L${NCBI_TOOLS_ROOT}/mongodb-2.4.6/lib -Wl,-rpath,/opt/ncbi/64/mongodb-2.4.6/lib:${NCBI_TOOLS_ROOT}/mongodb-2.4.6/lib -lmongodb -L${NCBI_TOOLS_ROOT}/boost-1.53.0-ncbi1/lib -Wl,-rpath,/opt/ncbi/64/boost-1.53.0-ncbi1/lib:${NCBI_TOOLS_ROOT}/boost-1.53.0-ncbi1/lib -lboost_filesystem-gcc48-mt-d-1_53-64-static -lboost_thread-gcc48-mt-d-1_53-64-static -lboost_system-gcc48-mt-d-1_53-64-static)

# Compress
set(COMPRESS_LDEP ${CMPRS_LIB})
set(COMPRESS_LIBS xcompress ${COMPRESS_LDEP})


#################################
# Useful sets of object libraries
#################################


set(SEQ_LIBS seq seqcode sequtil)
set(SOBJMGR_LDEP genome_collection seqedit seqset ${SEQ_LIBS} pub medline
    biblio general-lib xser xutil xncbi)
set(SOBJMGR_LIBS xobjmgr ${SOBJMGR_LDEP})
set(ncbi_xreader_pubseqos ncbi_xreader_pubseqos)
set(ncbi_xreader_pubseqos2 ncbi_xreader_pubseqos2)

set(GENBANK_READER_LDEP ${XCONNEXT} xconnect id1 id2 seqsplit ${COMPRESS_LIBS} ${SOBJMGR_LIBS})
set(GENBANK_READER_LIBS ncbi_xreader ${GENBANK_READER_LDEP})
set(GENBANK_READER_PUBSEQOS_LDEP ${XCONNEXT} xconnect ${DBAPI_DRIVER} ${GENBANK_READER_LIBS})
set(GENBANK_READER_PUBSEQOS_LIBS ${ncbi_xreader_pubseqos} ${GENBANK_READER_PUBSEQOS_LDEP})
set(GENBANK_LDEP
    ncbi_xreader_id1 ncbi_xreader_id2 ncbi_xreader_cache
    ${GENBANK_READER_PUBSEQOS_LIBS})
set(GENBANK_LIBS ncbi_xloader_genbank ${GENBANK_LDEP})



set(GENBANK_READER_ID1_LDEP xconnect id1 ${GENBANK_READER_LIBS})
set(GENBANK_READER_ID1_LIBS ncbi_xreader_id1 ${GENBANK_READER_ID1_LDEP})

set(GENBANK_READER_ID2_LDEP xconnect id2 seqsplit ${GENBANK_READER_LIBS})
set(GENBANK_READER_ID2_LIBS ncbi_xreader_id2 ${GENBANK_READER_ID2_LDEP})

set(GENBANK_READER_CACHE_LDEP ${GENBANK_READER_LIBS})
set(GENBANK_READER_CACHE_LIBS ncbi_xreader_cache ${GENBANK_READER_CACHE_LDEP})

set(GENBANK_READER_GICACHE_LDEP ${GENBANK_READER_LIBS})
set(GENBANK_READER_GICACHE_LIBS ncbi_xreader_gicache
        ${GENBANK_READER_GICACHE_LDEP})


# Interdependent sequence libraries + seqcode.  Does not include seqset.
set(OBJMGR_LIBS ${GENBANK_LIBS})


# Overlapping with qall is poor, so we have a second macro to make it
# easier to stay out of trouble.
set(QOBJMGR_ONLY_LIBS xobjmgr id2 seqsplit id1 genome_collection seqset
    ${SEQ_LIBS} pub medline biblio general-lib xcompress ${CMPRS_LIB})
set(QOBJMGR_LIBS ${QOBJMGR_ONLY_LIBS} qall)
set(QOBJMGR_STATIC_LIBS ${QOBJMGR_ONLY_LIBS} qall)

# EUtils
set(EUTILS_LIBS eutils egquery elink epost esearch espell esummary linkout
              einfo uilist ehistory)

# Object readers
set(OBJREAD_LIBS xobjread variation creaders submit)

# formatting code
set(XFORMAT_LIBS xformat gbseq submit mlacli mla medlars pubmed valid)



set(SRA_INCLUDE ${XYY}${includedir}/../src/sra/sdk/interfaces
                ${XYY}${top_srcdir}/src/sra/sdk/interfaces)

# For internal use; ${NO_STRICT_ALIASING} technically belongs in C(XX)FLAGS,
# but listing it here is more convenient and should be safe.
set(SRA_INTERNAL_CPPFLAGS " -D_GNU_SOURCE ${D_SFX} -DLIBPREFIX=lib -DSHLIBEXT=${DLL}${dll_ext} ${NO_STRICT_ALIASING}")

set(SRA_SDK_SYSLIBS ${CURL_LIBS} ${NETWORK_LIBS} ${BZ2_LIBS} ${Z_LIBS} ${DL_LIBS})

set(SRA_SDK_LIBS ncbi-vdb-read ${BZ2_LIB} ${Z_LIB})
set(SRAXF_LIBS ${SRA_SDK_LIBS})
set(SRA_LIBS ${SRA_SDK_LIBS})
set(BAM_LIBS ${SRA_SDK_LIBS})
set(SRAREAD_LDEP ${SRA_SDK_LIBS})
set(SRAREAD_LIBS sraread ${SRA_SDK_LIBS})


# Makefile.blast_macros.mk
set(BLAST_DB_DATA_LOADER_LIBS ncbi_xloader_blastdb ncbi_xloader_blastdb_rmt)
set(BLAST_FORMATTER_MINIMAL_LIBS xblastformat align_format taxon1 blastdb_format
    gene_info xalnmgr blastxml xcgi xhtml)
set(BLAST_INPUT_LIBS blastinput
    ${BLAST_DB_DATA_LOADER_LIBS} ${BLAST_FORMATTER_MINIMAL_LIBS})

set(BLAST_FORMATTER_LIBS ${BLAST_INPUT_LIBS})

# Libraries required to link against the internal BLAST SRA library
set(BLAST_SRA_LIBS=blast_sra ${SRAXF_LIBS} vxf ${SRA_LIBS})

# BLAST_FORMATTER_LIBS and BLAST_INPUT_LIBS need $BLAST_LIBS
set(BLAST_LIBS xblast xalgoblastdbindex composition_adjustment
xalgodustmask xalgowinmask seqmasks_io seqdb blast_services xobjutil
${OBJREAD_LIBS} xnetblastcli xnetblast blastdb scoremat tables xalnmgr)



# SDBAPI stuff
set(SDBAPI_LIB sdbapi ncbi_xdbapi_ftds ${FTDS_LIB} dbapi dbapi_driver ${XCONNEXT})
set(SDBAPI_LIBS ${FTDS_LIBS})


set(VARSVC_LIBS varsvcutil varsvccli varsvcobj)

set(GPIPE_COMMON_LIBS gpipe_attr gpipe_property gpipe_common)
set(GPIPE_LOADER_LIBS ncbi_xloader_asn_cache asn_cache cache_blob
ncbi_xloader_lds2 lds2 sqlitewrapp
ncbi_xloader_lds lds bdb
${BLAST_DB_DATA_LOADER_LIBS} ${BLAST_LIBS}
${ncbi_xreader_pubseqos2}
submit
ncbi_xloader_csra ncbi_xloader_wgs
${SRAREAD_LIBS})
set(GPIPE_LOADER_THIRDPARTY_LIBS ${SQLITE3_LIBS} ${BERKELEYDB_LIBS}
   ${SRA_SDK_SYSLIBS} ${CMPRS_LIBS})

set(GPIPE_GENCOLL_LIBS xgencoll gpipe_objutil ncbi_xloader_wgs aligndb_reader
 gencoll_client taxon1)
set(GPIPE_ALL_LIBS ${GPIPE_GENCOLL_LIBS}
 ${GPIPE_COMMON_LIBS}
 ${GPIPE_LOADER_LIBS})
set(GPIPE_DBAPI_LIB_STATIC   ncbi_xdbapi_ftds dbapi dbapi_driver)
set(GPIPE_DBAPI_LIB			 ${GPIPE_DBAPI_LIB_STATIC} ${FTDS_LIB})
set(GPIPE_DBAPI_LIBS		 ${FTDS_LIBS})

set(GPIPE_SDBAPI_LIB_STATIC  sdbapi ${GPIPE_DBAPI_LIB_STATIC})
set(GPIPE_SDBAPI_LIB  		 ${GPIPE_SDBAPI_LIB_STATIC} ${FTDS_LIB})
set(GPIPE_SDBAPI_LIBS 		 ${GPIPE_DBAPI_LIBS})



# Entrez Libs
set(ENTREZ_LIBS entrez2cli entrez2)
set(EUTILS_LIBS eutils egquery elink epost esearch espell esummary linkout einfo uilist ehistory)



