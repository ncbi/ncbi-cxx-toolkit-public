#!/bin/sh
# $Id$

### You can control where the C++ Toolkit's configure script looks for
### various packages by copying or renaming this file to "config.site"
### and then uncommenting and adjusting the relevant settings.

### NOTE: configure reads this file after parsing arguments but before
### setting anything else, so any configuration-dependent settings
### will have to manage with only $with_xxx and the original
### environment.


### Read in systemwide defaults

# . /usr/local/etc/config.site


### Make sure configure can find your favorite development toolchain.  (It
### then hardcodes programs' paths in makefiles, so setting PATH here should
### be sufficient as long as the compiler doesn't rely on finding auxiliary
### programs in $PATH.)

# PATH=/usr/foo/bin:$PATH


### Paths to various external libraries; the defaults are often relative to
### $NCBI, per our in-house setup.  Most of these also have --with-...
###  options that take a path.

# NCBI="/netopt/ncbi_tools"

### Sybase

# SYBASE_PATH="/netopt/Sybase/clients/current"
# SYBASE_LOCAL_PATH="/export/home/sybase/clients/current"

### FreeTDS (we recommend just using the bundled version)

# FTDS_PATH="/netopt/Sybase/clients-mssql/current"
## -L$FTDS_PATH/lib automatically prepended
# FTDS_LIBS="-lsybdb -ltds"

### MySQL (also looks for mysql_config in $PATH)

# MYSQL_PATH="/netopt/mysql/current"
# MYSQL_BINPATH="$MYSQL_PATH/bin"
# mysql_config="$MYSQL_BINPATH/mysql_config"
## Normally obtained from mysql_config, but still settable if necessary...
# MYSQL_INCLUDE="-I$MYSQL_PATH/include"
# MYSQL_LIBS="-L$MYSQL_PATH/lib -lmysqlclient"

### Berkeley DB

# BERKELEYDB_PATH="$NCBI/BerkeleyDB"

### ODBC

# ODBC_PATH="/opt/machine/merant/lib"

### NCBI C Toolkit

# NCBI_C_PATH="$NCBI"

### OpenGL extensions

# OSMESA_PATH="$NCBI/MesaGL"
# GLUT_PATH="$NCBI/glut"

### FLTK 1.1.x

# FLTK_PATH="$NCBI/fltk"
# FLTK_ARCH_PATH="$FLTK_PATH/..."
# FLTK_BINPATH="$FLTK_ARCH_PATH/bin"
# FLTK_LIBPATH="$FLTK_ARCH_PATH/lib"

### wxWindows

# WXWIN_PATH="$NCBI/wxwin"
# WXWIN_ARCH_PATH="$WXWIN_PATH/..."
# WXWIN_BINPATH="$WXWIN_ARCH_PATH/bin"
# WXWIN_LIBPATH="$WXWIN_ARCH_PATH/lib"

### FastCGI

# FASTCGI_PATH="$NCBI/fcgi-current"

### SP

# SP_PATH="$NCBI/SP"

### NCBI SSS libraries

# NCBI_SSS_PATH="$NCBI/sss/BUILD"
# NCBI_SSS_INCLUDE="$NCBI_SSS_PATH/include"

### NCBI PubMed libraries ($bit64_sfx automatically added unless present)

# NCBI_PM_PATH="$NCBI/pubmed"

### ORBacus (CORBA implementation)

# ORBACUS_PATH="$NCBI/corba/OB-4.0.1"

### XML/XSL support
# EXPAT_PATH="$NCBI/expat"
# SABLOT_PATH="$NCBI/Sablot"

### Image libraries

# JPEG_PATH="$NCBI/gd"
# PNG_PATH="$NCBI/gd"
# TIFF_PATH="/usr/sfw"
# XPM_PATH="/usr/X11"

### You shouldn't normally need to set anything below this point.

### Hand-pick particular programs (may include flags; may be overridden in
### some cases)

# CC="mycc"
# CXX="myCC"
# AR="ar"
# RANLIB="ranlib"
# STRIP="strip"
# LORDER="lorder"
# TSORT="tsort"


### Set special flags (normally not needed)

# CPPFLAGS="-DFOO -I/bar"
# LDFLAGS="-L/baz"
# CFLAGS="-g -O ..."
# CXXFLAGS="-g -O ..."
# MTFLAG="-mt"
# DEF_FAST_FLAGS="-O99"
# CFLAGS_DLL="-fPIC"
# CXXFLAGS_DLL="-fPIC"

### Libraries for various things (normally autodetected)

# THREAD_LIBS="-lpthread"
# NETWORK_LIBS="-lsocket -lnsl"
# RESOLVER_LIBS="-lresolv"
# MATH_LIBS="-lm"
# KSTAT_LIBS="-lkstat"
# RPCSVC_LIBS="-lrpcsvc"
# CRYPT_LIBS="-lcrypt"
# DL_LIBS="-ldl"
# RT_LIBS="-lrt"
# ICONV_LIBS="-liconv"
