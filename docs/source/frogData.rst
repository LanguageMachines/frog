.. _frogData:



  Frog in practice
----------------

Frog has been used in research projects mostly because of its capacity
to process Dutch texts efficiently and analyze the texts sufficiently
accurately. The purposes range from corpus construction to linguistic
research and natural language processing and text analytics
applications. We provide a overview of work reporting to use Frog, in
topical clusters.

Corpus construction
~~~~~~~~~~~~~~~~~~~

Frog, named Tadpole before 2011, has been used for the automated
annotation of, mostly, POS tags and lemmas of Dutch corpora. When the
material of Frog was post-corrected manually, this is usually done on
the basis of the probabilities produced by the POS tagger and setting a
confidentiality threshold [VandenBosch+06]_.

-  The 500-million-word SoNaR corpus of written contemporary Dutch, and
   its 50-million word predecessor D-Coi [Oostdijk+08]_ [oostdijk2013construction]_;

-  The 500-million word Lassy Large corpus [LASSY]_ that has also been parsed
   automatically with the ALPINO parser [ALPINO]_;

-  The 115-hour JASMIN corpus of transcribed Dutch, spoken by elderly,
   non-native speakers, and children [Cucchiarini+13]_;

-  The 7-million word Dutch subcorpus of a multilingual parallel corpus
   of automotive texts [DPL2009]_;

-  The *Insight Interaction* corpus of 15 20-minute transcribed
   multi-modal dialogues [brone2015insight]_;

-  The SUBTLEX-NL word frequency database was based on an automatically
   analyzed 44-million word corpus of Dutch subtitles of movies and
   television shows [subtlex]_.

Feature generation for text filtering and Natural Language Processing
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Frog’s analyses can help to zoom in on particular linguistic
abstractions over text, such as adjectives or particular constructions,
to be used in further processing. They can also help to generate
annotation layers that can act as features in further NLP processing
steps. POS tags and lemmas are mostly used for these purposes. We list a
number of examples across the NLP board:

-  Sentence-level analysis tasks such as word sense disambiguation [Uvt-wsd1]_ and entity recognition [Vandecamp+2011]_;

-  Text-level tasks such as authorship attribution
   [Luyckx2011]_, emotion detection
   [vaassen2011]_, sentiment analysis
   [hogenboom2014]_, and readability prediction
   [de2014using]_;

-  Text-to-text processing tasks such as machine translation
   [Haque+11]_ and sub-sentential alignment for machine translation [macken2010sub]_;

-  Filtering Dutch texts for resource development, such as filtering adjectives for developing a subjectivity lexicon
   [Pattern]_, and POS tagging to assist shallow chunking of Dutch texts for bilingual terminology extraction [texsis2013]_.




.. [Atterer+2007] Atterer, Michaela and Hinrich Schütze. 2007. Prepositional phrase attachment without oracles. Computational Linguistics, 33(4):469–476.





..[brone2015insigh]t Brône, Geert and Bert Oben. 2015. Insight interaction: a multimodal and multifocal dialogue corpus. Language resources and evaluation, 49(1):195–214.



.. [Cucchiarini+13]   Cucchiarini, Catia and Hugo Van hamme. 2013. The Jasmin speech corpus: Recordings of children, non-natives and elderly people. In Essential Speech and Language Technology for Dutch. Springer, pages 147–164.



De Clercq, Orphée, Veronique Hoste, Bart Desmet, Philip Van Oosten, Martine De Cock, and Lieve Macken. 2014. Using the crowd for readability prediction. Natural Language Engineering, 20(03):293–325.

..[Pattern] De Smedt, Tom and Walter Daelemans. 2012. ” vreselijk mooi!”(terribly beautiful): A subjectivity lexicon for dutch adjectives. In LREC, pages 3568–3572.





.. [hogenboom2014] Hogenboom, Alexander, Bas Heerschop, Flavius Frasincar, Uzay Kaymak, and Franciska de Jong. 2014. Multilingual support for lexicon-based sentiment analysis guided by semantics. Decision support systems, 62:43–53.

