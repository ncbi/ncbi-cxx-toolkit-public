#!/bin/sh
# $Id$
# Simple wrapper for xcodebuild that favors older SDK versions so as
# to yield more portable binaries.

cmd=xcodebuild
case "`uname -r`" in
  ?.* | 1[01].* ) # Mac OS X 10.7.x or older
    for v in 10.5 10.6 10.7; do
      sdk=/Developer/SDKs/MacOSX$v.sdk
      [ -d "$sdk" ] && break
    done
    ;;
  * )
    for v in 10.8 10.9 10.10; do
      sdk=/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX$v.sdk
      [ -d "$sdk" ] && break
    done
    ;;
esac
if [ -d "$sdk" ]; then
  cmd="$cmd -sdk $sdk"
fi
exec $cmd "$@"
