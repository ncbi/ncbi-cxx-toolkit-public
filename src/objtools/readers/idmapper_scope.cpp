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
 * Author:  Aleksey Grichenko
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/seq_map_ci.hpp>
#include <objtools/readers/idmapper.hpp>


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

 
CIdMapperScope::CIdMapperScope(CScope&               scope,
                               const CSeq_id_Handle& focus_idh)
    : m_Scope(&scope)
{
    x_Init(focus_idh);
}


CIdMapperScope::CIdMapperScope(CScope&        scope,
                               const CSeq_id& focus_id)
    : m_Scope(&scope)
{
    x_Init(CSeq_id_Handle::GetHandle(focus_id));
}


void CIdMapperScope::x_Init(const CSeq_id_Handle& focus_idh)
{
    CBioseq_Handle bh = m_Scope->GetBioseqHandle(focus_idh);
    if ( bh ) {
        x_AddMappings(bh);
        SSeqMapSelector sel(CSeqMap::fFindRef, size_t(-1));
        CSeqMap_CI it(bh, sel);
        for (; it; ++it) {
            _ASSERT(it.GetType() == CSeqMap::eSeqRef);
            CBioseq_Handle itbh = m_Scope->GetBioseqHandle(it.GetRefSeqid());
            x_AddMappings(itbh);
        }
    }
}


void CIdMapperScope::x_AddMappings(const CBioseq_Handle& bh)
{
    if ( !bh ) return;
    CBioseq_Handle::TId ids = bh.GetId();
    CSeq_id::TSeqIdHandles matches;
    ITERATE(CBioseq_Handle::TId, it, ids) {
        //AddMapping(*it, *it);
        matches.clear();
        it->GetSeqId()->GetMatchingIds(matches);
        ITERATE(CSeq_id::TSeqIdHandles, mit, matches) {
            AddMapping(*mit, *it);
        }
    }
}


END_NCBI_SCOPE
