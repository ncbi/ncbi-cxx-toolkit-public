/* $Id$
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's offical duties as a United States Government employee and
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
* ===========================================================================
*/



/// @file: blast_tabular.hpp
/// C++ implementation of the on-the-fly tabular formatting of BLAST results. 

#ifndef ALGO_BLAST_API___BLAST_TABULAR__HPP
#define ALGO_BLAST_API___BLAST_TABULAR__HPP

#ifdef Main
#undef Main
#endif
 
#include <corelib/ncbithr.hpp>
#include <algo/blast/api/blast_types.hpp>
#include <algo/blast/core/blast_hits.h>
#include <algo/blast/core/lookup_wrap.h>
#include <algo/blast/core/blast_seqsrc.h>
#include <algo/blast/core/blast_hspstream.h>
#include <algo/blast/core/blast_gapalign.h>

#include <algo/blast/api/db_blast.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

//BEGIN_NCBI_SCOPE
//BEGIN_SCOPE(blast)
USING_NCBI_SCOPE;
USING_SCOPE(blast);

/** Data structure containing all information necessary for production of the
 * tabular output.
 */
class CBlastTabularFormatThread : public CThread 
{
public:
    CBlastTabularFormatThread(const CDbBlast* blaster, 
        const TSeqLocVector& query_v, CNcbiOstream& ostream);
    ~CBlastTabularFormatThread();
protected:
    virtual void* Main(void);
    virtual void OnExit(void);
private:
    EBlastProgramType m_Program; /**< Type of BLAST program */
    BlastHSPStream* m_pHspStream; /**< Source of the BLAST results */
    BLAST_SequenceBlk* m_pQuery; /**< Query sequence */
    TSeqLocVector m_QueryVec; /**< Source of query sequences identifiers */
    CNcbiOstream* m_OutStream; /**< Output stream */
    BlastSeqSrc* m_ipSeqSrc; /**< Source of the subject sequences */
    BlastQueryInfo* m_ipQueryInfo; /**< Query information, including context
                                   offsets and effective lengths. */
    BlastScoringParameters* m_ipScoreParams;
    BlastExtensionParameters* m_ipExtParams;
    BlastHitSavingParameters* m_ipHitParams;
    BlastEffectiveLengthsParameters* m_ipEffLenParams;
    AutoPtr<Uint1, ArrayDeleter<Uint1> > m_iGenCodeString;
    BlastGapAlignStruct* m_ipGapAlign;
    bool m_ibPerformTraceback; /**< Must gapped extension with traceback be
                               performed before formatting? */
};

//END_SCOPE(blast)
//END_NCBI_SCOPE

/* @} */

/*
* ===========================================================================
*
* $Log$
* Revision 1.3  2004/08/11 11:38:13  ivanov
* Removed export specifier from private class declaration
*
* Revision 1.2  2004/07/06 15:54:20  dondosha
* Use EBlastProgramType enumeration type for program
*
* Revision 1.1  2004/06/15 18:51:16  dondosha
* Implementation of a thread producing on-the-fly output from a BLAST search
*
* ===========================================================================
*/

#endif /* ALGO_BLAST_API___BLAST_TABULAR__HPP */

