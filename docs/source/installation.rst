.. _installation:


Installation
==============

You can download Frog, manually compile and install it from source.
However, due to the many dependencies and required technical expertise
this is not an easy endeavor.

Linux users should first check whether their distribution’s package
manager has up-to-date packages for Frog, as this provides the easiest
way of installation.

If no up-to-date package exists, we recommend to use **LaMachine**. Frog
is part of our LaMachine software distribution and includes all
necessary dependencies. It runs on Linux, BSD and Mac OS X. It can also
run as a virtual machine under other operating systems, including
Windows. LaMachine makes the installation of Frog straightforward;
detailed instructions for the installation of LaMachine can be found
here: http://proycon.github.io/LaMachine/.

Manual compilation & installation
---------------------------------

The source code of Frog for manual installation can be obtained from
Github. Because of file sizes and to cleanly separate code from data,
the data and configuration files for the modules of Frog have been
packaged separately.

-  Source code repository: https://github.com/LanguageMachines/frog/

-  Stable releases [1]_:
   https://github.com/LanguageMachines/frog/releases/

-  Frog data repository: https://github.com/LanguageMachines/frogdata/
   (required dependency!)

To compile these manually, you first need current versions of the
following dependencies of our software, and compile and install them in
the order specified here:

-  ``ticcutils``\  [2]_ - A shared utility library

-  ``libfolia``\  [3]_- A library for the FoLiA format

-  ``ucto``\  [4]_ - A rule-based tokenizer

-  ``timbl``\  [5]_ - The memory-based classifier engine

-  ``mbt``\  [6]_ - The memory-based tagger

-  ``frogdata``\  [7]_ - Datafiles needed to run Frog

You will also need the following 3rd party dependencies:

-  **icu** - A C++ library for Unicode and Globalization support. On
   Debian/Ubuntu systems, install the package ``libicu-dev``.

-  **libxml2** - An XML library. On Debian/Ubuntu systems install the
   package ``libxml2-dev``.

-  **textcat** - A library for language detection. On Debian/Ubuntu systems install the
   package ``libexttextcat-dev``.

-  A sane build environment with a C++ compiler (e.g. gcc or clang),
   autotools, autoconf-archive, libtool, pkg-config

The actual compilation proceeds by entering the Frog directory and
issuing the following commands:

::
    $ bash bootstrap.sh
    $ ./configure
    $ make
    $ sudo make install

| To install in a non-standard location (``/usr/local/`` by default),
  you may use the ``–prefix`` option in the configure step:
| ``./configure –prefix=/desired/installation/path/``.


Quick start guide
==========

Frog aims to automatically enrich Dutch text with linguistic information
of various forms. Frog integrates several NLP modules that perform the
following tasks: tokenize text to split punctuation from word forms
(including recognition of sentence boundaries and multi-word units),
assignment of part-of-speech tags, lemmas, and morphological and
syntactic information to words.

We give a brief explanation on running Frog to get you started quickly, followed by a more elaborate description of
using Frog and how to manipulate the settings for each of the separate
modules in Chapter :[modulesDescription].

Frog is developed as a command line tool. We assume the reader already
has at least basic command line skills.

Typing ``frog -h`` on the command line results in a brief overview of all
available command line options. Frog is typically run on an input
document, which is specified using the -t option for plain text
documents, or -x for documents in the FoLiA XML format. It is, however,
also possible to run it interactively or as a server. We show an example
of the output of Frog when processing the contents of a plain-text file
``test.txt``, containing just the sentence *In ’41 werd aan de stamkaart
een z.g. inlegvel toegevoegd.*

We run Frog as follows: $ frog -t test.txt

Frog will present the output as shown in example [ex-frog-out] below:

[ex-frog-out]

