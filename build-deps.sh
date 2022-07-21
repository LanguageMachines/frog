#!/bin/sh

# Builds necessary dependencies from source

set -e

[ -z "$VERSION" ] && VERSION=stable
[ -z "$PREFIX" ] && [ -n "$1" ] && PREFIX=$1
[ -z "$PREFIX" ] && PREFIX=/usr/local

if [ "$VERSION" = "stable" ]; then 
    echo "------------------------------------------------------------------------">&2
    echo "    Building latest stable release of main dependencies from source.">&2
    echo "------------------------------------------------------------------------">&2
else
    echo "------------------------------------------------------------------------">&2
    echo "    Building development versions of main dependencie from source.">&2
    echo "      (This is experimental and may contain bugs! DO NOT PUBLISH!)">&2
    echo "-----------------------------------------------------------------------">&2
fi

PWD="$(pwd)"
BUILDDIR="$(mktemp -dt "build-deps.XXXXXX")"
cd "$BUILDDIR"
BUILD_SOURCES="LanguageMachines/ticcutils LanguageMachines/libfolia LanguageMachines/uctodata LanguageMachines/ucto LanguageMachines/timbl LanguageMachines/mbt LanguageMachines/timblserver LanguageMachines/mbtserver LanguageMachines/frogdata"
for SUFFIX in $BUILD_SOURCES; do \
    NAME="$(basename "$SUFFIX")"
    git clone "https://github.com/$SUFFIX"
    cd "$NAME"
    REF=$(git tag -l | grep -E "^v?[0-9]+(\.[0-9])*" | sort -t. -k 1.2,1n -k 2,2n -k 3,3n -k 4,4n | tail -n 1)
    if [ "$VERSION" = "$STABLE" ] && [ -n "$REF" ]; then
        git -c advice.detachedHead=false checkout "$REF"
    fi
    sh ./bootstrap.sh && ./configure --prefix "$PREFIX" && make && make install
    cd ..
done
cd "$PWD"
[ -n "$BUILDDIR" ] && rm -Rf "$BUILDDIR"
