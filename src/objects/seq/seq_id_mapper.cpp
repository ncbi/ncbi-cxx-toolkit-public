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
*   Seq-id mapper for Object Manager
*
*/

#include <ncbi_pch.hpp>
#include <objects/seq/seq_id_mapper.hpp>
#include <corelib/ncbimtx.hpp>
#include "seq_id_tree.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

////////////////////////////////////////////////////////////////////
//
//  CSeq_id_Mapper::
//


typedef CSeq_id_Mapper TInstance;

// slow implementation with mutex
static TInstance* s_Instance = 0;
DEFINE_STATIC_FAST_MUTEX(s_InstanceMutex);

CRef<TInstance> TInstance::GetInstance(void)
{
    CRef<TInstance> ret;
    {{
        CFastMutexGuard guard(s_InstanceMutex);
        ret.Reset(s_Instance);
        if ( !ret || ret->ReferencedOnlyOnce() ) {
            if ( ret ) {
                ret.Release();
            }
            ret.Reset(new TInstance);
            s_Instance = ret;
        }
    }}
    _ASSERT(ret == s_Instance);
    return ret;
}


static void s_ResetInstance(TInstance* instance)
{
    CFastMutexGuard guard(s_InstanceMutex);
    if ( s_Instance == instance ) {
        s_Instance = 0;
    }
}


CSeq_id_Mapper::CSeq_id_Mapper(void)
{
    CSeq_id_Which_Tree::Initialize(this, m_Trees);
}


CSeq_id_Mapper::~CSeq_id_Mapper(void)
{
    s_ResetInstance(this);
    ITERATE ( TTrees, it, m_Trees ) {
        _ASSERT((*it)->Empty());
    }
}


inline
CSeq_id_Which_Tree& CSeq_id_Mapper::x_GetTree(const CSeq_id_Handle& idh)
{
    CSeq_id::E_Choice type;
    if ( !idh ) {
        type = CSeq_id::e_not_set;
    }
    else if ( idh.IsGi() ) {
        type = CSeq_id::e_Gi;
    }
    else {
        return idh.m_Info->GetTree();
    }
    _ASSERT(size_t(type) < m_Trees.size());
    return *m_Trees[type];
}


CSeq_id_Handle CSeq_id_Mapper::GetGiHandle(TGi gi)
{
    _ASSERT(size_t(CSeq_id::e_Gi) < m_Trees.size());
    return m_Trees[CSeq_id::e_Gi]->GetGiHandle(gi);
}


CSeq_id_Handle CSeq_id_Mapper::GetHandle(const CSeq_id& id, bool do_not_create)
{
    CSeq_id_Which_Tree& tree = x_GetTree(id);
    return do_not_create? tree.FindInfo(id): tree.FindOrCreate(id);
}


bool CSeq_id_Mapper::HaveMatchingHandles(const CSeq_id_Handle& idh)
{
    return x_GetTree(idh).HaveMatch(idh);
}


void CSeq_id_Mapper::GetMatchingHandles(const CSeq_id_Handle& idh,
                                        TSeq_id_HandleSet& h_set)
{
    x_GetTree(idh).FindMatch(idh, h_set);
}


bool CSeq_id_Mapper::HaveReverseMatch(const CSeq_id_Handle& idh)
{
    return x_GetTree(idh).HaveReverseMatch(idh);
}


void CSeq_id_Mapper::GetReverseMatchingHandles(const CSeq_id_Handle& idh,
                                               TSeq_id_HandleSet& h_set)
{
    x_GetTree(idh).FindReverseMatch(idh, h_set);
}


bool CSeq_id_Mapper::HaveMatchingHandles(const CSeq_id_Handle& idh,
                                         EAllowWeakMatch allow_weak_match)
{
    if ( HaveMatchingHandles(idh) ) {
        return true;
    }
    if ( allow_weak_match == eNoWeakMatch ) {
        return false;
    }
    const CSeq_id_Which_Tree* base_tree = &x_GetTree(idh);
    if ( !dynamic_cast<const CSeq_id_Textseq_Tree*>(base_tree) ) {
        return false;
    }
    for ( size_t i = 0; i < m_Trees.size(); ++i ) {
        const CSeq_id_Which_Tree* tree = m_Trees[i];
        if ( tree == base_tree ) {
            continue;
        }
        if ( !dynamic_cast<const CSeq_id_Textseq_Tree*>(tree) ) {
            continue;
        }
        if ( tree == m_Trees[CSeq_id::e_Gi] && i != CSeq_id::e_Gi ) {
            continue;
        }
        if ( tree->HaveMatch(idh) ) {
            return true;
        }
    }
    return false;
}