+----+------------+-----------+--------------------+------------------------------+----------+---+------+---+------+
| 1  |      2     |   3       |   4                |     5                        |     6    | 7 |   8  | 9 |   10 |
+====+============+===========+====================+==============================+==========+===+======+===+======+
| 1  | In         | in        | [in]               | VZ(init)                     | 0.987660 | O | B-PP | 0 | ROOT |
+----+------------+-----------+--------------------+------------------------------+----------+---+------+---+------+
| 2  | ’41        | '41       |['41]               | TW(hoofd,vrij)               | 0.719498 | O | B-NP | 1 | obj1 |
+----+------------+-----------+--------------------+------------------------------+----------+---+------+---+------+
| 3  | werd       | worden    | [word]             | WW(pv,verl,ev)               | 0.999799 | O | B-VP | 0 | ROOT |
+----+------------+-----------+--------------------+------------------------------+----------+---+------+---+------+
| 4  | aan        | aan       | [aan]              | VZ(init)                     | 0.996734 | O | B-PP |10 | mod  |
+----+------------+-----------+--------------------+------------------------------+----------+---+------+---+------+
| 5  | de         | de        | [de]               | LID(bep,stan,rest)           | 0.999964 | O | B-NP | 6 | det  |
+----+------------+-----------+--------------------+------------------------------+----------+---+------+---+------+
| 6  | stamkaart  | stamkaart | [stam][kaart]      | N(soort,ev,basis,zijd,stan)  | 0.996536 | O | I-NP | 4 | obj1 |
+----+------------+-----------+--------------------+------------------------------+----------+---+------+---+------+
| 7  | een        | een       | [een]              | LID(onbep,stan,agr)          | 0.995147 | O | B-NP | 9 | det  |
+----+------------+-----------+--------------------+------------------------------+----------+---+------+---+------+
| 8  | z.g.       | z.g.      | [z.g.]             | ADJ(prenom,basis,met-e,stan) | 0.500000 | O | I-NP | 9 | mod  |
+----+------------+-----------+--------------------+------------------------------+----------+---+------+---+------+
| 9  | inlegvel   | inlegvel  | [in][leg][vel]     | N(soort,ev,basis,zijd,stan)  | 1.000000 | O | I-NP |10 | obj1 |
+----+------------+-----------+--------------------+------------------------------+----------+---+------+---+------+
| 10 | toegevoegd | toevoegen | [toe][ge][voeg][d] | WW(vd,vrij,zonder)           | 0.998549 | O | B-VP | 3 | vc   |
+----+------------+-----------+--------------------+------------------------------+----------+---+------+---+------+
| 11 |  .         | .         | [.]                | LET()                        | 1.000000 | O | O    |10 | punct|
+----+------------+-----------+--------------------+------------------------------+----------+---+------+---+------+

The ten TAB-delimited columns in the output of Frog contain the
information we list below. This columned output is intended for quick
interpretation on the terminal or in scripts. It does, however, not
contain every detail available to Frog.

1. Token number
    (Number is reset every sentence.)

2. Token
    The text of the token/word

3. Lemma
    The lemma

4. Morphological segmentation
    A morphological segmentation in which each morpheme is enclosed in
    square brackets

5. PoS tag
    The Part-of-Speech tag according to the CGN tagset [POS2004]_.

6. Confidence
    in the PoS tag, a number between 0 and 1, representing the
    probability mass assigned to the best guess tag in the tag
    distribution

7. Named entity type
    in BIO-encoding [8]_

8. Base phrase chunk
    in BIO-encoding

9. Token number of head word
    in dependency graph (according to the Frog parser)

10. Dependency relation type
    of the word with head word

For full output, you will want to instruct Frog to output to a FoLiA XML
file. This is done using the -X option, followed by the name of the
output file.

To run Frog in this way we execute: $ frog -t test.txt -X test.xml The
result is a file in FoLiA XML format [FOLIA]_ that
contains all information in a more structured and verbose fashion. More
information about this file format, including a full specification,
programming libraries, and other tools, can be found on
https://proycon.github.io/folia. We show an example of the XML structure
for the token *aangesneden* in example [ex-xml-tok] and explain the
details of this structure in section [sec-usage]. Each of these layers
of linguistic output will be discussed in more detail in the next
chapters.

[ex-xml-tok]

::

    <w xml:id="WP3452.p.1.s.1.w.4" class="WORD">
        <t>aangesneden</t>
        <pos class="WW(vd,vrij,zonder)" confidence="0.875" head="WW">
            <feat class="vd" subset="wvorm"/>
            <feat class="vrij" subset="positie"/>
            <feat class="zonder" subset="buiging"/>
        </pos>
        <lemma class="aansnijden"/>
        <morphology>
            <morpheme>
                <t>aan</t>
            </morpheme>
            <morpheme>
                <t>ge</t>
            </morpheme>
            <morpheme>
                <t>snijd</t>
            </morpheme>
            <morpheme>
                <t>en</t>
            </morpheme>
        </morphology>
    </w>

