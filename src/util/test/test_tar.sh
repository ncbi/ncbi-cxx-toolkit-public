#! /bin/sh

# Locate native tar;  exit if none found
string="`whereis tar 2>/dev/null`"
if [ -z "$string" ]; then
  tar="`which tar 2>/dev/null`"
else
  tar="`echo $string | cut -f2 -d' '`"
fi
test -x "$tar"  ||  exit 0

test_base="/tmp/test_tar.$$"
mkdir $test_base.1  ||  exit 1
trap 'rm -rf $test_base* &' 0 1 2 15

echo "Preparing file staging area"

cp -rp . $test_base.1/ 2>/dev/null

mkdir $test_base.1/testdir 2>/dev/null

date >$test_base.1/testdir/datefile 2>/dev/null

ln -s $test_base.1/testdir/datefile $test_base.1/testdir/ABS12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890 2>/dev/null

ln -s           ../testdir/datefile $test_base.1/testdir/REL12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890 2>/dev/null

ln    $test_base.1/testdir/datefile $test_base.1/testdir/linkfile 2>/dev/null

mkdir $test_base.1/testdir/12345678901234567890123456789012345678901234567890 2>/dev/null

touch $test_base.1/testdir/12345678901234567890123456789012345678901234567890/12345678901234567890123456789012345678901234567890 2>/dev/null

touch $test_base.1/12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890 2>/dev/null

echo "Creating test archive using native tar utility"

(cd $test_base.1  &&  $tar cvf $test_base.tar .)

rm -rf $test_base.1
mkdir  $test_base.1                                                   ||  exit 1

echo "Sanitizing test archive from unsupported features"

(cd $test_base.1  &&  $tar xf  $test_base.tar)

echo "Testing the archive"

test_tar -T -f $test_base.tar                                         ||  exit 1

sleep 1
mkdir $test_base.1/newdir 2>/dev/null
date 2>/dev/null | tee -a $test_base.1/testdir.$$/datefile $test_base.1/newdir/datefile >$test_base.1/datefile 2>/dev/null
cp -fp $test_base.1/newdir/datefile $test_base.1/newdir/dummyfile 2>/dev/null

echo "Testing simple update"

test_tar -C $test_base.1 -u -v -f $test_base.tar ./datefile ./newdir  ||  exit 1

mv -f $test_base.1/datefile $test_base.1/phonyfile 2>/dev/null
mkdir $test_base.1/datefile 2>/dev/null

sleep 1
date >>$test_base.1/newdir/datefile 2>/dev/null

echo "Testing incremental update"

test_tar -C $test_base.1 -u -U -v -E -f $test_base.tar ./newdir ./datefile ./phonyfile  ||  exit 1

rmdir $test_base.1/datefile 2>/dev/null
mv -f $test_base.1/phonyfile $test_base.1/datefile 2>/dev/null

mkdir $test_base.2                                                    ||  exit 1

echo "Testing piping in and extraction"

cat $test_base.tar | test_tar -C $test_base.2 -v -x -f -              ||  exit 1

diff -r $test_base.1 $test_base.2 2>/dev/null                         ||  exit 1

echo "Testing piping out and compatibility with native tar utility"

test_tar -C $test_base.2 -c -f - . 2>/dev/null | $tar tBvf -          ||  exit 1

echo "Testing backup feature"

test_tar -C $test_base.2 -x -B -f $test_base.tar '*testdir/?*'        ||  exit 1
echo $test_base.2/testdir/* | tr ' ' '\n' | grep -v -c '[.]bak$'
echo $test_base.2/testdir/* | tr ' ' '\n' | grep    -c '[.]bak$'
files="`echo $test_base.2/testdir/* | tr ' ' '\n' | grep -v -c '[.]bak$'`"
bkups="`echo $test_base.2/testdir/* | tr ' ' '\n' | grep    -c '[.]bak$'`"
echo "Files: $files --- Backups: $bkups"
test _"$files" = _"$bkups"                                            ||  exit 1

echo "Testing singe entry streaming feature"

test_tar -X -v -f $test_base.tar '*test_tar' | cmp -l - ./test_tar    ||  exit 1

exit 0
