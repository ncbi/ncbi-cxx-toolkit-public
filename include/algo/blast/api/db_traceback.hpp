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
* Author:  Ilya Dondoshansky
*
* File Description:
*   Class interface for performing BLAST traceback only, when preliminary 
*   BLAST results have already been found.
*
*/

#ifndef ALGO_BLAST_API___DBTRACEBACK__HPP
#define ALGO_BLAST_API___DBTRACEBACK__HPP

#include <algo/blast/api/db_blast.hpp>
#include <algo/blast/core/blast_gapalign.h>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

class NCBI_XBLAST_EXPORT CDbBlastTraceback : public CDbBlast
{
public:
    CDbBlastTraceback(const TSeqLocVector& queries, BlastSeqSrc* seq_src,
                      EProgram p, BlastHSPResults* results);
    CDbBlastTraceback(const TSeqLocVector& queries, BlastSeqSrc* seq_src,
                      CBlastOptionsHandle& opts, BlastHSPResults* results);
    void PartialRun(); 

protected:    
    int SetupSearch();
    void RunSearchEngine();
    void x_ResetQueryDs();
private:
    BlastScoringParameters* m_ipScoringParams;
    BlastExtensionParameters* m_ipExtParams;
    BlastHitSavingParameters* m_ipHitParams;
    BlastEffectiveLengthsParameters* m_ipEffLenParams;
    BlastGapAlignStruct* m_ipGapAlign;
};

inline void CDbBlastTraceback::PartialRun()
{
    // Don't need this method!
}

END_SCOPE(blast)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.5  2004/05/07 15:40:19  papadopo
* add BlastScoringParameters private member
*
* Revision 1.4  2004/03/16 23:30:25  dondosha
* Changed mi_ to m_i in member field names
*
* Revision 1.3  2004/03/09 18:41:31  dondosha
* Added effective lengths parameters member
*
* Revision 1.2  2004/02/24 20:38:53  dondosha
* Removed irrelevant CVS log comments
*
* Revision 1.1  2004/02/24 18:20:40  dondosha
* Class derived from CDbBlast to do only traceback, given precomputed HSP results
* ===========================================================================
*/

#endif  /* ALGO_BLAST_API___DBBLAST__HPP */
