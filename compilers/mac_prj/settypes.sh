#!/bin/sh
# For the NCBI C and C++ toolkits, reset the Macintosh file types properly.

#  starting with the current folder, recursively looks for files with certain extensions
#  to reset their file and creator types.
#  For certain extensions only looks in certain folders.

# BUG: chokes on file or path names with spaces.


PATH=$PATH:/Developer/Tools
export PATH

# set the types of the source files.
find .  \( -name '*.h' -or -name '*.hpp' -or -name '*.H' -or -name '*.c' -or -name '*.cpp' -or -name '*.cxx' -or -name '*.r' \) | \
	sed "s/\(.*\)/'\1'/" | \
	xargs SetFile -c 'CWIE' -t 'TEXT' 

# Metrowerks project files.
find .  \( -path '*/link/macmet/*.mcp' -or  -path '*/compilers/mac_prj/*.mcp' \) | \
	sed "s/\(.*\)/'\1'/" | \
	xargs SetFile -c 'CWIE' -t 'MMPr' 

# Our Applescripts that build Metrowerks projects.
find .  \( -path '*/make/*.met'  -or -path '*/compilers/mac_prj/*.met' \) | \
	sed "s/\(.*\)/'\1'/" | \
	xargs SetFile -c 'ToyS' -t 'TEXT' 

# Metrowerks property list compiler files.
find .  \( -path '*/link/macmet/*.plc' -or -path '*/compilers/mac_prj/*.plc' \) | \
	sed "s/\(.*\)/'\1'/" | \
	xargs SetFile -c 'CWIE' -t 'TEXT' 
