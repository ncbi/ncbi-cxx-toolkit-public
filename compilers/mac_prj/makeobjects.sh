#! /bin/sh


case `pwd` in
*/src/gui/plugin | */src/gui/config )]
	echo "Generating the asn1 classes in this directory."
	$NCBI/c++/Release/build/new_module.sh || echo "Failed to generate these serialization classes" ;
	;;
*/src/objects ) # do all the objects directories
	echo "Generating all asn1 object classes under this directory."
	{ make builddir=$NCBI/c++/Release/build  ||  echo "Failed to generate object serialization classes" ; } \
	| grep '\-m  *[a-zA-Z0-9_][a-zA-Z0-9_]*\.asn ' \
	| sed 's%^.*-m  *\([a-zA-Z0-9_][a-zA-Z0-9_]*\.asn\).*%  \1%g'
	;;
*) # do all the ones I know about
	echo 'Generating All serialization code from ASN.1 specs:'
	cd $HOME/ncbi_cxx/src/objects  ||  echo "Failed:  cd src/objects"
	{ make builddir=$NCBI/c++/Release/build  ||  echo "Failed to generate object serialization classes" ; } \
	| grep '\-m  *[a-zA-Z0-9_][a-zA-Z0-9_]*\.asn ' \
	| sed 's%^.*-m  *\([a-zA-Z0-9_][a-zA-Z0-9_]*\.asn\).*%  \1%g'
	
	cd $HOME/ncbi_cxx/src/gui/plugin  ||  echo "Failed:  cd src/gui/plugin"
	{ $NCBI/c++/Release/build/new_module.sh  ||  echo "Failed to generate plugin serialization classes" ; } \
		| grep '\-m  *[a-zA-Z0-9_][a-zA-Z0-9_]*\.asn ' \
		| sed 's%^.*-m  *\([a-zA-Z0-9_][a-zA-Z0-9_]*\.asn\).*%  \1%g'
	cd $HOME/ncbi_cxx/src/gui/config  ||  echo "Failed:  cd src/gui/config"
	{ $NCBI/c++/Release/build/new_module.sh  ||  echo "Failed to generate config serialization classes" ; } \
		| grep '\-m  *[a-zA-Z0-9_][a-zA-Z0-9_]*\.asn ' \
		| sed 's%^.*-m  *\([a-zA-Z0-9_][a-zA-Z0-9_]*\.asn\).*%  \1%g'
	;;
esac
