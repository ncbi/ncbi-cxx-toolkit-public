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
* Author: Eugene Vasilchenko
*
* File Description:
*   CSeq_annot_Info info -- entry for data source information about Seq-annot
*
*/


#include <corelib/ncbimtx.hpp>

#include <objmgr/impl/seq_annot_info.hpp>
#include <objmgr/impl/seq_entry_info.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/tse_chunk_info.hpp>
#include <objmgr/impl/annot_object.hpp>
#include <objmgr/impl/handle_range_map.hpp>
#include <objmgr/impl/data_source.hpp>
#include <objmgr/impl/snp_annot_info.hpp>

#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/seq/Annot_descr.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CSeq_annot_Info::CSeq_annot_Info(CSeq_annot& annot, CSeq_entry_Info& entry)
    : m_Seq_annot(&annot),
      m_Seq_entry_Info(0),
      m_TSE_Info(0),
      m_Chunk_Info(0),
      m_DirtyAnnotIndex(true)
{
    x_Seq_entryAttach(entry);
    x_UpdateName();
}


CSeq_annot_Info::CSeq_annot_Info(CSeq_annot_SNP_Info& snp_annot)
    : m_Seq_annot(&const_cast<CSeq_annot&>(snp_annot.GetSeq_annot())),
      m_Seq_entry_Info(0),
      m_TSE_Info(0),
      m_SNP_Info(&snp_annot),
      m_Chunk_Info(0),
      m_DirtyAnnotIndex(true)
{
    x_UpdateName();
}


CSeq_annot_Info::CSeq_annot_Info(CTSE_Chunk_Info& chunk_info)
    : m_Seq_annot(0),
      m_Seq_entry_Info(0),
      m_TSE_Info(&chunk_info.GetTSE_Info()),
      m_Chunk_Info(&chunk_info),
      m_DirtyAnnotIndex(true)
{
}


CSeq_annot_Info::~CSeq_annot_Info(void)
{
}


size_t CSeq_annot_Info::GetAnnotObjectIndex(const CAnnotObject_Info& info) const
{
    _ASSERT(&info.GetSeq_annot_Info() == this);
    _ASSERT(!m_ObjectInfos.m_Infos.empty());
    _ASSERT(&info >= &m_ObjectInfos.m_Infos.front() &&
            &info <= &m_ObjectInfos.m_Infos.back());
    return &info - &m_ObjectInfos.m_Infos.front();
}


CDataSource& CSeq_annot_Info::GetDataSource(void) const
{
    return GetSeq_entry_Info().GetDataSource();
}


const CSeq_entry& CSeq_annot_Info::GetTSE(void) const
{
    return GetSeq_entry_Info().GetTSE();
}


const CSeq_entry& CSeq_annot_Info::GetSeq_entry(void) const
{
    return GetSeq_entry_Info().GetSeq_entry();
}


const CAnnotName& CSeq_annot_Info::GetName(void) const
{
    return m_Name.IsNamed()? m_Name: GetTSE_Info().GetName();
}


void CSeq_annot_Info::x_UpdateName(void)
{
    if ( GetSeq_annot().IsSetDesc() ) {
        string name;
        ITERATE ( CSeq_annot::TDesc::Tdata, it,
                  GetSeq_annot().GetDesc().Get() ) {
            const CAnnotdesc& desc = **it;
            if ( desc.Which() == CAnnotdesc::e_Name ) {
                name = desc.GetName();
                break;
            }
        }
        m_Name.SetNamed(name);
    }
    else {
        m_Name.SetUnnamed();
    }
}


void CSeq_annot_Info::x_Seq_entryAttach(CSeq_entry_Info& entry)
{
    _ASSERT(!m_Seq_entry_Info);
    _ASSERT(!m_TSE_Info);
    _ASSERT(m_DirtyAnnotIndex);
    m_Seq_entry_Info = &entry;
    m_TSE_Info = &entry.GetTSE_Info();
    entry.m_Annots.push_back(Ref(this));
    entry.x_SetDirtyAnnotIndex();
}


void CSeq_annot_Info::x_DSAttachThis(void)
{
    GetDataSource().x_MapSeq_annot(*this);
}


void CSeq_annot_Info::x_DSDetachThis(void)
{
    GetDataSource().x_UnmapSeq_annot(*this);
}


void CSeq_annot_Info::x_SetSNP_annot_Info(CSeq_annot_SNP_Info& snp_info)
{
    if ( m_SNP_Info ) {
        THROW1_TRACE(runtime_error,
                     "CSeq_annot_Info::SetSNP_annot_Info: already set");
    }
    m_SNP_Info.Reset(&snp_info);
}


void CSeq_annot_Info::UpdateAnnotIndex(void) const
{
    if ( m_DirtyAnnotIndex ) {
        GetTSE_Info().UpdateAnnotIndex(*this);
        _ASSERT(!m_DirtyAnnotIndex);
    }
}


void CSeq_annot_Info::x_UpdateAnnotIndex(void)
{
    if ( m_DirtyAnnotIndex ) {
        x_UpdateAnnotIndexThis();
        m_DirtyAnnotIndex = false;
    }
}


