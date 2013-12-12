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
*
*/

#include <ncbi_pch.hpp>
#include <objmgr/annot_selector.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/seq_entry_info.hpp>
#include <objmgr/impl/seq_annot_info.hpp>
#include <objmgr/impl/annot_type_index.hpp>

#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/seq_annot_handle.hpp>
#include <objmgr/objmgr_exception.hpp>
#include <objmgr/impl/handle_range_map.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/Seq_annot.hpp>

#include <algorithm>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

////////////////////////////////////////////////////////////////////
//
//  SAnnotSelector
//

SAnnotSelector::SAnnotSelector(TAnnotType annot,
                               TFeatType  feat,
                               bool       feat_product)
    : SAnnotTypeSelector(annot),
      m_FeatProduct(feat_product),
      m_ResolveDepth(kMax_Int),
      m_OverlapType(eOverlap_Intervals),
      m_ResolveMethod(eResolve_TSE),
      m_SortOrder(eSortOrder_Normal),
      m_LimitObjectType(eLimit_None),
      m_UnresolvedFlag(eIgnoreUnresolved),
      m_MaxSize(kMax_UInt),
      m_NoMapping(false),
      m_AdaptiveDepthFlags(kAdaptive_None),
      m_ExactDepth(false),
      m_ExcludeExternal(false),
      m_CollectSeq_annots(false),
      m_CollectTypes(false),
      m_CollectNames(false),
      m_IgnoreStrand(false)
{
    if ( feat != CSeqFeatData::e_not_set ) {
        SetFeatType(feat);
    }
}


SAnnotSelector::SAnnotSelector(TFeatType feat,
                               bool      feat_product)
    : SAnnotTypeSelector(feat),
      m_FeatProduct(feat_product),
      m_ResolveDepth(kMax_Int),
      m_OverlapType(eOverlap_Intervals),
      m_ResolveMethod(eResolve_TSE),
      m_SortOrder(eSortOrder_Normal),
      m_LimitObjectType(eLimit_None),
      m_UnresolvedFlag(eIgnoreUnresolved),
      m_MaxSize(kMax_UInt),
      m_NoMapping(false),
      m_AdaptiveDepthFlags(kAdaptive_None),
      m_ExactDepth(false),
      m_ExcludeExternal(false),
      m_CollectSeq_annots(false),
      m_CollectTypes(false),
      m_CollectNames(false),
      m_IgnoreStrand(false)
{
}


SAnnotSelector::SAnnotSelector(TFeatSubtype feat_subtype)
    : SAnnotTypeSelector(feat_subtype),
      m_FeatProduct(false),
      m_ResolveDepth(kMax_Int),
      m_OverlapType(eOverlap_Intervals),
      m_ResolveMethod(eResolve_TSE),
      m_SortOrder(eSortOrder_Normal),
      m_LimitObjectType(eLimit_None),
      m_UnresolvedFlag(eIgnoreUnresolved),
      m_MaxSize(kMax_UInt),
      m_NoMapping(false),
      m_AdaptiveDepthFlags(kAdaptive_None),
      m_ExactDepth(false),
      m_ExcludeExternal(false),
      m_CollectSeq_annots(false),
      m_CollectTypes(false),
      m_CollectNames(false),
      m_IgnoreStrand(false)
{
}


SAnnotSelector::SAnnotSelector(const SAnnotSelector& sel)
{
    *this = sel;
}