.. [TTNWW] Kemps-Snijders, Marc, Ineke Schuurman, Walter Daelemans, Kris Demuynck, Brecht Desplanques, Véronique Hoste, Marijn Huijbregts, Jean-Pierre Martens, Joris Pelemans Hans Paulussen, Martin Reynaert, Vincent Van- deghinste, Antal van den Bosch, Henk van den Heuvel, Maarten van Gompel, Gertjan Van Noord, and Patrick Wambacq. 2017. TTNWW to the rescue: no need to know how to handle tools and resources. CLARIN in the Low Countries. pages 83-93.

.. [subtlex]  Keuleers, Emmanuel, Marc Brysbaert, and Boris New. 2010. Subtlex-nl: A new measure for dutch word frequency based on film subtitles. Behavior research methods, 42(3):643–650.

.. [de2014using] Lefever, Els, Lieve Macken, and Véronique Hoste. 2009. Language-independent bilingual terminology extraction from a multilingual parallel corpus. In Proceedings of the 12th Conference of the European Chapter of the Association for Computational Linguistics, pages 496–504. Association for Computational Linguistics.

.. [Luyckx2011] Luyckx, Kim. 2011. Scalability issues in authorship attribution. ASP/VUBPRESS/UPA.

.. [macken2010sub] Macken, Lieve. 2010. Sub-sentential alignment of translational correspondences. UPA University Press Antwerp.

.. [texsis2013] Macken, Lieve, Els Lefever, and Véronique Hoste. 2013. Texsis: bilingual terminology extraction from parallel corpora using chunk-based alignment. Terminology, 19(1):1–30.

.. [Oostdijk+08] Oostdijk, N., M. Reynaert, P. Monachesi, G. Van Noord, R. Ordelman, I. Schuurman, and V. Vandeghinste. 2008. From D-Coi to SoNaR: A reference corpus for Dutch. In Proceedings of the Sixth International Language Resources and Evaluation (LREC’08), Marrakech, Morocco.

.. [oostdijk2013construction] Oostdijk, Nelleke, Martin Reynaert, Véronique Hoste, and Ineke Schuurman. 2013. The construction of a 500- million-word reference corpus of contemporary written dutch. In Essential speech and language technology for Dutch. Springer, pages 219–247.

Petrov, Slav, Dipanjan Das, and Ryan McDonald. 2012. A universal part-of-speech tagset. In Nicoletta Calzolari (Conference Chair), Khalid Choukri, Thierry Declerck, Mehmet Ugūr Dogãn, Bente Maegaard, Joseph Mariani, Asuncion Moreno, Jan Odijk, and Stelios Piperidis, editors, Proceedings of the Eight International Con- ference on Language Resources and Evaluation (LREC’12), Istanbul, Turkey, may. European Language Resources Association (ELRA).

.. [vaassen2011] Vaassen, Frederik and Walter Daelemans. 2011. Automatic emotion classification for interpersonal communication. In Proceedings of the 2nd workshop on computational approaches to subjectivity and sentiment analysis, pages 104–110. Association for Computational Linguistics.

.. [vandecamp2011] Van de Camp, M. and A. Van den Bosch. 2011. A link to the past: Constructing historical social networks. In Proceedings of the 2nd Workshop on Computational Approaches to Subjectivity and Sentiment Analysis (WASSA 2.011), pages 61–69, Portland, Oregon, June. Association for Computational Linguistics.

.. [VandenBosch+06]  Van den Bosch, A., I. Schuurman, and V. Vandeghinste. 2006. Transferring PoS-tagging and lemmatization tools from spoken to written Dutch corpus development. In Proceedings of the Fifth International Conference on Language Resources and Evaluation, LREC-2006, Trento, Italy.


.. [MBMA] van den Bosch, Antal and Walter Daelemans. 1999. Memory-based morphological analysis. In Proceedings of the 37th Annual Meeting of the Association for Computational Linguistics on Computational Linguistics, ACL ’99, pages 285–292, Stroudsburg, PA, USA. Association for Computational Linguistics.

.. [POS2004] Van Eynde, Frank. 2004. Part of speech tagging en lemmatisering van het corpus gesproken nederlands. Technical report, Centrum voor Computerlinguıstiek, KU Leuven, Belgium.

.. [Uvt-wsd1] Van Gompel, M. 2010. Uvt-wsd1: A cross-lingual word sense disambiguation system. In SemEval ’10: Proceedings of the 5th International Workshop on Semantic Evaluation, pages 238–241, Morristown, NJ, USA. Association for Computational Linguistics.
