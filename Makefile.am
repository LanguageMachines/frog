ACLOCAL_AMFLAGS = -I m4 --install

SUBDIRS = src include m4 docs tests

EXTRA_DIST = bootstrap.sh AUTHORS TODO NEWS README.md codemeta.json

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = frog.pc

ChangeLog: $(top_srcdir)/NEWS
	git pull; git2cl > ChangeLog

docker:
	docker build -t frog:latest .

docker-dev:
	docker build -t frog:dev --build-arg VERSION=development .
