#! /bin/sh
#
# $Id$
# Author:  Eugene Vasilchenko, NCBI 


# detect root directory
p="`pwd`"
mp=
while true; do
    d="`basename \"$p\"`"
    p="`dirname \"$p\"`"
    if test -d "$p/src" && test -d "$p/include"; then
        makemodule="$p/src/Makefile.module"
        if test -f "$makemodule"; then
            break
        fi
        makemodule="$NCBI/c++/src/Makefile.module"
        if test -f "$makemodule"; then
            break
        fi
        echo "Makefile.module not found"
        exit 1
    fi
    if test -z "$mp"; then
        mp="$d"
    else
        mp="$d/$mp"
    fi
    if test "$p" = "/"; then
        echo "Root directory not found"
        exit 1
    fi
done
# here $p is root directory, $mp is module path so $p/src/$mp is current dir

# define function which will get variable value from file
# Synopsis: getvar <var_name> <file_name>
getvar () {
    grep "^[ 	]*$1[ 	]*=" "$2" | head -1 |
        sed "s/^[ 	]*$1[ 	]*=[ 	]*//" | sed "s/[ 	]*$//"
}

# detect command arguments
case "$1" in
    *.asn) module="`basename \"$1\" .asn`"; asn="$1";;
    *.module) module="`basename \"$1\" .module`";;
    '') echo "Usage: $0 [module]"; exit 1;;
    *) module="$1";;
esac
shift

if test -f "$module.module"; then
    if test -z "$asn"; then
        asn="`getvar MODULE_ASN \"$module.module\"`"
    fi
    imports="`getvar MODULE_IMPORT \"$module.module\"`"
else
    echo "File $module.module not found. Using defaults..."
fi
if test -z "$asn"; then
    asn="$module.asn"
fi

# now $asn is source ASN.1 file $module.module is module description file
# check files
if test -f "$asn"; then
    :
else
    echo "Source file $asn not found."
    exit 1
fi

# detect datatool program path
if test -n "$NCBI_DATATOOL_PATH"; then
    # NCBI_DATATOOL_PATH variable is set
    if test -x "$NCBI_DATATOOL_PATH/datatool"; then
        datatool="$NCBI_DATATOOL_PATH/datatool"
    else
        echo "$datatool not found"
        exit 1
    fi
elif test -x "$NCBI/c++/Release/bin/datatool"; then
    datatool="$NCBI/c++/Release/bin/datatool"
else
    datatool=datatool
fi

# guess the make command, if it is not specified in the $MAKE env. variable
if test -z "$MAKE" ; then
    case "`uname`" in
      SunOS) MAKE=make;;
      *) MAKE=gmake;;
    esac
fi

# run command
echo $MAKE -f "$makemodule" "MODULE=$module" "MODULE_PATH=$mp" "MODULE_ASN=$asn" "MODULE_IMPORT=$imports" "top_srcdir=$p" "DATATOOL=$datatool" "$@"
$MAKE -f "$makemodule" "MODULE=$module" "MODULE_PATH=$mp" "MODULE_ASN=$asn" "MODULE_IMPORT=$imports" "top_srcdir=$p" "DATATOOL=$datatool" "$@"
