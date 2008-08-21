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
    /// ctor that takes a vector of SSeqLoc
    /// @param queries vector of SSeqLoc [in]
    CObjMgr_QueryFactory(TSeqLocVector& queries);
    /// ctor that takes a CBlastQueryVector (preferred)
    /// @param queries for search [in]
    CObjMgr_QueryFactory(CBlastQueryVector& queries);

    /// Retrieve the CScope objects associated with the query sequences
    /// associated with this object. In the case when CSeq_loc or TSeqLocs are
    /// provided, a newly constructed CScope object will be returned per query
    /// @note This method is intended to be used for query splitting only
    vector< CRef<objects::CScope> > ExtractScopes();

    /// Retrieve any user specified masking locations
    /// @note This method is intended to be used for query splitting only
    TSeqLocInfoVector ExtractUserSpecifiedMasks();

    /// Retrieves the TSeqLocVector used to construct this object or a
    /// conversion of the CBlastQueryVector provided
    TSeqLocVector GetTSeqLocVector();

protected:
    CRef<ILocalQueryData> x_MakeLocalQueryData(const CBlastOptions* opts);
    CRef<IRemoteQueryData> x_MakeRemoteQueryData();

private:
    TSeqLocVector m_SSeqLocVector;
    CRef<CBlastQueryVector> m_QueryVector;
};

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

#endif  /* ALGO_BLAST_API__OBJMGR_QUERY_DATA_HPP */
