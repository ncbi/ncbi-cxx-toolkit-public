#! /bin/sh

# Locate native tar;  exit if none found
string="`whereis tar 2>/dev/null`"
if [ -z "$string" ]; then
  tar="`which tar 2>/dev/null`"
else
  tar="`echo $string | cut -f2 -d' '`"
fi
test -x "$tar"  ||  exit 0

mkdir /tmp/test_tar.$$.1  ||  exit 1
trap 'rm -rf /tmp/test_tar.$$.* &' 0 1 2 15

echo "Preparing file staging area"

cp -rp . /tmp/test_tar.$$.1/ 2>/dev/null

mkdir /tmp/test_tar.$$.1/testdir 2>/dev/null

date >/tmp/test_tar.$$.1/testdir/datefile 2>/dev/null

ln -s /tmp/test_tar.$$.1/testdir/datefile /tmp/test_tar.$$.1/testdir/ABS12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890 2>/dev/null

ln -s                 ../testdir/datefile /tmp/test_tar.$$.1/testdir/REL12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890 2>/dev/null

ln    /tmp/test_tar.$$.1/testdir/datefile /tmp/test_tar.$$.1/testdir/linkfile 2>/dev/null

mkdir /tmp/test_tar.$$.1/testdir/12345678901234567890123456789012345678901234567890 2>/dev/null

touch /tmp/test_tar.$$.1/testdir/12345678901234567890123456789012345678901234567890/12345678901234567890123456789012345678901234567890 2>/dev/null

touch /tmp/test_tar.$$.1/12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890 2>/dev/null

(cd /tmp/test_tar.$$.1  &&  $tar cvf /tmp/test_tar.$$.tar .)

rm -rf /tmp/test_tar.$$.1
mkdir  /tmp/test_tar.$$.1                                                         ||  exit 1

echo "Creating test archive using native tar utility"

(cd /tmp/test_tar.$$.1  &&  $tar xf  /tmp/test_tar.$$.tar)

echo "Testing the archive"

test_tar -T -f /tmp/test_tar.$$.tar                                               ||  exit 1

sleep 1
mkdir /tmp/test_tar.$$.1/newdir 2>/dev/null
date 2>/dev/null | tee -a /tmp/test_tar.$$.1/testdir.$$/datefile /tmp/test_tar.$$.1/newdir/datefile >/tmp/test_tar.$$.1/datefile 2>/dev/null
cp -fp /tmp/test_tar.$$.1/newdir/datefile /tmp/test_tar.$$.1/newdir/dummyfile 2>/dev/null

echo "Testing simple update"

test_tar -C /tmp/test_tar.$$.1 -u -v -f /tmp/test_tar.$$.tar ./datefile ./newdir  ||  exit 1

mv -f /tmp/test_tar.$$.1/datefile /tmp/test_tar.$$.1/phonyfile 2>/dev/null
mkdir /tmp/test_tar.$$.1/datefile 2>/dev/null

sleep 1
date >>/tmp/test_tar.$$.1/newdir/datefile 2>/dev/null

echo "Testing incremental update"

test_tar -C /tmp/test_tar.$$.1 -u -U -v -E -f /tmp/test_tar.$$.tar ./newdir ./datefile ./phonyfile  ||  exit 1

rmdir /tmp/test_tar.$$.1/datefile 2>/dev/null
mv -f /tmp/test_tar.$$.1/phonyfile /tmp/test_tar.$$.1/datefile 2>/dev/null

mkdir /tmp/test_tar.$$.2                                                          ||  exit 1

echo "Testing piping and extraction"

cat /tmp/test_tar.$$.tar | test_tar -C /tmp/test_tar.$$.2 -v -x -f -              ||  exit 1

diff -r /tmp/test_tar.$$.1 /tmp/test_tar.$$.2 2>/dev/null                         ||  exit 1

echo "Testing archive creation"

test_tar -C /tmp/test_tar.$$.2 -c -f - . 2>/dev/null | $tar tBvf -                ||  exit 1

echo "Testing piping and native archive format compatibility"

test_tar -C /tmp/test_tar.$$.2 -x -B -f /tmp/test_tar.$$.tar '*testdir/?*'        ||  exit 1

echo  "Testing backup feature"

(cd /tmp/test_tar.$$.2/testdir  &&  test "`echo * | tr ' ' '\n' | grep -v -c '[.]bak$'`" = "`echo * | tr ' ' '\n' | grep -c '[.]bak$'`")  ||  exit 1

echo "Testing singe entry streaming feature"

test_tar -X -v -f /tmp/test_tar.$$.tar '*test_tar' | cmp -l - ./test_tar          ||  exit 1

exit 0
