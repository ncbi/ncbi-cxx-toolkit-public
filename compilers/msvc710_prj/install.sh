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
incdir="$target"/include
srcdir="$target"/src
altsrc="$target"/altsrc
dbgdir="$target"/Debug
libdir="$target"/Release
bindir="$target"/bin


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
find . -type d -name CVS -exec rm -r {} \;


# Include dir
makedir "$incdir" -p
cp -pr "$builddir"/include/* "$incdir"
cd "$incdir"
find . -type d -name CVS -exec rm -r {} \;


# Source dir
makedir "$srcdir" -p
cp -pr "$builddir"/src/* "$srcdir"
cd "$srcdir"
find . -type d -name CVS -exec rm -r {} \;


# Debug src/inc dir -- put everything (sources, headers) into a single blob
makedir "$altsrc" -p
cd "$srcdir"
x_dirs=`find . -type d -print`
for ddd in $x_dirs ; do
  cd "$srcdir/$ddd"
  cp -p *.[ch]pp "$altsrc" >/dev/null 2>&1
  cp -p *.[ch]   "$altsrc" >/dev/null 2>&1
  cp -p *.inl    "$altsrc" >/dev/null 2>&1
done

cd "$incdir"
x_dirs=`find . -type d -print`
for ddd in $x_dirs ; do
  cd "$incdir/$ddd"
  cp -p *.* "$altsrc" >/dev/null 2>&1
done


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