void CSeq_annot_Info::x_UpdateAnnotIndexThis(void)
{
    m_ObjectInfos.m_Name = GetName();

    CSeq_annot::C_Data& data = GetSeq_annot().SetData();
    switch ( data.Which() ) {
    case CSeq_annot::C_Data::e_Ftable:
        x_MapAnnotObjects(data.SetFtable());
        break;
    case CSeq_annot::C_Data::e_Align:
        x_MapAnnotObjects(data.SetAlign());
        break;
    case CSeq_annot::C_Data::e_Graph:
        x_MapAnnotObjects(data.SetGraph());
        break;
    }
    if ( m_SNP_Info ) {
        CSeq_id id;
        id.SetGi(m_SNP_Info->GetGi());
        CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(id);
        GetTSE_Info().x_MapSNP_Table(GetName(), idh, m_SNP_Info);
    }
}


void CSeq_annot_Info::x_TSEDetach(void)
{
    if ( m_SNP_Info ) {
        CSeq_id id;
        id.SetGi(m_SNP_Info->GetGi());
        CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(id);
        GetTSE_Info().x_UnmapSNP_Table(GetName(), idh, m_SNP_Info);
    }
    x_UnmapAnnotObjects();
    m_DirtyAnnotIndex = true;
}


void CSeq_annot_Info::x_MapAnnotObjects(CSeq_annot::C_Data::TFtable& objs)
{
    _ASSERT(m_ObjectInfos.m_Keys.empty());

    size_t objCount = objs.size();

    m_ObjectInfos.m_Keys.reserve(size_t(1.1*objCount));
    m_ObjectInfos.m_Infos.reserve(objCount);

    CTSE_Info& tse_info = GetTSE_Info();
    CTSE_Info::TAnnotObjs& index = tse_info.x_SetAnnotObjs(GetName());

    SAnnotObject_Key key;
    SAnnotObject_Index annotRef;
    vector<CHandleRangeMap> hrmaps;
    NON_CONST_ITERATE ( CSeq_annot::C_Data::TFtable, fit, objs ) {
        CSeq_feat& feat = **fit;

        CAnnotObject_Info* info =
            m_ObjectInfos.AddInfo(CAnnotObject_Info(feat, *this));
        key.m_AnnotObject_Info = annotRef.m_AnnotObject_Info = info;

        info->GetMaps(hrmaps);
        annotRef.m_AnnotLocationIndex = 0;
        ITERATE ( vector<CHandleRangeMap>, hrmit, hrmaps ) {
            ITERATE ( CHandleRangeMap, hrit, *hrmit ) {
                key.m_Handle = hrit->first;
                const CHandleRange& hr = hrit->second;
                key.m_Range = hr.GetOverlappingRange();
                if ( hr.HasGaps() ) {
                    annotRef.m_HandleRange.Reset(new CObjectFor<CHandleRange>);
                    annotRef.m_HandleRange->GetData() = hr;
                }
                else {
                    annotRef.m_HandleRange.Reset();
                }
                
                tse_info.x_MapAnnotObject(index, key, annotRef, m_ObjectInfos);
            }
            ++annotRef.m_AnnotLocationIndex;
        }
    }
}


void CSeq_annot_Info::x_MapAnnotObjects(CSeq_annot::C_Data::TGraph& objs)
{
    _ASSERT(m_ObjectInfos.m_Keys.empty());

    size_t objCount = objs.size();

    m_ObjectInfos.m_Keys.reserve(objCount);
    m_ObjectInfos.m_Infos.reserve(objCount);

    CTSE_Info& tse_info = GetTSE_Info();
    CTSE_Info::TAnnotObjs& index = tse_info.x_SetAnnotObjs(GetName());

    SAnnotObject_Key key;
    SAnnotObject_Index annotRef;
    vector<CHandleRangeMap> hrmaps;
    NON_CONST_ITERATE ( CSeq_annot::C_Data::TGraph, git, objs ) {
        CSeq_graph& graph = **git;

        CAnnotObject_Info* info =
            m_ObjectInfos.AddInfo(CAnnotObject_Info(graph, *this));
        key.m_AnnotObject_Info = annotRef.m_AnnotObject_Info = info;

        info->GetMaps(hrmaps);
        annotRef.m_AnnotLocationIndex = 0;
        ITERATE ( vector<CHandleRangeMap>, hrmit, hrmaps ) {
            ITERATE ( CHandleRangeMap, hrit, *hrmit ) {
                key.m_Handle = hrit->first;
                const CHandleRange& hr = hrit->second;
                key.m_Range = hr.GetOverlappingRange();
                if ( hr.HasGaps() ) {
                    annotRef.m_HandleRange.Reset(new CObjectFor<CHandleRange>);
                    annotRef.m_HandleRange->GetData() = hr;
                }
                else {
                    annotRef.m_HandleRange.Reset();
                }

                tse_info.x_MapAnnotObject(index, key, annotRef, m_ObjectInfos);
            }
            ++annotRef.m_AnnotLocationIndex;
        }
    }
}


