.. _introduction:

Introduction
================================

Frog is an integration of memory-based natural language processing (NLP)
modules developed for Dutch. Frog performs tokenization, part-of-speech
tagging, lemmatization and morphological segmentation of word tokens. At
the sentence level Frog identifies non-embedded phrase chunks in the
sentence, recognizes named entities and assigns a dependency parse
graph. Frog produces output in either FoLiA XML or a simple
tab-delimited column format with one token per line. All NLP modules are
based on Timbl, the Tilburg memory-based learning software package. Most
modules were created in the 1990s at the ILK Research Group (Tilburg
University, the Netherlands) and the CLiPS Research Centre (University
of Antwerp, Belgium). Over the years they have been integrated into a
single text processing tool, which is currently maintained and developed
by the Language Machines Research Group and the Centre for Language and
Speech Technology at Radboud University (Nijmegen, the Netherlands).

License
========

Frog is free and open software. You can redistribute it and/or modify it
under the terms of the GNU General Public License v3 as published by the
Free Software Foundation. You should have received a copy of the GNU
General Public License along with Frog. If not, see
http://www.gnu.org/licenses/gpl.html.

In publication of research that makes use of the Software, a citation should be given of:

 Ko van der Sloot, Iris Hendrickx, Maarten van Gompel, Antal van den Bosch and Walter Daelemans.  Frog, A Natural Language Processing Suite for Dutch, Reference Guide, Language and Speech Technology Technical Report Series 18-02, Radboud University, Nijmegen, December 2018,
 Available from https://frognlp.readthedocs.io/en/latest/


For information about commercial licenses for the Software, contact lamasoftware@science.ru.nl.



Table of Contents
-----------------------

.. toctree::
   :maxdepth: 3

   self
   installation
   pythonfrog
   moduleDetails
   frogData
   advanced
   credits
