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
* Author:  Clifford Clausen, Aaron Ucko, Aleksey Grichenko
*
* File Description:
*   Seq-loc utilities requiring CScope
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbi_limits.hpp>
#include <objmgr/util/seq_loc_util.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/impl/synonyms.hpp>
#include <objmgr/scope.hpp>
#include <objects/seq/seq_id_mapper.hpp>
#include <objects/seq/seq_id_handle.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Packed_seqint.hpp>
#include <objects/seqloc/Seq_loc_mix.hpp>
#include <serial/iterator.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(sequence)


class CDefaultSynonymMapper : public ISynonymMapper
{
public:
    CDefaultSynonymMapper(CScope* scope);
    virtual ~CDefaultSynonymMapper(void);

    virtual CSeq_id_Handle GetBestSynonym(const CSeq_id& id);

private:
    typedef map<CSeq_id_Handle, CSeq_id_Handle> TSynonymMap;

    CRef<CSeq_id_Mapper> m_IdMapper;
    TSynonymMap          m_SynMap;
    CScope*              m_Scope;
};


CDefaultSynonymMapper::CDefaultSynonymMapper(CScope* scope)
    : m_IdMapper(CSeq_id_Mapper::GetInstance()),
      m_Scope(scope)
{
    return;
}


CDefaultSynonymMapper::~CDefaultSynonymMapper(void)
{
    return;
}


CSeq_id_Handle CDefaultSynonymMapper::GetBestSynonym(const CSeq_id& id)
{
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(id);
    if ( !m_Scope  ||  id.Which() == CSeq_id::e_not_set ) {
        return idh;
    }
    TSynonymMap::iterator id_syn = m_SynMap.find(idh);
    if (id_syn != m_SynMap.end()) {
        return id_syn->second;
    }
    CSeq_id_Handle best;
    int best_rank = kMax_Int;
    CConstRef<CSynonymsSet> syn_set = m_Scope->GetSynonyms(idh);
    ITERATE(CSynonymsSet, syn_it, *syn_set) {
        CSeq_id_Handle synh = syn_set->GetSeq_id_Handle(syn_it);
        int rank = synh.GetSeqId()->BestRankScore();
        if (rank < best_rank) {
            best = synh;
            best_rank = rank;
        }
    }
    if ( !best ) {
        // Synonyms set was empty
        m_SynMap[idh] = idh;
        return idh;
    }
    ITERATE(CSynonymsSet, syn_it, *syn_set) {
        m_SynMap[syn_set->GetSeq_id_Handle(syn_it)] = best;
    }
    return best;
}


class CDefaultLengthGetter : public ILengthGetter
{
public:
    CDefaultLengthGetter(CScope* scope);
    virtual ~CDefaultLengthGetter(void);

    virtual TSeqPos GetLength(const CSeq_id& id);

protected:
    CScope*              m_Scope;
};


CDefaultLengthGetter::CDefaultLengthGetter(CScope* scope)
    : m_Scope(scope)
{
    return;
}


CDefaultLengthGetter::~CDefaultLengthGetter(void)
{
    return;
}


TSeqPos CDefaultLengthGetter::GetLength(const CSeq_id& id)
{
    if (id.Which() == CSeq_id::e_not_set) {
        return 0;
    }
    CBioseq_Handle bh;
    if ( m_Scope ) {
        bh = m_Scope->GetBioseqHandle(id);
    }
    if ( !bh ) {
        NCBI_THROW(CException, eUnknown,
            "Can not get length of whole location");
    }
    return bh.GetBioseqLength();
}


CRef<CSeq_loc> Seq_loc_Merge(const CSeq_loc&    loc,
                             CSeq_loc::TOpFlags flags,
                             CScope*            scope)
{
    CDefaultSynonymMapper syn_mapper(scope);
    return loc.Merge(flags, &syn_mapper);
}


CRef<CSeq_loc> Seq_loc_Add(const CSeq_loc&    loc1,
                           const CSeq_loc&    loc2,
                           CSeq_loc::TOpFlags flags,
                           CScope*            scope)
{
    CDefaultSynonymMapper syn_mapper(scope);
    return loc1.Add(loc2, flags, &syn_mapper);
}


CRef<CSeq_loc> Seq_loc_Subtract(const CSeq_loc&    loc1,
                                const CSeq_loc&    loc2,
                                CSeq_loc::TOpFlags flags,
                                CScope*            scope)
{
    CDefaultSynonymMapper syn_mapper(scope);
    CDefaultLengthGetter len_getter(scope);
    return loc1.Subtract(loc2, flags, &syn_mapper, &len_getter);
}


END_SCOPE(sequence)
END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
* $Log$
* Revision 1.2  2004/11/15 15:07:57  grichenk
* Moved seq-loc operations to CSeq_loc, modified flags.
*
* Revision 1.1  2004/10/20 18:09:43  grichenk
* Initial revision
*
*
* ===========================================================================
*/
