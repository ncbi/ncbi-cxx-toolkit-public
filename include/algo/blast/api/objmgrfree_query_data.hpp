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
 * Author:  Christiam Camacho, Kevin Bealer
 *
 */

/** @file objmgrfree_query_data.hpp
 * NOTE: This file contains work in progress and the APIs are likely to change,
 * please do not rely on them until this notice is removed.
 */

#ifndef ALGO_BLAST_API__OBJMGRFREE_QUERY_DATA__HPP
#define ALGO_BLAST_API__OBJMGRFREE_QUERY_DATA__HPP

#include <algo/blast/api/query_data.hpp>
#include <objects/seq/Bioseq.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/// NCBI C++ Object Manager free implementation of IQueryFactory
/// @deprecated Please use CObjMgr_QueryFactory instead
NCBI_DEPRECATED_CLASS NCBI_XBLAST_EXPORT CObjMgrFree_QueryFactory : 
    public IQueryFactory
{
public:
    /// Parametrized constructor taking a single Bioseq
    /// @param bioseq Bioseq from which to obtain sequence data [in]
    CObjMgrFree_QueryFactory(CConstRef<objects::CBioseq> bioseq);

    /// Parametrized constructor taking a Bioseq-set
    /// @param bioseq_set Bioseq-set from which to obtain sequence data [in]
    CObjMgrFree_QueryFactory(CConstRef<objects::CBioseq_set> bioseq_set);

protected:
    CRef<ILocalQueryData> x_MakeLocalQueryData(const CBlastOptions* opts);
    CRef<IRemoteQueryData> x_MakeRemoteQueryData();

private:
    CConstRef<objects::CBioseq_set>         m_Bioseqs;
};

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

#endif  /* ALGO_BLAST_API__OBJMGRFREE_QUERY_DATA_HPP */
