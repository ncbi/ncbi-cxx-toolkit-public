#ifndef ALGO_ALIGN_SPLIGN_SIMPLE__HPP
#define ALGO_ALIGN_SPLIGN_SIMPLE__HPP
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
* Author:  Philip Johnson
*
* File Description:
*   CSplignSimple class definition -- simplified wrapper for calling splign
*
* ---------------------------------------------------------------------------
*/

#include <corelib/ncbistd.hpp>
#include <algo/align/splign/splign.hpp>
#include <algo/blast/api/bl2seq.hpp>

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)
    class CSeq_loc;
    class CScope;
    class CSeq_align_set;
END_SCOPE(objects)


class NCBI_XALGOALIGN_EXPORT CSplignSimple {
public:

    CSplignSimple(const objects::CSeq_loc &transcript,
                  const objects::CSeq_loc &genomic,
                  objects::CScope&);

    // Setters/Getters
    CRef<CSplign>&            SetSplign(void);
    CConstRef<CSplign>        GetSplign(void) const;
    CRef<blast::CBl2Seq>&     SetBlast(void);
    CConstRef<blast::CBl2Seq> GetBlast(void) const;

    const CSplign::TResults&  Run(void);

    CRef<objects::CSeq_align_set> GetResultsAsAln(void) const;

protected:

    CRef<CSplign>               m_Splign;
    CRef<blast::CBl2Seq>        m_Blast;

    CConstRef<objects::CSeq_id> m_TranscriptId;
    CConstRef<objects::CSeq_id> m_GenomicId;
};

END_NCBI_SCOPE

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.5  2004/06/29 20:48:18  kapustin
 * Use CRef to access CObject-derived members
 *
 * Revision 1.4  2004/06/09 14:24:40  kapustin
 * Add object getters
 *
 * Revision 1.3  2004/05/04 20:49:03  johnson
 * return correct seq-ids in seq-align
 *
 * Revision 1.2  2004/05/04 15:23:44  ucko
 * Split splign code out of xalgoalign into new xalgosplign.
 *
 * Revision 1.1  2004/05/03 15:39:10  johnson
 * initial revision
 *
 *
 * ===========================================================================
 */

#endif
