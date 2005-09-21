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

/** @file blast_seqsrc_adapter_priv.hpp
 * NOTE: This file contains work in progress and the APIs are likely to change,
 * please do not rely on them until this notice is removed.
 */

#ifndef ALGO_BLAST_API___BLAST_SEQSRC_ADAPTER_PRIV_HPP
#define ALGO_BLAST_API___BLAST_SEQSRC_ADAPTER_PRIV_HPP

#include <algo/blast/core/blast_seqsrc.h>
#include <algo/blast/api/uniform_search.hpp> // for CSearchDatabase

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

class CSearchDatabase;

/// Interface to convert
class NCBI_XBLAST_EXPORT IBlastSeqSrcAdapter : public CObject
{
public:
    IBlastSeqSrcAdapter(const CSearchDatabase& dbinfo);
    virtual ~IBlastSeqSrcAdapter();
    BlastSeqSrc* GetBlastSeqSrc();

protected:
    virtual BlastSeqSrc* 
    x_CalculateBlastSeqSrc(const CSearchDatabase& dbinfo) = 0;

    const CSearchDatabase& m_DbInfo;
    BlastSeqSrc* m_SeqSrc;
    
private:
    // Prevent auto methods
    IBlastSeqSrcAdapter(const IBlastSeqSrcAdapter&);
    IBlastSeqSrcAdapter & operator=(const IBlastSeqSrcAdapter&);
};


END_SCOPE(BLAST)
END_NCBI_SCOPE

/* @} */

#endif /* ALGO_BLAST_API___BLAST_SEQSRC_ADAPTER_PRIV__HPP */

