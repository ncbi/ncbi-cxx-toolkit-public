#ifndef ALGO_ALIGN_DEMO_SPLIGN_SEQ_LOADER__HPP
#define ALGO_ALIGN_DEMO_SPLIGN_SEQ_LOADER__HPP

/* $Id$
* ===========================================================================
*
*                            public DOMAIN NOTICE                          
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
* File Description:
*   CSeqLoader class
*
*/


#include <algo/align/splign/splign.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbi_limits.hpp>

BEGIN_NCBI_SCOPE


BEGIN_SCOPE(objects)
    class CSeq_entry;
END_SCOPE(objects)


//////////
// Batch mode sequence loader

class CSeqLoader: public CSplignSeqAccessor
{
public:

    void Open (const string& filename_index);  
    virtual void Load( const string& id, vector<char> *seq,
                       size_t start, size_t finish);
    
private:
    
    vector<string> m_filenames;
    
    struct SIdxTarget {
        SIdxTarget(): m_filename_idx(kMax_UInt), m_offset(kMax_UInt) {}
        size_t m_filename_idx;
        size_t m_offset;
    };
    map<string, SIdxTarget> m_idx;
    
    size_t m_min_idx;
    
};


//////////
// Pairwise mode sequence loader

class CSeqLoaderPairwise: public CSplignSeqAccessor
{
public:

    CSeqLoaderPairwise(const string& query_filename,
                       const string& subj_filename);
    
    virtual void Load( const string& id, vector<char> *seq,
                       size_t start, size_t finish);
    
    string GetQueryStringId(void) const {
        return m_QueryId;
    }

    string GetSubjStringId(void) const {
        return m_SubjId;
    }

    CRef<objects::CSeq_entry> GetQuerySeqEntry(void) const {
        return m_se_query;
    }

    CRef<objects::CSeq_entry> GetSubjSeqEntry(void) const {
        return m_se_subj;
    }

private:
    
    string m_QueryId, m_SubjId;
    vector<char> m_Query, m_Subj;
    CRef<objects::CSeq_entry> m_se_query, m_se_subj;
};


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.8  2004/05/10 16:39:56  kapustin
 * Add a pairwise mode sequence loader
 *
 * Revision 1.7  2004/05/04 15:23:45  ucko
 * Split splign code out of xalgoalign into new xalgosplign.
 *
 * Revision 1.6  2004/04/23 14:33:32  kapustin
 * *** empty log message ***
 *
 * Revision 1.4  2003/12/03 19:45:33  kapustin
 * Keep min index value to support non-zero based index
 *
 * Revision 1.3  2003/11/05 20:32:11  kapustin
 * Include source information into the index
 *
 * Revision 1.2  2003/10/31 19:43:15  kapustin
 * Format and compatibility update
 *
 * ===========================================================================
 */

#endif

