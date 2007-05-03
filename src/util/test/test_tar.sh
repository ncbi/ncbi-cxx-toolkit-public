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

cp -rp . /tmp/test_tar.$$.1/ 2>/dev/null

mkdir /tmp/test_tar.$$.1/testdir.$$ 2>/dev/null

date >/tmp/test_tar.$$.1/testdir.$$/datefile 2>/dev/null

ln -s /tmp/test_tar.$$.1/testdir.$$/datefile  /tmp/test_tar.$$.1/testdir.$$/ABS12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890 2>/dev/null

ln -s                 ../testdir.$$/datefile  /tmp/test_tar.$$.1/testdir.$$/REL12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890 2>/dev/null

ln    /tmp/test_tar.$$.1/testdir.$$/datefile  /tmp/test_tar.$$.1/testdir.$$/linkfile 2>/dev/null

mkdir /tmp/test_tar.$$.1/testdir.$$/12345678901234567890123456789012345678901234567890 2>/dev/null

touch /tmp/test_tar.$$.1/testdir.$$/12345678901234567890123456789012345678901234567890/12345678901234567890123456789012345678901234567890 2>/dev/null

touch /tmp/test_tar.$$.1/12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890 2>/dev/null

(cd /tmp/test_tar.$$.1  &&  $tar cvf /tmp/test_tar.$$.tar .)

rm -rf /tmp/test_tar.$$.1
mkdir  /tmp/test_tar.$$.1                                             ||  exit 1

(cd /tmp/test_tar.$$.1  &&  $tar xf  /tmp/test_tar.$$.tar)

test_tar -T -f /tmp/test_tar.$$.tar                                   ||  exit 1

mkdir /tmp/test_tar.$$.2                                              ||  exit 1

cat /tmp/test_tar.$$.tar | test_tar -C /tmp/test_tar.$$.2 -v -x -f -  ||  exit 1

diff -r /tmp/test_tar.$$.1 /tmp/test_tar.$$.2 2>/dev/null             ||  exit 1

test_tar -C /tmp/test_tar.$$.2 -c -f - . 2>/dev/null | $tar tBvf -    ||  exit 1

exit 0