SAnnotSelector& SAnnotSelector::operator=(const SAnnotSelector& sel)
{
    if ( this != &sel ) {
        static_cast<SAnnotTypeSelector&>(*this) = sel;
        m_FeatProduct = sel.m_FeatProduct;
        m_ResolveDepth = sel.m_ResolveDepth;
        m_OverlapType = sel.m_OverlapType;
        m_ResolveMethod = sel.m_ResolveMethod;
        m_SortOrder = sel.m_SortOrder;
        m_FeatComparator = sel.m_FeatComparator;
        m_LimitObjectType = sel.m_LimitObjectType;
        m_UnresolvedFlag = sel.m_UnresolvedFlag;
        m_LimitObject = sel.m_LimitObject;
        m_LimitTSE = sel.m_LimitTSE;
        m_MaxSize = sel.m_MaxSize;
        m_IncludeAnnotsNames = sel.m_IncludeAnnotsNames;
        m_ExcludeAnnotsNames = sel.m_ExcludeAnnotsNames;
        if ( sel.m_NamedAnnotAccessions ) {
            m_NamedAnnotAccessions.reset
                (new TNamedAnnotAccessions(*sel.m_NamedAnnotAccessions));
        }
        m_NoMapping = sel.m_NoMapping;
        m_AdaptiveDepthFlags = sel.m_AdaptiveDepthFlags;
        m_ExactDepth = sel.m_ExactDepth;
        m_ExcludeExternal = sel.m_ExcludeExternal;
        m_CollectSeq_annots = sel.m_CollectSeq_annots;
        m_CollectTypes = sel.m_CollectTypes;
        m_CollectNames = sel.m_CollectNames;
        m_IgnoreStrand = sel.m_IgnoreStrand;
        m_AdaptiveTriggers = sel.m_AdaptiveTriggers;
        m_ExcludedTSE = sel.m_ExcludedTSE;
        m_AnnotTypesBitset = sel.m_AnnotTypesBitset;
        if ( sel.m_SourceLoc ) {
            m_SourceLoc.reset(new CHandleRangeMap(*sel.m_SourceLoc));
        }
        else {
            m_SourceLoc.reset();
        }
        m_IgnoreFarLocationsForSorting = sel.m_IgnoreFarLocationsForSorting;
    }
    return *this;
}


SAnnotSelector::~SAnnotSelector(void)
{
}


SAnnotSelector& SAnnotSelector::SetLimitNone(void)
{
    m_LimitObjectType = eLimit_None;
    m_LimitObject.Reset();
    m_LimitTSE.Reset();
    return *this;
}


SAnnotSelector&
SAnnotSelector::SetLimitTSE(const CTSE_Handle& limit)
{
    if ( !limit )
        return SetLimitNone();
    
    m_LimitObjectType = eLimit_TSE_Info;
    m_LimitObject.Reset(&limit.x_GetTSE_Info());
    m_LimitTSE = limit;
    return *this;
}


SAnnotSelector&
SAnnotSelector::SetLimitTSE(const CSeq_entry_Handle& limit)
{
    return SetLimitTSE(limit.GetTSE_Handle());
}


SAnnotSelector&
SAnnotSelector::SetLimitSeqEntry(const CSeq_entry_Handle& limit)
{
    if ( !limit )
        return SetLimitNone();
    
    m_LimitObjectType = eLimit_Seq_entry_Info;
    m_LimitObject.Reset(&limit.x_GetInfo());
    m_LimitTSE = limit.GetTSE_Handle();
    return *this;
}


SAnnotSelector&
SAnnotSelector::SetLimitSeqAnnot(const CSeq_annot_Handle& limit)
{
    if ( !limit )
        return SetLimitNone();
    
    m_LimitObjectType = eLimit_Seq_annot_Info;
    m_LimitObject.Reset(&limit.x_GetInfo());
    m_LimitTSE = limit.GetTSE_Handle();
    return *this;
}


SAnnotSelector& SAnnotSelector::SetSearchExternal(const CTSE_Handle& tse)
{
    _ASSERT( tse );
    SetResolveTSE();
    SetLimitTSE(tse);
    SetSearchUnresolved();
    return *this;
}


SAnnotSelector& SAnnotSelector::SetSearchExternal(const CSeq_entry_Handle& se)
{
    _ASSERT( se );
    return SetSearchExternal(se.GetTSE_Handle());
}


SAnnotSelector& SAnnotSelector::SetSearchExternal(const CBioseq_Handle& seq)
{
    _ASSERT( seq );
    return SetSearchExternal(seq.GetTSE_Handle());
}


