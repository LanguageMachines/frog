#! /bin/sh

set `/home/sloot/usr/local/bin/frog -V`

if [ $? -ne 0 ]
then
    echo "unable to execute frog. Is it installed?"
    exit 1
fi

confdir=$2

cd $confdir
echo "entered frog config dir: " $confdir

if test -e frog.cfg
then
    last=`ls frog.cfg.* |sort -V | tail -1 `
    if [ "$last" = "" ]
    then
	cp frog.cfg frog.cfg.1
	echo "created a backup copy of 'frog.cfg' :: 'frog.cfg.1'"
    else
	echo found $last
	new=frog.cfg.`echo $last | awk -F \. {'print ++$3'}`
	cp frog.cfg $new
	echo "created a backup copy of 'frog.cfg' :: '$new'"
    fi
fi

echo "attempt to retrieve frog datafiles from http://ilk.uvt.nl/frog"

\rm -f Frog*.gz

wget http://ilk.uvt.nl/frog/Frog.mbdp-latest.tar.gz

if [ $? -ne 0 ]
then
    echo "unable to retrieve frog data, please inform us:"
    echo "send an e-mail to timbl@uvt.nl"
    exit 1
fi

echo "unpacking: Frog.mbdp-latest.tar.gz"
tar zxf Frog.mbdp-latest.tar.gz
if [ $? -ne 0 ]
then
    echo "unable to unpack frog data, do you have enough rights?"
    exit 1
fi

echo "done"

