#! /bin/sh
# $Id$
# Author:  Vladimir Ivanov (ivanov@ncbi.nlm.nih.gov)
#
# Build C++ Toolkit.


########### Arguments

script="$0"
cfgs="${1:-DebugDLL ReleaseDLL}"
arch="$2"


########### Global variables

dirs='static dll'
sol_static="ncbi_cpp.sln gui\ncbi_gui.sln"
#sol_dll="ncbi_cpp_dll.sln gui\ncbi_gui_dll.sln gbench\ncbi_gbench.sln"
sol_dll="ncbi_cpp_dll.sln gui\ncbi_gui_dll.sln"
timer="date +'%H:%M'"


########## Functions

error()
{
  echo "[`basename $script`] ERROR:  $1"
  exit 1

}

generate_msvc8_error_check_file() {
  cat <<-EOF >$1
	/.*--* (Reb|B)uild( All | )started: Project:/ {
	  expendable = ""
	}

	/^EXPENDABLE project/ {
	  expendable = \$0
	}

	/(^| : |^The source )([fatal error]* [CDULNKPRJVT]*[0-9]*: |The .* are both configured to produce |Error executing )/ {
	if (!expendable) {
	  print \$0
	  exit
	  }
	}
	EOF
}


########## Main

# Get build dir
build_dir=`dirname $script`
build_dir=`(cd "$build_dir"; pwd)`

if [ ! -d $build_dir ] ; then
  error "Build directory $build_dir not found"
  exit 1
fi
cd $build_dir

for cfg in $cfgs ; do
    if [ $cfg = Release -o $cfg = Debug ] ; then
       error "$cfg configuration is not buildable on this platform." 
    fi
done


# Configuration to build configure
cfg_configure='ReleaseDLL'
out=".build.$$"


# Configure

for dir in $dirs ; do
   if [ $dir = dll ] ; then
     test $cfg_configure != ReleaseDLL -a $cfg_configure != DebugDLL  &&  continue  
   fi
   sols=`eval echo "$"sol_${dir}""`
   for sol in $sols ; do
     alias=`echo $sol | sed -e 's|\\\\.*$||g' -e 's|_.*$||g'`
     start=`eval $timer`
     echo Start time: $start
     echo "INFO: Configure \"$dir\\$alias\""
     echo "Command line: " $build_dir/build_exec.bat "$dir\\build\\$sol" build "$arch" "$cfg_configure" "-CONFIGURE-" $out
     $build_dir/build_exec.bat "$dir\\build\\$sol" build "$arch" "$cfg_configure" "-CONFIGURE-" $out
     status=$?
# don't need to cat here, because msdev2005 double output to console also
#     cat $out
     rm -f $out >/dev/null 2>&1
     if [ $status -ne 0 ] ; then
       exit 3
     fi
     echo "Build time: $start - `eval $timer`"
   done
done


# Generate errors check script

check_awk=$build_dir/build_check.awk
generate_msvc8_error_check_file $check_awk


# Build

for cfg in $cfgs ; do
  for dir in $dirs ; do
     if [ $dir = dll ] ; then
       test $cfg != ReleaseDLL -a $cfg != DebugDLL  &&  continue  
     fi
     sols=`eval echo "$"sol_${dir}""`
     for sol in $sols ; do
       alias=`echo $sol | sed -e 's|\\\\.*$||g' -e 's|_.*$||g'`
       start=`eval $timer`
       echo Start time: $start
       echo "INFO: Building \"$dir\\$cfg\\$alias\""
       echo "Command line: " $build_dir/build_exec.bat "$dir\\build\\$sol" build "$arch" "$cfg" "-BUILD-ALL-" $out
       $build_dir/build_exec.bat "$dir\\build\\$sol" build "$arch" "$cfg" "-BUILD-ALL-" $out
       status=$?
# don't need to cat here, because msdev2005 double output to console also
#       cat $out
       echo "Build time: $start - `eval $timer`"
       if [ $status -ne 0 ] ; then
         # Check on errors (skip expendable projects)
         failed="1"
         grep '^==* Build: .* succeeded, .* failed' $out >/dev/null 2>&1  && \
           awk -f $check_awk $out >$out.res 2>/dev/null  &&  test ! -s $out.res  &&  failed="0"
         rm -f $out.res >/dev/null 2>&1
         rm -f $out     >/dev/null 2>&1
         if [ "$failed" = "1" ]; then
           exit 4
         fi
       fi
       rm -f $out >/dev/null 2>&1
     done
  done
done


exit 0
