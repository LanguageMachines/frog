#! /bin/sh

./frog --skip=p -t $srcdir/../tests/tst.txt -c $srcdir/../config/frog.cfg -o tst.out >& /dev/null
diff tst.out $srcdir/../tests/tst.ok
