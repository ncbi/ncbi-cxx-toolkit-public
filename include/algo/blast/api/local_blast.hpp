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
 * Author: Christiam Camacho, Kevin Bealer
 *
 */

/** @file local_blast.hpp
 * NOTE: This file contains work in progress and the APIs are likely to change,
 * please do not rely on them until this notice is removed.
 */

#ifndef ALGO_BLAST_API___LOCAL_BLAST_HPP
#define ALGO_BLAST_API___LOCAL_BLAST_HPP

#include <algo/blast/api/prelim_stage.hpp>
#include <algo/blast/api/traceback_stage.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/// Class to perform a BLAST search on local BLAST databases
/// Note that PHI-BLAST can be run using this class also, one only need to
/// configure it as a regular blastp or blastn search and set the pattern in
/// the CBlastOptionsHandle object
/// @todo should RPS-BLAST be moved out of this object?
class NCBI_XBLAST_EXPORT CLocalBlast : public CObject, public CThreadable
{
public:
    CLocalBlast(CRef<IQueryFactory> query_factory,
                //CConstRef<CBlastOptionsHandle> opts_handle,
                CRef<CBlastOptionsHandle> opts_handle,
                const CSearchDatabase& dbinfo);
    CLocalBlast(CRef<IQueryFactory> query_factory,
                //CConstRef<CBlastOptionsHandle> opts_handle,
                CRef<CBlastOptionsHandle> opts_handle,
                IBlastSeqSrcAdapter& db);
    CLocalBlast(CRef<IQueryFactory> query_factory,
                //CConstRef<CBlastOptionsHandle> opts_handle,
                CRef<CBlastOptionsHandle> opts_handle,
                BlastSeqSrc* seqsrc);
    
    CSearchResultSet Run();
    
private:
    /// Query factory from which to obtain the query sequence data
    CRef<IQueryFactory> m_QueryFactory;

    /// Options to use
    CRef<CBlastOptions> m_Opts;
    //CConstRef<CBlastOptionsHandle> m_OptsHandle;

    /// Internal core data structures which are used in the preliminary and
    /// traceback stages of the search
    CRef<SInternalData> m_InternalData;

    /// Object which runs the preliminary stage of the search
    CRef<CBlastPrelimSearch> m_PrelimSearch;

    /// Object which runs the traceback stage of the search
    CRef<CBlastTracebackSearch> m_TbackSearch;
};

END_SCOPE(BLAST)
END_NCBI_SCOPE

/* @} */

#endif /* ALGO_BLAST_API___LOCAL_BLAST__HPP */