namespace {
    template<class TNames, class TName>
    inline bool sx_Has(const TNames& names, const TName& name)
    {
        return find(names.begin(), names.end(), name) != names.end();
    }

    template<class TNames, class TName>
    inline void sx_Add(TNames& names, const TName& name)
    {
        if ( !sx_Has(names, name) ) {
            names.push_back(name);
        }
    }

    template<class TNames, class TName>
    void sx_Del(TNames& names, const TName& name)
    {
        NON_CONST_ITERATE( typename TNames, it, names ) {
            if ( *it == name ) {
                names.erase(it);
                break;
            }
        }
    }
}


bool SAnnotSelector::IncludedAnnotName(const CAnnotName& name) const
{
    return sx_Has(m_IncludeAnnotsNames, name);
}


bool SAnnotSelector::ExcludedAnnotName(const CAnnotName& name) const
{
    if ( IsIncludedAnyNamedAnnotAccession() && name.IsNamed() ) {
        string acc;
        int zoom_level;
        ExtractZoomLevel(name.GetName(), &acc, &zoom_level);

        int incl_level;
        TNamedAnnotAccessions::const_iterator it =
            m_NamedAnnotAccessions->find(acc);
        if ( it != m_NamedAnnotAccessions->end() ) {
            incl_level = it->second;
        }
        else {
            incl_level = 0; // ? do we include non-listed annot accessions?
        }

        if ( incl_level != -1 && zoom_level != incl_level ) {
            // excluded
            return true;
        }
    }
    return sx_Has(m_ExcludeAnnotsNames, name);
}


SAnnotSelector& SAnnotSelector::ResetAnnotsNames(void)
{
    m_IncludeAnnotsNames.clear();
    m_ExcludeAnnotsNames.clear();
    return *this;
}

namespace {
    void vector_erase(vector<CAnnotName>& v, const CAnnotName& name)
    {
        v.erase(remove(v.begin(), v.end(), name), v.end());
    }
}


SAnnotSelector& SAnnotSelector::ResetNamedAnnots(const CAnnotName& name)
{
    vector_erase(m_IncludeAnnotsNames, name);
    vector_erase(m_ExcludeAnnotsNames, name);
    return *this;
}


SAnnotSelector& SAnnotSelector::ResetNamedAnnots(const char* name)
{
    return ResetNamedAnnots(CAnnotName(name));
}


SAnnotSelector& SAnnotSelector::ResetUnnamedAnnots(void)
{
    return ResetNamedAnnots(CAnnotName());
}


SAnnotSelector& SAnnotSelector::AddNamedAnnots(const CAnnotName& name)
{
    sx_Add(m_IncludeAnnotsNames, name);
    sx_Del(m_ExcludeAnnotsNames, name);
    return *this;
}


SAnnotSelector& SAnnotSelector::AddNamedAnnots(const char* name)
{
    return AddNamedAnnots(CAnnotName(name));
}


SAnnotSelector& SAnnotSelector::AddUnnamedAnnots(void)
{
    return AddNamedAnnots(CAnnotName());
}


SAnnotSelector& SAnnotSelector::ExcludeNamedAnnots(const CAnnotName& name)
{
    sx_Add(m_ExcludeAnnotsNames, name);
    sx_Del(m_IncludeAnnotsNames, name);
    return *this;
}


SAnnotSelector& SAnnotSelector::ExcludeNamedAnnots(const char* name)
{
    return ExcludeNamedAnnots(CAnnotName(name));
}


SAnnotSelector& SAnnotSelector::ExcludeUnnamedAnnots(void)
{
    return ExcludeNamedAnnots(CAnnotName());
}


SAnnotSelector& SAnnotSelector::SetAllNamedAnnots(void)
{
    ResetAnnotsNames();
    ExcludeUnnamedAnnots();
    return *this;
}


