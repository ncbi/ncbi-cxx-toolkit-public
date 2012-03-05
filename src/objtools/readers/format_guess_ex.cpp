/*  $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Author:  Nathan Bouk
 *
 * File Description:
 *   Wrapper and extention to CFormatGuess, using actual file readers
 *     when CFormatGuess fails
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>              
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbithr.hpp>
#include <corelib/ncbiutil.hpp>
#include <corelib/ncbiexpt.hpp>
#include <corelib/stream_utils.hpp>

#include <util/static_map.hpp>
#include <util/line_reader.hpp>

#include <serial/iterator.hpp>
#include <serial/objistrasn.hpp>

#include <objtools/readers/format_guess_ex.hpp>


//#include <objtools/hgvs/hgvs_parser.hpp>
#include <objtools/readers/gff3_reader.hpp>
#include <objtools/readers/gff2_data.hpp>
#include <objtools/readers/gff2_reader.hpp>
#include <objtools/readers/gtf_reader.hpp>
#include <objtools/readers/bed_reader.hpp>
#include <objtools/readers/microarray_reader.hpp>
#include <objtools/readers/wiggle_reader.hpp>
#include <objtools/readers/fasta.hpp>
#include <objtools/readers/agp_read.hpp>
#include <objtools/readers/rm_reader.hpp>


#define NCBI_USE_ERRCODE_X   Objtools_Rd_RepMask

BEGIN_NCBI_SCOPE
using namespace ncbi;
using namespace objects;
using namespace std;



CFormatGuessEx::CFormatGuessEx() 
    : m_Guesser(new CFormatGuess) 
{
    ;
}


CFormatGuessEx::CFormatGuessEx(const string& FileName)
    : m_Guesser(new CFormatGuess(FileName))
{
    CNcbiIfstream FileIn(FileName.c_str());
    x_FillLocalBuffer(FileIn);
}


CFormatGuessEx::CFormatGuessEx(CNcbiIstream& In)
    : m_Guesser(new CFormatGuess(In))
{
    x_FillLocalBuffer(In);
}


CFormatGuessEx::~CFormatGuessEx()
{
    ;
}


CFormatGuess::EFormat CFormatGuessEx::GuessFormat()
{
    CFormatGuess::EFormat Guess;
    Guess = m_Guesser->GuessFormat();
	
    ERR_POST(Info << " CFormatGuessEx:: Initial CFormatGuess: " << (int)Guess);
	
    if(Guess != CFormatGuess::eUnknown) {
        return Guess;
    }
    else {
        CFormatGuess::EFormat CheckOrder[] = {
            //CFormatGuess::eRmo
            CFormatGuess::eAgp,
            //case CFormatGuess::eXml:
            CFormatGuess::eWiggle,
            CFormatGuess::eBed,
            CFormatGuess::eBed15,
            CFormatGuess::eFasta,
            //case CFormatGuess::eTextAsn:
            CFormatGuess::eGtf,
            CFormatGuess::eGff3,
            CFormatGuess::eGff2//,
            //CFormatGuess::eHgvs
        };

        for(int Loop = 0; Loop < 8; Loop++ ) {
            bool Found = x_TryFormat(CheckOrder[Loop]);
            if(Found)
                return CheckOrder[Loop];
        }
        return CFormatGuess::eUnknown;
    }
}


bool CFormatGuessEx::TestFormat(CFormatGuess::EFormat Format)
{
    bool TestResult = m_Guesser->TestFormat(Format);

    if(TestResult) {
        return true;
    }
    else {
        return x_TryFormat(Format);
    }
}



bool CFormatGuessEx::x_FillLocalBuffer(CNcbiIstream& In) 
{
    m_LocalBuffer.str().clear();
    m_LocalBuffer.clear();
	
    streamsize Total = 0;
    while(!In.eof()) {
        char buff[4096];
        In.read(buff, sizeof(buff));
        streamsize count = In.gcount();
        if(count == 0)
            break;
        m_LocalBuffer.write(buff, count);
        Total += count;
        if(Total >= (1024*1024))
            break;
    }

    CStreamUtils::Pushback(In, m_LocalBuffer.str().c_str(), Total);
    In.clear();

    return true;
}


bool CFormatGuessEx::x_TryFormat(CFormatGuess::EFormat Format)
{
    switch(Format) {
	
	//case CFormatGuess::eBinaryAsn:
	//	return x_TryBinaryAsn();
	case CFormatGuess::eRmo:
            return x_TryRmo();
	case CFormatGuess::eAgp:
            return x_TryAgp();
            //case CFormatGuess::eXml:
            //	return x_TryXml();
	case CFormatGuess::eWiggle:
            return x_TryWiggle();
	case CFormatGuess::eBed:
            return x_TryBed();
	case CFormatGuess::eBed15:
            return x_TryBed15();
	case CFormatGuess::eFasta:
            return x_TryFasta();
            //case CFormatGuess::eTextAsn:
            //	return x_TryTextAsn();
	case CFormatGuess::eGtf:
            return x_TryGtf();
	case CFormatGuess::eGff3:
            return x_TryGff3();
	case CFormatGuess::eGff2:
            return x_TryGff2();
            //case CFormatGuess::eHgvs:
            //	return x_TryHgvs();

	default:
            return false;
    };
}


//	bool x_TryBinaryAsn();

bool CFormatGuessEx::x_TryRmo()
{
    m_LocalBuffer.clear();
    m_LocalBuffer.seekg(0);

    CRmReader::TFlags Flags =
        CRmReader::fIncludeRepeatClass |
        CRmReader::fIncludeRepeatName;
    CRef<CSeq_annot> Result;
    
    try {
        CRmReader* Reader;
    	Reader = CRmReader::OpenReader(m_LocalBuffer);
    	Reader->Read(Result, Flags);
    	CRmReader::CloseReader(Reader);
    } catch(CException&) {
    } catch(...) {
    }
	
    return (bool)(Result);
}

bool CFormatGuessEx::x_TryAgp()
{
    m_LocalBuffer.clear();
    m_LocalBuffer.seekg(0);
	
    vector<CRef<CBioseq> > Bioseqs;
    try {
        AgpRead(m_LocalBuffer, Bioseqs);
    } catch(CException&) {
    } catch(...) {
    }

    return (!Bioseqs.empty());
}

//	bool x_TryXml();

bool CFormatGuessEx::x_TryWiggle()
{
    m_LocalBuffer.clear();
    m_LocalBuffer.seekg(0);

    int WiggleCount = 0;
	
    CWiggleReader Reader;
    CStreamLineReader LineReader(m_LocalBuffer);
		
    CRef<CSeq_annot> Annot;
    try {
        Annot = Reader.ReadSeqAnnot(LineReader);
    } catch(CException&) {
    } catch(...) {
    }

    if (!Annot.IsNull() &&
        Annot->CanGetData() && 
        Annot->GetData().IsFtable())
        WiggleCount++;

    return (WiggleCount > 0);
}

bool CFormatGuessEx::x_TryBed()
{
    m_LocalBuffer.clear();
    m_LocalBuffer.seekg(0);

    int BedCount = 0;
	
    CBedReader Reader;
    CStreamLineReader LineReader(m_LocalBuffer);
	
    vector<CRef<CSeq_annot> > LocalAnnots;
    try {
        Reader.ReadSeqAnnots(LocalAnnots, LineReader);
    } catch(CException&) {
    } catch(...) {
    }

    ITERATE(vector<CRef<CSeq_annot> >, AnnotIter, LocalAnnots) {
        if(!AnnotIter->IsNull() && (*AnnotIter)->CanGetData() && 
           (*AnnotIter)->GetData().IsFtable())
            BedCount++;
    }

    return (BedCount > 0);
}

bool CFormatGuessEx::x_TryBed15()
{
    m_LocalBuffer.clear();
    m_LocalBuffer.seekg(0);

    int Bed15Count = 0;
	
    CMicroArrayReader Reader;
    CStreamLineReader LineReader(m_LocalBuffer);
		
    CRef<CSeq_annot> Annot;
    try {
        Annot = Reader.ReadSeqAnnot(LineReader);
    } catch(CException&) {
    } catch(...) {
    }

    if (!Annot.IsNull() &&
        Annot->CanGetData() && 
        Annot->GetData().IsFtable())
        Bed15Count++;

    return (Bed15Count > 0);
}

bool CFormatGuessEx::x_TryFasta()
{
    m_LocalBuffer.clear();
    m_LocalBuffer.seekg(0);

    CRef<CSeq_entry> Result;
    try {
        CFastaReader Reader(m_LocalBuffer);
     	Result = Reader.ReadSet();
    } catch(CException&) {
    } catch(...) {
    }

    return (bool)(Result);
}

//	bool x_TryTextAsn();

bool CFormatGuessEx::x_TryGtf()
{
    m_LocalBuffer.clear();
    m_LocalBuffer.seekg(0);

    int GtfCount = 0;
	
    CGtfReader Reader(CGtfReader::fNewCode);
    CStreamLineReader LineReader(m_LocalBuffer);
		
    vector<CRef<CSeq_annot> > LocalAnnots;
    try {
        Reader.ReadSeqAnnotsNew(LocalAnnots, LineReader);
    } catch(CException&) {
    } catch(...) {
    }

    ITERATE(vector<CRef<CSeq_annot> >, AnnotIter, LocalAnnots) {
        if(!AnnotIter->IsNull() && (*AnnotIter)->CanGetData() && 
           (*AnnotIter)->GetData().IsFtable())
            GtfCount++;
    }

    return (GtfCount > 0);
}

bool CFormatGuessEx::x_TryGff3()
{
    m_LocalBuffer.clear();
    m_LocalBuffer.seekg(0);

    int Gff3Count = 0;
	
    CGff3Reader Reader(CGff3Reader::fNewCode);
    CStreamLineReader LineReader(m_LocalBuffer);
		
    vector<CRef<CSeq_annot> > LocalAnnots;
    try {
        Reader.ReadSeqAnnotsNew(LocalAnnots, LineReader);
    } catch(CException&) {
    } catch(...) {
    }

    ITERATE(vector<CRef<CSeq_annot> >, AnnotIter, LocalAnnots) {
        if(!AnnotIter->IsNull() && (*AnnotIter)->CanGetData() && 
           (*AnnotIter)->GetData().IsFtable())
            Gff3Count++;
    }

    return (Gff3Count > 0);
}

bool CFormatGuessEx::x_TryGff2()
{
    m_LocalBuffer.clear();
    m_LocalBuffer.seekg(0);

    int Gff2Count = 0;
	
    CGff2Reader Reader(CGff2Reader::fNewCode);
    CStreamLineReader LineReader(m_LocalBuffer);
		
    vector<CRef<CSeq_annot> > LocalAnnots;
    try {
        Reader.ReadSeqAnnotsNew(LocalAnnots, LineReader);
    } catch(CException&) {
    } catch(...) {
    }

    ITERATE(vector<CRef<CSeq_annot> >, AnnotIter, LocalAnnots) {
        if(!AnnotIter->IsNull() && (*AnnotIter)->CanGetData() && 
           (*AnnotIter)->GetData().IsFtable())
            Gff2Count++;
    }

    return (Gff2Count > 0);
}

/*
  bool CFormatGuessEx::x_TryHgvs()
  {
  m_LocalBuffer.clear();
  m_LocalBuffer.seekg(0);

  CScope* Dummy = NULL;	
  CHgvsParser Parser(*Dummy);

  int HgvsCount = 0;
  while(m_LocalBuffer) {
  string Line;
  NcbiGetlineEOL(m_LocalBuffer, Line);

  if(m_LocalBuffer.eof() || Line.empty() || Line[0] == '#')
  continue;

  NStr::ReplaceInPlace(Line, "\r", "");
  NStr::ReplaceInPlace(Line, "\n", "");

  bool Parsed;
  try {
  Parsed = Parser.CanParseHgvsExpression(Line);
  //Feat = Parser.AsVariationFeat(Line);
  } catch(CException&) {
  } catch(...) {
  }

  if(Parsed) 
  HgvsCount++;
  }

  return (HgvsCount > 0);
  }
*/



END_NCBI_SCOPE
