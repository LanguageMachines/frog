#! /bin/sh

./frog --skip=p -t $srcdir/../tests/tst.txt -o tst.out >& /dev/null
diff -w tst.out $srcdir/../tests/tst.ok
