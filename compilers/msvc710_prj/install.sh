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
compiler="${3:-msvc710}"
compiler="${compiler}_prj"


if test -n "$3" ; then
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
dbgdir="$target"/Debug
libdir="$target"/Release
bindir="$target"/bin
cldir="$target"/compilers


# Check
test -d "$builddir"  ||  error "Absent build dir \"$builddir\""


# Reset the public directory
test -d "$target"  &&  find "$target" -type f -exec rm -f {} \; >/dev/null 2>&1
test -d "$target"  ||  mkdir -p "$target"
test -d "$target"  ||  error "Cannot create target dir \"$target\""


# Documentation
makedir "$docdir" -p
cp -pr "$builddir"/doc/* "$docdir"
cd "$docdir"
find . -type d -name CVS -exec rm -r {} \; >/dev/null 2>&1


# Scripts
makedir "$scriptdir" -p
cp -pr "$builddir"/scripts/* "$scriptdir"
cd "$scriptdir"
find . -type d -name CVS -exec rm -r {} \; >/dev/null 2>&1


# Include dir
makedir "$incdir" -p
cp -pr "$builddir"/include/* "$incdir"
cd "$incdir"
find . -type d -name CVS -exec rm -r {} \; >/dev/null 2>&1


# Source dir
makedir "$srcdir" -p
cp -pr "$builddir"/src/* "$srcdir"
cd "$srcdir"
find . -type d -name CVS -exec rm -r {} \; >/dev/null 2>&1


# Libraries
for i in 'Debug' 'Release' ; do
  for j in '' 'DLL' ; do
    if test -d "$builddir"/compilers/$compiler/static/lib/$i$j ; then
      makedir "$target/$i$j" -p
      cd "$builddir"/compilers/$compiler/static/lib/$i$j
      cp -p *.lib "$target/$i$j"
    fi
    if test -d "$builddir"/compilers/$compiler/dll/bin/$i$j ; then
      cd "$builddir"/compilers/$compiler/dll/bin/$i$j
      cp -p *.lib *.dll *.exp "$target/$i$j"
    fi
  done
done


# Executables
makedir "$bindir" -p
for i in 'DLL' '' ; do
  if test -d "$builddir"/compilers/$compiler/static/bin/Release$i ; then
    if ls "$builddir"/compilers/$compiler/static/bin/Release$i/*.exe >/dev/null 2>&1 ; then
      cd "$builddir"/compilers/$compiler/static/bin/Release$i
      cp -p *.exe *.dll *.exp "$bindir"
      break
    fi
  fi
done


# Compiler dir (copy all .pdb files for debug purposes)
makedir "$cldir" -p
pdb_files=`find "$builddir"/compilers -type f -name '*.pdb' 2>/dev/null`
cd "$cldir"
for pdb in $pdb_files ; do
  rel_dir=`echo $pdb | sed -e "s|$builddir/compilers/||" -e 's|/[^/]*$||'`
  mkdir -p $rel_dir -p > /dev/null 2>&1
  cp -pr "$pdb" "$rel_dir"
done

# Compiler dir (other common stuff)
makedir "$cldir"/$compiler/static -p
makedir "$cldir"/$compiler/dll -p
cp -p "$builddir"/compilers/$compiler/*        "$cldir"
cp -p "$builddir"/compilers/$compiler/static/* "$cldir"/$compiler/static
cp -p "$builddir"/compilers/$compiler/dll/*    "$cldir"/$compiler/dll


# Gbench public installation
#for i in ReleaseDLL DebugDLL; do
#  if test -d "$builddir"/compilers/$compiler/dll/bin/"$i" ; then
#    makedir "$bindir"/gbench/"$i" -p
#    cp -pr "$builddir"/compilers/$compiler/dll/bin/$i/gbench/* "$bindir"/gbench/"$i"
#  fi
#done


# CVS checkout info file
cp -p "$builddir"/cvs_info "$target"


exit 0
