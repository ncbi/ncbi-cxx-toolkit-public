#! /bin/sh

version=.current

case `pwd` in
*/src/gui/plugin | */src/gui/config | */src/objects/* )
	echo "Generating the asn1 classes in this directory."
	$NCBI/c++${version}/Release/build/new_module.sh || echo "Failed to generate these serialization classes" 
	;;
*/src/objects ) # do all the objects directories
	echo "Generating all asn1 object classes under this directory."
	make builddir=$NCBI/c++${version}/Release/build  ||  echo "Failed to generate object serialization classes" 
	;;
*) # do all the ones I know about
	echo 'Generating All serialization code from ASN.1 specs:'
	cd $HOME/ncbi_cxx/src/objects  ||  echo "Failed:  cd src/objects"
	make builddir=$NCBI/c++${version}/Release/build  ||  echo "Failed to generate object serialization classes"  
	
	cd $HOME/ncbi_cxx/src/gui/plugin  ||  echo "Failed:  cd src/gui/plugin"
	$NCBI/c++${version}/Release/build/new_module.sh  ||  echo "Failed to generate plugin serialization classes" 
	cd $HOME/ncbi_cxx/src/gui/config  ||  echo "Failed:  cd src/gui/config"
	$NCBI/c++${version}/Release/build/new_module.sh  ||  echo "Failed to generate config serialization classes" 
	;;
esac
