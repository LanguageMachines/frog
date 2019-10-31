.. _moduleDetails:

Frog Modules
--------------

Character encoding
~~~~~~~~~~~~~~~~~~

Frog assumes the input text to be plain text in the UTF-8 character
encoding. However, Frog offers the option to specify another character
encoding as input with the option t -e. This options is passed on to the
*Ucto* Tokenizer. It has some limitations, (see [tokenizer]) and will be
ignored when the Tokenizer is disabled. The character encodings are
derived from the ubiquitous unix tool *iconv*  [12]_. The output of Frog
will always be in UTF-8 character encoding. Likewise, FoLiA XML defaults
to UTF-8 as well.

Tokenizer
~~~~~~~~~~

Frog uses the tokenization software *Ucto* :raw-latex:`\cite{UCTO}` for
sentence boundary detection and to separate punctuation from words. In
general, recognizing sentence boundaries and punctuation is a simple
task but recognizing names and abbreviations is essential to perfom this
task well. As shown in example [ex-frog-out], the tokenizer recognizes
abbreviations such as z.g. and ‘41 and considers them to be one token.
Ucto uses manually constructed rules and lists of Dutch names and
abbreviations. Detailed information on Ucto can be found on
https://languagemachines.github.io/ucto/.

The tokenizer module in Frog can be adjusted in several ways. If the
input text is already split on sentence boundaries and has one sentence
per line, the -n option can be used to prevent Frog from changing the
existing sentence boundaries. When sentence boundaries were already
marked with a specific marker, one can specify this marker as –uttmarker
“marker”. The marker strings will be ignored and their positions will be
taken as sentence boundaries.

If the input text is already fully tokenized, the tokenization step in
Frog can be skipped altogether using the skip parameter –skip=t.  [13]_

Multi-word units
~~~~~~~~~~~~~~~~~

Frog recognizes certain special multi-word units (mwu) where a group of
consecutive, related tokens is treated as one toke https://github.com/proycon/pynlpl, supports both Python 2 and Python. This behavior
accommodates, and is in fact required for Frog’s dependency parser as it
is trained on a data set with such multi-word units. In the output the
parts of the multi-word unit will be connected with an underscore. The
PoS-tag, morphological analysis, named entity label and chunk label are
concatenated in the same manner.

| This multi-word detection can be disabled using the option –skip=m.
  When using this option, each element of the MWU is treated as separate
  token. We shown an example sentence in [ex\_mwu] that has two
  multi-word units: *Albert Heijn* and *’s avonds*.

+----+---------------+---------------+-------------------+------------------------------------------+----------+--------------+------------+
| [ex\_mwu] Sentence                 | Supermarkt Albert Heijn is tegenwoordig tot ’s avonds laat open.                                                    |
+====+===============+===============+===================+==========================================+==========+==============+============+
| 1  | Supermarkt    | supermarkt    | [super][markt]    | N(soort,ev,basis,zijd,stan)              | 0.542056 | O            | B_NP       |
+----+---------------+---------------+-------------------+------------------------------------------+----------+--------------+------------+
| 2  | Albert\_Heijn | Albert\_Heijn | [Albert]\_[Heijn] | SPEC(deeleigen)\_SPEC(deeleigen)         | 1.000000 | B-ORG\_I-ORG | B-NP\_I-NP |
+----+---------------+---------------+-------------------+------------------------------------------+----------+--------------+------------+
| 3  | is            | zijn          | [zijn]            | WW(pv,tgw,ev)                            | 0.999150 | O            | B-VP       |
+----+---------------+---------------+-------------------+------------------------------------------+----------+--------------+------------+
| 4  | tegenwoordig  | tegenwoordig  | [tegenwoordig]    | ADJ(vrij,basis,zonder)                   | 0.994033 | O            | B-ADVP     |
+----+---------------+---------------+-------------------+------------------------------------------+----------+--------------+------------+
| 5  | tot           | tot           | [tot]             | VZ(init)                                 | 0.964286 | O            | B-PP       |
+----+---------------+---------------+-------------------+------------------------------------------+----------+--------------+------------+
| 6  | ’s\_avonds    | ’s\_avond     | [’s]\_[avond][s]  | LID(bep,gen,evmo)\_N(soort,ev,basis,gen) | 0.962560 | O\_O         | O\_B-ADVP  |
+----+---------------+---------------+-------------------+------------------------------------------+----------+--------------+------------+
| 7  | laat          | laat          | [laat]            | ADJ(vrij,basis,zonder)                   | 1.000000 | O            | B-VP       |
+----+---------------+---------------+-------------------+------------------------------------------+----------+--------------+------------+
| 8  | open          | open          | [open]            | ADJ(vrij,basis,zonder)                   | 0.983755 | O            | B-ADJP     |                                                       
+----+---------------+---------------+-------------------+------------------------------------------+----------+--------------+------------+
| 9  | .             | .             | [.]               | LET()                                    | 1.000000 | O            | O          |
+----+---------------+---------------+-------------------+------------------------------------------+----------+--------------+------------+

Lemmatizer
~~~~~~~~~~