void CSeq_annot_Info::x_MapAnnotObjects(CSeq_annot::C_Data::TAlign& objs)
{
    _ASSERT(m_ObjectInfos.m_Keys.empty());

    size_t objCount = objs.size();

    m_ObjectInfos.m_Keys.reserve(objCount);
    m_ObjectInfos.m_Infos.reserve(objCount);

    CTSE_Info& tse_info = GetTSE_Info();
    CTSE_Info::TAnnotObjs& index = tse_info.x_SetAnnotObjs(GetName());

    SAnnotObject_Key key;
    SAnnotObject_Index annotRef;
    vector<CHandleRangeMap> hrmaps;
    NON_CONST_ITERATE ( CSeq_annot::C_Data::TAlign, ait, objs ) {
        CSeq_align& align = **ait;

        CAnnotObject_Info* info =
            m_ObjectInfos.AddInfo(CAnnotObject_Info(align, *this));
        key.m_AnnotObject_Info = annotRef.m_AnnotObject_Info = info;

        info->GetMaps(hrmaps);
        annotRef.m_AnnotLocationIndex = 0;
        ITERATE ( vector<CHandleRangeMap>, hrmit, hrmaps ) {
            ITERATE ( CHandleRangeMap, hrit, *hrmit ) {
                key.m_Handle = hrit->first;
                const CHandleRange& hr = hrit->second;
                key.m_Range = hr.GetOverlappingRange();
                if ( hr.HasGaps() ) {
                    annotRef.m_HandleRange.Reset(new CObjectFor<CHandleRange>);
                    annotRef.m_HandleRange->GetData() = hr;
                }
                else {
                    annotRef.m_HandleRange.Reset();
                }

                tse_info.x_MapAnnotObject(index, key, annotRef, m_ObjectInfos);
            }
            ++annotRef.m_AnnotLocationIndex;
        }
    }
}


void CSeq_annot_Info::x_UnmapAnnotObjects(void)
{
    GetTSE_Info().x_UnmapAnnotObjects(m_ObjectInfos);
}


void CSeq_annot_Info::x_DropAnnotObjects(void)
{
    m_ObjectInfos.Clear();
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.9  2003/10/07 13:43:23  vasilche
 * Added proper handling of named Seq-annots.
 * Added feature search from named Seq-annots.
 * Added configurable adaptive annotation search (default: gene, cds, mrna).
 * Fixed selection of blobs for loading from GenBank.
 * Added debug checks to CSeq_id_Mapper for easier finding lost CSeq_id_Handles.
 * Fixed leaked split chunks annotation stubs.
 * Moved some classes definitions in separate *.cpp files.
 *
 * Revision 1.8  2003/09/30 16:22:03  vasilche
 * Updated internal object manager classes to be able to load ID2 data.
 * SNP blobs are loaded as ID2 split blobs - readers convert them automatically.
 * Scope caches results of requests for data to data loaders.
 * Optimized CSeq_id_Handle for gis.
 * Optimized bioseq lookup in scope.
 * Reduced object allocations in annotation iterators.
 * CScope is allowed to be destroyed before other objects using this scope are
 * deleted (feature iterators, bioseq handles etc).
 * Optimized lookup for matching Seq-ids in CSeq_id_Mapper.
 * Added 'adaptive' option to objmgr_demo application.
 *
 * Revision 1.7  2003/08/27 14:29:52  vasilche
 * Reduce object allocations in feature iterator.
 *
 * Revision 1.6  2003/07/18 19:41:46  vasilche
 * Removed unused variable.
 *
 * Revision 1.5  2003/07/17 22:51:31  vasilche
 * Fixed unused variables warnings.
 *
 * Revision 1.4  2003/07/17 20:07:56  vasilche
 * Reduced memory usage by feature indexes.
 * SNP data is loaded separately through PUBSEQ_OS.
 * String compression for SNP data.
 *
 * Revision 1.3  2003/07/09 17:54:29  dicuccio
 * Fixed uninitialized variables in CDataSource and CSeq_annot_Info
 *
 * Revision 1.2  2003/06/02 16:06:38  dicuccio
 * Rearranged src/objects/ subtree.  This includes the following shifts:
 *     - src/objects/asn2asn --> arc/app/asn2asn
 *     - src/objects/testmedline --> src/objects/ncbimime/test
 *     - src/objects/objmgr --> src/objmgr
 *     - src/objects/util --> src/objmgr/util
 *     - src/objects/alnmgr --> src/objtools/alnmgr
 *     - src/objects/flat --> src/objtools/flat
 *     - src/objects/validator --> src/objtools/validator
 *     - src/objects/cddalignview --> src/objtools/cddalignview
 * In addition, libseq now includes six of the objects/seq... libs, and libmmdb
 * replaces the three libmmdb? libs.
 *
 * Revision 1.1  2003/04/24 16:12:38  vasilche
 * Object manager internal structures are splitted more straightforward.
 * Removed excessive header dependencies.
 *
 *
 * ===========================================================================
 */
