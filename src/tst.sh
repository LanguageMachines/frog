#! /bin/sh

./frog --skip=p -t $srcdir/../tests/tst.txt -c $srcdir/../config/small-frog.cfg -o tst.out >& /dev/null
diff tst.out $srcdir/../tests/tst.ok
