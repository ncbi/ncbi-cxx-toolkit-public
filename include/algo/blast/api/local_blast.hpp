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
//#include <algo/blast/api/uniform_search.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

// Forward declarations
class CSearchDatabase;
class IBlastSeqSrcAdapter;

/// Class to perform a BLAST search on local BLAST databases
class NCBI_XBLAST_EXPORT CLocalBlast : public CObject, public CThreadable
{
    CLocalBlast(CRef<IQueryFactory> qf,
                CConstRef<CBlastOptions> opts,
                const CSearchDatabase& dbinfo);
    CLocalBlast(CRef<IQueryFactory> qf,
                CConstRef<CBlastOptions> opts,
                IBlastSeqSrcAdapter& db);
    CLocalBlast(CRef<IQueryFactory> qf,
                CConstRef<CBlastOptions> opts,
                BlastSeqSrc* seqsrc);
    
    ISearch::TResults Run();
    
private:
    CRef<CBlastPrelimSearch> m_PrelimSearch;
    CRef<CBlastTracebackSearch> m_TbackSearch;
};

END_SCOPE(BLAST)
END_NCBI_SCOPE

/* @} */

#endif /* ALGO_BLAST_API___LOCAL_BLAST__HPP */
