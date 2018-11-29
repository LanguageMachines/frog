

Frog generator
==============

Frog is developed for Dutch and intended as a ready tool to feed your
text and to get detailed linguistic information as output. However, it
is possible to create a Frog lemmatizer and PoS-tagger for your own data
set, either for a specific Dutch corpus or for a corpus in a different
language. Froggen is a Frog-generator that expects as input a training
corpus in a TAB separated column format of words, lemmas and PoS-tags.
This Dutch tweet annotated with lemma and PoS-tag information is an
example of input required for froggen:

::

    Coolio coolio TSW
    mam mam N
    , , LET
    echt echt ADJ
    vet vet ADJ
    lekker lekker ADJ
    ' ' LET
    <utt>
    Eh eh TSW
    ... ... LET
    watte wat VNW
    <utt>
    ? ? LET
    #kleinemannetjeswordengroot #kleinemannetjeswordengroot HTAG

Running froggen -T taggedcorpus -c my\_frog.cfg will create a set of
Frog files, including the specified configuration file, that can be used
to run your own Frog. Your own instance of Frog should then be invoked
using frog -c my\_frog.cfg.

Besides the option to specify the Frog configuration file name to save
the settings, one can also name the output directory where the frog
files will be saved with . Froggen also allows an additional dictionary
list of words, lemmas and PoS-tags to improve the lemmatizer module with
the option .

-
Configuration
===============

Frog was developed as a modular system that allows for flexibility in
usage. In the current version of Frog, modules have a minimum of
dependencies between each other so that the different modules can
actually run in parallel processes, speeding up the performance time of
Frog. The tokenizer, lemmatizer, phrase chunker and dependency parser
are all independent from each other. All modules expect tokenized text
as input. The NER module, lemmatizer, morphological analyzer and parser do depend on
the PoS-tagger output. These dependencies are depicted in figure
[fig-arch]. The tokenizer and multi-word chunker are rule-based modules
while all other modules are based on trained memory-based classifiers.


For advanced usage of Frog, we can define the individual settings of
each module in the Frog configuration file (frog.cfg in the frog source
directory) or adapt some of the standard options. Editing this file
requires detailed knowledge about the modules and relevant options will
be discussed in the next sections. You can create your own frog
configuration file and run frog with frog -c myconfigfile.cfg . The
configuation file follows the INI file format [15]_ and is divided in
individual sections for each of the modules. Some parts of the config
file are obligatory and we show the

::

    ------------------------------------
    [[tagger]]
    set=http://ilk.uvt.nl/folia/sets/frog-mbpos-cgn
    settings=morgen.settings

    [[mblem]]
    timblOpts=-a1 -w2 +vS
    treeFile=morgen.tree

    -------------------------------------

There are some settings that each of the modules uses:

-  debug Alternative to using –debug on the command line. Debug values
   ranges from 1 (not very verbose) to 10??? (very verbose). Default
   setting isdebug=0.

-  version module version that will be mentioned in FoLia XML output
   file.

-  char\_filter\_file file name of file where you can specify whether
   certain characters need to be replaced or ignored. For example, by
   default we translate all forms of exotic single quotes to the
   standard quote character.

-  set reference to the appropriate Folia XML set definition that is
   used in the module.

Tokenizer
~~~~~~~~~

The tokenizer UCTO has its own reference guide [UCTO]_
and more detailed information can also be found on
https://languagemachines.github.io/ucto/.

Multi-word Units
~~~~~~~~~~~~~~~~

Extraction of multi-word units is a necessary pre-processing step for
the Frog parser. The mwu module is a simple script that takes as input
the tokenized and PoS-tagged text and concatenates certain tokens like
fixed expressions and names. Common mwu such as ‘ad hoc’ are found with
a dictionary lookup, and consecutive tokens that are labeled as
‘SPEC(deeleigen)’ by the PoS-tagger are concatenated (gluetag in the
Frog config file). The dictionary list of common mwu contains 1325 items
and is distributed with the Frog source code and can be found under
/etc/frog/Frog.mwu.1.0 . These settings can be modified in the Frog
config file.

Lemmatizer
~~~~~~~~~~

