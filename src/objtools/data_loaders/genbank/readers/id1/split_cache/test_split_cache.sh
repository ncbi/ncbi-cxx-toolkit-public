#! /bin/sh
#$Id$

root_dir="../../../../../../.."
status_dir="$root_dir/status"
bin_dir="$root_dir/bin"
split_cache="$bin_dir/split_cache"
objmgr_demo="$bin_dir/objmgr_demo"

idlist="21622695 32141095"

exitcode=0
for id in $idlist; do
    echo "Checking id $id:"
    $CHECK_EXEC "$split_cache" -resplit -compress -recurse -id "$id"
    error=$?
    if test $error -ne 0; then
        echo "split_cache of $id failed: $error"
        exitcode=$error
        case $error in
            # signal 1 (HUP), 2 (INTR), 9 (KILL), or 15 (TERM).
            129|130|137|143) echo "Apparently killed"; exit $exitcode;;
        esac
    else
        for args in "-only_features -unnamed" "-unnamed"; do
            $CHECK_EXEC "$objmgr_demo" $args -cache -id "$id"
            error=$?
            if test $error -ne 0; then
                echo "objmgr $args -cache -id $id failed: $error"
                exitcode=$error
                case $error in
                    # signal 1 (HUP), 2 (INTR), 9 (KILL), or 15 (TERM).
                    129|130|137|143) echo "Apparently killed"; exit $exitcode;;
                esac
            fi
        done
    fi
done

exit $exitcode
