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
 * Author:  Christiam Camacho
 *
 */

/** @file objmgr_query_data.hpp
 * NOTE: This file contains work in progress and the APIs are likely to change,
 * please do not rely on them until this notice is removed.
 */

#ifndef ALGO_BLAST_API__OBJMGR_QUERY_DATA__HPP
#define ALGO_BLAST_API__OBJMGR_QUERY_DATA__HPP

#include <algo/blast/api/query_data.hpp>
#include <algo/blast/api/sseqloc.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/// NCBI C++ Object Manager dependant implementation of IQueryFactory
class NCBI_XBLAST_EXPORT CObjMgr_QueryFactory : public IQueryFactory
{
public:
    CObjMgr_QueryFactory(const TSeqLocVector& queries);
    CObjMgr_QueryFactory(CRef<objects::CSeq_loc> seqloc);
    
    /// FIXME: perhaps this can be changed to CSeq_intervals
    typedef list< CRef<objects::CSeq_loc> > TSeqLocs;
    CObjMgr_QueryFactory(const TSeqLocs& seqlocs);
    ~CObjMgr_QueryFactory();

protected:
    CRef<ILocalQueryData> x_MakeLocalQueryData(const CBlastOptions* opts);
    CRef<IRemoteQueryData> x_MakeRemoteQueryData();

private:
    const TSeqLocVector* m_SSeqLocVector;
    const TSeqLocs* m_SeqLocs;
    const bool m_OwnSeqLocs;
};

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

#endif  /* ALGO_BLAST_API__OBJMGR_QUERY_DATA_HPP */