SAnnotSelector& SAnnotSelector::SetDataSource(const string& source)
{
    if ( source.empty() ) {
        AddUnnamedAnnots();
    }
    return AddNamedAnnots(source);
}


SAnnotSelector&
SAnnotSelector::ResetNamedAnnotAccessions(void)
{
    m_NamedAnnotAccessions.reset();
    return *this;
}


SAnnotSelector&
SAnnotSelector::IncludeNamedAnnotAccession(const string& acc,
                                           int zoom_level)
{
    if ( !m_NamedAnnotAccessions ) {
        m_NamedAnnotAccessions.reset(new TNamedAnnotAccessions());
    }
    string acc_part;
    int zoom_part;
    if ( ExtractZoomLevel(acc, &acc_part, &zoom_part) ) {
        if ( zoom_level != 0 && zoom_part != zoom_level ) {
            NCBI_THROW_FMT(CAnnotException, eOtherError,
                           "SAnnotSelector::IncludeNamedAnnotAccession: "
                           "Incompatible zoom levels: "
                           <<acc<<" vs "<<zoom_level);
        }
        zoom_level = zoom_part;
    }
    (*m_NamedAnnotAccessions)[acc_part] = zoom_level;
    return *this;
}


bool SAnnotSelector::IsIncludedNamedAnnotAccession(const string& acc) const
{
    // The argument acc may contain version like "accession.123".
    // In this case we also check if plain accession ("accession"),
    // or all accesions ("accession.*") are included.
    if ( !IsIncludedAnyNamedAnnotAccession() ) {
        // no accessions are included at all
        return false;
    }
    TNamedAnnotAccessions::const_iterator it =
        m_NamedAnnotAccessions->lower_bound(acc);
    if ( it != m_NamedAnnotAccessions->end() && it->first == acc ) {
        // direct match
        return true;
    }
    SIZE_TYPE acc_size = acc.find('.');
    if ( acc_size == NPOS ) {
        // no version -> stop looking
        return false;
    }
    CTempString acc_name(acc.data(), acc_size);
    // find "accession" or "accession.*" which should be before iterator it
    while ( it != m_NamedAnnotAccessions->begin() &&
            NStr::StartsWith((--it)->first, acc_name) ) {
        const string& tacc = it->first;
        if ( tacc.size() == acc_size ) {
            // plain accession ("accession")
            return true;
        }
        if ( tacc.size() == acc_size+2 &&
             tacc[acc_size] == '.' &&
             tacc[acc_size+1] == '*' ) {
            // all accessions ("accession.*")
            return true;
        }
    }
    // no more matching accessions
    return false;
}


SAnnotSelector& SAnnotSelector::SetAdaptiveDepth(bool value)
{
    m_AdaptiveDepthFlags = value? kAdaptive_DefaultBits: kAdaptive_None;
    return *this;
}


SAnnotSelector&
SAnnotSelector::SetAdaptiveDepthFlags(TAdaptiveDepthFlags flags)
{
    m_AdaptiveDepthFlags =
        flags & fAdaptive_Default? kAdaptive_DefaultBits: flags;
    return *this;
}


SAnnotSelector&
SAnnotSelector::SetAdaptiveTrigger(const SAnnotTypeSelector& sel)
{
    ITERATE ( TAdaptiveTriggers, it, m_AdaptiveTriggers ) {
        if ( *it == sel ) {
            return *this;
        }
    }
    m_AdaptiveTriggers.push_back(sel);
    return *this;
}


SAnnotSelector&
SAnnotSelector::ExcludeTSE(const CTSE_Handle& tse)
{
    if ( !ExcludedTSE(tse) ) {
        m_ExcludedTSE.push_back(tse);
    }
    return *this;
}


SAnnotSelector&
SAnnotSelector::ExcludeTSE(const CSeq_entry_Handle& tse)
{
    return ExcludeTSE(tse.GetTSE_Handle());
}


SAnnotSelector&
SAnnotSelector::ResetExcludedTSE(void)
{
    m_ExcludedTSE.clear();
    return *this;
}