void CSeq_id_Mapper::GetMatchingHandles(const CSeq_id_Handle& idh,
                                        TSeq_id_HandleSet& h_set,
                                        EAllowWeakMatch allow_weak_match)
{
    GetMatchingHandles(idh, h_set);
    if ( allow_weak_match == eNoWeakMatch ) {
        return;
    }
    const CSeq_id_Which_Tree* base_tree = &x_GetTree(idh);
    if ( !dynamic_cast<const CSeq_id_Textseq_Tree*>(base_tree) ) {
        return;
    }
    for ( size_t i = 0; i < m_Trees.size(); ++i ) {
        const CSeq_id_Which_Tree* tree = m_Trees[i];
        if ( tree == base_tree ) {
            continue;
        }
        if ( !dynamic_cast<const CSeq_id_Textseq_Tree*>(tree) ) {
            continue;
        }
        if ( tree == m_Trees[CSeq_id::e_Gi] && i != CSeq_id::e_Gi ) {
            continue;
        }
        tree->FindMatch(idh, h_set);
    }
}


bool CSeq_id_Mapper::HaveReverseMatch(const CSeq_id_Handle& idh,
                                      EAllowWeakMatch allow_weak_match)
{
    if ( HaveReverseMatch(idh) ) {
        return true;
    }
    if ( allow_weak_match == eNoWeakMatch ) {
        return false;
    }
    const CSeq_id_Which_Tree* base_tree = &x_GetTree(idh);
    if ( !dynamic_cast<const CSeq_id_Textseq_Tree*>(base_tree) ) {
        return false;
    }
    for ( size_t i = 0; i < m_Trees.size(); ++i ) {
        const CSeq_id_Which_Tree* tree = m_Trees[i];
        if ( tree == base_tree ) {
            continue;
        }
        if ( !dynamic_cast<const CSeq_id_Textseq_Tree*>(tree) ) {
            continue;
        }
        if ( tree == m_Trees[CSeq_id::e_Gi] && i != CSeq_id::e_Gi ) {
            continue;
        }
        if ( tree->HaveReverseMatch(idh) ) {
            return true;
        }
    }
    return false;
}


void CSeq_id_Mapper::GetReverseMatchingHandles(const CSeq_id_Handle& idh,
                                               TSeq_id_HandleSet& h_set,
                                               EAllowWeakMatch allow_weak_match)
{
    GetReverseMatchingHandles(idh, h_set);
    if ( allow_weak_match == eNoWeakMatch ) {
        return;
    }
    const CSeq_id_Which_Tree* base_tree = &x_GetTree(idh);
    if ( !dynamic_cast<const CSeq_id_Textseq_Tree*>(base_tree) ) {
        return;
    }
    for ( size_t i = 0; i < m_Trees.size(); ++i ) {
        CSeq_id_Which_Tree* tree = m_Trees[i];
        if ( tree == base_tree ) {
            continue;
        }
        if ( !dynamic_cast<const CSeq_id_Textseq_Tree*>(tree) ) {
            continue;
        }
        if ( tree == m_Trees[CSeq_id::e_Gi] && i != CSeq_id::e_Gi ) {
            continue;
        }
        tree->FindReverseMatch(idh, h_set);
    }
}


void CSeq_id_Mapper::GetMatchingHandlesStr(string sid,
                                           TSeq_id_HandleSet& h_set)
{
    if (sid.find('|') != string::npos) {
        NCBI_THROW(CSeq_id_MapperException, eSymbolError,
                   "Symbol \'|\' is not supported here");
    }

    ITERATE(TTrees, tree_it, m_Trees) {
        (*tree_it)->FindMatchStr(sid, h_set);
    }
}


bool CSeq_id_Mapper::x_Match(const CSeq_id_Handle& h1,
                                const CSeq_id_Handle& h2)
{
    CSeq_id_Which_Tree& tree1 = x_GetTree(h1);
    CSeq_id_Which_Tree& tree2 = x_GetTree(h2);
    if ( &tree1 != &tree2 )
        return false;

    // Compare versions if any
    return tree1.Match(h1, h2);
}


bool CSeq_id_Mapper::x_IsBetter(const CSeq_id_Handle& h1,
                                const CSeq_id_Handle& h2)
{
    CSeq_id_Which_Tree& tree1 = x_GetTree(h1);
    CSeq_id_Which_Tree& tree2 = x_GetTree(h2);
    if ( &tree1 != &tree2 )
        return false;

    // Compare versions if any
    return tree1.IsBetterVersion(h1, h2);
}


size_t CSeq_id_Mapper::Dump(CNcbiOstream& out, EDumpDetails details) const
{
    size_t total_bytes = 0;
    for ( size_t i = 0; i < m_Trees.size(); ++i ) {
        size_t bytes = m_Trees[i]->Dump(out, CSeq_id::E_Choice(i), details);
        total_bytes += bytes;
    }
    if ( details >= eDumpTotalBytes ) {
        out << "Total CSeq_id_Mapper bytes: "<<total_bytes<<endl;
    }
    return total_bytes;
}


END_SCOPE(objects)
END_NCBI_SCOPE
