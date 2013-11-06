#! /bin/sh
# $Id$
# Authors:  Denis Vakatov    (vakatov@ncbi.nlm.nih.gov)
#           Anton Lavrentiev (lavr@ncbi.nlm.nih.gov)
#
# Deploy sources, headers, libraries and executables for the further use
# by the "external" users' projects


# Cmd.-line args  -- source and destination
script="$0"
builddir="$1"
target="$2"
compiler="${3:-vs2012}"

# Real number of argument is 2.
# The 3th argument do not used here (32|64-bit architecture),
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


# Derive the destination dirs
docdir="$target"/doc
scriptdir="$target"/scripts
incdir="$target"/include
srcdir="$target"/src
libdir="$target"/lib
bindir="$target"/bin
cldir="$target"/compilers
logdir="$target"/logs
tmpdir="$target"/tmp


install()
{
    test -d "$1"  ||  return;
    makedir "$2" -p
    tmp_cwd=`pwd`
    cd "$1"
    find . -type f |
    grep -v '/\.svn/' > "$tmpdir"/flist
    tar cf - -T "$tmpdir"/flist | (cd "$2" ; tar xf - )
    cd "$tmp_cwd"
}



# Check
test -d "$builddir"  ||  error "Absent build dir \"$builddir\""


# Reset the public directory
test -d "$target"  &&  find "$target" -type f -exec rm -f {} \; >/dev/null 2>&1
makedir "$target" -p
makedir "$tmpdir" -p


# Documentation
echo "[`basename $script`] Installing documentation..."
install "$builddir/doc" "$docdir"

# Scripts
echo "[`basename $script`] Installing scripts..."
install "$builddir/scripts" "$scriptdir"

# Include dir
echo "[`basename $script`] Installing include files..."
install "$builddir/include" "$incdir"

# Source dir
echo "[`basename $script`] Installing source files..."
install "$builddir/src" "$srcdir"

# Build logs
echo "[`basename $script`] Installing build logs..."
install "$builddir/logs" "$logdir"

rm -rf "$tmpdir"


# Libraries
echo "[`basename $script`] Installing libraries..."
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
echo "[`basename $script`] Installing executables..."
makedir "$bindir" -p
for i in 'DLL' '' ; do
  if test -d "$builddir"/compilers/$compiler/static/bin/Release$i ; then
    cd "$builddir"/compilers/$compiler/static/bin/Release$i
    if ls *.exe >/dev/null 2>&1 ; then
      cp -p *.exe *.dll *.exp *.manifest "$bindir"
      break
    fi
  fi
done


# Install additional files (scripts and etc) into binary directory
cp -p "$builddir"/src/app/blast/legacy_blast.pl "$bindir"


# Gbench public installation
echo "[`basename $script`] Installing Gbench..."
for i in ReleaseDLL DebugDLL; do
  if test -d "$builddir"/compilers/$compiler/dll/bin/"$i" ; then
    cp -pr "$builddir"/compilers/$compiler/dll/bin/$i/gbench "$bindir"
    break
  fi
done


# Compiler dir (copy all .pdb and configurable files files for debug purposes)
echo "[`basename $script`] Installing .pdb files..."
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
cp -p  "$builddir"/compilers/$compiler/*          "$cldir"/$compiler
cp -p  "$builddir"/compilers/$compiler/static/*   "$cldir"/$compiler/static
cp -p  "$builddir"/compilers/$compiler/dll/*      "$cldir"/$compiler/dll
for b in 'static' 'dll' ; do
  cp -pr "$builddir"/compilers/$compiler/$b/inc "$cldir"/$compiler/$b
  for c in 'DebugMT' 'ReleaseMT' 'DebugDLL' 'ReleaseDLL'; do
    cp -pr "$builddir"/compilers/$compiler/$b/$c "$cldir"/$compiler/$b
  done 
done 

# Makefile.*.mk files
find "$builddir/src" -type f -name 'Makefile.*.mk' -exec cp -pr {} "$srcdir"/build-system/ \;

# Copy info files
cp -p "$builddir"/*_info "$target"

exit 0