bool SAnnotSelector::ExcludedTSE(const CTSE_Handle& tse) const
{
    return find(m_ExcludedTSE.begin(), m_ExcludedTSE.end(), tse)
        != m_ExcludedTSE.end();
}


bool SAnnotSelector::ExcludedTSE(const CSeq_entry_Handle& tse) const
{
    return ExcludedTSE(tse.GetTSE_Handle());
}


void SAnnotSelector::x_ClearAnnotTypesSet(void)
{
    m_AnnotTypesBitset.reset();
}


void SAnnotSelector::x_InitializeAnnotTypesSet(bool default_value)
{
    if ( m_AnnotTypesBitset.any() ) {
        return;
    }
    if ( default_value ) {
        m_AnnotTypesBitset.set();
    }
    else {
        m_AnnotTypesBitset.reset();
    }
    // Do not try to use flags from an uninitialized selector
    if (GetAnnotType() != CSeq_annot::C_Data::e_not_set) {
        // Copy current state to the set
        CAnnotType_Index::TIndexRange range =
            CAnnotType_Index::GetIndexRange(*this);
        for (size_t i = range.first; i < range.second; ++i) {
            m_AnnotTypesBitset.set(i);
        }
    }
}


SAnnotSelector& SAnnotSelector::IncludeAnnotType(TAnnotType type)
{
    if (GetAnnotType() == CSeq_annot::C_Data::e_not_set) {
        SAnnotTypeSelector::SetAnnotType(type);
    }
    else if ( !IncludedAnnotType(type) ) {
        x_InitializeAnnotTypesSet(false);
        CAnnotType_Index::TIndexRange range =
            CAnnotType_Index::GetAnnotTypeRange(type);
        for (size_t i = range.first; i < range.second; ++i) {
            m_AnnotTypesBitset.set(i);
        }
    }
    return *this;
}


SAnnotSelector& SAnnotSelector::ExcludeAnnotType(TAnnotType type)
{
    if (GetAnnotType() == CSeq_annot::C_Data::e_not_set
        ||  IncludedAnnotType(type)) {
        x_InitializeAnnotTypesSet(true);
        CAnnotType_Index::TIndexRange range =
            CAnnotType_Index::GetAnnotTypeRange(type);
        for (size_t i = range.first; i < range.second; ++i) {
            m_AnnotTypesBitset.reset(i);
        }
    }
    return *this;
}


SAnnotSelector& SAnnotSelector::IncludeFeatType(TFeatType type)
{
    if (GetAnnotType() == CSeq_annot::C_Data::e_not_set) {
        SAnnotTypeSelector::SetFeatType(type);
    }
    else if (!IncludedFeatType(type)) {
        x_InitializeAnnotTypesSet(false);
        ForceAnnotType(CSeq_annot::C_Data::e_Ftable);
        CAnnotType_Index::TIndexRange range =
            CAnnotType_Index::GetFeatTypeRange(type);
        for (size_t i = range.first; i < range.second; ++i) {
            m_AnnotTypesBitset.set(i);
        }
    }
    return *this;
}


SAnnotSelector& SAnnotSelector::ExcludeFeatType(TFeatType type)
{
    if (GetAnnotType() == CSeq_annot::C_Data::e_not_set
        ||  IncludedFeatType(type)) {
        x_InitializeAnnotTypesSet(true);
        ForceAnnotType(CSeq_annot::C_Data::e_Ftable);
        CAnnotType_Index::TIndexRange range =
            CAnnotType_Index::GetFeatTypeRange(type);
        for (size_t i = range.first; i < range.second; ++i) {
            m_AnnotTypesBitset.reset(i);
        }
    }
    return *this;
}