The lemmatizer is trained on the e-Lex lexicon :raw-latex:`\cite{e-lex}`
with 595,664 unique word form - PoS-tag - lemma combinations. The e-Lex
lexicon has been manually cleaned to correct several errors. A timbl
classifier [Timbl]_ is trained to learn the conversion of word forms to their
lemmas. Each word form in the lexicon is represented as training
instance consisting of the last 20 characters of the word form. Note
that this abbreviates long words such as *consumptiemaatschappijen* to u
m p t i e m a a t s c h a p p i j e n.In total, the training instance
base has 402,734 unique word forms. As in the Dutch language
morphological changes occur in the word suffix, leaving out the word
beginning will not hinder the lemma assignment. The class label of each
instance is the PoStag and a rewrite rule to transform the word form to
the lemma form. The rewrite rules are applied to the word form endings
and delete or insert one or multiple characters. For example to get the
lemma *bij* for the noun *bijen* we need to delete the ending *en* to
derive the lemma. We show some examples of instances in [ex-lem] where
the rewrite rules should be read as follows. For the example the word
form *haarspleten* with label N(soort,mv,basis)+Dten+Iet is a plural
noun with lemma *haarspleet* that is derived by deleting(+D) the ending
*ten* and inserting (+I) the new ending *et*. For ambiguous word forms,
the class label consists of multiple rewrite rules. The first rewrite
rule with the same PoS tag is selected in such case. Let’s take as
example the word *staken* that can be the plural noun form of *staak*,
the present tense of the verb *staken* or the past tense of the verb
*steken*. Here the PoS determines which rewrite rule is applied. The
lemmatizer does not take into account the sentence context of the words
and in those rare cases where a word form has different lemmas
associated with the same PoS-tags, a random choice is made.

[ex-lem]

Morphological Analyzer
~~~~~~~~~~~~~~~~~~~~~~

The Morphological analyser MBMA [MBMA]_ aims to
decompose tokens into their morphemes reflecting possible spelling
changes. Here we show two example words:

[ex-longword]

::

    [leven][s][ver][zeker][ing][s][maatschappij][en]
    [aan][deel][houd][er][s][vergader][ing][en]

Internally, MBMA not only tries to split tokens into morphemes but also
aims to classify each splitting point and its relation to the adjacent
morpheme. MBMA is trained on the CELEX database
:raw-latex:`\cite{Baayen+93}`. Each word is represented by a set of
instances that each represent one character of the word in context of 6
characters to the left and right. As example we show the 10 instances
that were created for the word form *gesneden* in [ex-mbma]. The general
rule in Dutch to create a part particle of a verb is to add *ge-* at the
beginning and add *-en* at the end. The first character ’g’ is labeled
with pv+Ige indicating the start of an past particle (pv) where a prefix
*ge* was inserted (+Ige). Instance 3 represents the actual start of the
verb (V) and instance 5 reflects the spelling change that transforms the
root form *snijd* to the actual used form *sned* (0+Rij\ :math:`>`\ e:
replace current character ’ij’ with ’e’ ). Instance 7 also has label
’pv’ and denotes the end boundary of the root morpheme.

Timbl IGtree :raw-latex:`\cite{timbl}` trained on 3179,331 instances
extracted from that were based on the CELEX lexicon of 293,570 word
forms. The morphological character type classes result in a total 2708
class labels where the most frequent class ’0’ occurs in 69% of the
cases as most characters are inside an morpheme and to do not signify
any morpheme border or spelling change. 7% of the instance represent a
noun (N) starting point and 4% a verb (V) starting point. The most
frequent spelling changes are the insertion of an ’e’ after the morpheme
(0/e) (klopt dit?) or a plural inflection (denoted as ’m’).

The MBMA module of Frog does not analyze every token in the text, it
uses the PoS tags assigned by the PoS module to filter out punctuation
and names (PoS ’SPEC’) and words that we labeled as ABBREVIATION by the
Tokeniser. For these cases, Frog keeps the token as it is without
further analysis.

*Running frog with the parameter –deep-morph results in a much richer
morphological analysis including grammatical classes and spelling
changes*.

[ex-mbma]

