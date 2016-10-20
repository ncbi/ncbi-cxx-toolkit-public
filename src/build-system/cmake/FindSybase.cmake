# Find Sybase

FIND_PATH( SYBASE_INCLUDE_DIR sybdb.h 
  PATHS /export/home/sybase/clients/12.5-64bit/include
  NO_DEFAULT_PATH )

IF (SYBASE_INCLUDE_DIR)
  set (SYBASE_INCLUDE ${SYBASE_INCLUDE_DIR})
  
  FIND_LIBRARY( SYBASE_LIBRARY NAMES sybdb
    PATHS "/export/home/sybase/clients/12.5-64bit/lib64/"
    NO_DEFAULT_PATH )

  IF (SYBASE_LIBRARY)
    set(SYBASE_FOUND true)
    set(HAVE_LIBSYBASE true)
    #set(SYBASE_LIBS   -L/opt/sybase/clients/15.7-64bit/OCS-15_0/lib -Wl,-rpath,/export/home/sybase/clients/12.5-64bit/lib64 -lblk_r64 -lct_r64 -lcs_r64 -lsybtcl_r64 -lcomn_r64 -lintl_r64)
    set(SYBASE_LIBS   -L/opt/sybase/clients/15.7-64bit/OCS-15_0/lib -Wl,-rpath,/opt/sybase/clients/15.7-64bit/OCS-15_0/lib -lsybblk_r64 -lsybct_r64 -lsybcs_r64 -lsybtcl_r64 -lsybcomn_r64 -lsybintl_r64 -lsybunic64)
    #set(SYBASE_DBLIBS  -L/export/home/sybase/clients/12.5-64bit/lib64 -Wl,-rpath,/export/home/sybase/clients/12.5-64bit/lib64 -lsybdb64)
    set(SYBASE_DBLIBS   -L/opt/sybase/clients/15.7-64bit/OCS-15_0/lib -Wl,-rpath,/opt/sybase/clients/15.7-64bit/OCS-15_0/lib -lsybdb64 -lsybunic64)
  ELSE (SYBASE_LIBRARY)
    MESSAGE(WARNING "Include ${SYBASE_INCLUDE}/sysdb.h found, but no library in /export/home/sybase/clients/12.5-64bit/lib64")
  ENDIF (SYBASE_LIBRARY)
ENDIF (SYBASE_INCLUDE_DIR)