SAnnotSelector& SAnnotSelector::IncludeFeatSubtype(TFeatSubtype subtype)
{
    if (GetAnnotType() == CSeq_annot::C_Data::e_not_set) {
        SAnnotTypeSelector::SetFeatSubtype(subtype);
    }
    else if ( !IncludedFeatSubtype(subtype) ) {
        x_InitializeAnnotTypesSet(false);
        ForceAnnotType(CSeq_annot::C_Data::e_Ftable);
        m_AnnotTypesBitset.set(CAnnotType_Index::GetSubtypeIndex(subtype));
    }
    return *this;
}


SAnnotSelector& SAnnotSelector::ExcludeFeatSubtype(TFeatSubtype subtype)
{
    if (GetAnnotType() == CSeq_annot::C_Data::e_not_set
        ||  IncludedFeatSubtype(subtype)) {
        x_InitializeAnnotTypesSet(true);
        ForceAnnotType(CSeq_annot::C_Data::e_Ftable);
        m_AnnotTypesBitset.reset(CAnnotType_Index::GetSubtypeIndex(subtype));
    }
    return *this;
}


SAnnotSelector& SAnnotSelector::ForceAnnotType(TAnnotType type)
{
    if ( type == CSeq_annot::C_Data::e_Ftable ) {
        // Remove all non-feature types from the list
        if ( m_AnnotTypesBitset.any() ) {
            CAnnotType_Index::TIndexRange range =
                CAnnotType_Index::GetAnnotTypeRange(type);
            for (size_t i = 0; i < range.first; ++i) {
                m_AnnotTypesBitset.reset(i);
            }
            for (size_t i = range.second; i < m_AnnotTypesBitset.size(); ++i) {
                m_AnnotTypesBitset.reset(i);
            }
        }
        else {
            SetAnnotType(type);
        }
    }
    else if ( type != CSeq_annot::C_Data::e_not_set ) {
        // Force the type
        SetAnnotType(type);
    }
    return *this;
}


bool SAnnotSelector::IncludedAnnotType(TAnnotType type) const
{
    if ( m_AnnotTypesBitset.any() ) {
        CAnnotType_Index::TIndexRange range =
            CAnnotType_Index::GetAnnotTypeRange(type);
        for (size_t i = range.first; i < range.second; ++i) {
            if ( m_AnnotTypesBitset.test(i) ) {
                return true;
            }
        }
        return false;
    }
    return GetAnnotType() == CSeq_annot::C_Data::e_not_set
        || GetAnnotType() == type;
}


bool SAnnotSelector::IncludedFeatType(TFeatType type) const
{
    if ( m_AnnotTypesBitset.any() ) {
        CAnnotType_Index::TIndexRange range =
            CAnnotType_Index::GetFeatTypeRange(type);
        for (size_t i = range.first; i < range.second; ++i) {
            if ( m_AnnotTypesBitset.test(i) ) {
                return true;
            }
        }
        return false;
    }
    // Make sure features are selected
    return GetAnnotType() == CSeq_annot::C_Data::e_not_set ||
        (GetAnnotType() == CSeq_annot::C_Data::e_Ftable &&
         (GetFeatType() == CSeqFeatData::e_not_set ||
          GetFeatType() == type));
}


bool SAnnotSelector::IncludedFeatSubtype(TFeatSubtype subtype) const
{
    if ( m_AnnotTypesBitset.any() ) {
        return m_AnnotTypesBitset
            .test(CAnnotType_Index::GetSubtypeIndex(subtype));
    }
    // Make sure features are selected
    return GetAnnotType() == CSeq_annot::C_Data::e_not_set ||
        (GetAnnotType() == CSeq_annot::C_Data::e_Ftable &&
         (GetFeatType() == CSeqFeatData::e_not_set ||
          subtype == CSeqFeatData::eSubtype_any ||
          GetFeatSubtype() == subtype ||
          (GetFeatSubtype() == CSeqFeatData::eSubtype_any &&
           GetFeatType() == CSeqFeatData::GetTypeFromSubtype(subtype))));
}


