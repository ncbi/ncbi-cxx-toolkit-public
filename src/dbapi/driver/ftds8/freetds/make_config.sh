#! /bin/sh

# $Id$
# Authors:  Denis Vakatov, Vladimir Ivanov, Aaron Ucko
#
###########################################################################
#
#  Auxilary script -- to be called by "./Makefile.config"
#
#  Create the FreeTDS config file using "ncbiconf.h"
#
###########################################################################


ncbiconf="$1/ncbiconf.h"
srcdir="$2"


if test ! -f $ncbiconf ; then
  echo "ERROR:  File $ncbiconf does not exist."
  exit 1
fi

tdsconf_dir="$srcdir/../../../../../include/dbapi/driver/ftds8/freetds"
test -d $tdsconf_dir  ||  exit 0
tdsconf="$tdsconf_dir/tds_config.h"

DEF_NAME="DBAPI_DRIVER_FTDS_FREETDS___FTDS_CONFIG__H"

cat > $tdsconf <<EOF
/*  Generated from "$1/ncbiconf.h"
 */
#ifndef $DEF_NAME
#define $DEF_NAME

#define NCBI_FTDS 8
#define TDS80 1
#define FREETDS_SYSCONFDIR "/etc"
#define FREETDS_SYSCONFFILE "/etc/freetds.conf"
#define FREETDS_POOLCONFFILE "/etc/pool.conf"
#define FREETDS_LOCALECONFFILE "/etc/locales.conf"
EOF

# Grep necessary defines
for d in BSD_COMP HAVE_ARPA_INET_H HAVE_ATOLL HAVE_GETPAGESIZE HAVE_MALLOC_H \
         HAVE_NETINET_IN_H HAVE_SYS_SOCKET_H HAVE_SYS_TYPES_H HAVE_VASPRINTF \
         INADDR_NONE STDC_HEADERS WORDS_BIGENDIAN; do
    grep "^#define $d " $ncbiconf >> $tdsconf
done

# We need special treatment in some cases
sed -ne 's/\(#define HAVE_\)\(GET....BY...._R\) \([0-9]\)/\1FUNC_\2_\3 1/p' \
    $ncbiconf >> $tdsconf
sed -ne 's/#define HAVE_LIBICONV/#define HAVE_ICONV/p' $ncbiconf >> $tdsconf
sed -ne 's/#define SIZEOF_LONG_LONG 8/#define HAVE_INT64 1/p' $ncbiconf >> $tdsconf

cat >> $tdsconf <<EOF

#endif  /* $DEF_NAME */
EOF

touch -r "$ncbiconf" "$tdsconf"

exit $?