The lemmatizer assigns the canonical form of a word to each word. For https://github.com/proycon/pynlpl, supports both Python 2 and Pytho
verbs the canonical form is the infinitive, and for nouns it is the
singular form. The lemmatizer trained on the e-Lex lexicon
:raw-latex:`\cite{e-lex}`. It is dependent on the Part-of-Speech tagger
as it uses both the word form and the assigned PoS tag to disambiguate
between different candidate lemmas. For example the word *zakken* used
as a noun has the lemma *zak* while the verb has lemma *zakken*. Section
[sec-bg-lem] presents further details on the lemmatizer.

Morphological Analyzer
~~~~~~~~~~~~~~~~~~~~~~

The morphological Analyzer (MBMA) cuts each word into its morphemes and
shows the spelling changes that took place to create the word form. The
fourth column in example [ex-frog-out] shows the morphemes of the
example sentence. MBMA tries to decompose every token into morphemes,
except for punctuation marks and names. Note that MBMA sometimes makes
mistakes with unknown words such as abbreviations that are not included
in the MBMA lexicon. The abbreviation z.g. in the example is wrongly
analyzed as consisting of two parts. As shown in the earlier XML example
[ex-xml-tok] the past particle *aangesneden* is split into
*[aan][ge][snijd][en]* where the morpheme *[snijd]* is the root form of https://github.com/proycon/pynlpl, supports both Python 2 and Pytho
*sned*. More information about the MBMA architecture can be found in
[sec-bg-morf].

Part-of-Speech Tagger
~~~~~~~~~~~~~~~~~~~~~

The Part-of-Speech tagger uses the tag set of *Corpus Gesproken
Nederlands (CNG)* :raw-latex:`\cite{vanEynde2004}`. It has 12 main PoS
tags (shown in table [tab-pos-tags]) and detailed features for type,
gender, number, case, position, degree, and tense.

We show an example of the PoS tagger output in table [tab-pos-conf]. The
tagger also expresses how certain it was about its tag label in a
confidence score between 0 (not sure) and 1 (absolutely sure). In the
example the PoS tagger is very sure about the first four tokens but not
about the label N(soort,ev,basis,zijd,stan) for the token *Psychologie*
as it only has a confidence score of 0.67. *Psychologie* is an ambiguous
token and can also be used as a name (tag SPEC).

+--------+---------------------+
| ADJ    | Adjective           |
+--------+---------------------+
| BW     | Adverb              |
+--------+---------------------+
| LET    | Punctuation         |
+--------+---------------------+
| LID    | Determiner          |
+--------+---------------------+
| N      | Noun                |
+--------+---------------------+
| SPEC   | Names and unknown   |
+--------+---------------------+
| TSW    | Interjection        |
+--------+---------------------+
| TW     | Numerator           |
+--------+---------------------+
| VG     | Conjunction         |
+--------+---------------------+
| VNW    | Pronoun             |
+--------+---------------------+
| VZ     | Preposition         |
+--------+---------------------+
| WW     | Verb                |
+--------+---------------------+

Table: [tab-pos-tags] The main tags in the CGN PoS-tag set.

+------+---------------+---------------------------------+----------------+
| 34   | Ik            | VNW(pers,pron,nomin,vol,1,ev)   | 0.999791       |
+------+---------------+---------------------------------+----------------+
| 35   | ben           | WW(pv,tgw,ev)                   | 0.999589       |
+------+---------------+---------------------------------+----------------+
| 36   | ook           | BW()                            | 0.999979       |
+------+---------------+---------------------------------+----------------+
| 37   | professor     | N(soort,ev,basis,zijd,stan)     | 0.997691       |
+------+---------------+---------------------------------+----------------+
| 38   | Psychologie   | N(soort,ev,basis,zijd,stan)     | **0.666667**   |
+------+---------------+---------------------------------+----------------+

Table: [tab-pos-conf] The PoS tagger assigns a confidence score to each
tag.

Named Entity Recognition
~~~~~~~~~~~~~~~~~~~~~~~~

The Named Entity Recognizer (NER) detects names in the text and labels
them as location (LOC), person (PER), organization (ORG), product (PRO),
event (EVE) or miscellaneous (MISC).

Internally and in Frog’s columned output, the tags use a so-called BIO
paradigm where B stands for the beginning of the name, I signifies
Inside the name, and O outside the name.

More detailed information about the NER module can be found in
[sec-bg-ner].

Phrase Chunker
~~~~~~~~~~~~~~

The phrase chunker represents an intermediate step between
part-of-speech tagging and full parsing as it produces a non-recursive,
non-overlapping flat structure of recognized phrases in the text and
classifies them with their grammatical function such as adverbial phrase
(ADVP), verb phrase (VP) or noun phrase (NP). The tag labels produced by
the chunker use the same type of BIO-tags (Beginning-Inside-Outside) as
the named entity recognizer. We show an example sentence in [ex-chunk]
where the four-word noun phrase *het cold case team* is recognized as
one phrase. The prepositional phrases (PP) consist only of the
preposition themselves due to the flat structure in which the relation
between prepositions and noun phrases is not expressed (note that the
dependency parse labels, section [sec-dep] do express these relations).
Here *Midden-Nederland* is recognized by the PoS tagger as name and
therefor marked as a separate noun phrase that follows the noun phrase
*de politie*.

