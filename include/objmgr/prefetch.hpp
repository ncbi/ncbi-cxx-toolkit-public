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
#include <objmgr/seq_id_handle.hpp>
#include <objects/seqloc/Seq_id.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class CPrefetchToken
{
public:
    typedef CPrefetchToken_Impl::TIds TIds;

    CPrefetchToken(void);
    // Find the first loader in the scope, request prefetching from
    // this loader. Scope may be destroyed after creating token, but
    // the scope used in NextBioseqHandle() should contain the same
    // loader. Depth limits number of TSEs to be prefetched.
    CPrefetchToken(CScope& scope, const CSeq_id& id);
    CPrefetchToken(CScope& scope, const CSeq_id_Handle& id);
    CPrefetchToken(CScope& scope, const TIds& ids, unsigned int depth = 2);
    ~CPrefetchToken(void);

    CPrefetchToken(const CPrefetchToken& token);
    CPrefetchToken& operator =(const CPrefetchToken& token);

    operator bool(void) const;
    // Get bioseq handle and move to the next requested id
    CBioseq_Handle NextBioseqHandle(CScope& scope);

private:
    CRef<CPrefetchToken_Impl> m_Impl;
};


inline
CPrefetchToken::CPrefetchToken(void)
{
    return;
}


inline
CPrefetchToken::~CPrefetchToken(void)
{
    if (m_Impl) {
        m_Impl->RemoveTokenReference();
    }
    return;
}


inline
CPrefetchToken::CPrefetchToken(CScope& scope, const CSeq_id& id)
    : m_Impl(new CPrefetchToken_Impl(id))
{
    m_Impl->AddTokenReference();
    m_Impl->x_InitPrefetch(scope);
    return;
}


inline
CPrefetchToken::CPrefetchToken(CScope& scope, const CSeq_id_Handle& id)
    : m_Impl(new CPrefetchToken_Impl(id))
{
    m_Impl->AddTokenReference();
    m_Impl->x_InitPrefetch(scope);
    return;
}


inline
CPrefetchToken::CPrefetchToken(CScope& scope,
                               const TIds& ids,
                               unsigned int depth)
    : m_Impl(new CPrefetchToken_Impl(ids, depth))
{
    m_Impl->AddTokenReference();
    m_Impl->x_InitPrefetch(scope);
    return;
}


inline
CPrefetchToken::CPrefetchToken(const CPrefetchToken& token)
{
    *this = token;
}


inline
CPrefetchToken& CPrefetchToken::operator =(const CPrefetchToken& token)
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
CBioseq_Handle CPrefetchToken::NextBioseqHandle(CScope& scope)
{
    _ASSERT(bool(*this));
    return m_Impl->NextBioseqHandle(scope);
}


inline
CPrefetchToken::operator bool(void) const
{
    return bool(m_Impl)  &&  bool(*m_Impl);
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.3  2004/04/21 15:34:28  gorelenk
* Removed export prefix from inline class CPrefetchToken.
*
* Revision 1.2  2004/04/19 14:52:29  grichenk
* Added prefetch depth limit, redesigned prefetch queue.
*
* Revision 1.1  2004/04/16 13:30:34  grichenk
* Initial revision
*
*
* ===========================================================================
*/

#endif  // PREFETCH__HPP
