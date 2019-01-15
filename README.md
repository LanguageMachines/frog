[![Build Status](https://travis-ci.org/LanguageMachines/frog.svg?branch=master)](https://travis-ci.org/LanguageMachines/frog) [![Language Machines Badge](http://applejack.science.ru.nl/lamabadge.php/frog)](http://applejack.science.ru.nl/languagemachines/) [![DOI](https://zenodo.org/badge/20526435.svg)](https://zenodo.org/badge/latestdoi/20526435)

Frog - A Tagger-Lemmatizer-Morphological-Analyzer-Dependency-Parser for Dutch
==============================================================================

    Copyright 2006-2017
    Bertjan Busser, Antal van den Bosch,  Ko van der Sloot, Maarten van Gompel

    Centre for Language and Speech Technology, Radboud University Nijmegen
    Induction of Linguistic Knowledge Research Group, Tilburg University

**Website:** https://languagemachines.github.io/frog

Frog is an integration of memory-based natural language processing (NLP)
modules developed for Dutch. All NLP modules are based on Timbl, the Tilburg
memory-based learning software package. Most modules were created in the 1990s
at the ILK Research Group (Tilburg University, the Netherlands) and the CLiPS
Research Centre (University of Antwerp, Belgium). Over the years they have been
integrated into a single text processing tool, which is currently maintained
and developed by the Language Machines Research Group and the Centre for
Language and Speech Technology at Radboud University Nijmegen. A dependency
parser, a base phrase chunker, and a named-entity recognizer module were added
more recently. Where possible, Frog makes use of multi-processor support to run
subtasks in parallel.

Various (re)programming rounds have been made possible through funding by NWO,
the Netherlands Organisation for Scientific Research, particularly under the
CGN project, the IMIX programme, the Implicit Linguistics project, the
CLARIN-NL programme and the CLARIAH programme.

-----------------------------------------------------------------------------

Frog is free software; you can redistribute it and/or modify it under the terms
of the GNU General Public License as published by the Free Software Foundation;
either version 3 of the License, or (at your option) any later version (see the file COPYING)

frog is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.

Comments and bug-reports are welcome at our issue tracker at
https://github.com/LanguageMachines/frog/issues or by mailing
lamasoftware (at) science.ru.nl.
Updates and more info may be found on
https://languagemachines.github.io/frog .


----------------------------------------------------------------------------

This software has been tested on:
- Intel platforms running several versions of Linux, including Ubuntu, Debian,
  Arch Linux, Fedora (both 32 and 64 bits)
- Apple platform running Mac OS X 10.10

Contents of this distribution:
- Sources
- Licensing information ( COPYING )
- Installation instructions ( INSTALL )
- Build system based on GNU Autotools
- Example data files ( in the demos directory )
- Documentation ( in the docs directory )

To install Frog, first consult whether your distribution's package manager has
an up-to-date package.  If not, for easy installation of Frog and its many
dependencies, it is included as part of our software distribution
**LaMachine**: https://proycon.github.io/LaMachine .

To be able to succesfully build Frog from source instead, you need the following dependencies:
- A sane C++ build enviroment with autoconf, automake, autoconf-archive, pkg-config, gcc or clang,  libtool
- libxml2-dev
- libicu-dev
- [ticcutils](https://github.com/LanguageMachines/ticcutils)
- [libfolia](https://github.com/LanguageMachines/libfolia)
- [uctodata](https://github.com/LanguageMachines/uctodata)
- [ucto](https://github.com/LanguageMachines/ucto)
- [timbl](https://github.com/LanguageMachines/timbl)
- [mbt](https://github.com/LanguageMachines/mbt)
- [frogdata](https://github.com/LanguageMachines/frogdata)

The data for Frog is packaged seperately and needs to be installed prior to installing frog:
- [frogdata](https://github.com/LanguageMachines/frogdata)

To compile and install manually from source instead, provided you have all the dependencies installed:

    $ bash bootstrap.sh
    $ ./configure
    $ make
    $ make install

and optionally:
    $ make check


Credits
-------------------------------------------------------------------------------

Many thanks go out to the people who made the developments of the Frog
components possible: Walter Daelemans, Jakub Zavrel, Ko van der Sloot, Sabine
Buchholz, Sander Canisius, Gert Durieux, Peter Berck and Maarten van Gompel.

Thanks to Erik Tjong Kim Sang and Lieve Macken for stress-testing the first
versions of Tadpole, the predecessor of Frog
