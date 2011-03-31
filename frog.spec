# $Id$
# $URL$

Summary: Tagger and parser for Dutch language
Name: frog
Version: 0.10.4
Release: 1
License: GPL
Group: Applications/System
URL: http://ilk.uvt.nl/frog/

Packager: Joost van Baal <joostvb-ilk@ad1810.com>
Vendor: ILK, http://ilk.uvt.nl/

Source: http://ilk.uvt.nl/downloads/pub/software/frog-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root

Requires: libicu, ucto, timbl, mbt
BuildRequires: gcc-c++, libicu-devel, ucto, timbl, mbt, libxml2-devel, python-devel

%description
Memory-Based Learning (MBL) is a machine-learning method applicable to a wide
range of tasks in Natural Language Processing (NLP).

Frog is a modular system integrating a morphosyntactic tagger, lemmatizer,
morphological analyzer, and dependency parser for the Dutch language.  It is
based upon it's predecessor TADPOLE (TAgger, Dependency Parser, and
mOrphoLogical analyzEr).  Using Memory-Based Learning techniques, Tadpole
tokenizes, tags, lemmatizes, and morphologically segments word tokens in
incoming Dutch UTF-8 text files, and assigns a dependency graph to each
sentence.  Tadpole is particularly targeted at the increasing need for fast,
automatic NLP systems applicable to very large (multi-million to billion word)
document collections that are becoming available due to the progressive
digitization of both new and old textual data.

NB: Frog can be considered alpha software, and is in a fair state of flux.

Frog is a product of the ILK Research Group (Tilburg University,
The Netherlands) and the CLiPS Research Centre (University of Antwerp,
Belgium).

If you do scientific research in NLP, Frog will likely be of use to you.

%prep
%setup

%build
%configure

%install
%{__rm} -rf %{buildroot}
%makeinstall

%clean
%{__rm} -rf %{buildroot}

%files
%defattr(-, root, root, 0755)
%doc AUTHORS ChangeLog NEWS README TODO
%{_bindir}/Frog
%{_includedir}/%{name}/*.h
%{_sysconfdir}/%{name}/*
%{python_sitelib}/%{name}

%changelog
* Sat Mar 12 2011 Joost van Baal <joostvb-timbl@ad1810.com> - 0.10.4-1
- New upstream release
- Homepage moved from http://ilk.uvt.nl/tadpole to http://ilk.uvt.nl/frog .
* Sun Feb 27 2011 Joost van Baal <joostvb-timbl@ad1810.com> - 0.10.3-1
- Initial release.