+-----+------+------+------+------+------+------+---------+------+------+------+------+------+------+-----------------------+
| 1   | \_   | \_   | \_   | \_   | \_   | \_   | **g**   | e    | s    | n    | e    | d    | e    | pv+Ige                |
+-----+------+------+------+------+------+------+---------+------+------+------+------+------+------+-----------------------+
| 2   | \_   | \_   | \_   | \_   | \_   | g    | e       | s    | n    | e    | d    | e    | n    | 0                     |
+-----+------+------+------+------+------+------+---------+------+------+------+------+------+------+-----------------------+
| 3   | \_   | \_   | \_   | \_   | g    | e    | s       | n    | e    | d    | e    | n    | \_   | V                     |
+-----+------+------+------+------+------+------+---------+------+------+------+------+------+------+-----------------------+
| 4   | \_   | \_   | \_   | g    | e    | s    | n       | e    | d    | e    | n    | \_   | \_   | 0                     |
+-----+------+------+------+------+------+------+---------+------+------+------+------+------+------+-----------------------+
| 5   | \_   | \_   | g    | e    | s    | n    | e       | d    | e    | n    | \_   | \_   | \_   | 0+Rij\ :math:`>`\ e   |
+-----+------+------+------+------+------+------+---------+------+------+------+------+------+------+-----------------------+
| 6   | \_   | g    | e    | s    | n    | e    | d       | e    | n    | \_   | \_   | \_   | \_   | 0                     |
+-----+------+------+------+------+------+------+---------+------+------+------+------+------+------+-----------------------+
| 7   | g    | e    | s    | n    | e    | d    | e       | n    | \_   | \_   | \_   | \_   | \_   | pv                    |
+-----+------+------+------+------+------+------+---------+------+------+------+------+------+------+-----------------------+
| 8   | e    | s    | n    | e    | d    | e    | n       | \_   | \_   | \_   | \_   | \_   | \_   | 0                     |
+-----+------+------+------+------+------+------+---------+------+------+------+------+------+------+-----------------------+

Note that the older version of the morphological analyzer reported in
:raw-latex:`\cite{Tadpole}` was trained on a slightly different version
of the data with a context of only 5 instead of 6 characters left and
right. In that older study the performance of the morphological analyzer
was evaluated on a 10% held out set and an accuracy of 79% on *unseen*
words was attained.

MBMA Configuration file options
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

When you want to set certain options for the MBMA module, place them
under the heading ] in the Frog configuration file.

-  set FoLiA set name for the morphological tag set that MBMA uses.

-  clex-set FoLiA set name PoS-tag- set that MBMA uses internally. As
   MBMA is trained on CELEX, it uses the CELEX POS-tag set, and not the
   default PoS-tag set (CGN tag set) of the Frog PoS tagger module.
   However, these internal pos-tags are mapped back to the CGN tag set.

-  cgn\_clex\_mainFile name of file that contains the mapping of CGN
   tags to CELEX tags.

-  deep-morph Alternative to using –deep-morph on the command line.

-  treeFile Name of the trained MVMA Timbl tree (usually IG tree is
   used).

-  timbOpts Timbl options that were used for creating the MBMA treeFile.

PoS Tagger
~~~~~~~~~~

The PoS tagger in Frog is based on MBT, a memory-based tagger-generator
and tagger [MBT]_ [16]_ trained on a large Dutch corpus
of 10,975,324 tokens in 933,891 sentences. This corpus is a mix of
several manually annotated corpora but about 90% of the data comes from
the transcribed Spoken Dutch Corpus of about nine million tokens
[CGN]_ The other ten precent of the training data
comes from the ILK corpus (46K tokens), the D-Coi corpus (330K tokens)
and the Eindhoven corpus (75K tokens) citeuit den Boogaart 1975 that
were re-tagged with CGN tag set. The tag set consists of 12 coarse
grained PoS-tags and 280 fine-grained PoS tag labels. Note that the
chosen main categories (shown in table [tab-pos-tags]) are well in line
with a universal PoS tag set as proposed by which has almost the same
tags. The universal set has a *particles* tag for function words that
signify negation, mood or tense while CGN has an *interjection* tag to
label words like ‘aha’ and ‘oké’ that are typically used in spoken
utterances.

Named Entity Recognition
~~~~~~~~~~~~~~~~~~~~~~~~

The Named Entity Recognizer (NER) is an MBT classifier [MBT]_
 trained on the SoNar 1 million word corpus
