#ifndef ALGO_ALIGN_SPLIGN_UTIL__HPP
#define ALGO_ALIGN_SPLIGN_UTIL__HPP

/* $Id$
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
* Author:  Yuri Kapustin
*
* File Description:  Helper functions
*                   
* ===========================================================================
*/


#include <algo/align/nw_aligner.hpp>
#include <algo/align/splign/splign_hit.hpp>


BEGIN_NCBI_SCOPE


#ifdef GENOME_PIPELINE

//////////////////////////////////////
// INF file readers and parsers

class CInfTable
{

public:

  CInfTable(size_t cols);
  virtual ~CInfTable() {}

  size_t Load(const string& filename);

  struct SInfo {
    enum EStrand {
      eUnknown,
      ePlus,
      eMinus
    } m_strand;
    int m_polya_start;
    int m_polya_end;
    size_t m_size;

    SInfo(): m_strand(eUnknown),
	     m_polya_start(-1), m_polya_end(-1), m_size(0) {}
  };

  bool GetInfo(const string& id, SInfo& info);

protected:

  size_t m_cols; // number of columns
  map<string, SInfo>  m_map;

  vector<const char*> m_data;

  const char*  x_GetCol(size_t col) {
    return col < m_cols? m_data[col]: 0;
  }

  virtual bool x_ParseLine (const char* line,
			    string& accession, SInfo& info) = 0;

private:

  bool x_ReadColumns(char* line);

};


class CInf_mRna: public CInfTable
{

public:

  CInf_mRna(): CInfTable(11) {}
  virtual ~CInf_mRna() {}

protected:

  virtual bool x_ParseLine (const char* line,
			    string& accession, SInfo& info);
};


class CInf_EST: public CInfTable
{

public:

  CInf_EST(): CInfTable(9) {}
  virtual ~CInf_EST() {}

protected:

  virtual bool x_ParseLine (const char* line,
			    string& accession, SInfo& info);
};


#endif // GENOME_PIPELINE


/////////////////////
// helpers

void   CleaveOffByTail(vector<CHit>* hits, size_t polya_start);
void   GetHitsMinMax(const vector<CHit>& hits,
		     size_t* qmin, size_t* qmax,
		     size_t* smin, size_t* smax);
string RLE(const string& in);

void   XFilter(vector<CHit>* hits);
CNWAligner::TScore ScoreByTranscript(const CNWAligner& aligner,
                                     const char* transcript);
bool   IsConsensus(const char* donor, const char* acceptor);

struct SCompliment
{
    char operator() (char c) {
        switch(c) {
        case 'A': return 'T';
        case 'G': return 'C';
        case 'T': return 'A';
        case 'C': return 'G';
        }
        return c;
    }
};

END_NCBI_SCOPE

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.4  2004/05/04 15:23:45  ucko
 * Split splign code out of xalgoalign into new xalgosplign.
 *
 * Revision 1.3  2004/04/23 14:37:44  kapustin
 * *** empty log message ***
 *
 *
 * ===========================================================================
 */


#endif
