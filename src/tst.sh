#! /bin/sh

./frog --skip=p -t $srcdir/../tests/tst.txt -o tst.out
diff -w -B tst.out $srcdir/../tests/tst.ok