labeled with manually verified NER labels. The annotation is flat and in
case of nested names, the longest name is annotated. For example a
phrase like ’het Gentse Stadsbestuur’ is labeled as
:math:`het [Gentse stadsbestuur]_{ORG}`. ’Gentse’ also refers to a
location but the overarching phrase is the name of an organization and
this label takes precedence. Dutch determiners are never included as
part of the name. Details about the annotation of the training data can
be found in the Sonar NE annotation guidelines [NERmanual]_.
| The NER module does not use PoS tags but learns the relation between
  words and name tags directly. An easy way to adapt the NER module to a
  new domain is to give an additional name list to the NER module. The
  names on this list has the following format: the full name followed by
  a tab and the name label as exemplified here.The name list can be
  specified in the configuration file under :math:`[[NER]]` as
  known\_ners=nerfile.

+----------------------+-------+
| Zwarte Zwadderneel   | per   |
+----------------------+-------+
| LaMa groep           | org   |
+----------------------+-------+

Phrase Chunker
~~~~~~~~~~~~~~

The phrase chunker module is based on the chunker developed in the 90’s [Daelemans1999]_ and uses MBT [MBT]_ as
classifier. The chunker adopted the BIO tags to represent chunking as a
tagging task where B-tags signal the start of the chunk, I-tags inside
the chunk and O-tags outside the chunk. In the context of the TTNW
[TTNWW]_, the chunker was updated and trained on a newer and larger corpus of one million words, the Lassy Small
[lassysmall]_. This corpus a annotated with syntactic trees that were fist converted to a flat structure with a
script.

Parser
~~~~~~

The Constraint-satisfaction inference-based dependency parser (CSI-DP)
[Canisius2009]_ is trained on the manually verified
*Lassy small* corpus [lassysmall]_ and several million
tokens of automatically parsed text by the Alpino parser
[ALPINO]_ from Wikipedia pages, newspaper
articles, and the Eindhoven corpus. When CSI-DP is parsing a new
sentence, the parser first aims to predict low level syntactic
information, such as the syntactic dependency relation between each pair
of tokens in the sentence and the possible relation types a certain
token can have. These low level predictions take the form of soft
weighted constraints. In the second step, the parser aims to generate
the final parse tree where each token has only one relation with another
token using a constraint solver based on the Eisner parsing algorithm
[Eisner2000]_. The soft constraints determine the
search space for the constraint solver to find the optimal solution.

CSI-DP applies three types of constraints: dependency constraints,
modifier constraints and direction constraints. For each constraint
type, a separate timbl classifier is trained. Each pair of tokens in the
training set occurs with a certain set of possible dependency relations
and this information is learned by the dependency constraint classifier.
An instance is created for each token pair and its relation where one is
the modifier and and one is head. Note that a pair always creates two
instances where these roles are switched. The timbl classifier trained
on this instance base will then for each token pair predict zero, one or
multiple relations and these relations form the soft constraints that
are the input for the general solver who selects the overall best parse
tree. The potential relation between the token pair is expressed in the
following features: the words and PoS tags of each token and its left
and right neighboring token, the distance between the two tokens in
number of intermediate tokens, and a position feature expressing whether
the token is located right or left of the potential head word.

For each token in the sentence, instances are created between the token
and all other tokens in the sentence with a maximum distance of 8 tokens
left and right. The maximum distance of 8 tokens covers 95% of all
present dependency relations in the training set
[Canisius+2006]_. This leads to a unbalance of
instances that express an actual syntactic relation between a word pair
and negative cases. Therefore, the negative instances in the training
set were reduced by randomly sampling a set of negative cases that is
twice as big the number of positive cases (based on experiments in
[Canisius2009]_).

The second group of constraints are the modifier constraints that
express for each single token the possible syntactic relations that it
has in the training set. The feature set for these instances consist of
the local context in 1 or 2 ?? words and PoS tags of the token.

The third group of direction constraints specify for each token in the
sentence whether the potential linked head word is left or right of the
word, or the root. Based on evidence in the training set, a word is
added with one, two or three possible positions as soft weighted
constraints. For example the token *politie* might occur in a left
positioned subject relation to a root verb, a right positioned direct
object relation, of in an elliptic sentence as the root form itself.


References
=============

.. [15]
   More about the INI file
   format:\ https://en.wikipedia.org/wiki/INI_file)

.. [16]
   MBT available at http://languagemachines.github.io/mbt/

.. [17]
      https://github.com/proycon/python-frog

.. [18]
      Part of PyNLPL: https://github.com/proycon/pynlpl

