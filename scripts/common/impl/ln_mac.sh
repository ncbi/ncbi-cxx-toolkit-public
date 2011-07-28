#!/bin/sh
# Preserve metadata if present; otherwise, make hard links to save space
case `/Developer/Tools/GetFileInfo -t "$1" 2>/dev/null` in
    \"????\") exec /Developer/Tools/CpMac -p "$1" "$2" ;;
    *)        exec /bin/ln -f "$1" "$2"
esac
