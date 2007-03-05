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
* Author: Maxim Didenko
*
* File Description:
*
*/


#include <ncbi_pch.hpp>

#include <objmgr/impl/annot_finder.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/annot_object.hpp>
#include <objmgr/impl/handle_range_map.hpp>
#include <objmgr/impl/annot_type_index.hpp>
#include <objmgr/impl/tse_split_info.hpp>
#include <objmgr/impl/tse_chunk_info.hpp>
#include <objmgr/impl/seq_annot_info.hpp>

#include <objmgr/annot_name.hpp>

#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqres/Seq_graph.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/seq/Annotdesc.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class IFindContext 
{
public:
    IFindContext() : m_FoundObj(NULL) {}
    virtual ~IFindContext() {};
    
    virtual void CollectRangeMaps( vector<CHandleRangeMap>& ) const = 0;
    virtual CAnnotType_Index::TIndexRange GetIndexRange() const = 0;
    virtual bool CheckAnnotObject(const CAnnotObject_Info& ) = 0;    

    const CAnnotObject_Info* GetFoundObj() const { return m_FoundObj; }

protected:
    const CAnnotObject_Info* m_FoundObj;
    
};

///////////////////////////////////////////////////////////////////////////////
class CFeatFindContext : public IFindContext 
{
public:
    explicit CFeatFindContext(const CSeq_feat& feat) : m_Feat(feat) {}
    virtual ~CFeatFindContext() {}

    virtual void CollectRangeMaps( vector<CHandleRangeMap>& hrmaps) const
    {
        CAnnotObject_Info::x_ProcessFeat(hrmaps, m_Feat);        
    }
    virtual CAnnotType_Index::TIndexRange GetIndexRange() const
    {
        CAnnotType_Index::TIndexRange ret;
        ret.first = CAnnotType_Index::
            GetSubtypeIndex(m_Feat.GetData().GetSubtype());
        ret.second = ret.first+1;
        return ret;
    }
    virtual bool CheckAnnotObject(const CAnnotObject_Info& info)
    {
        if ( !info.IsFeat() ) return false;
        if ( !info.GetFeat().Equals(m_Feat) ) return false;
        m_FoundObj = &info;
        return true;
    }
private:
    const CSeq_feat& m_Feat;
};
///////////////////////////////////////////////////////////////////////////////
class CAlignFindContext : public IFindContext 
{
public:
    explicit CAlignFindContext(const CSeq_align& align) : m_Align(align) {}
    virtual ~CAlignFindContext() {}

    virtual void CollectRangeMaps( vector<CHandleRangeMap>& hrmaps) const
    {
        CAnnotObject_Info::x_ProcessAlign(hrmaps, m_Align);        
    }
    virtual CAnnotType_Index::TIndexRange GetIndexRange() const
    {
        return CAnnotType_Index::
            GetAnnotTypeRange(CSeq_annot::TData::e_Align);
    }
    virtual bool CheckAnnotObject(const CAnnotObject_Info& info)
    {
        if ( !info.IsAlign() ) return false;
        if ( !info.GetAlign().Equals(m_Align) ) return false;
        m_FoundObj = &info;
        return true;
    }
private:
    const CSeq_align& m_Align;
};

///////////////////////////////////////////////////////////////////////////////
class CGraphFindContext : public IFindContext 
{
public:
    explicit CGraphFindContext(const CSeq_graph& graph) : m_Graph(graph) {}
    virtual ~CGraphFindContext() {}

    virtual void CollectRangeMaps( vector<CHandleRangeMap>& hrmaps) const
    {
        CAnnotObject_Info::x_ProcessGraph(hrmaps, m_Graph);        
    }
    virtual CAnnotType_Index::TIndexRange GetIndexRange() const
    {
        return CAnnotType_Index::
            GetAnnotTypeRange(CSeq_annot::TData::e_Graph);
    }
    virtual bool CheckAnnotObject(const CAnnotObject_Info& info)
    {
        if ( !info.IsGraph() ) return false;
        if ( !info.GetGraph().Equals(m_Graph) ) return false;
        m_FoundObj = &info;
        return true;
    }
private:
    const CSeq_graph& m_Graph;
};

///////////////////////////////////////////////////////////////////////////////


const CAnnotObject_Info* 
CSeq_annot_Finder::Find(const CSeq_entry_Info& entry,
                        const CAnnotName& name, 
                        const CSeq_feat& feat)
{
    CFeatFindContext context(feat);
    x_Find(entry, name, context);
    return context.GetFoundObj();
}