Input and Output options
~~~~~~~~~~~~~~~~~~~~~~~~

By default the output of Frog is written to screen (i.e. standard
output). There are two options for outputting to file (which can also be
called simultaneously):

-  ``-o <filename>`` – Writes columned (TAB delimited) data to file.

-  ``-X <filename>`` – Writes FoLiA XML to file.

We already saw the input option -t <filename> for plain-text files. It
is also possible to read FoLiA XML documents instead, using the -x
<filename> option.

Besides input of a single plain text file, Frog also accepts a directory
of plain text files as input –testdir=<directory> , which can also be
written to an output directory with parameter –outputdir=<dir>. The
FoLiA equivalent for –outputdir is –xmldir. To read multiple FoLiA
documents, instead of plain-text documents, from a directory, use -x
–testdir=<directory>.

Interactive Mode
~~~~~~~~~~~~~~~~~~

Frog can be started in an interactive mode by simply typing ``frog`` on
the command line. Frog will present a ``frog>`` prompt after which you
can type text for processing. By default, you will press ENTER at an
empty prompt before Frog will process the prior input. This allows for
multiline sentences to be entered. To change this behavior, you may want
to start Frog with the -n option instead, which tells it to assume each
input line is a sentence. FoLiA input or output is not supported in
interactive mode.

To exit this mode, type CTRL-D.

Server mode
~~~~~~~~~~~

Frog offers a server mode that launches it as a daemon to which multiple
clients can connect over TCP. The server mode is started using the
``-S <port>`` option. Note that options like ``-n`` and ``–skip`` are
valid in this mode too.

You can for example start a Frog server on port 12345 as follows:
``$ frog -S 12345``.

The simple protocol clients should adhere to is as follows:

-  The client sends text to process (may contain newlines)

-  The client sends the string ``EOT`` followed by a newline

-  The server responds with columned, TAB delimited output, one token
   per line, and an empty line between sentences.

-  FoLiA input and output are also possible, using the ``-x`` and ``-X``
   options without parameters. When ``-X`` is selected, TAB delimited
   output is suppressed.

-  The last line of the server response consists of the string
   ``READY``, so the client knows it received the full response.

Communicating with Frog on such a low-level may not be necessary, as
there are already some libraries available to communicate with Frog for
several programming languages:

-  Python – **pynlpl.clients.frogclient**\  [9]_

-  R – **frogr**\  [10]_ – by Wouter van Atteveldt

-  Go – **gorf**\  [11]_ – by Machiel Molenaar

The following example shows how to communicate with the Frog server from
Python using the Frog client in PyNLPl, which can generally be installed
with a simple ``pip install pynlpl``, or is already available if you use
our LaMachine distribution.

::

    from pynlpl.clients.frogclient import FrogClient

    port = 12345
    frogclient = FrogClient('localhost',port)

    for data in frogclient.process("Dit is de tekst om te verwerken.")
      word, lemma, morph, pos = data[:4]
      #TODO: Further processing per word

Do note that Python users may prefer using the ``python-frog`` binding
instead, which will be described in Section :[pythonfrog]:. This binds
with Frog natively without using a client/server model and therefore has
better performance.





.. [1]
   The source code repository points to the latest development version
   by default, which may contain experimental features. Stable releases
   are deliberate snapshots of the source code. It is recommended to
   grab the latest stable release.

.. [2]
   https://github.com/LanguageMachines/ticcutils

.. [3]
   https://github.com/LanguageMachines/libfolia

.. [4]
   https://languagemachines.github.io/ucto

.. [5]
   https://languagemachines.github.io/timbl

.. [6]
   https://languagemachines.github.io/mbt

.. [7]
   https://github.com/LanguageMachines/frogdata

.. [8]
   B (begin) indicates the begin of the named entity, I (inside)
   indicates the continuation of a named entity, and O (outside)
   indicates that something is not a named entity

.. [9]
  
   3

.. [10]
   https://github.com/vanatteveldt/frogr/

.. [11]
   https://github.com/Machiel/gorf





.. [POS2004] Van Eynde, Frank. 2004. Part of speech tagging en lemmatisering van het corpus gesproken nederlands. Technical report, Centrum voor Computerlinguıstiek, KU Leuven, Belgium.
