#!/bin/sh

if [ -z "$1" -o -z "$2" -o -z "$3" ]; then
  echo "Usage:"
  echo "   thirdpartylib <NCBI C++ Toolkit directory> <Temporary working directory> <Installation directory>"
  echo ""
  echo "   example:"
  echo "     thirdpartylib /Users/vlad/c++ /Users/vlad/tmp /Users/vlad/sw"
  exit -1
else
  NCBICPP=$1
  TEMP=$2
  DEST=$3
fi

if [ -f $TEMP ]; then
  echo "Temporary directory does not exist"
  exit -1
fi


LIBS="
 giflib-4.1.3*http://cogent.dl.sourceforge.net/sourceforge/libungif
 libpng-1.2.8-config*cogent.dl.sourceforge.net/sourceforge/libpng
 tiff-3.7.1*ftp://ftp.remotesensing.org/libtiff/old
 sqlite-2.8.16*http://www.sqlite.org
 db-4.3.27.NC*http://downloads.sleepycat.com
 fltk-1.1.6-source*http://cogent.dl.sourceforge.net/sourceforge/fltk"


cd $TEMP
if [ "$4" = "download" ]; then
	for lib in $LIBS; do
		name=`echo $lib | awk -F* '{ print $1 }'` 
		url=`echo $lib | awk -F* '{ print $2 }'` 

		filename=$name".tar.gz"
		fullurl=$url"/"$filename
		echo Downloading: $filename from $url
		`curl --disable-epsv -o $TEMP/$filename $fullurl`
		if [ "$?" != "0" ]; then  
			echo Could not download $filename
			exit -1 
		fi
		echo $filename downloaded successfully
	done
	echo All libraries downloaded!
fi 


cd $TEMP
for lib in $LIBS; do
	name=`echo $lib | awk -F* '{ print $1 }'` 
	filename=$name".tar.gz"
	echo Processing $filename
	
	conf="--prefix="$DEST
	echo Unpacking $filename
	tar -xzvf $filename
	
	# Apply NCBI patches to FLTK
	if [ $name = "fltk-1.1.6-source" ]; then
		name="fltk-1.1.6"
		cd $name
		echo Appling NCBI FLTK patches
		for patch_file in `ls $NCBICPP/src/gui/patches/fltk/*1.1.6.patch`; do
		   echo "Applying patch: $patch_file"
		   patch -p1 <$patch_file
		done
		
		conf="--prefix="$DEST" --enable-shared --enable-threads --disable-localpng --disable-localjpeg --disable-localzlib"
		CPPFLAGS="-I$DEST/include"
		LDFLAGS="-L$DEST/lib"
		CFLAGS=$CPPFLAGS
		CXXFLAGS="$CPPFLAGS $LDFLAGS"
		export CPPFLAGS LDFLAGS CFLAGS CXXFLAGS
		cd $TEMP
	fi
	
	if [ $name = "giflib-4.1.3" ]; then
		conf="--prefix="$DEST" --with-x=no"
	fi

	
	echo Configuring in $name
	if [ $name = "db-4.3.27.NC" ]; then
		cd $name/build_unix
		../dist/configure $conf
	else
		cd $name
		./configure $conf
	fi
	
	echo Make and Install in $name
	make
	make install
	if [ "$?" != "0" ]; then  
		echo Error installing $filename. Check the log output.
		exit -1 
	fi
	cd $TEMP
done

echo "All libraries were successfully installed"
exit 0