.. [19]
      https://github.com/vanatteveldt/frogr/

.. [20]
      https://github.com/Machiel/gorf


.. [CELEX] Baayen, R. H., R. Piepenbrock, and H. van Rijn. 1993. The CELEX lexical data base on CD-ROM. Linguistic Data Consortium, Philadelphia, PA.


.. [ELEX] TST-centrale. 2007. E-lex voor taal- en spraaktechnologie, version 1.1. Technical report, Nederlandse TaalUnie.

.. [Alpino] Bouma, G., G. Van Noord R., and Malouf. 2001. Alpino: Wide-coverage computational analysis of dutch. Language and Computers, 37(1):45–59.

.. [Canisius2009] Canisius, Sander. 2009. Structured prediction for natural language processing. A constraint satisfaction approach. Ph.D. thesis, Tilburg University.

.. [Canisius+2006]  Canisius, Sander, Toine Bogers, Antal van den Bosch, Jeroen Geertzen, and Erik Tjong Kim Sang. 2006. Dependency parsing by inference over high-recall dependency predictions. In Proceedings of the Tenth Conference on Computational Natural Language Learning, CoNLL-X ’06, pages 176–180, Stroudsburg, PA, USA. Association for Computational Linguistics.

..[MBT] Daelemans, W., J. Zavrel, A. Van den Bosch, and K. Van der Sloot. 2010. MBT: Memory-based tagger, version 3.2, reference guide. Technical Report ILK Research Group Technical Report Series 10-04, ILK, Tilburg University, The Netherlands.

..[Timbl] Daelemans, W., J. Zavrel, K. Van der Sloot, and A. Van den Bosch. 2004. TiMBL: Tilburg Memory Based Learner, version 6.3, reference manual. Technical Report ILK Research Group Technical Report Series 10-01, ILK, Tilburg University, The Netherlands.

.. [Daelemans1999] Daelemans, Walter, Sabine Buchholz, and Jorn Veenstra. 1999. Memory-based shallow parsing. In Proceedings of CoNLL-99, pages 53–60.

..[NERmanual] Desmet, Bart and Veronique Hoste. 2009. Named Entity Annotatierichtlijnen voor het Nederlands. Technical Report LT3 09.01.,  LT3, University Ghent, Belgium.


.. [Eisner2000]   Eisner, Jason, 2000. Bilexical grammars and their cubic-time parsing algorithms, pages 29–61. Springer.  Haque, R., S. Kumar Naskar, A. Van den Bosch, and A. Way. 2011. Integrating source-language context into phrase-based statistical machine translation. Machine Translation, 25(3):239–285, September.

.. [CGN] Schuurman, Ineke, Machteld Schouppe, Heleen Hoekstra, and Ton van der Wouden. 2003. CGN, an annotated corpus of spoken Dutch. In Anne Abeillé, Silvia Hansen-Schirra, and Hans Uszkoreit, editors, Proceedings of 4th International Workshop on Linguistically Interpreted Corpora (LINC-03), pages 101–108, Budapest, Hungary.

.. [Tadpole2007] van den Bosch, Antal, B. Busser, S. Canisius, and Walter Daelemans, 2007. An efficient memory-based morphosyntactic tagger and parser for Dutch, pages 191–206. LOT, Utrecht.

.. [Lassysmall] van Noord, Gertjan, Ineke Schuurman, and Gosse Bouma. 2011. Lassy syntactische annotatie. Technical report.

.. [LASSY] Van Noord, Gertjan, Gosse Bouma, Frank Van Eynde, Daniel De Kok, Jelmer Van der Linde, Ineke Schuurman, Erik Tjong Kim Sang, and Vincent Vandeghinste. 2013a. Large scale syntactic annotation of written dutch: Lassy. In Essential Speech and Language Technology for Dutch. Springer, pages 147–164.
.. [Folia] van Gompel, M. and M. Reynaert. 2013. Folia: A practical xml format for linguistic annotation - a descriptive and comparative study. Computational Linguistics in the Netherlands Journal, 3.

.. [UCTO] Maarten van Gompel, Ko van der Sloot, Iris Hendrickx and Antal van den Bosch. Ucto: Unicode Tokeniser. Reference Guide, Language and Speech Technology Technical Report Series 18-01, Radboud University, Nijmegen, October, 2018, Available from https://ucto.readthedocs.io/
