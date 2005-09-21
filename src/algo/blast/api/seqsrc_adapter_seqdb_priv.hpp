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

/** @file seqsrc_adapter_seqdb_priv.hpp
 * NOTE: This file contains work in progress and the APIs are likely to change,
 * please do not rely on them until this notice is removed.
 */

#ifndef ALGO_BLAST_API___SEQSRC_ADAPTER_SEQDB_PRIV_HPP
#define ALGO_BLAST_API___SEQSRC_ADAPTER_SEQDB_PRIV_HPP

#include <objtools/readers/seqdb/seqdb.hpp>
#include "blast_seqsrc_adapter_priv.hpp"

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

class NCBI_XBLAST_EXPORT CSeqDbBSSAdapter : public IBlastSeqSrcAdapter
{
public:
    CSeqDbBSSAdapter(const CSearchDatabase& dbinfo);
protected:
    BlastSeqSrc* x_CalculateBlastSeqSrc(const CSearchDatabase& dbinfo);
private:
    CRef<CSeqDB> m_DbHandle;
};

END_SCOPE(BLAST)
END_NCBI_SCOPE

/* @} */

#endif /* ALGO_BLAST_API___SEQSRC_ADAPTER_SEQDB_PRIV__HPP */

