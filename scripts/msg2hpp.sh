#! /bin/sh

# $Id$
# Author:  Vladimir Ivanov, NCBI 
#
###########################################################################
#
# Make C/C++ header for error message file (*.msg).
#
# Usage:
#    msg2hpp.sh <filename_msg> [<filename_header>]
#
# Note:
#    Read error codes for messages from <filename_msg> and generate
#    the header file. If the <filename_header> argument has been omitted
#    then the value of MODULE definition will be used for it instead.
#
###########################################################################


# Check number of arguments
if test $# -lt 1 -o $# -gt 2 ; then
	cat > $x_out <<EOF
Invalid number of arguments.

Usage: $0 <filename_msg> [<filename_header>]

EOF
	exit 1;
fi

# Parameters
filename_msg=$1
filename_header=${2-?}

# Check existence of the message file
if [ ! -f "$filename_msg" ] ; then
    echo "ERROR: Message file \"$filename_msg\" not found."
	exit 1
fi

# Get module name if file header name is absent
module=`grep '^ *MODULE ' "$filename_msg" | sed -e 's/^.*MODULE *//' -e 's/ *//g'`
if [ -z "$module" ] ; then
    echo "ERROR: Module name not defined."
	exit 2
fi

if [ "x$filename_header" = "x?" ] ; then
	filename_header="$module.h"
	if [ -z "$filename_header" ] ; then
	    echo "ERROR: Output file name not found."
		exit 2
	fi
fi

# Write header
cat > $filename_header <<EOF
#ifndef __MODULE_${module}__
#define __MODULE_${module}__

EOF

# Read error names
rows=`grep '^ *$[$^]' "$filename_msg" | sed -e 's/ //g'`

err_name=""
err_subname=""
err_code=0
err_subcode=0

# Process each row from list
for row in $rows; do
	echo $row | sed 
	if [ `echo $row | grep '$[$]'` ] ; then
		row=`echo "$row" | sed -e 's/^\$[$]//' -e 's/:.*$//'`
		err_name=`echo "$row" | sed -e 's/,.*$//'`
		err_code=`echo "$row" | sed -e "s/^.*$err_name,//" -e 's/,.*$//`
		test -z "$err_code"  &&  err_code="-1"
		err_subname=""
		err_subcode=0
	else
		if [ -z "$err_name" ] ; then
			echo "ERROR: First level name for subname \"$err_subname\" is not defined."
			echo "Line : $row"
			exit 3
		fi
		row=`echo "$row" | sed -e 's/^\$[\^]//' -e 's/:.*$//'`
		err_subname=`echo "$row" | sed -e 's/,.*$//'`
		err_subcode=`echo "$row" | sed -e "s/^.*$err_subname,//" -e 's/,.*$//`
	fi
    # Print message
	if [ -z "$err_subname" ] ; then
	 	name="$err_name"
	else
	 	name="${err_name}_$err_subname"
	fi
	printf "#define %-30s %10s" "ERR_$name" "$err_code,$err_subcode" >> $filename_header
	echo >> $filename_header
done

# Finalize
cat >> $filename_header <<EOF

#endif
EOF

exit 0
