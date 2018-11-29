.. _pythonfrog:


Using Frog from Python
-----------------------

It is possible to call Frog directly from Python using the
``python-frog`` software library. Contrary to the Frog client for Python
discussed in Section [servermode], this library is a direct binding with
code from Frog and does not use a client/server model. It therefore
offers the tightest form of integration, and highest performance,
possible.

Installation
~~~~~~~~~~~~~

The Python-Frog library is not included with Frog itself, but is shipped
separately from https://github.com/proycon/python-frog.

Users who installed Frog using LaMachine, however, will already find
that this software has been installed.

Other users will need to compile and install it from source. First
ensure Frog itself is installed, then install the dependency
``cython``\  [14]_. Installation of Python-Frog is then done by running:
``$ python setup.py install`` from its directory.

Usage
~~~~~~

The Python 3 example below illustrates how to parse text with Frog:

::

    from frog import Frog, FrogOptions

    frog = Frog(FrogOptions(parser=False))

    output = frog.process_raw("Dit is een test")
    print("RAW OUTPUT=",output)
    output = frog.process("Dit is nog een test.")
    print("PARSED OUTPUT=",output)

To instantiate the ``Frog`` class, two arguments are needed. The first
is a ``FrogOptions`` instance that specifies the configuration options
you want to pass to Frog.

The ``Frog`` instance offers two methods: ``process_raw(text)`` and
``process(text)``. The former just returns a string containing the usual
multiline, columned, and TAB delimiter output. The latter parses this
string into a dictionary. The example output of this from the script
above is shown below:

::

    PARSED OUTPUT = [
     {'chunker': 'B-NP', 'index': '1', 'lemma': 'dit', 'ner': 'O',
      'pos': 'VNW(aanw,pron,stan,vol,3o,ev)', 'posprob': 0.777085, 'text': 'Dit', 'morph': '[dit]'},
     {'chunker': 'B-VP', 'index': '2', 'lemma': 'zijn', 'ner': 'O',
      'pos': 'WW(pv,tgw,ev)', 'posprob': 0.999966, 'text': 'is', 'morph': '[zijn]'},
     {'chunker': 'B-NP', 'index': '3', 'lemma': 'nog', 'ner': 'O',
      'pos': 'BW()', 'posprob': 0.99982, 'text': 'nog', 'morph': '[nog]'},
     {'chunker': 'I-NP', 'index': '4', 'lemma': 'een', 'ner': 'O',
      'pos': 'LID(onbep,stan,agr)', 'posprob': 0.995781, 'text': 'een', 'morph': '[een]'},
     {'chunker': 'I-NP', 'index': '5', 'lemma': 'test', 'ner': 'O',
      'pos': 'N(soort,ev,basis,zijd,stan)', 'posprob': 0.903055, 'text': 'test', 'morph': '[test]'},
     {'chunker': 'O', 'index': '6', 'eos': True, 'lemma': '.', 'ner': 'O',
      'pos': 'LET()', 'posprob': 1.0, 'text': '.', 'morph': '[.]'}
    ]

There are various options you can set when creating an instance of
``FrogOptions``, they are set as keyword arguments:

-  ``tok`` – *bool* – Do tokenisation? (default: True)

-  ``lemma`` – *bool* – Do lemmatisation? (default: True)

-  ``morph`` – *bool* – Do morphological analysis? (default: True)

-  ``daringmorph`` – *bool* – Do morphological analysis in new
   experimental style? (default: False)

-  ``mwu`` – *bool* – Do Multi Word Unit detection? (default: True)

-  ``chunking`` – *bool* – Do Chunking/Shallow parsing? (default: True)

-  ``ner`` – *bool* – Do Named Entity Recognition? (default: True)

-  ``parser`` – *bool* – Do Dependency Parsing? (default: False).

-  ``xmlin`` – *bool* – Input is FoLiA XML (default: False)

-  ``xmlout`` – *bool* – Output is FoLiA XML (default: False)

-  ``docid`` – *str* – Document ID (for FoLiA)

-  ``numThreads`` – *int* – Number of threads to use (default: unset,
   unlimited)


   .. [14]
      Versions for Python 3 may be called ``cython3`` on distributions such
      as Debian or Ubuntu