:math:`[`\ Dat\ :math:`]_{NP} [`\ bevestigt\ :math:`]_{VP} [`\ het cold
case team\ :math:`]_{NP} [`\ van\ :math:`]_{PP}] [`\ de
politie\ :math:`]_{NP} [`\ Midden-Nederland\ :math:`]_{NP} [` aan
:math:`]_{PP} [`\ de Telegraaf\ :math:`]_{NP} [` .

+------+--------------------+--------+
| 1    | Dat                | B-NP   |
+------+--------------------+--------+
| 2    | bevestigt          | B-VP   |
+------+--------------------+--------+
| 3    | het                | B-NP   |
+------+--------------------+--------+
| 4    | cold               | I-NP   |
+------+--------------------+--------+
| 5    | case               | I-NP   |
+------+--------------------+--------+
| 6    | team               | I-NP   |
+------+--------------------+--------+
| 7    | van                | B-PP   |
+------+--------------------+--------+
| 8    | de                 | B-NP   |
+------+--------------------+--------+
| 9    | politie            | I-NP   |
+------+--------------------+--------+
| 10   | Midden-Nederland   | B-NP   |
+------+--------------------+--------+
| 11   | aan                | B-PP   |
+------+--------------------+--------+
| 12   | de                 | B-NP   |
+------+--------------------+--------+
| 13   | Telegraaf          | I-NP   |
+------+--------------------+--------+
| 14   | .                  | O      |
+------+--------------------+--------+

Table: [ex-chunk] The phrase chunker detects phrase boundaries and
labels the phrases with their grammatical information.

Dependency Parser
~~~~~~~~~~~~~~~~~~

The Constraint-satisfaction inference-based dependency parser (CSI-DP)
:raw-latex:`\cite{Canisius+2006}` predicts grammatical relations between
pairs of tokens. In each token pair relation, one token is the head and
the other is the dependent. Together these relations represent the
syntactic tree of the sentence. One token, usually the main verb in he
sentence, forms the root of the tree and the other tokens depend on the
root in a direct or indirect relation. CSI-DP is trained on the Alpino
treebank :raw-latex:`\cite{Bouma+01}` for Dutch and uses the Alpino
syntactic labels listed in appendix [app-dep]. In the plain text output
of Frog ( example [ex-frog-out]) the dependency information is presented
in the last two columns. The one-but-last column shows number of the
token number of the head word of the dependency relation and the last
column shows the grammatical relation type. We show the last two columns
of the CSI-DP output in table [ex-dep]. The main verb *bevestigt* is
root element of the sentence, the head of the subject relation (su) with
the pronoun *Dat* and head in the object relation (obj1) with *team*.
The noun *team* is the head in three relations: the determiner(det)
*het* and the two modifiers(mod) *cold case*. The name
*Midden-Nederland* is linked as an apposition to the noun *politie*. The
prepositional phrase *van* is correctly assigned to the head noun *team*
but the phrase *aan* is mistakenly linked to *politie* instead of the
root verb *bevestigt*. Linking prepositional phrases is a hard task for
parsers :raw-latex:`\cite{atterer2007}`. More details on the
architecture of the CSI-DP can be found in section [sec-bg-dep]

+------+--------------------+------+---------+
| 1    | Dat                | 2    | su      |
+------+--------------------+------+---------+
| 2    | bevestigt          | 0    | ROOT    |
+------+--------------------+------+---------+
| 3    | het                | 6    | det     |
+------+--------------------+------+---------+
| 4    | cold               | 5    | mod     |
+------+--------------------+------+---------+
| 5    | case               | 6    | mod     |
+------+--------------------+------+---------+
| 6    | team               | 2    | obj1    |
+------+--------------------+------+---------+
| 7    | van                | 6    | mod     |
+------+--------------------+------+---------+
| 8    | de                 | 9    | det     |
+------+--------------------+------+---------+
| 9    | politie            | 7    | obj1    |
+------+--------------------+------+---------+
| 10   | Midden-Nederland   | 9    | app     |
+------+--------------------+------+---------+
| 11   | aan                | 9    | mod     |
+------+--------------------+------+---------+
| 12   | de                 | 13   | det     |
+------+--------------------+------+---------+
| 13   | Telegraaf          | 11   | obj1    |
+------+--------------------+------+---------+
| 14   | .                  | 13   | punct   |
+------+--------------------+------+---------+

Table: [ex-dep] The dependency parser labels each token with a
dependency relation to its head token and assigns the grammatical
relation. https://github.com/proycon/pynlpl, supports both Python 2 and Pytho



.. [12]
   In the current Frog version UTF-16 is not accepted as input in Frog.

.. [13]
   In fact the tokenizer still is used, but in ``PassThru`` mode. This
   allows for conversion to FoLiA XML and sentence detection.
