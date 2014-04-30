#! /bin/sh

./$srcdir/frog --skip=p -t $srcdir/../tests/tst.txt -o tst.out

diff -w tst.out $srcdir/../tests/tst.ok