bool SAnnotSelector::MatchType(const CAnnotObject_Info& annot_info) const
{
    if (annot_info.GetFeatSubtype() != CSeqFeatData::eSubtype_any) {
        return IncludedFeatSubtype(annot_info.GetFeatSubtype());
    }
    if (annot_info.GetFeatType() != CSeqFeatData::e_not_set) {
        return IncludedFeatType(annot_info.GetFeatType());
    }
    return IncludedAnnotType(annot_info.GetAnnotType());
}


void SAnnotSelector::CheckLimitObjectType(void) const
{
    if ( !m_LimitObject ) {
        m_LimitObjectType = eLimit_None;
    }
}


SAnnotSelector& SAnnotSelector::SetSourceLoc(const CSeq_loc& loc)
{
    m_SourceLoc.reset(new CHandleRangeMap);
    m_SourceLoc->AddLocation(loc);
    return *this;
}


SAnnotSelector& SAnnotSelector::ResetSourceLoc(void)
{
    m_SourceLoc.reset();
    return *this;
}


IFeatComparator::~IFeatComparator()
{
}


/////////////////////////////////////////////////////////////////////////////
// Zoom level manipulation functions
/////////////////////////////////////////////////////////////////////////////


bool ExtractZoomLevel(const string& full_name,
                      string* acc_ptr, int* zoom_level_ptr)
{
    SIZE_TYPE pos = full_name.find(NCBI_ANNOT_TRACK_ZOOM_LEVEL_SUFFIX);
    if ( pos != NPOS ) {
        if ( acc_ptr ) {
            *acc_ptr = full_name.substr(0, pos);
        }
        SIZE_TYPE num_pos = pos+strlen(NCBI_ANNOT_TRACK_ZOOM_LEVEL_SUFFIX);
        if ( num_pos+1 == full_name.size() && full_name[num_pos] == '*' ) {
            if ( zoom_level_ptr ) {
                *zoom_level_ptr = -1;
            }
            return true;
        }
        else {
            try {
                int zoom_level = NStr::StringToInt(full_name.substr(num_pos));
                if ( zoom_level_ptr ) {
                    *zoom_level_ptr = zoom_level;
                }
                return true;
            }
            catch ( CException& ) {
                // invalid zoom level suffix, assume no zoom level
            }
        }
    }
    // no explicit zoom level
    if ( acc_ptr ) {
        *acc_ptr = full_name;
    }
    if ( zoom_level_ptr ) {
        *zoom_level_ptr = 0;
    }
    return false;
}


string CombineWithZoomLevel(const string& acc, int zoom_level)
{
    int incl_level;
    if ( !ExtractZoomLevel(acc, 0, &incl_level) ) {
        if ( zoom_level == -1 ) {
            // wildcard
            return acc + NCBI_ANNOT_TRACK_ZOOM_LEVEL_SUFFIX "*";
        }
        else {
            return acc +
                NCBI_ANNOT_TRACK_ZOOM_LEVEL_SUFFIX +
                NStr::IntToString(zoom_level);
        }
    }
    else if ( incl_level != zoom_level ) {
        // different zoom level
        NCBI_THROW_FMT(CAnnotException, eOtherError,
                       "AddZoomLevel: Incompatible zoom levels: "
                       <<acc<<" vs "<<zoom_level);
    }
    return acc;
}


void AddZoomLevel(string& acc, int zoom_level)
{
    int incl_level;
    if ( !ExtractZoomLevel(acc, 0, &incl_level) ) {
        if ( zoom_level == -1 ) {
            // wildcard
            acc += NCBI_ANNOT_TRACK_ZOOM_LEVEL_SUFFIX "*";
        }
        else {
            acc += NCBI_ANNOT_TRACK_ZOOM_LEVEL_SUFFIX;
            acc += NStr::IntToString(zoom_level);
        }
    }
    else if ( incl_level != zoom_level ) {
        // different zoom level
        NCBI_THROW_FMT(CAnnotException, eOtherError,
                       "AddZoomLevel: Incompatible zoom levels: "
                       <<acc<<" vs "<<zoom_level);
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE
