if ("${CMAKE_BUILDTYPE}" EQUAL "Debug")
    add_definitions(-D_DEBUG)
ELSE()
    add_definitions(-DNDEBUG)
ENDIF()
add_definitions(-D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64)

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

include(FindBoost)
set(CMAKE_INCLUDE_PATH $ENV{NCBI}/boost-1.41.0/include)
set(CMAKE_LIBRARY_PATH $ENV{NCBI}/boost-1.41.0/lib)
find_package(Boost)
if (NOT Boost_FOUND)
    message(WARNING "BOOST not found...")
endif (NOT Boost_FOUND)
set(BOOST_INCLUDE ${Boost_INCLUDE_DIRS})

#
# Image Libraries
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

#
# Threading libraries
find_package(Threads)
if (CMAKE_USE_PTHREADS_INIT)
    add_definitions(-D_MT -D_REENTRANT -D_THREAD_SAFE)
    set(NCBI_POSIX_THREADS 1)
endif (CMAKE_USE_PTHREADS_INIT)

#
# Compression libraries
include(FindZLIB)
find_package(ZLIB)

set(Z_INCLUDE ${ZLIB_INCLUDE_DIRS})
set(Z_LIBS ${ZLIB_LIBRARIES})
set(Z_LIB)

include(FindBZip2)
find_package(BZip2)

set(BZ2_INCLUDE ${BZIP2_INCLUDE_DIRS})
set(BZ2_LIBS ${BZIP2_LIBRARIES})
set(BZ2_LIB)

FIND_PATH(LZO_INCLUDE_DIR lzo/lzoconf.h
          ${LZO_ROOT}/include/lzo/
          $ENV{NCBI}/lzo-2.05/include/
          /usr/include/lzo/
          /usr/local/include/lzo/
          /sw/lib/lzo/
          /sw/local/lib/lzo/
         )
FIND_LIBRARY(LZO_LIBRARIES NAMES lzo2
             PATHS
             ${LZO_ROOT}/lib
             $ENV{NCBI}/lzo-2.05/lib64/
             $ENV{NCBI}/lzo-2.05/lib/
             /sw/lib
             /sw/local/lib
             /usr/lib
             /usr/local/lib
            )
set(LZO_INCLUDE ${LZO_INCLUDE_DIR})
set(LZO_LIBS ${LZO_LIBRARIES})

set(CMPRS_INCLUDE ${Z_INCLUDE} ${BZ2_INCLUDE} ${LZO_INCLUDE})
set(CMPRS_LIBS ${Z_LIBS} ${BZ2_LIBS} ${LZO_LIBS})
set(COMPRESS_LIBS xcompress ${CMPRS_LIBS})

#
# PCRE
include(FindPCRE)
find_package(PCRE)
set(PCRE_INCLUDE ${PCRE_INCLUDE_DIR})
set(PCRE_LIBS ${PCRE_LIBRARIES})
set(PCRE_LIB)

#
# GNU TLS
#include(FindGnuTLS)
find_package(GnuTLS)
set(GNUTLS_INCLUDE ${GNUTLS_INCLUDE_DIR})
set(GNUTLS_LIBS ${GNUTLS_LIBRARIES})

#
# NCBI C Toolkit stuff
set(NCBI_C_INCLUDE  $ENV{NCBI}/include64)
set(NCBI_C_LIBPATH  "-L/netopt/ncbi_tools64/lib64")
set(NCBI_C_ncbi     ncbi)

#
# SQLITE3
set(SQLITE3_INCLUDE $ENV{NCBI}/sqlite3/include)
set(SQLITE3_LIBS    -L"$ENV{NCBI}/sqlite3/lib" -lsqlite3)

#
# BerkeleyDB
set(BERKELEYDB_INCLUDE $ENV{NCBI}/BerkeleyDB/include)
set(BERKELEYDB_LIBS    -L"$ENV{NCBI}/BerkeleyDB/${CMAKE_BUILD_TYPE}" -ldb)

#
# wxWidgets
set(foo "${CMAKE_PREFIX_PATH}")
#set(CMAKE_PREFIX_PATH "$ENV{NCBI}/wxwidgets/${CMAKE_BUILD_TYPE}/bin")
set(CMAKE_PREFIX_PATH "$ENV{NCBI}/wxGTK/DebugMT64/bin")
FIND_PACKAGE(wxWidgets
    COMPONENTS core gl base
    OPTIONAL)
set(WXWIDGETS_INCLUDE "${wxWidgets_INCLUDE_DIRS}")
set(WXWIDGETS_LIBS "${wxWidgets_LIBRARIES}")
set(CMAKE_PREFIX_PATH "${foo}")
set(foo)

#
# FastCGI stuff
set(FASTCGI_INCLUDE $ENV{NCBI}/fcgi-current/include64)

#
# Sybase stuff
set(SYBASE_INCLUDE /export/home/sybase/clients/12.5-64bit/include)
set(SYBASE_LIBS   -L/export/home/sybase/clients/12.5-64bit/lib64 -Wl,-rpath,/export/home/sybase/clients/12.5-64bit/lib64 -lblk_r64 -lct_r64 -lcs_r64 -lsybtcl_r64 -lcomn_r64 -lintl_r64)
# HACK: this should not be a global definition; it should be restricted to only places in which FreeTDS / Sybase is used
add_definitions(-DSYB_LP64)

#
# FTDS stuff
set(FTDS64_INCLUDE ${includedir}/dbapi/driver/ftds64/freetds ${includedir}/dbapi/driver/ftds64/freetds)
set(FTDS64_LIBS -lgssapi_krb5 -lkrb5 -lk5crypto -lcom_err)
set(FTDS64_LIB ct_ftds64 tds_ftds64)
set(ftds64 ftds)

set(FTDS_INCLUDE ${FTDS64_INCLUDE})
set(FTDS_LIBS ${FTDS64_LIBS})
set(FTDS_LIB ${FTDS64_LIB})


if(HAVE_LIBDL)
    set(DL_LIBS -ldl)
endif(HAVE_LIBDL)
set (ORIG_LIBS -lrt -lm  -lpthread)

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

#
# Specialized libs settings
#
set(LIBS ${LIBS} ${DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})

set(XCONNEXT xconnext)

set(LOCAL_LBSM ncbi_lbsm ncbi_lbsmd ncbi_lbsm_ipc)

# SDBAPI stuff
set(SDBAPI_LIB sdbapi ncbi_xdbapi_ftds ${FTDS_LIB} dbapi dbapi_driver ${XCONNEXT})
set(SDBAPI_LIBS ${FTDS_LIBS})

# SEQ_LIBS
set(SEQ_LIBS seq seqcode sequtil)

# Object manager Libs
set(SOBJMGR_LIBS
    xobjmgr genome_collection seqset seqedit ${SEQ_LIBS} pub medline biblio general-lib xser xconnext xconnect xutil xncbi)
set(OBJMGR_LIBS
    ncbi_xloader_genbank
    ncbi_xreader_id1 id1
    ncbi_xreader_pubseqos2 ncbi_xreader_id2 id2 seqsplit
    ncbi_xreader_cache
    ${XCONNEXT} ${COMPRESS_LIBS} ${SOBJMGR_LIBS})

set(OBJREAD_LIBS xobjread variation creaders submit)

set(XFORMAT_LIBS xformat gbseq submit mlacli mla medlars pubmed)

# Entrez Libs
set(ENTREZ_LIBS entrez2cli entrez2)
set(EUTILS_LIBS eutils egquery elink epost esearch espell esummary linkout einfo uilist ehistory)

# BLAST libs
set(BLAST_FORMATTER_MINIMAL_LIBS
    xblastformat align_format taxon1 blastdb_format 
    gene_info xalnmgr blastxml xcgi xhtml)
set(BLAST_DB_DATA_LOADER_LIBS
    ncbi_xloader_blastdb_rmt ncbi_xloader_blastdb)
set(BLAST_INPUT_LIBS
    blastinput
    ${BLAST_DB_DATA_LOADER_LIBS} ${BLAST_FORMATTER_MINIMAL_LIBS})
set(BLAST_FORMATTER_LIBS
    ${BLAST_INPUT_LIBS})

set(BLAST_LIBS
    xblast blast xalgoblastdbindex composition_adjustment
    xalgodustmask xalgowinmask seqmasks_io seqdb blast_services xobjutil
    xobjread creaders xnetblastcli xnetblast blastdb scoremat tables xalnmgr)

#
# Compatbility layer
#
set(WXWIDGETS_INCLUDE ${wxWidgets_INCLUDE_DIRS})
set(WXWIDGETS_LIBS ${wxWidgets_LIBRARIES})
