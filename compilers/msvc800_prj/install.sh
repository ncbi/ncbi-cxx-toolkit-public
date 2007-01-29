#! /bin/sh
# $Id$
# Authors:  Denis Vakatov    (vakatov@ncbi.nlm.nih.gov)
#           Anton Lavrentiev (lavr@ncbi.nlm.nih.gov)
#
# Deploy sources, headers, libraries and executables for the further use
# by the "external" users' projects


# Cmd.-line args  -- source and destination
script="$0"
builddir="${1:-//u/coremake/cxx}"
target="${2:-//u/coremake/public/cxx.last}"
compiler="${3:-msvc800}"
compiler="${compiler}_prj"

# Real number of argument is 2.
# The 3th argument don not used here (32|64-bit architecture),
# but is needed for master installation script.
if test -n "$4" ; then
  echo "USAGE:  `basename $script` [build_dir] [install_dir]"
fi


error()
{
  echo "[`basename $script`] ERROR:  $1"
  exit 1
}


makedir()
{
  test -d "$1"  ||  mkdir $2 "$1"  ||  error "Cannot create \"$1\""
}


echo "[`basename $script`] NCBI C++:  \"$builddir\" to \"$target\"..."
sleep 2


# Derive the destination dirs
docdir="$target"/doc
scriptdir="$target"/scripts
incdir="$target"/include
srcdir="$target"/src
libdir="$target"/lib
bindir="$target"/bin
cldir="$target"/compilers


# Check
test -d "$builddir"  ||  error "Absent build dir \"$builddir\""


# Reset the public directory
test -d "$target"  &&  find "$target" -type f -exec rm -f {} \; >/dev/null 2>&1
makedir "$target" -p


# Documentation
makedir "$docdir" -p
cp -pr "$builddir"/doc/* "$docdir"
cd "$docdir"
find . -type d -name ".svn" -exec rm -rf {} \; >/dev/null 2>&1


# Scripts
makedir "$scriptdir" -p
cp -pr "$builddir"/scripts/* "$scriptdir"
cd "$scriptdir"
find . -type d -name ".svn" -exec rm -rf {} \; >/dev/null 2>&1


# Include dir
makedir "$incdir" -p
cp -pr "$builddir"/include/* "$incdir"
cd "$incdir"
find . -type d -name ".svn" -exec rm -rf {} \; >/dev/null 2>&1


# Source dir
makedir "$srcdir" -p
cp -pr "$builddir"/src/* "$srcdir"
cd "$srcdir"
find . -type d -name ".svn" -exec rm -rf {} \; >/dev/null 2>&1


# Libraries
for i in 'Debug' 'Release' ; do
  for j in '' 'DLL' ; do
    for b in 'static' 'dll' ; do

      if test -d "$builddir"/compilers/$compiler/$b/lib/$i$j ; then
        makedir "$libdir/$b/$i$j" -p
        cd "$builddir"/compilers/$compiler/$b/lib/$i$j
        cp -p *.lib "$libdir/$b/$i$j"
      fi
      if test "$b"=='dll' ; then
        if test -d "$builddir"/compilers/$compiler/$b/bin/$i$j ; then
          makedir "$libdir/$b/$i$j" -p
          cd "$builddir"/compilers/$compiler/$b/bin/$i$j
          cp -p *.lib *.dll *.exp "$libdir/$b/$i$j"
        fi
      fi
    done
  done
done


# Executables
makedir "$bindir" -p
for i in 'DLL' '' ; do
  if test -d "$builddir"/compilers/$compiler/static/bin/Release$i ; then
    cd "$builddir"/compilers/$compiler/static/bin/Release$i
    if ls *.exe >/dev/null 2>&1 ; then
      cp -p *.exe *.dll *.exp "$bindir"
      break
    fi
  fi
done

# Gbench public installation
for i in ReleaseDLL DebugDLL; do
  if test -d "$builddir"/compilers/$compiler/dll/bin/"$i" ; then
    cp -pr "$builddir"/compilers/$compiler/dll/bin/$i/gbench "$bindir"
    break
  fi
done


# Compiler dir (copy all .pdb and configurable files files for debug purposes)
makedir "$cldir" -p
pdb_files=`find "$builddir"/compilers -type f -a \( -name '*.pdb' -o  -name '*.c' -o  -name '*.cpp' \) 2>/dev/null`
cd "$cldir"
for pdb in $pdb_files ; do
  rel_dir=`echo $pdb | sed -e "s|$builddir/compilers/||" -e 's|/[^/]*$||'`
  makedir "$rel_dir" -p
  cp -pr "$pdb" "$rel_dir"
done

# Compiler dir (other common stuff)
makedir "$cldir"/$compiler/static -p
makedir "$cldir"/$compiler/dll -p
cp -p "$builddir"/compilers/$compiler/*        "$cldir"/$compiler
cp -p "$builddir"/compilers/$compiler/static/* "$cldir"/$compiler/static
cp -p "$builddir"/compilers/$compiler/dll/*    "$cldir"/$compiler/dll


# Copy checkout info file
cp -p "$builddir"/checkout_info "$target"


exit 0
