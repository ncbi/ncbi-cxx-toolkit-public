#ifndef ALGO_ALIGN_SPLIGN_FORMATTER__HPP
#define ALGO_ALIGN_SPLIGN_FORMATTER__HPP

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
*   Splign formatter
*/

#include <algo/align/splign/splign.hpp>


BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)
    class CSeq_id;
    class CDense_seg;
    class CSeq_align;
    class CSeq_align_set;
END_SCOPE(objects)


class NCBI_XALGOALIGN_EXPORT CSplignFormatter: public CObject
{
public:

    CSplignFormatter(const CSplign::TResults& results);
    CSplignFormatter(const CSplign& splign);

    // setters
    void SetSeqIds(CConstRef<objects::CSeq_id> id1, 
                   CConstRef<objects::CSeq_id> id2);

    // formatters
    string AsText(const CSplign::TResults* results = 0) const;
    CRef<objects::CSeq_align_set> AsSeqAlignSet(const CSplign::TResults*
                                                results = 0) const;

private:

    const CSplign::TResults            m_splign_results;

    CConstRef<objects::CSeq_id>        m_QueryId, m_SubjId;

    CRef<objects::CSeq_align> x_Compartment2SeqAlign(
                               const vector<size_t>& boxes,
                               const vector<string>& transcripts,
                               const vector<CNWAligner::TScore>& scores) const;

    void x_Exon2DS(const size_t* box, const string& trans,
                   objects::CDense_seg* pds) const;

    void x_Init(void);
};


END_NCBI_SCOPE


/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.12  2005/09/12 16:22:31  kapustin
 * Move compartmentization to xalgoutil
 *
 * Revision 1.11  2005/01/04 15:48:30  kapustin
 * Move SetSeqIds() implementation to the cpp file
 *
 * Revision 1.10  2005/01/03 22:47:20  kapustin
 * Implement seq-ids with CSeq_id instead of generic strings
 *
 * Revision 1.9  2004/11/29 14:36:45  kapustin
 * CNWAligner::GetTranscript now returns TTranscript and direction can be 
 * specified. x_ScoreByTanscript renamed to ScoreFromTranscript with two 
 * additional parameters to specify starting coordinates.
 *
 * Revision 1.8  2004/06/21 17:44:46  kapustin
 * Add result param to AsText and AsSeqAlignSet with zero default
 *
 * Revision 1.7  2004/05/04 15:23:44  ucko
 * Split splign code out of xalgoalign into new xalgosplign.
 *
 * Revision 1.6  2004/04/30 15:00:32  kapustin
 * Support ASN formatting
 *
 * Revision 1.5  2004/04/23 14:36:24  kapustin
 * Initial revision
 *
 * ===========================================================================
 */

#endif
