/* ex: set tabstop=8 expandtab: */
/*
  $Id$
  $URL$

  Copyright (c) 2006 - 2014
  Tilburg University

  This file is part of frog

  frog is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 3 of the License, or
  (at your option) any later version.

  frog is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.

  For questions and suggestions, see:
      http://ilk.uvt.nl/software.html
  or send mail to:
      timbl@uvt.nl
*/


#ifndef FROG_H
#define FROG_H

#include <set>
#include "ticcutils/LogStream.h"
#include "ticcutils/Configuration.h"
#include "ticcutils/Timer.h"
#include "libfolia/document.h"

class FrogOptions {
    public:
    bool doTok;
    bool doLemma;
    bool doMorph;
    bool doDaringMorph;
    bool doMwu;
    bool doIOB;
    bool doNER;
    bool doParse;
    bool doSentencePerLine;
    bool doQuoteDetection;
    bool doDirTest;
    bool doServer;
    
    bool doXMLin;
    bool doXMLout;
    bool doKanon;
    

    bool interactive;

    std::string encoding;
    std::string uttmark;
    std::string listenport;
    std::string docid;
    std::string textclass;
    

    FrogOptions() {
        doTok = doLemma = doMorph = doMwu = doIOB = doNER = doParse = true;
        doDaringMorph = false;
        doSentencePerLine = false;
        doQuoteDetection = false;
        doDirTest = false;
        doServer = false;
        doXMLin =  false;
        doXMLout =  false;
        doKanon =  false;

        interactive = false;

        encoding = "";
        uttmark = "";
        listenport = "void";
        docid = "untitled";
        textclass = "";
    }

    FrogOptions(const FrogOptions & ref) {
        doTok = ref.doTok;
        doLemma = ref.doLemma;
        doMorph = ref.doMorph;
        doMwu = ref.doMwu;
        doIOB = ref.doIOB;
        doNER = ref. doNER;
        doParse = ref.doParse;
        doDaringMorph = ref.doDaringMorph;
        doSentencePerLine = ref.doSentencePerLine;
        doQuoteDetection = ref.doQuoteDetection;
        doDirTest = ref.doDirTest;
        doServer = ref.doServer;
        doXMLin = ref.doXMLin;
        doXMLout = ref.doXMLout;
        doKanon = ref.doKanon;
        interactive = ref.interactive;
        encoding = ref.encoding;
        uttmark = ref.uttmark;
        listenport = ref.listenport;
        docid = ref.docid;
        textclass = ref.textclass;

    }

};

//declared here but not defined in Frog.cxx (bit messy)
std::string prefix( const std::string&, const std::string& );
std::string getTime();
bool existsDir( const std::string& );
void getFileNames( const std::string&, const std::string&, std::set<std::string>& );


extern TiCC::LogStream *theErrLog;

class TimerBlock{
public:
  TiCC::Timer parseTimer;
  TiCC::Timer tokTimer;
  TiCC::Timer mblemTimer;
  TiCC::Timer mbmaTimer;
  TiCC::Timer mwuTimer;
  TiCC::Timer tagTimer;
  TiCC::Timer iobTimer;
  TiCC::Timer nerTimer;
  TiCC::Timer prepareTimer;
  TiCC::Timer pairsTimer;
  TiCC::Timer relsTimer;
  TiCC::Timer dirTimer;
  TiCC::Timer csiTimer;
  TiCC::Timer frogTimer;
};

bool froginit(FrogOptions & options, TiCC::Configuration & configuration);
void Test( const std::string& infilename, std::ostream &os, FrogOptions & options, const std::string& xmlOutF );
void Test( folia::Document& doc, std::ostream& outStream, FrogOptions & options,  bool interactive = false, const std::string& xmlOutFile = "" );
bool TestSentence( folia::Sentence* sent, FrogOptions & options, TimerBlock& timers);

#endif