const CAnnotObject_Info* 
CSeq_annot_Finder::Find(const CSeq_entry_Info& entry,
                        const CAnnotName& name, 
                        const CSeq_align& align)
{
    CAlignFindContext context(align);
    x_Find(entry, name, context);
    return context.GetFoundObj();
}
const CAnnotObject_Info* 
CSeq_annot_Finder::Find(const CSeq_entry_Info& entry,
                        const CAnnotName& name, 
                        const CSeq_graph& graph)
{
    CGraphFindContext context(graph);
    x_Find(entry, name, context);
    return context.GetFoundObj();
}


const CSeq_annot_Info* 
CSeq_annot_Finder::Find(const CSeq_entry_Info& entry,
                        const CAnnotName& name,
                        const CAnnot_descr& descr)
{
    ITERATE(CSeq_entry_Info::TAnnot, annot_it, entry.GetLoadedAnnot()) {
        const CSeq_annot_Info& annot = **annot_it;
        if (annot.GetName() == name) {
            CConstRef<CSeq_annot> rannot = annot.GetCompleteSeq_annot();
            if (rannot->IsSetDesc() && rannot->GetDesc().Equals(descr))
                return &annot;
        }
    }
    return NULL;
}
const CSeq_annot_Info* 
CSeq_annot_Finder::Find(const CSeq_entry_Info& entry,
                        const CAnnotName& name)
{
    ITERATE(CSeq_entry_Info::TAnnot, annot_it, entry.GetLoadedAnnot()) {
        const CSeq_annot_Info& annot = **annot_it;
        if (annot.GetName() == name) {
            CConstRef<CSeq_annot> rannot = annot.GetCompleteSeq_annot();
            if (!rannot->IsSetDesc() || !rannot->GetDesc().IsSet() ||
                rannot->GetDesc().Get().empty())
                return &annot;
        }
    }
    return NULL;
}


///////////////////////////////////////////////////////////////////////////////

void CSeq_annot_Finder::x_Find(const CSeq_entry_Info& entry,
                               const CAnnotName& name, 
                               IFindContext& context)
{
    vector<CHandleRangeMap> hrmaps;
    context.CollectRangeMaps(hrmaps);
    if (hrmaps.empty()) {
        _ASSERT(0);
        return;
    }
    CHandleRangeMap& r0 = *hrmaps.begin();
    if (r0.empty()) {
        _ASSERT(0);
        return;
    }
    const CSeq_id_Handle& idh = r0.begin()->first;
    CHandleRange::TRange overlap_range = r0.begin()->second.GetOverlappingRange();
    
    m_TSE.UpdateAnnotIndex(idh);
    CTSE_Info::TAnnotLockReadGuard guard(m_TSE.GetAnnotLock());

    const SIdAnnotObjs* objs = NULL;
    if (name.IsNamed()) 
        objs = m_TSE.x_GetIdObjects(name,idh);
    else 
        objs = m_TSE.x_GetUnnamedIdObjects(idh);
    
    if (!objs)
        return;
    CAnnotType_Index::TIndexRange range = context.GetIndexRange();

    for (size_t index = range.first; index < range.second; ++index) {
        if (objs->x_RangeMapIsEmpty(index))
            continue;

        const CTSE_Info::TRangeMap& rmap = objs->x_GetRangeMap(index);

        bool run_again;
        do {
            run_again = false;
            CTSE_Info::TRangeMap::const_iterator it = rmap.find(overlap_range);
            while (it && it.GetInterval() == overlap_range) {
                const CAnnotObject_Info& annot_info = *it->second.m_AnnotObject_Info;
                ++it;
                if ( annot_info.IsChunkStub() ) {
                    const CTSE_Chunk_Info& chunk = annot_info.GetChunk_Info();
                    if ( chunk.NotLoaded() ) {
                        guard.Release();
                        chunk.Load();
                        guard.Guard(m_TSE.GetAnnotLock());
                        run_again = true;
                    }
                    continue;
                }
                if ( &entry == &annot_info.GetSeq_entry_Info() && 
                     context.CheckAnnotObject(annot_info) ) {
                    return;
                }
            }            
        } while ( run_again );
    }        
}


END_SCOPE(objects)
END_NCBI_SCOPE
