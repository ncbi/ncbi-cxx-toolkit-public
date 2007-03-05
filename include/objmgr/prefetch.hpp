#ifndef PREFETCH__HPP
#define PREFETCH__HPP

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
* Author: Aleksey Grichenko, Eugene Vasilchenko
*
* File Description:
*   Prefetch token
*
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <objmgr/impl/prefetch_impl.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/scope.hpp>
#include <objects/seq/seq_id_handle.hpp>
#include <objects/seqloc/Seq_id.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


/** @addtogroup ObjectManagerCore
 *
 * @{
 */


/////////////////////////////////////////////////////////////////////////////
///
///  CPrefetchToken --
///
///  Data prefetching token


class CPrefetchTokenOld
{
public:
    typedef CPrefetchTokenOld_Impl::TIds TIds;

    enum ENon_locking_prefetch {
        eNon_locking_prefetch
    };

    CPrefetchTokenOld(void);

    /// Find the first loader in the scope, request prefetching from
    /// this loader. Scope may be destroyed after creating token, but
    /// the scope used in NextBioseqHandle() should contain the same
    /// loader.
    ///
    /// @param scope
    ///  Scope used to access data loader and initialize prefetching
    /// @param ids
    ///  Set of seq-ids to prefetch
    /// @param depth
    ///  Number of TSEs allowed to be prefetched.
    CPrefetchTokenOld(CScope& scope, const TIds& ids, unsigned int depth = 2);

    /// Do not lock prefetched TSEs, prefetch depth is ignored.
    CPrefetchTokenOld(CScope& scope, const TIds& ids, ENon_locking_prefetch);
    ~CPrefetchTokenOld(void);

    CPrefetchTokenOld(const CPrefetchTokenOld& token);
    CPrefetchTokenOld& operator =(const CPrefetchTokenOld& token);

    DECLARE_OPERATOR_BOOL(m_Impl  &&  *m_Impl);

    /// Get bioseq handle and move to the next requested id
    /// Scope must contain the loader used for prefetching.
    /// @sa CPrefetchTokenOld
    CBioseq_Handle NextBioseqHandle(CScope& scope);

private:
    CRef<CPrefetchTokenOld_Impl> m_Impl;
};


inline
CPrefetchTokenOld::CPrefetchTokenOld(void)
{
    return;
}


inline
CPrefetchTokenOld::~CPrefetchTokenOld(void)
{
    if (m_Impl) {
        m_Impl->RemoveTokenReference();
    }
    return;
}


inline
CPrefetchTokenOld::CPrefetchTokenOld(CScope& scope,
                               const TIds& ids,
                               unsigned int depth)
    : m_Impl(new CPrefetchTokenOld_Impl(ids, depth))
{
    m_Impl->AddTokenReference();
    m_Impl->x_InitPrefetch(scope);
    return;
}


inline
CPrefetchTokenOld::CPrefetchTokenOld(CScope& scope,
                               const TIds& ids,
                               ENon_locking_prefetch)
    : m_Impl(new CPrefetchTokenOld_Impl(ids, 2))
{
    m_Impl->AddTokenReference();
    m_Impl->x_SetNon_locking();
    m_Impl->x_InitPrefetch(scope);
    return;
}


inline
CPrefetchTokenOld::CPrefetchTokenOld(const CPrefetchTokenOld& token)
{
    *this = token;
}


inline
CPrefetchTokenOld& CPrefetchTokenOld::operator =(const CPrefetchTokenOld& token)
{
    if (this != &token) {
        if (m_Impl) {
            m_Impl->RemoveTokenReference();
        }
        m_Impl = token.m_Impl;
        if (m_Impl) {
            m_Impl->AddTokenReference();
        }
    }
    return *this;
}


inline
CBioseq_Handle CPrefetchTokenOld::NextBioseqHandle(CScope& scope)
{
    _ASSERT(*this);
    return m_Impl->NextBioseqHandle(scope);
}


/* @} */


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // PREFETCH__HPP
