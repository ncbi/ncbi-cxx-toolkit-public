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
* Author:  Clifford Clausen
*
* File Description:
*   Sequence utilities requiring CScope
*/

#include <ncbi_pch.hpp>
#include <serial/iterator.hpp>
#include <util/static_map.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/seq_vector_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/impl/handle_range_map.hpp>
#include <objmgr/impl/synonyms.hpp>
#include <objmgr/util/seq_loc_util.hpp>
#include <objmgr/util/create_defline.hpp>

#include <objects/general/Int_fuzz.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/Date.hpp>

#include <objects/misc/sequence_util_macros.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Linkage_evidence.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seq/Seg_ext.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Seq_gap.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seq/seqport_util.hpp>
#include <objects/seq/Seq_hist.hpp>
#include <objects/seq/Seq_hist_rec.hpp>

#include <objects/seqloc/Packed_seqpnt.hpp>
#include <objects/seqloc/Seq_bond.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_loc_equiv.hpp>
#include <objects/seqloc/Seq_loc_mix.hpp>
#include <objects/seqloc/Seq_point.hpp>

#include <objects/seqset/Seq_entry.hpp>

#include <objects/seqfeat/seqfeat__.hpp>

#include <objmgr/seq_loc_mapper.hpp>
#include <objmgr/seq_entry_ci.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/error_codes.hpp>
#include <util/strsearch.hpp>

#include <list>
#include <algorithm>


#define NCBI_USE_ERRCODE_X   ObjMgr_SeqUtil

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(sequence)


static string s_GetFastaTitle(const CBioseq& bs,
                              CDeflineGenerator::TUserFlags flags)
{
    string title;
    bool has_molinfo = false;

    const CSeq_descr::Tdata& descr = bs.GetDescr().Get();
    for ( CSeq_descr::Tdata::const_iterator it = descr.begin(); it != descr.end(); ++it ) {
        const CSeqdesc* sd = *it;
        if ( sd->Which() == CSeqdesc::e_Title ) {
            if ( title == "" ) {
                title = sd->GetTitle();
            }
        }
        if ( sd->Which() == CSeqdesc::e_Molinfo ) {
            has_molinfo = true;
        }
    }
    
    if ( !title.empty() && ! has_molinfo ) {
        while (NStr::EndsWith(title, ".")  ||  NStr::EndsWith(title, " ")) {
            title.erase(title.end() - 1);
        }
        return title;
    }
    else {
        CScope scope(*CObjectManager::GetInstance());
        CDeflineGenerator gen;
        return gen.GenerateDefline(bs, scope, flags);
    }
}


const CBioSource* GetBioSource(const CBioseq_Handle& handle)
{
    {{
        CSeqdesc_CI desc(handle, CSeqdesc::e_Source);
        if (desc) {
            return &desc->GetSource();
        }
    }}
    {{
        CSeqdesc_CI desc(handle.GetTopLevelEntry(), CSeqdesc::e_Source);
        if (desc) {
            return &desc->GetSource();
        }
    }}

    return NULL;
}


const COrg_ref* GetOrg_refOrNull(const CBioseq_Handle& handle)
{
    vector<CSeqdesc::E_Choice> types;
    types.push_back(CSeqdesc::e_Source);
    types.push_back(CSeqdesc::e_Org);
    CSeqdesc_CI desc_it(handle, types);
    if ( desc_it ) {
        const CSeqdesc& desc = *desc_it;
        if ( desc.IsSource() ) {
            return &desc.GetSource().GetOrg();
        }
        if ( desc.IsOrg() ) {
            return &desc.GetOrg();
        }
    }
    return 0;
}


const COrg_ref& GetOrg_ref(const CBioseq_Handle& handle)
{
    const COrg_ref* org_ref = GetOrg_refOrNull(handle);
    if ( org_ref ) {
        return *org_ref;
    }
    NCBI_THROW(CException, eUnknown, "No organism set");
}


int GetTaxId(const CBioseq_Handle& handle)
{
    const COrg_ref* org_ref = GetOrg_refOrNull(handle);
    if ( org_ref ) {
        return org_ref->GetTaxId();
    }
    return 0;
}


const CMolInfo* GetMolInfo(const CBioseq_Handle& handle)
{
    CSeqdesc_CI desc_iter(handle, CSeqdesc::e_Molinfo);
    for ( ;  desc_iter;  ++desc_iter) {
        return &desc_iter->GetMolinfo();
    }

    return NULL;
}



CBioseq_Handle GetBioseqFromSeqLoc
(const CSeq_loc& loc,
 CScope& scope,
 CScope::EGetBioseqFlag flag)
{
    CBioseq_Handle retval;

    try {
        if (IsOneBioseq(loc, &scope)) {
            return scope.GetBioseqHandle(GetId(loc, &scope), flag);
        } 

        // assuming location is annotated on parts of a segmented bioseq
        for (CSeq_loc_CI it(loc); it; ++it) {
            CBioseq_Handle part = scope.GetBioseqHandle(it.GetSeq_id(), flag);
            if (part) {
                retval = GetParentForPart(part);
            }
            break;  // check only the first part
        }

        // if multiple intervals and not parts, look for the first loaded bioseq
        if (!retval) {
            for (CSeq_loc_CI it(loc); it; ++it) {
                retval = 
                    scope.GetBioseqHandle(it.GetSeq_id_Handle(), CScope::eGetBioseq_Loaded);
                if (retval) {
                    break;
                }
            }
        }

        if (!retval  &&  flag == CScope::eGetBioseq_All) {
            for (CSeq_loc_CI it(loc); it; ++it) {
                retval = 
                    scope.GetBioseqHandle(it.GetSeq_id_Handle(), flag);
                if (retval) {
                    break;
                }
            }
        }
    } catch (exception&) {
        retval.Reset();
    }

    return retval;
}


const char* CSeqIdFromHandleException::GetErrCodeString(void) const
{
    switch (GetErrCode()) {
    case eNoSynonyms:           return "eNoSynonyms";
    case eRequestedIdNotFound:  return "eRequestedIdNotFound";
    default:                    return CException::GetErrCodeString();
    }
}


int Score_SeqIdHandle(const CSeq_id_Handle& idh)
{
    CConstRef<CSeq_id> id = idh.GetSeqId();
    CRef<CSeq_id> id_non_const
        (const_cast<CSeq_id*>(id.GetPointer()));
    return CSeq_id::Score(id_non_const);
}


int BestRank_SeqIdHandle(const CSeq_id_Handle& idh)
{
    CConstRef<CSeq_id> id = idh.GetSeqId();
    CRef<CSeq_id> id_non_const
        (const_cast<CSeq_id*>(id.GetPointer()));
    return CSeq_id::BestRank(id_non_const);
}


int WorstRank_SeqIdHandle(const CSeq_id_Handle& idh)
{
    CConstRef<CSeq_id> id = idh.GetSeqId();
    CRef<CSeq_id> id_non_const
        (const_cast<CSeq_id*>(id.GetPointer()));
    return CSeq_id::WorstRank(id_non_const);
}


int FastaAARank_SeqIdHandle(const CSeq_id_Handle& idh)
{
    CConstRef<CSeq_id> id = idh.GetSeqId();
    CRef<CSeq_id> id_non_const
        (const_cast<CSeq_id*>(id.GetPointer()));
    return CSeq_id::FastaAARank(id_non_const);
}


int FastaNARank_SeqIdHandle(const CSeq_id_Handle& idh)
{
    CConstRef<CSeq_id> id = idh.GetSeqId();
    CRef<CSeq_id> id_non_const
        (const_cast<CSeq_id*>(id.GetPointer()));
    return CSeq_id::FastaNARank(id_non_const);
}



CSeq_id_Handle x_GetId(const CScope::TIds& ids, EGetIdType type)
{
    if ( ids.empty() ) {
        return CSeq_id_Handle();
    }

    switch ( (type & eGetId_TypeMask) ) {
    case eGetId_ForceGi:
        {{
            ITERATE (CScope::TIds, iter, ids) {
                if (iter->IsGi()) {
                    return *iter;
                }
            }
        }}
        if ((type & eGetId_ThrowOnError) != 0) {
            NCBI_THROW(CSeqIdFromHandleException, eRequestedIdNotFound,
                    "sequence::GetId(): gi seq-id not found in the list");
        }
        break;

    case eGetId_ForceAcc:
        {{
            CSeq_id_Handle best = x_GetId(ids, eGetId_Best);
            if (best  &&
                best.GetSeqId()->GetTextseq_Id() != NULL  &&
                best.GetSeqId()->GetTextseq_Id()->IsSetAccession()) {
                return best;
            }
        }}
        if ((type & eGetId_ThrowOnError) != 0) {
            NCBI_THROW(CSeqIdFromHandleException, eRequestedIdNotFound,
                    "sequence::GetId(): text seq-id not found in the list");
        }
        break;

    case eGetId_Best:
        {{
            return FindBestChoice(ids, Score_SeqIdHandle);
        }}

    case eGetId_Seq_id_Score:
        {{
            return FindBestChoice(ids, Score_SeqIdHandle);
        }}

    case eGetId_Seq_id_BestRank:
        {{
            return FindBestChoice(ids, BestRank_SeqIdHandle);
        }}

    case eGetId_Seq_id_WorstRank:
        {{
            return FindBestChoice(ids, WorstRank_SeqIdHandle);
        }}

    case eGetId_Seq_id_FastaAARank:
        {{
            return FindBestChoice(ids, FastaAARank_SeqIdHandle);
        }}

    case eGetId_Seq_id_FastaNARank:
        {{
            return FindBestChoice(ids, FastaNARank_SeqIdHandle);
        }}

    default:
        break;
    }
    return CSeq_id_Handle();
}


CSeq_id_Handle GetId(const CSeq_id& id, CScope& scope, EGetIdType type)
{
    return GetId(CSeq_id_Handle::GetHandle(id), scope, type);
}


CSeq_id_Handle GetId(const CSeq_id_Handle& idh, CScope& scope,
                     EGetIdType type)
{
    CSeq_id_Handle ret;
    if (!idh) return ret;
    try {
        if ( (type & eGetId_TypeMask) == eGetId_ForceGi ) {
            if ( idh.IsGi()  &&  (type & eGetId_VerifyId) == 0 ) {
                return idh;
            }
            TGi gi = scope.GetGi(idh);
            if (gi != ZERO_GI) {
                ret = CSeq_id_Handle::GetGiHandle(gi);
            }
        }
        else if ( (type & eGetId_TypeMask) == eGetId_Canonical) {
            /// Short-cuts for commonly used IDs that are
            /// known unambiguously to be canonical:
            /// - ID/GenBank: GI
            /// - Trace: gnl|ti|<tid> in the C++ Toolkit;
            ///          note that in the C Toolkit, the
            ///          canonical ID appears to be gnl|TRACE|<tid>.
            /// - Short Read Archive: gnl|SRA|...
            if (idh.IsGi()) return idh;
            if (idh.Which() == CSeq_id::e_General) {
                CConstRef<CSeq_id> id = idh.GetSeqId();
                _ASSERT(id  &&  id->IsGeneral());
                const CSeq_id::TGeneral::TDb& db = id->GetGeneral().GetDb();
                if (db == "ti"  ||  db == "SRA") return idh;
            }

            /// Fallback to retrieve IDs.
            ret = x_GetId(scope.GetIds(idh), type);
            if ( !ret ) {
                /// failed to retrieve IDs
                /// assume input is the best that we can do
                ret = idh;
            }
        }
        else if ( (type & eGetId_TypeMask) == eGetId_ForceAcc ) {
            ret = scope.GetAccVer(idh);
        }
        else {
            ret = x_GetId(scope.GetIds(idh), type);
        }
    }
    catch (exception& e) {
        ERR_POST("sequence::GetId(): exception: "<<e.what());
        if ( (type & eGetId_ThrowOnError) != 0 ) {
            throw;
        }
        ret.Reset();
        return ret;
    }
    if ( !ret  &&  (type & eGetId_ThrowOnError) != 0 ) {
        NCBI_THROW(CSeqIdFromHandleException, eRequestedIdNotFound,
                "sequence::GetId(): seq-id not found in the scope");
    }
    return ret;
}


CSeq_id_Handle GetId(const CBioseq_Handle& handle,
                     EGetIdType type)
{
    _ASSERT(handle);

    const CScope::TIds& ids = handle.GetId();
    CSeq_id_Handle idh = x_GetId(ids, type);

    if ( !idh  &&  (type & eGetId_ThrowOnError) != 0 ) {
        NCBI_THROW(CSeqIdFromHandleException, eRequestedIdNotFound,
                   "Unable to get Seq-id from handle");
    }

    return idh;
}


TGi GetGiForAccession(const string& acc, CScope& scope, EGetIdType flags)
{
    // Clear throw-on-error flag
    EGetIdType get_id_flags = (flags & eGetId_VerifyId) | eGetId_ForceGi;
    CSeq_id acc_id(acc);
    // Get gi only if acc a real accession.
    if ( acc_id.GetTextseq_Id() ) {
        CSeq_id_Handle idh = GetId(acc_id, scope, get_id_flags);
        if ( idh.IsGi() ) {
            return idh.GetGi();
        }
    }
    if ( (flags & eGetId_ThrowOnError) != 0 ) {
        NCBI_THROW(CSeqIdFromHandleException, eRequestedIdNotFound,
            "sequence::GetGiForAccession(): invalid seq-id type");
    }
    return ZERO_GI;
}


TGi GetGiForId(const objects::CSeq_id& id, CScope& scope, EGetIdType flags)
{
    // Clear throw-on-error flag
    EGetIdType get_id_flags = (flags & eGetId_VerifyId) | eGetId_ForceGi;
    CSeq_id_Handle idh = GetId(id, scope, get_id_flags);
    if ( idh.IsGi() ) {
        return idh.GetGi();
    }
    if ( (flags & eGetId_ThrowOnError) != 0 ) {
        NCBI_THROW(CSeqIdFromHandleException, eRequestedIdNotFound,
            "sequence::GetGiForId(): seq-id not found in the scope");
    }
    return ZERO_GI;
}


string GetAccessionForGi(TGi           gi,
                         CScope&       scope,
                         EAccessionVersion use_version,
                         EGetIdType flags)
{
    // Clear throw-on-error flag
    EGetIdType get_id_flags = (flags & eGetId_VerifyId) | eGetId_ForceAcc;
    bool with_version = (use_version == eWithAccessionVersion);

    CSeq_id gi_id(CSeq_id::e_Gi, gi);
    CSeq_id_Handle idh = GetId(gi_id, scope, get_id_flags);
    if ( idh ) {
        return idh.GetSeqId()->GetSeqIdString(with_version);
    }
    if ( (flags & eGetId_ThrowOnError) != 0 ) {
        NCBI_THROW(CSeqIdFromHandleException, eRequestedIdNotFound,
            "sequence::GetAccessionForGi(): seq-id not found in the scope");
    }
    return kEmptyStr;
}


string GetAccessionForId(const objects::CSeq_id& id,
                         CScope&       scope,
                         EAccessionVersion use_version,
                         EGetIdType flags)
{
    // Clear throw-on-error flag
    EGetIdType get_id_flags = (flags & eGetId_VerifyId) | eGetId_ForceAcc;
    bool with_version = (use_version == eWithAccessionVersion);

    CSeq_id_Handle idh = GetId(id, scope, get_id_flags);
    if ( idh ) {
        return idh.GetSeqId()->GetSeqIdString(with_version);
    }
    if ( (flags & eGetId_ThrowOnError) != 0 ) {
        NCBI_THROW(CSeqIdFromHandleException, eRequestedIdNotFound,
            "sequence::GetAccessionForId(): seq-id not found in the scope");
    }
    return kEmptyStr;
}


CSeq_id_Handle x_FindLatestSequence(const CSeq_id_Handle& idh,
                                   CScope&               scope,
                                   const CTime*          tlim)
{
    CBioseq_Handle h = scope.GetBioseqHandle(idh);
    set<CSeq_id_Handle> visited;
    CSeq_id_Handle next = idh;
    while (h  &&  h.IsSetInst() && h.GetInst().IsSetHist()
        && h.GetInst().GetHist().IsSetReplaced_by()) {
        const CSeq_hist_rec& rec = h.GetInst().GetHist().GetReplaced_by();

        // Check if the next bioseq is newer than the limit.
        if (tlim  &&  rec.IsSetDate()  &&
            rec.GetDate().AsCTime().DiffTimeSpan(*tlim).GetSign() == ePositive) {
            break;
        }
        // Make sure the list of ids is not empty
        if ( rec.GetIds().empty() ) {
            return CSeq_id_Handle();
        }
        visited.insert(next);
        // If there are several replaced-by entries, use the first one
        next = CSeq_id_Handle::GetHandle(
            *h.GetInst().GetHist().GetReplaced_by().GetIds().front());
        if (visited.find(next) != visited.end()) {
            // Infinite recursion detected
            return CSeq_id_Handle();
        }
        h = scope.GetBioseqHandle(next);
    }
    return h ? next : CSeq_id_Handle();
}


CConstRef<CSeq_id> FindLatestSequence(const CSeq_id& id, CScope& scope)
{
    return x_FindLatestSequence(CSeq_id_Handle::GetHandle(id),
        scope, NULL).GetSeqId();
}

CSeq_id_Handle FindLatestSequence(const CSeq_id_Handle& idh, CScope& scope)
{
    return x_FindLatestSequence(idh, scope, NULL);
}

CConstRef<CSeq_id> FindLatestSequence(const CSeq_id& id,
                                      CScope&        scope,
                                      const CTime&   tlim)
{
    return x_FindLatestSequence(CSeq_id_Handle::GetHandle(id),
        scope, &tlim).GetSeqId();
}

CSeq_id_Handle FindLatestSequence(const CSeq_id_Handle& idh,
                                  CScope&               scope,
                                  const CTime&          tlim)
{
    return x_FindLatestSequence(idh, scope, &tlim);
}


CRef<CSeq_loc> SourceToProduct(const CSeq_feat& feat,
                               const CSeq_loc& source_loc, TS2PFlags flags,
                               CScope* scope, int* frame)
{
    SRelLoc::TFlags rl_flags = 0;
    if (flags & fS2P_NoMerge) {
        rl_flags |= SRelLoc::fNoMerge;
    }
    SRelLoc rl(feat.GetLocation(), source_loc, scope, rl_flags);
    _ASSERT(!rl.m_Ranges.empty());
    rl.m_ParentLoc.Reset(&feat.GetProduct());
    if (feat.GetData().IsCdregion()) {
        // 3:1 ratio
        const CCdregion& cds         = feat.GetData().GetCdregion();
        int              base_frame  = cds.GetFrame();
        if (base_frame > 0) {
            --base_frame;
        }
        if (frame) {
            *frame = (3 + rl.m_Ranges.front()->GetFrom() - base_frame) % 3 + 1;
        }
        TSeqPos prot_length;
        try {
            prot_length = GetLength(feat.GetProduct(), scope);
        } catch (CObjmgrUtilException) {
            prot_length = numeric_limits<TSeqPos>::max();
        }
        NON_CONST_ITERATE (SRelLoc::TRanges, it, rl.m_Ranges) {
            if (IsReverse((*it)->GetStrand())) {
                ERR_POST_X(6, Warning
                           << "SourceToProduct:"
                           " parent and child have opposite orientations");
            }
            (*it)->SetFrom(((*it)->GetFrom() - base_frame) / 3);
            (*it)->SetTo  (((*it)->GetTo()   - base_frame) / 3);
            if ((flags & fS2P_AllowTer)  &&  (*it)->GetTo() == prot_length) {
                --(*it)->SetTo();
            }
        }
    } else {
        if (frame) {
            *frame = 0; // not applicable; explicitly zero
        }
    }

    return rl.Resolve(scope, rl_flags);
}


CRef<CSeq_loc> ProductToSource(const CSeq_feat& feat, const CSeq_loc& prod_loc,
                               TP2SFlags flags, CScope* scope)
{
    SRelLoc rl(feat.GetProduct(), prod_loc, scope);
    _ASSERT(!rl.m_Ranges.empty());
    rl.m_ParentLoc.Reset(&feat.GetLocation());
    if (feat.GetData().IsCdregion()) {
        // 3:1 ratio
        const CCdregion& cds        = feat.GetData().GetCdregion();
        int              base_frame = cds.GetFrame();
        if (base_frame > 0) {
            --base_frame;
        }
        TSeqPos nuc_length, prot_length;
        try {
            nuc_length = GetLength(feat.GetLocation(), scope);
        } catch (CObjmgrUtilException) {
            nuc_length = numeric_limits<TSeqPos>::max();
        }
        try {
            prot_length = GetLength(feat.GetProduct(), scope);
        } catch (CObjmgrUtilException) {
            prot_length = numeric_limits<TSeqPos>::max();
        }
        NON_CONST_ITERATE(SRelLoc::TRanges, it, rl.m_Ranges) {
            _ASSERT( !IsReverse((*it)->GetStrand()) );
            TSeqPos from, to;
            if ((flags & fP2S_Extend)  &&  (*it)->GetFrom() == 0) {
                from = 0;
            } else {
                from = (*it)->GetFrom() * 3 + base_frame;
            }
            if ((flags & fP2S_Extend)  &&  (*it)->GetTo() == prot_length - 1) {
                to = nuc_length - 1;
            } else {
                to = (*it)->GetTo() * 3 + base_frame + 2;
            }
            (*it)->SetFrom(from);
            (*it)->SetTo  (to);
        }
    }

    return rl.Resolve(scope);
}


typedef pair<Int8, CConstRef<CSeq_feat> > TFeatScore;
typedef vector<TFeatScore> TFeatScores;

template <class T, class U>
struct SPairLessByFirst
    : public binary_function< pair<T,U>, pair<T,U>, bool >
{
    bool operator()(const pair<T,U>& p1, const pair<T,U>& p2) const
    {
        return p1.first < p2.first;
    }
};

template <class T, class U>
struct SPairLessBySecond
    : public binary_function< pair<T,U>, pair<T,U>, bool >
{
    bool operator()(const pair<T,U>& p1, const pair<T,U>& p2) const
    {
        return p1.second < p2.second;
    }
};

class COverlapPairLess
{
public:
    COverlapPairLess( CScope *scope_arg ) : scope(scope_arg) { }

    bool operator()( const pair<Int8,CConstRef<CSeq_feat> >& gene1, 
        const pair<Int8, CConstRef<CSeq_feat> >& gene2 )
    {
        // First, compare by overlap amount
        if( gene1.first != gene2.first ) {
            return gene1.first < gene2.first;
        }

        const CSeq_loc &loc1 = gene1.second->GetLocation();
        const CSeq_loc &loc2 = gene2.second->GetLocation();

         // If genes are at identical positions, we fall back on the label
         if( sequence::Compare( loc1, loc2, scope ) == sequence::eSame ) {
            if( gene1.second->IsSetData() && gene1.second->GetData().IsGene() && 
                gene2.second->IsSetData() && gene2.second->GetData().IsGene() ) 
            {
                string gene1_label;
                string gene2_label;

                gene1.second->GetData().GetGene().GetLabel( &gene1_label );
                gene2.second->GetData().GetGene().GetLabel( &gene2_label );
                return gene1_label < gene2_label;
            }
         }

        return false;
    }
private:
    CScope *scope;
};

void GetOverlappingFeatures(const CSeq_loc& loc,
                            CSeqFeatData::E_Choice feat_type,
                            CSeqFeatData::ESubtype feat_subtype,
                            EOverlapType overlap_type,
                            TFeatScores& feats,
                            CScope& scope,
                            const TBestFeatOpts opts,
                            CGetOverlappingFeaturesPlugin *plugin )
{
    bool revert_locations = false; 
    SAnnotSelector::EOverlapType annot_overlap_type;
    switch (overlap_type) {
    case eOverlap_Simple:
    case eOverlap_Contained:
    case eOverlap_Contains:
        // Require total range overlap 
        annot_overlap_type = SAnnotSelector::eOverlap_TotalRange;
        break;
    case eOverlap_Subset:
    case eOverlap_SubsetRev:
    case eOverlap_CheckIntervals:
    case eOverlap_Interval:
    case eOverlap_CheckIntRev:
        revert_locations = true;
        // there's no break here - proceed to "default"
    default:
        // Require intervals overlap
        annot_overlap_type = SAnnotSelector::eOverlap_Intervals;
        break;
    }

    CConstRef<CSeq_feat> feat_ref;

    CBioseq_Handle bioseq_handle;
    CRange<TSeqPos> range;
    ENa_strand strand = eNa_strand_unknown;
    if ( loc.IsWhole() ) {
        bioseq_handle = scope.GetBioseqHandle(loc.GetWhole());
        range = range.GetWhole();
    }
    else if ( loc.IsInt() || loc.IsPnt() || loc.IsPacked_int() || loc.IsMix() || loc.IsPacked_pnt() ) {
        const CSeq_id* id = loc.GetId();
        if( NULL != id ) {
            bioseq_handle = scope.GetBioseqHandle(*id);
            range.SetFrom(loc.GetStart(eExtreme_Positional));
            range.SetTo(loc.GetStop(eExtreme_Positional));
            if ( loc.IsSetStrand() ) {
                strand = loc.GetStrand();
            }
        }
    }
    else {
        range = range.GetEmpty();
    }

    // Check if the sequence is circular
    TSeqPos circular_length = kInvalidSeqPos;
    const CSeq_id* circular_id = NULL;
    if ( bioseq_handle ) {
        if ( bioseq_handle.IsSetInst_Topology() &&
             bioseq_handle.GetInst_Topology() == CSeq_inst::eTopology_circular ) {
            circular_length = bioseq_handle.GetBioseqLength();
            circular_id = bioseq_handle.GetSeqId().GetPointer();
        }
    }
    else {
        try {
            try {
                loc.CheckId(circular_id);
            }
            catch (exception&) {
                circular_id = 0;
            }
            if ( circular_id ) {
                CBioseq_Handle bioseq_handle = scope.GetBioseqHandle(*circular_id);
                if ( bioseq_handle && bioseq_handle.IsSetInst_Topology() &&
                     bioseq_handle.GetInst_Topology() == CSeq_inst::eTopology_circular ) {
                    circular_length = bioseq_handle.GetBioseqLength();
                }
            }
        }
        catch (exception& _DEBUG_ARG(e)) {
            _TRACE("test for circularity failed: " << e.what()) ;
        }
    }

    CRef<CSeq_loc> circular_loc;
    if (circular_id  &&  range.GetFrom() > range.GetTo()) {
        // Circular bioseq, the location crosses zero. Can't use a single
        // total range.
        circular_loc.Reset(new CSeq_loc);
        CRef<CSeq_interval> sub_loc(new CSeq_interval);
        sub_loc->SetId().Assign(*circular_id);
        sub_loc->SetFrom(0);
        sub_loc->SetTo(range.GetTo());
        if ( loc.IsSetStrand() ) {
            sub_loc->SetStrand(loc.GetStrand());
        }
        // First interval - no matter front or back
        circular_loc->SetPacked_int().Set().push_back(sub_loc);
        sub_loc.Reset(new CSeq_interval);
        sub_loc->SetId().Assign(*circular_id);
        sub_loc->SetFrom(range.GetFrom());
        sub_loc->SetTo(circular_length == kInvalidSeqPos
            ? kInvalidSeqPos : circular_length - 1);
        if ( loc.IsSetStrand() ) {
            sub_loc->SetStrand(loc.GetStrand());
        }
        if ( IsReverse(strand) ) {
            circular_loc->SetPacked_int().Set().push_front(sub_loc);
        }
        else {
            circular_loc->SetPacked_int().Set().push_back(sub_loc);
        }
    }
    try {
        SAnnotSelector sel;
        sel.SetFeatType(feat_type)
            .SetFeatSubtype(feat_subtype)
            .SetOverlapType(annot_overlap_type)
            .SetResolveTSE();
        if( opts & fBestFeat_IgnoreStrand ) {
            sel.SetIgnoreStrand();
            if( ! circular_id  &&  range.GetFrom() > range.GetTo() ) {
                // switch from and to
                range = CRange<TSeqPos>( range.GetTo(), range.GetFrom() );
            }
        }
        if( plugin ) {
            plugin->processSAnnotSelector( sel );
        }

        auto_ptr<CFeat_CI> feat_it_ptr;
        if( plugin ) {
            plugin->setUpFeatureIterator( bioseq_handle, feat_it_ptr,
                circular_length, range, loc, sel, scope, strand);
        } else {
            if ( circular_loc ) {
                feat_it_ptr.reset( new CFeat_CI(scope, *circular_loc, sel) );
            }
            else if ( bioseq_handle ) {
                feat_it_ptr.reset( new CFeat_CI(bioseq_handle, range, strand, sel) );
            }
            else {
                feat_it_ptr.reset( new CFeat_CI(scope, loc, sel) );
            }
        }
        // convenience variable so we don't have to keep dereferencing the auto_ptr
        CFeat_CI &feat_it = *feat_it_ptr;

        CRef<CSeq_loc> cleaned_loc( new CSeq_loc );
        cleaned_loc->Assign( loc );
        if( opts & fBestFeat_IgnoreStrand ) {
            cleaned_loc->SetStrand(eNa_strand_plus);
        }
        if( plugin ) {
            plugin->processLoc( bioseq_handle, cleaned_loc, circular_length );
        }

        for ( ;  feat_it;  ++feat_it) {
            CRef<CSeq_loc> cleaned_loc_this_iteration = cleaned_loc;
            CRef<CSeq_loc> candidate_feat_loc( new CSeq_loc ); 
            candidate_feat_loc->Assign( feat_it->GetOriginalFeature().GetLocation() );
            if( opts & fBestFeat_IgnoreStrand ) {
                candidate_feat_loc->SetStrand(eNa_strand_plus);
            }
            EOverlapType overlap_type_this_iteration = overlap_type;
            bool revert_locations_this_iteration = revert_locations;

            if( plugin ) {
                bool shouldContinueToNextIteration = false;
                plugin->processMainLoop( 
                    shouldContinueToNextIteration,
                    cleaned_loc_this_iteration,
                    candidate_feat_loc,
                    overlap_type_this_iteration,
                    revert_locations_this_iteration,
                    bioseq_handle,
                    *feat_it,
                    circular_length,
                    annot_overlap_type);
                if( shouldContinueToNextIteration ) {
                    continue;
                }
            }

            // treat subset as a special case
            Int8 cur_diff = ( !revert_locations_this_iteration ) ?
                TestForOverlap64(*candidate_feat_loc,
                *cleaned_loc_this_iteration,
                overlap_type_this_iteration,
                circular_length,
                &scope) :
            TestForOverlap64(*cleaned_loc_this_iteration,
                *candidate_feat_loc,
                overlap_type_this_iteration,
                circular_length,
                &scope);
            if( plugin ) {
                plugin->postProcessDiffAmount( cur_diff, cleaned_loc_this_iteration, 
                    candidate_feat_loc, scope, sel, circular_length );
            }
            if (cur_diff < 0) {
                continue;
            }

            TFeatScore sc(cur_diff,
                ConstRef(&feat_it->GetMappedFeature()));
            feats.push_back(sc);
        }
    }
    catch (exception&) {
        _TRACE("GetOverlappingFeatures(): error: feature iterator failed");
    }

    std::stable_sort(feats.begin(), feats.end(),
        COverlapPairLess( &scope ) );
}


CConstRef<CSeq_feat> GetBestOverlappingFeat(const CSeq_loc& loc,
                                            CSeqFeatData::E_Choice feat_type,
                                            EOverlapType overlap_type,
                                            CScope& scope,
                                            TBestFeatOpts opts,
                                            CGetOverlappingFeaturesPlugin *plugin )
{
    TFeatScores scores;
    GetOverlappingFeatures(loc,
                           feat_type, CSeqFeatData::eSubtype_any,
                           overlap_type, scores, scope, opts, plugin );
    if (scores.size()) {
        if (opts & fBestFeat_FavorLonger) {
            return scores.back().second;
        } else {
            return scores.front().second;
        }
    }
    return CConstRef<CSeq_feat>();
}


CConstRef<CSeq_feat> GetBestOverlappingFeat(const CSeq_loc& loc,
                                            CSeqFeatData::ESubtype feat_type,
                                            EOverlapType overlap_type,
                                            CScope& scope,
                                            TBestFeatOpts opts,
                                            CGetOverlappingFeaturesPlugin *plugin )
{
    TFeatScores scores;
    GetOverlappingFeatures(loc,
        CSeqFeatData::GetTypeFromSubtype(feat_type), feat_type,
        overlap_type, scores, scope, opts, plugin );

    if (scores.size()) {
        if (opts & fBestFeat_FavorLonger) {
            return scores.back().second;
        } else {
            return scores.front().second;
        }
    }
    return CConstRef<CSeq_feat>();
}


static
CConstRef<CSeq_feat> x_GetBestOverlapForSNP(const CSeq_feat& snp_feat,
                                            CSeqFeatData::E_Choice type,
                                            CSeqFeatData::ESubtype subtype,
                                            CScope& scope,
                                            bool search_both_strands = true)
{
    TFeatScores scores;
    CConstRef<CSeq_feat> overlap;
    GetOverlappingFeatures(snp_feat.GetLocation(),
                           type, subtype,
                           eOverlap_Contained, scores,
                           scope);
    if (scores.size()) {
        overlap = scores.front().second;
    }

    if (search_both_strands  &&  !overlap) {
        CRef<CSeq_loc> loc(new CSeq_loc);
        loc->Assign(snp_feat.GetLocation());

        ENa_strand strand = GetStrand(*loc, &scope);
        if (strand == eNa_strand_plus  ||  strand == eNa_strand_minus) {
            loc->FlipStrand();
        } else if (strand == eNa_strand_unknown) {
            loc->SetStrand(eNa_strand_minus);
        }

        scores.clear();
        GetOverlappingFeatures(*loc,
                               type, subtype,
                               eOverlap_Contained, scores,
                               scope);
        if (scores.size()) {
            overlap = scores.front().second;
        }
    }

    return overlap;
}


CConstRef<CSeq_feat> GetBestOverlapForSNP(const CSeq_feat& snp_feat,
                                          CSeqFeatData::E_Choice type,
                                          CScope& scope,
                                          bool search_both_strands)
{
    return x_GetBestOverlapForSNP(snp_feat, type, CSeqFeatData::eSubtype_any,
                                  scope, search_both_strands);
}


CConstRef<CSeq_feat> GetBestOverlapForSNP(const CSeq_feat& snp_feat,
                                          CSeqFeatData::ESubtype subtype,
                                          CScope& scope,
                                          bool search_both_strands)
{
    return x_GetBestOverlapForSNP(snp_feat,
        CSeqFeatData::GetTypeFromSubtype(subtype), subtype, scope,
        search_both_strands);
}


CConstRef<CSeq_feat> GetOverlappingGene(
    const CSeq_loc& loc, CScope& scope,
    ETransSplicing eTransSplicing )
{
    return GetBestOverlappingFeat(
        loc, CSeqFeatData::eSubtype_gene,
        eOverlap_Contained, scope,
        ( eTransSplicing == eTransSplicing_Yes ? fBestFeat_IgnoreStrand : 0 ) );
}


CConstRef<CSeq_feat> GetOverlappingmRNA(const CSeq_loc& loc, CScope& scope)
{
    return GetBestOverlappingFeat(loc, CSeqFeatData::eSubtype_mRNA,
                                  eOverlap_Contained, scope);
}


CConstRef<CSeq_feat> GetOverlappingCDS(const CSeq_loc& loc, CScope& scope)
{
    return GetBestOverlappingFeat(loc, CSeqFeatData::eSubtype_cdregion,
                                  eOverlap_Contained, scope);
}


CConstRef<CSeq_feat> GetOverlappingPub(const CSeq_loc& loc, CScope& scope)
{
    return GetBestOverlappingFeat(loc, CSeqFeatData::eSubtype_pub,
                                  eOverlap_Contained, scope);
}


CConstRef<CSeq_feat> GetOverlappingSource(const CSeq_loc& loc, CScope& scope)
{
    return GetBestOverlappingFeat(loc, CSeqFeatData::eSubtype_biosrc,
                                  eOverlap_Contained, scope);
}


CConstRef<CSeq_feat> GetOverlappingOperon(const CSeq_loc& loc, CScope& scope)
{
    return GetBestOverlappingFeat(loc, CSeqFeatData::eSubtype_operon,
                                  eOverlap_Contained, scope);
}


const char* kRibosomalSlippageText = "ribosomal slippage";

CConstRef<CSeq_feat> GetBestMrnaForCds(const CSeq_feat& cds_feat,
                                       CScope& scope,
                                       TBestFeatOpts opts,
                                       CGetOverlappingFeaturesPlugin *plugin )
{
    _ASSERT(cds_feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_cdregion);
    CConstRef<CSeq_feat> mrna_feat;

    // search for a best overlapping mRNA
    // we start with a scan through the product accessions because we need
    // to insure that the chosen transcript does indeed match what we want
    TFeatScores feats;
    EOverlapType overlap_type = eOverlap_CheckIntRev;
    if (cds_feat.IsSetExcept()  &&  cds_feat.GetExcept()  &&
        cds_feat.IsSetExcept_text()  &&
        cds_feat.GetExcept_text() == kRibosomalSlippageText) {
        overlap_type = eOverlap_SubsetRev;
    }
    GetOverlappingFeatures(cds_feat.GetLocation(),
                           CSeqFeatData::e_Rna,
                           CSeqFeatData::eSubtype_mRNA,
                           overlap_type,
                           feats, scope, opts, plugin );
    /// easy out: 0 or 1 possible features
    if (feats.size() < 2) {
        if (feats.size() == 1) {
            mrna_feat = feats.front().second;
        }
        return mrna_feat;
    }

    if (cds_feat.IsSetProduct()) {
        try {
            // this may throw, if the product spans multiple sequences
            // this would be extremely unlikely, but we catch anyway
            const CSeq_id& product_id =
                sequence::GetId(cds_feat.GetProduct(), &scope);

            ITERATE (TFeatScores, feat_iter, feats) {
                const CSeq_feat& feat = *feat_iter->second;
                if ( !feat.IsSetExt() ) {
                    continue;
                }

                /// scan the user object in the ext field
                /// we look for a user object of type MrnaProteinLink
                /// this should contain a seq-d string that we can match
                CTypeConstIterator<CUser_object> obj_iter(feat);
                for ( ;  obj_iter;  ++obj_iter) {
                    if (obj_iter->IsSetType()  &&
                        obj_iter->GetType().IsStr()  &&
                        obj_iter->GetType().GetStr() == "MrnaProteinLink") {
                        string prot_id_str = obj_iter->GetField("protein seqID")
                            .GetData().GetStr();
                        CSeq_id prot_id(prot_id_str);
                        vector<CSeq_id_Handle> ids = scope.GetIds(prot_id);
                        ids.push_back(CSeq_id_Handle::GetHandle(prot_id));
                        ITERATE (vector<CSeq_id_Handle>, id_iter, ids) {
                            if (product_id.Match(*id_iter->GetSeqId())) {
                                mrna_feat.Reset(&feat);
                                return mrna_feat;
                            }
                        }
                    }
                }
            }
        }
        catch (exception&) {
        }
    }

    if (cds_feat.IsSetProduct()  &&  !(opts & fBestFeat_NoExpensive) ) {
        try {
            // this may throw, if the product spans multiple sequences
            // this would be extremely unlikely, but we catch anyway
            const CSeq_id& product_id =
                sequence::GetId(cds_feat.GetProduct(), &scope);

            TFeatScores matching_feats;
            ITERATE (TFeatScores, feat_iter, feats) {

                // we grab the mRNA product, if available, and scan it for
                // a CDS feature.  the CDS feature should point to the same
                // product as our current feature.
                const CSeq_feat& mrna = *feat_iter->second;
                if ( !mrna.IsSetProduct() ) {
                    continue;
                }

                CBioseq_Handle handle =
                    scope.GetBioseqHandle(mrna.GetProduct());
                if ( !handle ) {
                    continue;
                }

                SAnnotSelector cds_sel;
                cds_sel.SetOverlapIntervals()
                    .ExcludeNamedAnnots("SNP")
                    .SetResolveTSE()
                    .SetFeatSubtype(CSeqFeatData::eSubtype_cdregion);
                CFeat_CI other_iter(scope, mrna.GetProduct(), cds_sel);
                for ( ;  other_iter  &&  !mrna_feat;  ++other_iter) {
                    const CSeq_feat& cds = other_iter->GetOriginalFeature();
                    if ( !cds.IsSetProduct() ) {
                        continue;
                    }

                    CBioseq_Handle prot_handle =
                        scope.GetBioseqHandle(cds.GetProduct());
                    if ( !prot_handle ) {
                        continue;
                    }

                    if (prot_handle.IsSynonym(product_id)) {
                        // got it!
                        matching_feats.push_back(*feat_iter);
                        break;
                    }
                }
            }
            if ( !matching_feats.empty() ) {
                // keep only matching features
                feats.swap(matching_feats);
                if ( feats.size() == 1 ) {
                    mrna_feat = feats.front().second;
                    return mrna_feat;
                }
            }
        }
        catch (exception&) {
        }
    }

    // check for transcript_id; this is a fast check
    string transcript_id = cds_feat.GetNamedQual("transcript_id");
    if ( !transcript_id.empty() ) {
        ITERATE (vector<TFeatScore>, feat_iter, feats) {
            const CSeq_feat& feat = *feat_iter->second;
            string other_transcript_id =
                feat.GetNamedQual("transcript_id");
            if (transcript_id == other_transcript_id) {
                mrna_feat.Reset(&feat);
                return mrna_feat;
            }
        }
    }

    //
    // try to find the best by overlaps alone
    //

    if ( !mrna_feat  &&  !(opts & fBestFeat_StrictMatch) ) {
        if (opts & fBestFeat_FavorLonger) {
            mrna_feat = feats.back().second;
        } else {
            mrna_feat = feats.front().second;
        }
    }

    return mrna_feat;
}


// Plugin for GetOverlappingFeatures - uses eOverlap_CheckIntervals
// or eOverlap_Subset depending on the "ribosomal slippage" flag
// in the current feature.

class CCdsForMrnaPlugin : public CGetOverlappingFeaturesPlugin
{
public:
    CCdsForMrnaPlugin(CGetOverlappingFeaturesPlugin* prev_plugin)
        : m_PrevPlugin(prev_plugin) {}
    virtual ~CCdsForMrnaPlugin() {}

    virtual void processSAnnotSelector(
        SAnnotSelector &sel)
    {
        if ( m_PrevPlugin ) {
            m_PrevPlugin->processSAnnotSelector(sel);
        }
    }

    virtual void setUpFeatureIterator(
        CBioseq_Handle &bioseq_handle,
        auto_ptr<CFeat_CI> &feat_ci,
        TSeqPos circular_length ,
        CRange<TSeqPos> &range,
        const CSeq_loc& loc,
        SAnnotSelector &sel,
        CScope &scope,
        ENa_strand &strand)
    {
        if ( m_PrevPlugin ) {
            m_PrevPlugin->setUpFeatureIterator(bioseq_handle,
                feat_ci, circular_length, range, loc, sel, scope, strand);
            return;
        }
        if ( bioseq_handle ) {
            feat_ci.reset(new CFeat_CI(bioseq_handle, range, strand, sel));
        } else {
            feat_ci.reset(new CFeat_CI(scope, loc, sel));
        }
    }

    virtual void processLoc(
        CBioseq_Handle &bioseq_handle,
        CRef<CSeq_loc> &loc,
        TSeqPos circular_length)
    {
        if ( m_PrevPlugin ) {
            m_PrevPlugin->processLoc(bioseq_handle, loc, circular_length);
        }
    }

    virtual void processMainLoop(
        bool &shouldContinueToNextIteration,
        CRef<CSeq_loc> &cleaned_loc_this_iteration, 
        CRef<CSeq_loc> &candidate_feat_loc,
        EOverlapType &overlap_type_this_iteration,
        bool &revert_locations_this_iteration,
        CBioseq_Handle &bioseq_handle,
        const CMappedFeat &feat,
        TSeqPos circular_length,
        SAnnotSelector::EOverlapType annot_overlap_type)
    {
        const CSeq_feat& cds = feat.GetOriginalFeature();
        _ASSERT(cds.GetData().GetSubtype() ==
            CSeqFeatData::eSubtype_cdregion);
        // If the feature has "ribosomal slippage" flag set, use
        // eOverlap_Subset. Otherwise use more strict eOverlap_CheckIntervals.
        if (cds.IsSetExcept()  &&  cds.GetExcept()  &&
            cds.IsSetExcept_text()  &&
            cds.GetExcept_text() == kRibosomalSlippageText) {
            overlap_type_this_iteration = eOverlap_Subset;
        }
        if ( m_PrevPlugin ) {
            m_PrevPlugin->processMainLoop(shouldContinueToNextIteration,
                cleaned_loc_this_iteration, candidate_feat_loc, 
                overlap_type_this_iteration,
                revert_locations_this_iteration,
                bioseq_handle, feat, circular_length, annot_overlap_type);
        }
    }

    virtual void postProcessDiffAmount( 
        Int8 &cur_diff, 
        CRef<CSeq_loc> &cleaned_loc, 
        CRef<CSeq_loc> &candidate_feat_loc, 
        CScope &scope, 
        SAnnotSelector &sel, 
        TSeqPos circular_length ) 
    {
        if ( m_PrevPlugin ) {
            m_PrevPlugin->postProcessDiffAmount(cur_diff, 
                cleaned_loc, candidate_feat_loc, 
                scope, sel, circular_length);
        }
    }

private:
    CGetOverlappingFeaturesPlugin* m_PrevPlugin;
};


CConstRef<CSeq_feat>
GetBestCdsForMrna(const CSeq_feat& mrna_feat,
                  CScope& scope,
                  TBestFeatOpts opts,
                  CGetOverlappingFeaturesPlugin *plugin )
{
    _ASSERT(mrna_feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_mRNA);
    CConstRef<CSeq_feat> cds_feat;

    auto_ptr<CGetOverlappingFeaturesPlugin> cds_plugin(
        new CCdsForMrnaPlugin(plugin));
    // search for a best overlapping CDS
    // we start with a scan through the product accessions because we need
    // to insure that the chosen transcript does indeed match what we want
    TFeatScores feats;
    GetOverlappingFeatures(mrna_feat.GetLocation(),
                           CSeqFeatData::e_Cdregion,
                           CSeqFeatData::eSubtype_cdregion,
                           eOverlap_CheckIntervals,
                           feats, scope, opts, cds_plugin.get());

    /// easy out: 0 or 1 possible features
    if (feats.size() < 2) {
        if (feats.size() == 1) {
            cds_feat = feats.front().second;
        }
        return cds_feat;
    }

    if (mrna_feat.IsSetExt()) {
        /// scan the user object in the ext field
        /// we look for a user object of type MrnaProteinLink
        /// this should contain a seq-d string that we can match
        string prot_id_str;
        CTypeConstIterator<CUser_object> obj_iter(mrna_feat);
        for ( ;  obj_iter;  ++obj_iter) {
            if (obj_iter->IsSetType()  &&
                obj_iter->GetType().IsStr()  &&
                obj_iter->GetType().GetStr() == "MrnaProteinLink") {
                string prot_id_str = obj_iter->GetField("protein seqID")
                    .GetData().GetStr();
                break;
            }
        }
        if ( !prot_id_str.empty() ) {
            CSeq_id prot_id(prot_id_str);
            vector<CSeq_id_Handle> ids = scope.GetIds(prot_id);
            ids.push_back(CSeq_id_Handle::GetHandle(prot_id));

            try {
                /// look for a CDS feature that matches this expected ID
                ITERATE (TFeatScores, feat_iter, feats) {
                    const CSeq_feat& feat = *feat_iter->second;
                    if ( !feat.IsSetProduct() ) {
                        continue;
                    }
                    const CSeq_id& id =
                        sequence::GetId(feat.GetLocation(), &scope);
                    ITERATE (vector<CSeq_id_Handle>, id_iter, ids) {
                        if (id.Match(*id_iter->GetSeqId())) {
                            cds_feat.Reset(&feat);
                            return cds_feat;
                        }
                    }
                }
            }
            catch (exception&) {
            }
        }
    }

    // scan through the product accessions because we need to insure that the
    // chosen transcript does indeed match what we want
    if (mrna_feat.IsSetProduct()  &&  !(opts & fBestFeat_NoExpensive) ) {
        do {
            try {
                // this may throw, if the product spans multiple sequences
                // this would be extremely unlikely, but we catch anyway
                const CSeq_id& mrna_product  =
                    sequence::GetId(mrna_feat.GetProduct(), &scope);
                CBioseq_Handle mrna_handle =
                    scope.GetBioseqHandle(mrna_product);

                // find the ID of the protein accession we're looking for
                CConstRef<CSeq_id> protein_id;
                {{
                    SAnnotSelector sel;
                    sel.SetOverlapIntervals()
                        .ExcludeNamedAnnots("SNP")
                        .SetResolveTSE()
                        .SetFeatSubtype(CSeqFeatData::eSubtype_cdregion);

                     CFeat_CI iter(mrna_handle, sel);
                     for ( ;  iter;  ++iter) {
                         if (iter->IsSetProduct()) {
                             protein_id.Reset
                                 (&sequence::GetId(iter->GetProduct(),
                                 &scope));
                             break;
                         }
                     }
                 }}

                if ( !protein_id ) {
                    break;
                }

                TFeatScores::const_iterator feat_iter = feats.begin();
                TFeatScores::const_iterator feat_end  = feats.end();
                for ( ;  feat_iter != feat_end  &&  !cds_feat;  ++feat_iter) {
                    /// look for all contained CDS features; for each, check
                    /// to see if the protein product is the expected protein
                    /// product
                    const CSeq_feat& cds = *feat_iter->second;
                    if ( !cds.IsSetProduct() ) {
                        continue;
                    }

                    CBioseq_Handle prot_handle =
                        scope.GetBioseqHandle(cds.GetProduct());
                    if ( !prot_handle ) {
                        continue;
                    }

                    if (prot_handle.IsSynonym(*protein_id)) {
                        // got it!
                        cds_feat.Reset(&cds);
                        return cds_feat;
                    }
                }
            }
            catch ( exception& ) {
            }
        }
        while (false);
    }

    // check for transcript_id
    // this is generally only available in GTF/GFF-imported features
    string transcript_id = mrna_feat.GetNamedQual("transcript_id");
    if ( !transcript_id.empty() ) {
        ITERATE (TFeatScores, feat_iter, feats) {
            const CSeq_feat& feat = *feat_iter->second;
            string other_transcript_id =
                feat.GetNamedQual("transcript_id");
            if (transcript_id == other_transcript_id) {
                cds_feat.Reset(&feat);
                return cds_feat;
            }
        }
    }

    //
    // try to find the best by overlaps alone
    //

    if ( !cds_feat  &&  !(opts & fBestFeat_StrictMatch) ) {
        if (opts & fBestFeat_FavorLonger) {
            cds_feat = feats.back().second;
        } else {
            cds_feat = feats.front().second;
        }
    }

    return cds_feat;
}


CConstRef<CSeq_feat> GetBestGeneForMrna(const CSeq_feat& mrna_feat,
                                          CScope& scope,
                                          TBestFeatOpts opts,
                                          CGetOverlappingFeaturesPlugin *plugin )
{
    _ASSERT(mrna_feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_mRNA);
    CConstRef<CSeq_feat> gene_feat;

    // search for a best overlapping gene
    TFeatScores feats;
    GetOverlappingFeatures(mrna_feat.GetLocation(),
                           CSeqFeatData::e_Gene,
                           CSeqFeatData::eSubtype_any,
                           eOverlap_Contained,
                           feats, scope, opts, plugin );
    /// easy out: 0 or 1 possible features
    if (feats.size() < 2) {
        if (feats.size() == 1) {
            gene_feat = feats.front().second;
        }
        return gene_feat;
    }

    ///
    /// compare gene xrefs to see if ew can find a match
    ///
    const CGene_ref* ref = mrna_feat.GetGeneXref();
    if (ref) {
        if (ref->IsSuppressed()) {
            /// 'suppress' case
            return gene_feat;
        }

        string ref_str;
        ref->GetLabel(&ref_str);

        ITERATE (TFeatScores, feat_it, feats) {
            const CSeq_feat& feat      = *feat_it->second;
            const CGene_ref& other_ref = feat.GetData().GetGene();
            string other_ref_str;
            other_ref.GetLabel(&other_ref_str);
            if (ref_str == other_ref_str) {
                gene_feat = &feat;
                return gene_feat;
            }
        }
    }

    ///
    /// compare by dbxrefs
    ///
    if (mrna_feat.IsSetDbxref()) {
        int gene_id = 0;
        ITERATE (CSeq_feat::TDbxref, dbxref, mrna_feat.GetDbxref()) {
            if ((*dbxref)->GetDb() == "GeneID"  ||
                (*dbxref)->GetDb() == "LocusID") {
                gene_id = (*dbxref)->GetTag().GetId();
                break;
            }
        }

        if (gene_id != 0) {
            ITERATE (TFeatScores, feat_it, feats) {
                const CSeq_feat& feat = *feat_it->second;
                ITERATE (CSeq_feat::TDbxref, dbxref, feat.GetDbxref()) {
                    const string& db = (*dbxref)->GetDb();
                    if ((db == "GeneID"  ||  db == "LocusID")  &&
                        (*dbxref)->GetTag().GetId() == gene_id) {
                        gene_feat = &feat;
                        return gene_feat;
                    }
                }
            }
        }
    }

    if ( !gene_feat  &&  !(opts & fBestFeat_StrictMatch) ) {
        if (opts & fBestFeat_FavorLonger) {
            gene_feat = feats.back().second;
        } else {
            gene_feat = feats.front().second;
        }
    }

    return gene_feat;
}


CConstRef<CSeq_feat> GetBestGeneForCds(const CSeq_feat& cds_feat,
                                         CScope& scope,
                                         TBestFeatOpts opts,
                                         CGetOverlappingFeaturesPlugin *plugin )
{
    _ASSERT(cds_feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_cdregion);

    CConstRef<CSeq_feat> feat_ref;

    // search for a best overlapping gene
    TFeatScores feats;
    GetOverlappingFeatures(cds_feat.GetLocation(),
                           CSeqFeatData::e_Gene,
                           CSeqFeatData::eSubtype_any,
                           eOverlap_Contained,
                           feats, scope, opts, plugin );
    /// easy out: 0 or 1 possible features
    if (feats.size() < 2) {
        if (feats.size() == 1) {
            feat_ref = feats.front().second;
        }
        return feat_ref;
    }

    // next: see if we can match based on gene xref
    const CGene_ref* ref = cds_feat.GetGeneXref();
    if (ref) {
        if (ref->IsSuppressed()) {
            /// 'suppress' case
            return feat_ref;
        }

        string ref_str;
        ref->GetLabel(&ref_str);

        ITERATE (TFeatScores, feat_it, feats) {
            const CSeq_feat& feat = *feat_it->second;

            string ref_str;
            ref->GetLabel(&ref_str);

            const CGene_ref& other_ref = feat.GetData().GetGene();
            string other_ref_str;
            other_ref.GetLabel(&other_ref_str);
            if (ref_str == other_ref_str) {
                feat_ref = &feat;
                return feat_ref;
            }
        }
    }

    /// last check: expensive: need to proxy through mRNA match
    if ( !feat_ref  &&  !(opts & fBestFeat_NoExpensive) ) {
        feat_ref = GetBestMrnaForCds(cds_feat, scope,
                                     opts | fBestFeat_StrictMatch);
        if (feat_ref) {
            feat_ref = GetBestGeneForMrna(*feat_ref, scope, opts);
            if (feat_ref) {
                return feat_ref;
            }
        }
    }

    if ( !feat_ref  &&  !(opts & fBestFeat_StrictMatch) ) {
        feat_ref = feats.front().second;
    }
    return feat_ref;
}


void GetMrnasForGene(const CSeq_feat& gene_feat, CScope& scope,
                     list< CConstRef<CSeq_feat> >& mrna_feats,
                     TBestFeatOpts opts,
                     CGetOverlappingFeaturesPlugin *plugin )
{
    _ASSERT(gene_feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_gene);
    SAnnotSelector sel;
    sel.SetResolveTSE()
        .SetAdaptiveDepth()
        .IncludeFeatSubtype(CSeqFeatData::eSubtype_mRNA);
    CFeat_CI feat_it(scope, gene_feat.GetLocation(), sel);
    if (feat_it.GetSize() == 0) {
        return;
    }

    ///
    /// pass 1: compare by gene xref
    ///
    {{
         const CGene_ref& ref = gene_feat.GetData().GetGene();
         string ref_str;
         ref.GetLabel(&ref_str);
         size_t count = 0;
         for ( ;  feat_it;  ++feat_it) {

             const CGene_ref* other_ref =
                 feat_it->GetOriginalFeature().GetGeneXref();
             if ( !other_ref  ||  other_ref->IsSuppressed() ) {
                 continue;
             }

             string other_ref_str;
             other_ref->GetLabel(&other_ref_str);
             if (other_ref_str != ref_str) {
                 continue;
             }

             ECompare comp = sequence::Compare(gene_feat.GetLocation(),
                                               feat_it->GetLocation(),
                                               &scope);
             if (comp != eSame  &&  comp != eContains) {
                 continue;
             }

             CConstRef<CSeq_feat> feat_ref(&feat_it->GetOriginalFeature());
             mrna_feats.push_back(feat_ref);
             ++count;
         }

         if (count) {
             return;
         }
     }}

    ///
    /// pass 2: compare by gene id
    ///
    {{
        int gene_id = 0;
        if (gene_feat.IsSetDbxref()) {
            ITERATE (CSeq_feat::TDbxref, dbxref, gene_feat.GetDbxref()) {
                if ((*dbxref)->GetDb() == "GeneID"  ||
                    (*dbxref)->GetDb() == "LocusID") {
                    gene_id = (*dbxref)->GetTag().GetId();
                    break;
                }
            }
        }

        if (gene_id) {
            size_t count = 0;
            feat_it.Rewind();
            for ( ;  feat_it;  ++feat_it) {
                /// check the suppress case
                /// regardless of the gene-id binding, we always ignore these
                const CGene_ref* other_ref =
                    feat_it->GetOriginalFeature().GetGeneXref();
                if ( other_ref  &&  other_ref->IsSuppressed() ) {
                    continue;
                }

                CConstRef<CSeq_feat> ref(&feat_it->GetOriginalFeature());

                ECompare comp = sequence::Compare(gene_feat.GetLocation(),
                                                feat_it->GetLocation(),
                                                &scope);
                if (comp != eSame  &&  comp != eContains) {
                    continue;
                }

                if (feat_it->IsSetDbxref()) {
                    ITERATE (CSeq_feat::TDbxref, dbxref, feat_it->GetDbxref()) {
                        if (((*dbxref)->GetDb() == "GeneID"  ||
                            (*dbxref)->GetDb() == "LocusID")  &&
                            (*dbxref)->GetTag().GetId() == gene_id) {
                            mrna_feats.push_back(ref);
                            ++count;
                            break;
                        }
                    }
                }
            }

            if (count) {
                return;
            }
        }
    }}

    // gene doesn't have a gene_id or a gene ref
    CConstRef<CSeq_feat> feat =
        sequence::GetBestOverlappingFeat(gene_feat.GetLocation(),
                                         CSeqFeatData::eSubtype_mRNA,
                                         sequence::eOverlap_Contains,
                                         scope, opts, plugin );
    if (feat) {
        mrna_feats.push_back(feat);
    }
}


void GetCdssForGene(const CSeq_feat& gene_feat, CScope& scope,
                    list< CConstRef<CSeq_feat> >& cds_feats,
                    TBestFeatOpts opts,
                    CGetOverlappingFeaturesPlugin *plugin )
{
    _ASSERT(gene_feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_gene);
    list< CConstRef<CSeq_feat> > mrna_feats;
    GetMrnasForGene(gene_feat, scope, mrna_feats, opts);
    if (mrna_feats.size()) {
        ITERATE (list< CConstRef<CSeq_feat> >, iter, mrna_feats) {
            CConstRef<CSeq_feat> cds = GetBestCdsForMrna(**iter, scope, opts);
            if (cds) {
                cds_feats.push_back(cds);
            }
        }
    } else {
        CConstRef<CSeq_feat> feat =
            sequence::GetBestOverlappingFeat(gene_feat.GetLocation(),
                                             CSeqFeatData::eSubtype_cdregion,
                                             sequence::eOverlap_CheckIntervals,
                                             scope, opts, plugin );
        if (feat) {
            cds_feats.push_back(feat);
        }
    }
}


CConstRef<CSeq_feat>
GetBestOverlappingFeat(const CSeq_feat& feat,
                       CSeqFeatData::E_Choice feat_type,
                       sequence::EOverlapType overlap_type,
                       CScope& scope,
                       TBestFeatOpts opts,
                       CGetOverlappingFeaturesPlugin *plugin )
{
    CConstRef<CSeq_feat> feat_ref;
    switch (feat_type) {
    case CSeqFeatData::e_Gene:
        return GetBestOverlappingFeat(feat,
                                      CSeqFeatData::eSubtype_gene,
                                      overlap_type, scope, opts, plugin );

    case CSeqFeatData::e_Rna:
        feat_ref = GetBestOverlappingFeat(feat,
                                          CSeqFeatData::eSubtype_mRNA,
                                          overlap_type, scope, opts, plugin );
        break;

    case CSeqFeatData::e_Cdregion:
        return GetBestOverlappingFeat(feat,
                                      CSeqFeatData::eSubtype_cdregion,
                                      overlap_type, scope, opts, plugin );

    default:
        break;
    }

    if ( !feat_ref ) {
        feat_ref = sequence::GetBestOverlappingFeat
            (feat.GetLocation(), feat_type, overlap_type, scope, opts, plugin );
    }

    return feat_ref;
}


CConstRef<CSeq_feat>
GetBestOverlappingFeat(const CSeq_feat& feat,
                       CSeqFeatData::ESubtype subtype,
                       sequence::EOverlapType overlap_type,
                       CScope& scope,
                       TBestFeatOpts opts,
                       CGetOverlappingFeaturesPlugin *plugin )
{
    CConstRef<CSeq_feat> feat_ref;
    switch (feat.GetData().GetSubtype()) {
    case CSeqFeatData::eSubtype_mRNA:
        switch (subtype) {
        case CSeqFeatData::eSubtype_gene:
            return GetBestGeneForMrna(feat, scope, opts);

        case CSeqFeatData::eSubtype_cdregion:
            return GetBestCdsForMrna(feat, scope, opts);

        default:
            break;
        }
        break;

    case CSeqFeatData::eSubtype_cdregion:
        switch (subtype) {
        case CSeqFeatData::eSubtype_mRNA:
            return GetBestMrnaForCds(feat, scope, opts);

        case CSeqFeatData::eSubtype_gene:
            return GetBestGeneForCds(feat, scope, opts);

        default:
            break;
        }
        break;

    case CSeqFeatData::eSubtype_variation:
        return GetBestOverlapForSNP(feat, subtype, scope, true);

    default:
        break;
    }

    if ( !feat_ref ) {
        feat_ref = GetBestOverlappingFeat
            (feat.GetLocation(), subtype, overlap_type, scope, opts, plugin );
    }

    return feat_ref;
}


namespace {

void x_GetFeatsById(CSeqFeatData::ESubtype subtype,
                    const CSeq_feat& feat,
                    const CTSE_Handle& tse,
                    list< CConstRef<CSeq_feat> >& result)
{
    vector<CSeq_feat_Handle> handles;
    if ( feat.IsSetXref() ) {
        ITERATE ( CSeq_feat::TXref, it, feat.GetXref() ) {
            const CSeqFeatXref& xref = **it;
            if ( xref.IsSetId() ) {
                const CFeat_id& id = xref.GetId();
                if ( id.IsLocal() ) {
                    const CObject_id& obj_id = id.GetLocal();
                    if ( obj_id.IsId() ) {
                        int local_id = obj_id.GetId();
                        handles = tse.GetFeaturesWithId(subtype, local_id);
                        ITERATE( vector<CSeq_feat_Handle>, feat_it, handles ) {
                            result.push_back(feat_it->GetSeq_feat());
                        }
                    }
                }
            }
        }
    }
}


void x_GetFeatsByXref(CSeqFeatData::ESubtype subtype,
                      const CFeat_id& id,
                      const CTSE_Handle& tse,
                      list< CConstRef<CSeq_feat> >& result)
{
    if ( id.IsLocal() ) {
        const CObject_id& obj_id = id.GetLocal();
        if ( obj_id.IsId() ) {
            int local_id = obj_id.GetId();
            vector<CSeq_feat_Handle> handles =
                tse.GetFeaturesWithId(subtype, local_id);
            ITERATE( vector<CSeq_feat_Handle>, feat_it, handles ) {
                result.push_back(feat_it->GetSeq_feat());
            }
        }
    }
}

void x_GetFeatsByXref(CSeqFeatData::ESubtype subtype,
                      const CSeq_feat& feat,
                      const CTSE_Handle& tse,
                      list< CConstRef<CSeq_feat> >& result)
{
    if ( feat.IsSetId() ) {
        x_GetFeatsByXref(subtype, feat.GetId(), tse, result);
    }
    if ( feat.IsSetIds() ) {
        ITERATE ( CSeq_feat::TIds, it, feat.GetIds() ) {
            x_GetFeatsByXref(subtype, **it, tse, result);
        }
    }
}


CConstRef<CSeq_feat> x_GetFeatById(CSeqFeatData::ESubtype subtype,
                                   const CSeq_feat& feat,
                                   const CTSE_Handle& tse)
{
    if ( feat.IsSetXref() ) {
        ITERATE ( CSeq_feat::TXref, it, feat.GetXref() ) {
            const CSeqFeatXref& xref = **it;
            if ( xref.IsSetId() ) {
                const CFeat_id& id = xref.GetId();
                if ( id.IsLocal() ) {
                    const CObject_id& obj_id = id.GetLocal();
                    if ( obj_id.IsId() ) {
                        int local_id = obj_id.GetId();
                        CSeq_feat_Handle feat_handle =
                            tse.GetFeatureWithId(subtype, local_id);
                        if ( feat_handle ) {
                            return feat_handle.GetSeq_feat();
                        }
                    }
                }
            }
        }
    }
    return null;
}

}


CConstRef<CSeq_feat>
GetBestGeneForMrna(const CSeq_feat& mrna_feat,
                   const CTSE_Handle& tse,
                   TBestFeatOpts opts)
{
    _ASSERT(mrna_feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_mRNA);
    CConstRef<CSeq_feat> ret =
        x_GetFeatById(CSeqFeatData::eSubtype_gene, mrna_feat, tse);
    if ( !ret ) {
        ret = GetBestGeneForMrna(mrna_feat, tse.GetScope(), opts);
    }
    return ret;
}

CConstRef<CSeq_feat>
GetBestGeneForCds(const CSeq_feat& cds_feat,
                  const CTSE_Handle& tse,
                  TBestFeatOpts opts)
{
    _ASSERT(cds_feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_cdregion);
    CConstRef<CSeq_feat> ret =
        x_GetFeatById(CSeqFeatData::eSubtype_gene, cds_feat, tse);
    if ( !ret ) {
        ret = GetBestGeneForCds(cds_feat, tse.GetScope(), opts);
    }
    return ret;
}

CConstRef<CSeq_feat>
GetBestMrnaForCds(const CSeq_feat& cds_feat,
                  const CTSE_Handle& tse,
                  TBestFeatOpts opts)
{
    _ASSERT(cds_feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_cdregion);
    CConstRef<CSeq_feat> ret =
        x_GetFeatById(CSeqFeatData::eSubtype_mRNA, cds_feat, tse);
    if ( !ret ) {
        ret = GetBestMrnaForCds(cds_feat, tse.GetScope(), opts);
    }
    return ret;
}

CConstRef<CSeq_feat>
GetBestCdsForMrna(const CSeq_feat& mrna_feat,
                  const CTSE_Handle& tse,
                  TBestFeatOpts opts)
{
    _ASSERT(mrna_feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_mRNA);
    CConstRef<CSeq_feat> ret =
        x_GetFeatById(CSeqFeatData::eSubtype_cdregion, mrna_feat, tse);
    if ( !ret ) {
        ret = GetBestCdsForMrna(mrna_feat, tse.GetScope(), opts);
    }
    return ret;
}

void GetMrnasForGene(const CSeq_feat& gene_feat,
                     const CTSE_Handle& tse,
                     list< CConstRef<CSeq_feat> >& mrna_feats,
                     TBestFeatOpts opts)
{
    _ASSERT(gene_feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_gene);
    GetMrnasForGene(gene_feat, tse.GetScope(), mrna_feats, opts);
}

void GetCdssForGene(const CSeq_feat& gene_feat,
                    const CTSE_Handle& tse,
                    list< CConstRef<CSeq_feat> >& cds_feats,
                    TBestFeatOpts opts)
{
    _ASSERT(gene_feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_gene);
    GetCdssForGene(gene_feat, tse.GetScope(), cds_feats, opts);
}

// Get the encoding CDS feature of a given protein sequence.
const CSeq_feat* GetCDSForProduct(const CBioseq& product, CScope* scope)
{
    if ( scope == 0 ) {
        return 0;
    }

    return GetCDSForProduct(scope->GetBioseqHandle(product));
}

const CSeq_feat* GetCDSForProduct(const CBioseq_Handle& bsh)
{
    if ( bsh ) {
        CFeat_CI fi(bsh,
                    SAnnotSelector(CSeqFeatData::e_Cdregion)
                    .SetByProduct());
        if ( fi ) {
            // return the first one (should be the one packaged on the
            // nuc-prot set).
            return &(fi->GetOriginalFeature());
        }
    }

    return 0;
}

CMappedFeat GetMappedCDSForProduct(const CBioseq_Handle& bsh)
{
    if ( bsh ) {
        CFeat_CI fi(bsh,
                    SAnnotSelector(CSeqFeatData::e_Cdregion)
                    .SetByProduct());
        if ( fi ) {
            // return the first one (should be the one packaged on the
            // nuc-prot set).
            return *fi;
        }
    }

    return CMappedFeat();
}


// Get the mature peptide feature of a protein
const CSeq_feat* GetPROTForProduct(const CBioseq& product, CScope* scope)
{
    if ( scope == 0 ) {
        return 0;
    }

    return GetPROTForProduct(scope->GetBioseqHandle(product));
}

const CSeq_feat* GetPROTForProduct(const CBioseq_Handle& bsh)
{
    if ( bsh ) {
        CFeat_CI fi(bsh, SAnnotSelector(CSeqFeatData::e_Prot).SetByProduct());
        if ( fi ) {
            return &(fi->GetOriginalFeature());
        }
    }

    return 0;
}



// Get the encoding mRNA feature of a given mRNA (cDNA) bioseq.
const CSeq_feat* GetmRNAForProduct(const CBioseq& product, CScope* scope)
{
    if ( scope == 0 ) {
        return 0;
    }

    return GetmRNAForProduct(scope->GetBioseqHandle(product));
}

const CSeq_feat* GetmRNAForProduct(const CBioseq_Handle& bsh)
{
    if ( bsh ) {
        SAnnotSelector as(CSeqFeatData::eSubtype_mRNA);
        as.SetByProduct();
 
        CFeat_CI fi(bsh, as);
        if ( fi ) {
            return &(fi->GetOriginalFeature());
        }
    }

    return 0;
}


CMappedFeat GetMappedmRNAForProduct(const CBioseq_Handle& bsh)
{
    if ( bsh ) {
        CFeat_CI fi(bsh,
                    SAnnotSelector(CSeqFeatData::eSubtype_mRNA)
                    .SetByProduct());
        if ( fi ) {
            // return the first one (should be the one packaged on the
            // nuc-prot set).
            return *fi;
        }
    }

    return CMappedFeat();
}


// Get the encoding sequence of a protein
const CBioseq* GetNucleotideParent(const CBioseq& product, CScope* scope)
{
    if ( scope == 0 ) {
        return 0;
    }
    CBioseq_Handle bsh = GetNucleotideParent(scope->GetBioseqHandle(product));
    return bsh ? bsh.GetCompleteBioseq() : reinterpret_cast<const CBioseq*>(0);
}

CBioseq_Handle GetNucleotideParent(const CBioseq_Handle& bsh)
{
    // If protein use CDS to get to the encoding Nucleotide.
    // if nucleotide (cDNA) use mRNA feature.
    const CSeq_feat* sfp = bsh.GetInst().IsAa() ?
        GetCDSForProduct(bsh) : GetmRNAForProduct(bsh);

    CBioseq_Handle ret;
    if ( sfp ) {
        ret = bsh.GetScope().GetBioseqHandle(sfp->GetLocation());
    }
    return ret;
}


CBioseq_Handle GetParentForPart(const CBioseq_Handle& part)
{
    CBioseq_Handle seg;

    if (part) {
        CSeq_entry_Handle segset =
            part.GetExactComplexityLevel(CBioseq_set::eClass_segset);
        if (segset) {
            for (CSeq_entry_CI it(segset); it; ++it) {
                if (it->IsSeq()) {
                    seg = it->GetSeq();
                    break;
                }
            }
        }
    }

    return seg;
}


CRef<CBioseq> CreateBioseqFromBioseq(const CBioseq_Handle& bsh,
                                     TSeqPos from, TSeqPos to,
                                     const CSeq_id_Handle& new_seq_id,
                                     TCreateBioseqFlags opts,
                                     int delta_seq_level)
{
    CRef<CBioseq> bioseq(new CBioseq);
    CSeq_inst& inst = bioseq->SetInst();

    if (opts & fBioseq_CreateDelta) {
        ///
        /// create a delta-seq to match the base sequence in this range
        ///
        inst.SetRepr(CSeq_inst::eRepr_delta);

        SSeqMapSelector sel(CSeqMap::fDefaultFlags);
        sel.SetRange(from, to)
            .SetResolveCount(delta_seq_level);
        CSeqMap_CI map_iter(bsh, sel);

        TSeqPos bioseq_len = 0;
        for ( ;  map_iter;  ++map_iter) {
            TSeqPos seq_start = map_iter.GetRefPosition();
            TSeqPos seq_end   = map_iter.GetRefEndPosition() - 1;

            if (map_iter.GetEndPosition() > to) {
                seq_end -= map_iter.GetEndPosition() - to;
            }

            TSeqPos len = seq_end - seq_start + 1;

            switch (map_iter.GetType()) {
            case CSeqMap::eSeqGap:
                /// add a gap only for our length
                inst.SetExt().SetDelta().AddLiteral(len);
                bioseq_len += len;
                break;

            case CSeqMap::eSeqData:
                {{
                    ///
                    /// copy the data chunk
                    /// this is potentially truncated
                    ///
                    CRef<CDelta_seq> seq(new CDelta_seq);
                    seq->SetLiteral().SetLength(len);
                    CSeq_data& data = seq->SetLiteral().SetSeq_data();
                    data.Assign(map_iter.GetRefData());
                    if (len != map_iter.GetLength()) {
                        switch (data.Which()) {
                        case CSeq_data::e_Iupacna:
                            data.SetIupacna().Set().resize(len);
                            break;
                        case CSeq_data::e_Ncbi2na:
                            data.SetNcbi2na().Set()
                                .resize(len / 4 + (len % 4 ? 1 : 0));
                            break;
                        case CSeq_data::e_Ncbi4na:
                            data.SetNcbi4na().Set()
                                .resize(len / 2 + (len % 2 ? 1 : 0));
                            break;
                        case CSeq_data::e_Ncbi8na:
                            data.SetNcbi8na().Set().resize(len);
                            break;
                        case CSeq_data::e_Ncbipna:
                            data.SetNcbipna().Set().resize(len * 5);
                            break;
                        case CSeq_data::e_Iupacaa:
                            data.SetIupacaa().Set().resize(len);
                            break;
                        case CSeq_data::e_Ncbi8aa:
                            data.SetNcbi8aa().Set().resize(len);
                            break;
                        case CSeq_data::e_Ncbieaa:
                            data.SetNcbieaa().Set().resize(len);
                            break;
                        case CSeq_data::e_Ncbipaa:
                            data.SetNcbipaa().Set().resize(len * 25);
                            break;
                        case CSeq_data::e_Ncbistdaa:
                            data.SetNcbistdaa().Set().resize(len);
                            break;
                        default:
                            break;
                        }
                    }
                    inst.SetExt().SetDelta().Set().push_back(seq);
                    bioseq_len += len;
                }}
                break;

            case CSeqMap::eSeqSubMap:
                break;

            case CSeqMap::eSeqRef:
                {{
                    ///
                    /// create a segment referring to our far delta seq
                    ///
                    inst.SetExt().SetDelta()
                        .AddSeqRange(*map_iter.GetRefSeqid().GetSeqId(),
                                    seq_start, seq_end,
                                    (map_iter.GetRefMinusStrand() ?
                                    eNa_strand_minus : eNa_strand_plus));
                    bioseq_len += len;
                }}
                break;
            case CSeqMap::eSeqEnd:
                break;

            case CSeqMap::eSeqChunk:
                break;

            default:
                NCBI_THROW(CException, eUnknown,
                           "unhandled gap type in CreateBioseqFromBioseq()");
            }

            if (map_iter.GetEndPosition() > to) {
                break;
            }
        }
        inst.SetLength(bioseq_len);
    } else {
        ///
        /// just create a raw sequence
        ///
        CSeqVector vec(bsh, CBioseq_Handle::eCoding_Iupac);
        string seq;
        vec.GetSeqData(from, to, seq);
        size_t len = seq.size();
        if (bsh.IsNa()) {
            inst.SetSeq_data().SetIupacna().Set().swap(seq);
            CSeqportUtil::Pack(&inst.SetSeq_data(), len);
        } else {
            inst.SetSeq_data().SetIupacaa().Set().swap(seq);
        }
        inst.SetRepr(CSeq_inst::eRepr_raw);
        inst.SetLength(len);
    }

    /// Make sure we copy our mol-type
    inst.SetMol(bsh.GetInst_Mol());

    /// set our seq-id
    CRef<CSeq_id> id(new CSeq_id);
    id->Assign(*new_seq_id.GetSeqId());
    bioseq->SetId().push_back(id);

    ///
    /// copy descriptors, if requested
    ///
    if (opts & fBioseq_CopyDescriptors) {
        CSeqdesc_CI desc_it(bsh);
        for ( ;  desc_it;  ++desc_it) {
            CRef<CSeqdesc> desc(new CSeqdesc);
            desc->Assign(*desc_it);
            bioseq->SetDescr().Set().push_back(desc);
        }
    }

    ///
    /// copy annotations, if requested
    ///
    if (opts & fBioseq_CopyAnnot) {
        CSeq_loc from_loc;
        from_loc.SetInt().SetFrom(from);
        from_loc.SetInt().SetTo  (to);
        from_loc.SetId(*bsh.GetSeqId());

        CSeq_loc to_loc;
        to_loc.SetInt().SetFrom(0);
        to_loc.SetInt().SetTo(to - from);
        to_loc.SetId(*id);

        CSeq_loc_Mapper mapper(from_loc, to_loc, &bsh.GetScope());

        CRef<CSeq_annot> annot(new CSeq_annot);

        SAnnotSelector sel;
        sel.SetResolveAll()
            .SetResolveDepth(delta_seq_level);
        CFeat_CI feat_iter(bsh, TSeqRange(from, to), sel);
        for ( ;  feat_iter;  ++feat_iter) {
            CRef<CSeq_loc> loc = mapper.Map(feat_iter->GetLocation());
            if ( !loc ) {
                continue;
            }
            CRef<CSeq_feat> feat(new CSeq_feat);
            feat->Assign(feat_iter->GetOriginalFeature());
            feat->SetLocation(*loc);
            annot->SetData().SetFtable().push_back(feat);
        }

        if (annot->IsSetData()) {
            bioseq->SetAnnot().push_back(annot);
        }
    }

    return bioseq;
}



END_SCOPE(sequence)

CFastaOstream::CFastaOstream(CNcbiOstream& out)
    : m_Out(out),
      m_Flags(fInstantiateGaps | fAssembleParts),
      m_GapMode(eGM_letters)
{
    m_Gen.reset(new sequence::CDeflineGenerator);
    SetWidth(70);
}

CFastaOstream::~CFastaOstream()
{
    m_Out << flush;
}

void CFastaOstream::Write(const CSeq_entry_Handle& handle,
                          const CSeq_loc* location)
{
    for (CBioseq_CI it(handle);  it;  ++it) {
        if ( !SkipBioseq(*it) ) {
            if (location) {
                CSeq_loc loc2;
                loc2.SetWhole().Assign(*it->GetSeqId());
                int d = sequence::TestForOverlap
                    (*location, loc2, sequence::eOverlap_Interval,
                     kInvalidSeqPos, &handle.GetScope());
                if (d < 0) {
                    continue;
                }
            }
            Write(*it, location);
        }
    }
}


void CFastaOstream::Write(const CBioseq_Handle& handle,
                          const CSeq_loc* location,
                          const string& custom_title)
{
    WriteTitle(handle, location, custom_title);
    WriteSequence(handle, location);
}


void CFastaOstream::x_WriteSeqIds(const CBioseq& bioseq,
                                  const CSeq_loc* location)
{
    bool have_range = (location != NULL  &&  !location->IsWhole()
                       &&  !(m_Flags & fSuppressRange) );

    if ( !have_range  &&  (m_Flags & fNoDupCheck) == 0) {
        ITERATE (CBioseq::TId, id, bioseq.GetId()) {
            CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(**id);
            pair<TSeq_id_HandleSet::iterator, bool> p
                = m_PreviousWholeIds.insert(idh);
            if ( !p.second ) {
                NCBI_THROW(CObjmgrUtilException, eBadLocation,
                           "Duplicate Seq-id " + (*id)->AsFastaString()
                           + " in FASTA output");
            }
        }
    }

    m_Out << '>';
    CSeq_id::WriteAsFasta(m_Out, bioseq);

    if (have_range) {
        char delim = ':';
        for (CSeq_loc_CI it(*location);  it;  ++it) {
            CSeq_loc::TRange range = it.GetRange();
            TSeqPos from = range.GetFrom() + 1, to = range.GetTo() + 1;
            _ASSERT(from <= to);
            m_Out << delim;
            if (it.IsSetStrand()  &&  IsReverse(it.GetStrand())) {
                m_Out << 'c' << to << '-' << from;
            } else {
                m_Out << from << '-' << to;
            }
            delim = ',';
        }
    }
}

void CFastaOstream::x_WriteModifiers ( const CBioseq_Handle & handle )
{
    // print [topology=...], if necessary
    if( handle.CanGetInst_Topology() ) {
        CSeq_inst::ETopology topology = handle.GetInst_Topology();
        if( topology == CSeq_inst::eTopology_circular ) {
            m_Out << " [topology=circular]";
        }
    }

    // handle modifiers retrieved from Biosource.Org-ref
    // [organism=...], etc.

    bool organism_seen = false;
    bool strain_seen = false;
    bool gcode_seen = false;
    
    try {
        const COrg_ref & org = sequence::GetOrg_ref(handle);
        if( org.IsSetTaxname() ) {
            x_PrintStringModIfNotDup( &organism_seen, "organism", org.GetTaxname() );
        }
        if( org.IsSetOrgname() ) {
            const COrg_ref::TOrgname & orgname = org.GetOrgname();
            if( orgname.IsSetMod() ) {
                ITERATE( COrgName::TMod, mod_iter, orgname.GetMod() ) {
                    const COrgMod & mod = **mod_iter;
                    if( mod.IsSetSubtype() ) {
                        switch( mod.GetSubtype() ) {
                            case COrgMod::eSubtype_strain:
                                if( mod.IsSetSubname() ) {
                                    x_PrintStringModIfNotDup( &strain_seen, "strain", mod.GetSubname() );
                                }
                                break;
                            default:
                                // ignore; do nothing
                                break;
                        }
                    }
                }
            }
            if( orgname.IsSetGcode() ) {
                x_PrintIntModIfNotDup( &gcode_seen, "gcode", orgname.GetGcode() );
            }
        }
    } catch( CException & ) {
        // ignore exception; it probably just means there's no org-ref
    }

    typedef SStaticPair<CMolInfo::TTech, const char*> TTechMapEntry;
    static const TTechMapEntry sc_TechArray[] = {
        // note that the text values do *NOT* precisely correspond with
        // the names in the ASN.1 schema files
        { CMolInfo::eTech_unknown,            "?" },
        { CMolInfo::eTech_standard,           "standard" },
        { CMolInfo::eTech_est,                "EST" },
        { CMolInfo::eTech_sts,                "STS" },
        { CMolInfo::eTech_survey,             "survey" },
        { CMolInfo::eTech_genemap,            "genetic map" },
        { CMolInfo::eTech_physmap,            "physical map" },
        { CMolInfo::eTech_derived,            "derived" },
        { CMolInfo::eTech_concept_trans,      "concept-trans" },
        { CMolInfo::eTech_seq_pept,           "seq-pept" },
        { CMolInfo::eTech_both,               "both" },
        { CMolInfo::eTech_seq_pept_overlap,   "seq-pept-overlap" },
        { CMolInfo::eTech_seq_pept_homol,     "seq-pept-homol" },
        { CMolInfo::eTech_concept_trans_a,    "concept-trans-a" },
        { CMolInfo::eTech_htgs_1,             "htgs 1" },
        { CMolInfo::eTech_htgs_2,             "htgs 2" },
        { CMolInfo::eTech_htgs_3,             "htgs 3" },
        { CMolInfo::eTech_fli_cdna,           "fli cDNA" },
        { CMolInfo::eTech_htgs_0,             "htgs 0" },
        { CMolInfo::eTech_htc,                "htc" },
        { CMolInfo::eTech_wgs,                "wgs" },
        { CMolInfo::eTech_barcode,            "barcode" },
        { CMolInfo::eTech_composite_wgs_htgs, "composite-wgs-htgs" },
        { CMolInfo::eTech_tsa,                "tsa" }
    };
    typedef CStaticPairArrayMap<CMolInfo::TTech, const char*>  TTechMap;
    DEFINE_STATIC_ARRAY_MAP(TTechMap, sc_TechMap, sc_TechArray);

    // print some key-value pairs
    bool tech_seen = false;
    const CMolInfo * pMolInfo = sequence::GetMolInfo(handle);
    if( pMolInfo != NULL ) {
        const CMolInfo & molinfo = *pMolInfo;
        if( molinfo.IsSetTech() ) {
            TTechMap::const_iterator find_iter = sc_TechMap.find(molinfo.GetTech());
            if( find_iter != sc_TechMap.end() ) {
                x_PrintStringModIfNotDup( &tech_seen, "tech", 
                    find_iter->second );
            }
        }
    }

    m_Out << '\n';
}

inline
sequence::CDeflineGenerator::TUserFlags
CFastaOstream::x_GetTitleFlags(void) const
{
    sequence::TGetTitleFlags title_flags = 0;
    if ((m_Flags & fNoExpensiveOps) != 0) {
        title_flags |= sequence::CDeflineGenerator::fNoExpensiveOps;
    }
    return title_flags;
}

void CFastaOstream::x_WriteSeqTitle(const CBioseq& bioseq,
                                    CScope* scope,
                                    const string& custom_title)
{
    string safe_title;
    if ( !custom_title.empty() ) {
        safe_title = custom_title;
    } else if (scope) {
        safe_title = m_Gen->GenerateDefline(bioseq, *scope, x_GetTitleFlags());
    } else {
        safe_title = sequence::s_GetFastaTitle(bioseq, x_GetTitleFlags());
    }

    if ( !(m_Flags & fKeepGTSigns) ) {
        NON_CONST_ITERATE (string, it, safe_title) {
            switch (*it) {
            case '>':  *it = '_';  break;
            default:   break;
            }
        }
    }

    if ( !safe_title.empty() ) {
        m_Out << ' ' << safe_title;
    }
    m_Out << '\n';
}

void CFastaOstream::x_PrintStringModIfNotDup(
    bool *seen, const CTempString & key, const CTempString & value )
{
    _ASSERT( NULL != seen );
    _ASSERT( ! key.empty() );
    if( *seen ) {
        ERR_POST_X(9, Warning << "CFastaOstream::x_PrintStringModIfNotDup: "
            << "key " << key << " would appear multiple times, but only using the first." );
        return;
    }

    if( value.empty() ) {
        return;
    }

    m_Out << " [" << key << '=';
    // The case of no quotes is much more common, so optimize for that
    if( value.find_first_of("\"=") == string::npos ) {
        // normal case: no weird characters in value name
        m_Out << value ;
    } else {
        // rarer case: bad characters in value name, so
        // we need surrounding double-quotes and we need to change
        // double-quotes to single-quotes.
        m_Out << '"' << NStr::Replace( value, "\"", "'") << '"';
    }
    m_Out << ']';
    *seen = true;
}

void CFastaOstream::x_PrintIntModIfNotDup(
    bool *seen, const CTempString & key, const int value )
{
    CNcbiOstrstream strm;
    strm << value;
    x_PrintStringModIfNotDup( seen, key, 
        (string)CNcbiOstrstreamToString(strm) );
}


void CFastaOstream::WriteTitle(const CBioseq& bioseq,
                               const CSeq_loc* location,
                               bool no_scope,
                               const string& custom_title)
{
    if ( no_scope && ! location ) {
        x_WriteSeqIds(bioseq, NULL);
        if( (m_Flags & fShowModifiers) != 0 ) {
            CScope scope(*CObjectManager::GetInstance());
            CBioseq_Handle bioseq_handle = scope.AddBioseq(bioseq);
            x_WriteModifiers(bioseq_handle);
        } else {
            x_WriteSeqTitle(bioseq, NULL, custom_title);
        }
    }
    else {
        CScope scope(*CObjectManager::GetInstance());
        WriteTitle(scope.AddBioseq(bioseq), location, custom_title);
    }
}


void CFastaOstream::WriteTitle(const CBioseq_Handle& handle,
                               const CSeq_loc* location,
                               const string& custom_title)
{
    x_WriteSeqIds(*handle.GetBioseqCore(), location);
    if( (m_Flags & fShowModifiers) != 0 ) {
        x_WriteModifiers(handle);
    } else {
        string safe_title = 
            (custom_title.empty()
             ? m_Gen->GenerateDefline(handle, x_GetTitleFlags())
             : custom_title);
        if ((m_Flags & fKeepGTSigns) == 0) {
            NStr::ReplaceInPlace(safe_title, ">", "_");
        }
        m_Out << ' ' << safe_title << '\n';
    }
}


CConstRef<CSeq_loc> CFastaOstream::x_MapMask(CSeq_loc_Mapper& mapper,
                                             const CSeq_loc& mask,
                                             const CSeq_id* base_seq_id,
                                             CScope* scope)
{
    CConstRef<CSeq_loc> mapped_mask(&mask);

    // Mapping down requires the higher-level ID as a reference, even
    // when given a scope, and as such should precede mapping up to
    // keep sequence::GetId from bombing out.
    if ((m_Flags & fMapMasksDown) != 0  &&  scope) {
        try {
            CSeq_loc_Mapper mapper_down
                (scope->GetBioseqHandle(sequence::GetId(*mapped_mask, scope)),
                 CSeq_loc_Mapper::eSeqMap_Down);
            mapped_mask = mapped_mask->Add(*mapper_down.Map(*mapped_mask),
                                           CSeq_loc::fSortAndMerge_All, 0);
        } catch (CObjmgrUtilException&) {
        }
    }
    if ((m_Flags & fMapMasksUp) != 0  &&  scope  &&  base_seq_id) {
        CSeq_loc_Mapper mapper_up(scope->GetBioseqHandle(*base_seq_id),
                                  CSeq_loc_Mapper::eSeqMap_Up);
        mapped_mask = mapped_mask->Add(*mapper_up.Map(*mapped_mask),
                                       CSeq_loc::fSortAndMerge_All, 0);
    }
    mapped_mask = mapper.Map(*mapped_mask);
    return mapped_mask;
}


void CFastaOstream::x_GetMaskingStates(TMSMap& masking_state,
                                       const CSeq_id* base_seq_id,
                                       const CSeq_loc* location,
                                       CScope* scope)
{
    CRef<CSeq_loc_Mapper> mapper;
    CBioseq_Handle        bsh;

    if (m_SoftMask.NotEmpty()  ||  m_HardMask.NotEmpty()) {
        _ASSERT(base_seq_id);
        CSeq_loc whole;
        whole.SetWhole().Assign(*base_seq_id);
        if (location) {
            mapper.Reset(new CSeq_loc_Mapper(*location, whole, scope));
        } else {
            // still useful for filtering out locations on other sequences
            mapper.Reset(new CSeq_loc_Mapper(whole, whole, scope));
        }
        mapper->SetMergeAll();
        mapper->TruncateNonmappingRanges();

        if (scope  &&  (m_Flags & (fMapMasksUp | fMapMasksDown))) {
            bsh = scope->GetBioseqHandle(*base_seq_id);
        }

        const CSeq_loc&     mask      = m_SoftMask ? *m_SoftMask : *m_HardMask;
        int                 type      = m_SoftMask ? eSoftMask : eHardMask;
        CConstRef<CSeq_loc> mapped_mask = x_MapMask(*mapper, mask, base_seq_id,
                                                    scope);

        masking_state[0] = 0;
        for (CSeq_loc_CI it(*mapped_mask);  it;  ++it) {
            CSeq_loc_CI::TRange loc_range = it.GetRange();
            masking_state[loc_range.GetFrom()]   = type;
            masking_state[loc_range.GetToOpen()] = 0;
        }
    }

    if (m_SoftMask.NotEmpty()  &&  m_HardMask.NotEmpty()) {
        CConstRef<CSeq_loc> mapped_mask = x_MapMask(*mapper, *m_HardMask,
                                                    base_seq_id, scope);
        for (CSeq_loc_CI it(*mapped_mask);  it;  ++it) {
            CSeq_loc_CI::TRange loc_range = it.GetRange();
            TSeqPos             from      = loc_range.GetFrom();
            TSeqPos             to        = loc_range.GetToOpen();
            TMSMap::iterator    ms_it     = masking_state.lower_bound(from);
            int                 prev_state;

            if (ms_it == masking_state.end()) {
                masking_state[loc_range.GetFrom()]   = eHardMask;
                masking_state[loc_range.GetToOpen()] = 0;
                continue;
            } else if (ms_it->first == from) {
                prev_state = ms_it->second;
                ms_it->second |= eHardMask;
            } else {
                // NB: lower_bound's name is misleading, as it actually
                // returns the least element whose key >= from.
                _ASSERT(ms_it != masking_state.begin());
                TMSMap::iterator prev_it = ms_it;
                --prev_it;
                prev_state = prev_it->second;
                TMSMap::value_type value(from, prev_state | eHardMask);

                // Add the new element (using ms_it as a position hint),
                // and repoint ms_it at it so that the below loop will
                // start at the correct position.
                ms_it = masking_state.insert(ms_it, value);
            }
            while (++ms_it != masking_state.end()  &&  ms_it->first < to) {
                prev_state = ms_it->second;
                ms_it->second |= eHardMask;
            }
            if (ms_it == masking_state.end()  ||  ms_it->first != to) {
                masking_state.insert(ms_it, TMSMap::value_type(to, prev_state));
            }
        }
    }
}


void CFastaOstream::x_WriteSequence(const CSeqVector& vec,
                                    const TMSMap& masking_state)
{
    TSeqPos                 rem_line      = m_Width;
    CSeqVector_CI           it(vec);
    TMSMap::const_iterator  ms_it         = masking_state.begin();
    TSeqPos                 rem_state
        = (ms_it == masking_state.end() ? numeric_limits<TSeqPos>::max()
           : ms_it->first);
    int                     current_state = 0;
    CTempString             uc_hard_mask_str
        (vec.IsProtein() ? m_UC_Xs.get() : m_UC_Ns.get(), m_Width);
    CTempString             lc_hard_mask_str
        (vec.IsProtein() ? m_LC_Xs.get() : m_LC_Ns.get(), m_Width);
    EGapMode                native_gap_mode
        = ((vec.GetGapChar() == '-') ? eGM_dashes : eGM_letters);
    CTempString             alt_gap_str;

    if (native_gap_mode == eGM_dashes) {
        alt_gap_str = uc_hard_mask_str;
    } else {
        alt_gap_str.assign(m_Dashes.get(), m_Width);
    }

    if ((m_Flags & fReverseStrand) != 0) {
        it.SetStrand(Reverse(it.GetStrand()));
    }

    while ( it ) {
        if (rem_state == 0) {
            _ASSERT(ms_it->first == it.GetPos());
            current_state = ms_it->second;
            if (++ms_it == masking_state.end()) {
                rem_state = numeric_limits<TSeqPos>::max();
            } else {
                rem_state = ms_it->first - it.GetPos();
            }
        }
        if( (m_Flags & fShowGapsOfSizeZero) != 0 &&
            it.HasZeroGapBefore() ) 
        {
            m_Out << "-\n";
            rem_line = m_Width;
        }
        if ((m_GapMode != native_gap_mode || (m_Flags & fInstantiateGaps) == 0)
            &&  it.GetGapSizeForward()) 
        {
            TSeqPos gap_size = it.SkipGap();
            if (m_GapMode == eGM_one_dash
                ||  (m_Flags & fInstantiateGaps) == 0) {
                m_Out << "-\n";
                rem_line = m_Width;
            } else if (m_GapMode == eGM_count) {
                if (rem_line < m_Width) {
                    m_Out << '\n';
                }
                CSeqMap_CI smci = vec.GetSeqMap().FindSegment
                    (it.GetPos() - gap_size, &vec.GetScope());
                _ASSERT(smci.GetType() == CSeqMap::eSeqGap);
                if (smci.IsUnknownLength()) {
                    // conventional designation, regardless of nominal length
                    if( gap_size > 0 && (m_Flags & fKeepUnknGapNomLen) != 0 )
                    {
                        m_Out << ">?unk" << gap_size;
                    } else {
                        m_Out << ">?unk100";
                    }
                } else {
                    m_Out << ">?" << gap_size;
                }
                // print gap mods, if requested
                if( (m_Flags & fShowGapModifiers) != 0 )
                {
                    CConstRef<CSeq_literal> pGapLiteral = 
                        smci.GetRefGapLiteral();
                    if( pGapLiteral &&
                        FIELD_IS_SET_AND_IS(*pGapLiteral, Seq_data, Gap) )
                    {
                        const CSeq_gap & seq_gap = 
                            pGapLiteral->GetSeq_data().GetGap();
                        SGapModText gap_mod_text;
                        GetGapModText(seq_gap, gap_mod_text);

                        CNcbiOstrstream gap_mod_strm;
                        gap_mod_text.WriteAllModsAsFasta(gap_mod_strm);
                        const string sGapModText = 
                            CNcbiOstrstreamToString(gap_mod_strm);
                        if( ! sGapModText.empty() ) {
                            m_Out << ' ' << sGapModText;
                        }
                    }
                }
                m_Out << '\n';
                rem_line = m_Width;
            } else {
                TSeqPos rem_gap = gap_size;
                while (rem_gap >= rem_line) {
                    m_Out.write(alt_gap_str.data(), rem_line);
                    m_Out << '\n';
                    rem_gap -= rem_line;
                    rem_line = m_Width;
                }
                if (rem_gap > 0) {
                    m_Out.write(alt_gap_str.data(), rem_gap);
                    rem_line -= rem_gap;
                }
            }
            if (rem_state >= gap_size) {
                rem_state -= gap_size;
            } else {
                while (++ms_it != masking_state.end()
                       &&  ms_it->first < it.GetPos()) {
                    current_state = ms_it->second;
                }
                if (ms_it == masking_state.end()) {
                    rem_state = numeric_limits<TSeqPos>::max();
                } else {
                    rem_state = ms_it->first - it.GetPos();
                }
            }
        } else {
            TSeqPos     count   = min(TSeqPos(it.GetBufferSize()), rem_state);
            TSeqPos     new_pos = it.GetPos() + count;
            const char* ptr     = it.GetBufferPtr();
            string      lc_buffer;

            rem_state -= count;
            if (current_state & eHardMask) {
                ptr = (current_state & eSoftMask) ? lc_hard_mask_str.data()
                    : uc_hard_mask_str.data();
            } else if (current_state & eSoftMask) {
                // ToLower() always operates in place. :-/
                lc_buffer.assign(ptr, count);
                NStr::ToLower(lc_buffer);
                ptr = lc_buffer.data();
            }
            while ( count >= rem_line ) {
                m_Out.write(ptr, rem_line);
                if ( !(current_state & eHardMask) ) {
                    ptr += rem_line;
                }
                count -= rem_line;
                m_Out << '\n';
                rem_line = m_Width;
            }
            if ( count > 0 ) {
                m_Out.write(ptr, count);
                rem_line -= count;
            }
            it.SetPos(new_pos);
        }
    }
    if ( rem_line < m_Width ) {
        m_Out << '\n';
    }
    // m_Out << NcbiFlush;
}


void CFastaOstream::WriteSequence(const CBioseq_Handle& handle,
                                  const CSeq_loc* location)
{
    vector<CTSE_Handle> used_tses;
    if ( !(m_Flags & fAssembleParts)  &&  !handle.IsSetInst_Seq_data() ) {
        SSeqMapSelector sel(CSeqMap::fFindInnerRef, (size_t)-1);
        sel.SetLinkUsedTSE(handle.GetTSE_Handle());
        sel.SetLinkUsedTSE(used_tses);
        if ( !handle.GetSeqMap().CanResolveRange(&handle.GetScope(), sel) ) {
            return;
        }
    }

    CScope&    scope = handle.GetScope();
    CSeqVector v;
    if (location) {
        if (sequence::SeqLocCheck(*location, &scope)
            == sequence::eSeqLocCheck_error) {
            string label;
            location->GetLabel(&label);
            NCBI_THROW(CObjmgrUtilException, eBadLocation,
                       "CFastaOstream: location out of range: " + label);
        }
        CRef<CSeq_loc> merged
            = sequence::Seq_loc_Merge(*location, CSeq_loc::fMerge_All, &scope);
        v = CSeqVector(*merged, scope, CBioseq_Handle::eCoding_Iupac);
    } else {
        v = handle.GetSeqVector(CBioseq_Handle::eCoding_Iupac);
    }
    if (v.IsProtein()) { // allow extensions
        v.SetCoding(CSeq_data::e_Ncbieaa);
    }

    TMSMap masking_state;
    if (m_SoftMask.NotEmpty()  ||  m_HardMask.NotEmpty()) {
        x_GetMaskingStates(masking_state, handle.GetSeqId(), location, &scope);
    }
    x_WriteSequence(v, masking_state);
}


void CFastaOstream::Write(const CSeq_entry& entry, const CSeq_loc* location,
                          bool no_scope)
{
    if (location || !no_scope) {
        CScope scope(*CObjectManager::GetInstance());        
        Write(scope.AddTopLevelSeqEntry(entry), location);
    } else {
        switch (entry.Which()) {
        case CSeq_entry::e_Seq:
            Write(entry.GetSeq(), location, no_scope);
            break;
        case CSeq_entry::e_Set:
            ITERATE (CBioseq_set::TSeq_set, it, entry.GetSet().GetSeq_set()) {
                Write(**it, location, no_scope);
            }
            break;
        default:
            // throw
            break;
        }
    }
}


void CFastaOstream::Write(const CBioseq& seq, const CSeq_loc* location,
                          bool no_scope, const string& custom_title )
{
    if (location || !no_scope) {
        CScope scope(*CObjectManager::GetInstance());
        Write(scope.AddBioseq(seq), location, custom_title);
    } else {
        /// write our title
        x_WriteSeqIds(seq, NULL);
        x_WriteSeqTitle(seq, NULL, custom_title);

        /// write the sequence
        TMSMap masking_state;
        x_GetMaskingStates(masking_state, NULL, NULL, NULL);

        /// check to see if all of our segments are resolvable
        bool is_raw = true;
        switch (seq.GetInst().GetRepr()) {
        case CSeq_inst::eRepr_raw:
            break;
        case CSeq_inst::eRepr_delta:
            ITERATE (CSeq_inst::TExt::TDelta::Tdata, iter,
                     seq.GetInst().GetExt().GetDelta().Get()) {
                if ((*iter)->Which() == CDelta_seq::e_Loc) {
                    is_raw = false;
                    break;
                }
            }
            break;
        default:
            is_raw = false;
            break;
        }

        if (is_raw) {
            CSeqVector vec(seq, NULL, CBioseq_Handle::eCoding_Iupac);
            if (vec.IsProtein()) { // allow extensions
                vec.SetCoding(CSeq_data::e_Ncbieaa);
            }
            x_WriteSequence(vec, masking_state);
        } else {
            /// we require far-pointer resolution
            CScope scope(*CObjectManager::GetInstance());
            CBioseq_Handle bsh = scope.AddBioseq(seq);
            CSeqVector vec(bsh, CBioseq_Handle::eCoding_Iupac);
            if (vec.IsProtein()) {
                vec.SetCoding(CSeq_data::e_Ncbieaa);
            }
            x_WriteSequence(vec, masking_state);
        }
    }
}


CConstRef<CSeq_loc> CFastaOstream::GetMask(EMaskType type) const
{
    return (type == eSoftMask) ? m_SoftMask : m_HardMask;
}


void CFastaOstream::SetMask(EMaskType type, CConstRef<CSeq_loc> location)
{
    ((type == eSoftMask) ? m_SoftMask : m_HardMask) = location;
}


void CFastaOstream::SetWidth(TSeqPos width)
{
    m_Width = width;
    m_Dashes.reset(new char[width]);  memset(m_Dashes.get(), '-', width);
    m_LC_Ns .reset(new char[width]);  memset(m_LC_Ns .get(), 'n', width);
    m_LC_Xs .reset(new char[width]);  memset(m_LC_Xs .get(), 'x', width);
    m_UC_Ns .reset(new char[width]);  memset(m_UC_Ns .get(), 'N', width);
    m_UC_Xs .reset(new char[width]);  memset(m_UC_Xs .get(), 'X', width);
}

void 
CFastaOstream::SGapModText::WriteAllModsAsFasta(
    CNcbiOstream & out ) const
{
    string sPrefix = kEmptyStr;
    if( ! gap_type.empty() ) {
        out << sPrefix << "[gap-type=" << gap_type << ']';
        sPrefix = " ";
    }
    if( ! gap_linkage_evidences.empty() ) {
        out << sPrefix << "[linkage-evidence=" << NStr::Join(gap_linkage_evidences, ";") << ']';
        sPrefix = " ";
    }
}

// static
void 
CFastaOstream::GetGapModText(
    const CSeq_gap & seq_gap,
    SGapModText & out_gap_info )
{
    // convenience references
    string & gap_type = out_gap_info.gap_type;
    vector<string> & gap_linkage_evidences = 
        out_gap_info.gap_linkage_evidences;

    // make sure initialized
    gap_type.clear();
    gap_linkage_evidences.clear();

    // true if we need to have a /linkage-evidence tag.
    // Also, if this is false, we should *not* have any
    // linkage-evidence tag
    bool need_evidence = false;

    // determine if we're linked, and also determine if
    // we need linkage-evidence
    bool is_linkage =
        seq_gap.CanGetLinkage() && 
        seq_gap.GetLinkage() == CSeq_gap::eLinkage_linked;

    if ( seq_gap.IsSetLinkage_evidence() ) {
        is_linkage = true; /* do not rely solely on Seq-gap.linkage, which is not always set correctly */
    }

    // For /gap_type qual
    if( seq_gap.CanGetType() ) {
        switch( seq_gap.GetType() ) {
        case CSeq_gap::eType_unknown:
            // don't show /gap_type
            break;
        case CSeq_gap::eType_fragment:
            gap_type = "within scaffold";
            need_evidence = true;
            break;
        case CSeq_gap::eType_clone:
            gap_type = ( is_linkage ? "within scaffold" : "between scaffolds" );
            need_evidence = is_linkage;
            break;
        case CSeq_gap::eType_short_arm:
            gap_type = "short_arm";
            break;
        case CSeq_gap::eType_heterochromatin:
            gap_type = "heterochromatin";
            break;
        case CSeq_gap::eType_centromere:
            gap_type = "centromere";
            break;
        case CSeq_gap::eType_telomere:
            gap_type = "telomere";
            break;
        case CSeq_gap::eType_repeat:
            gap_type = ( is_linkage ? 
                "repeat within scaffold" : 
            "repeat between scaffolds" );
            need_evidence = is_linkage;
            break;
        case CSeq_gap::eType_contig:
            gap_type = "between scaffolds";
            break;
        case CSeq_gap::eType_scaffold:
            gap_type = "within scaffold";
            need_evidence = is_linkage;
            break;
        case CSeq_gap::eType_other:
            gap_type = "other";
            break;
        default:
            gap_type = "(ERROR: UNRECOGNIZED_GAP_TYPE:" +
                NStr::IntToString(seq_gap.GetType()) + ")";
            break;
        }
    }

    // For linkage evidence
    if( seq_gap.CanGetLinkage_evidence() ) {
        ITERATE( CSeq_gap::TLinkage_evidence, 
            evidence_iter, 
            seq_gap.GetLinkage_evidence() ) 
        {
            const CLinkage_evidence & evidence = **evidence_iter;
            if( evidence.CanGetType() ) {
                switch( evidence.GetType() ) {
                case CLinkage_evidence::eType_paired_ends:
                    gap_linkage_evidences.push_back("paired-ends");
                    break;
                case CLinkage_evidence::eType_align_genus:
                    gap_linkage_evidences.push_back("align genus");
                    break;
                case CLinkage_evidence::eType_align_xgenus:
                    gap_linkage_evidences.push_back("align xgenus");
                    break;
                case CLinkage_evidence::eType_align_trnscpt:
                    gap_linkage_evidences.push_back("align trnscpt");
                    break;
                case CLinkage_evidence::eType_within_clone:
                    gap_linkage_evidences.push_back("within clone");
                    break;
                case CLinkage_evidence::eType_clone_contig:
                    gap_linkage_evidences.push_back("clone contig");
                    break;
                case CLinkage_evidence::eType_map:
                    gap_linkage_evidences.push_back("map");
                    break;
                case CLinkage_evidence::eType_strobe:
                    gap_linkage_evidences.push_back("strobe");
                    break;
                case CLinkage_evidence::eType_unspecified:
                    gap_linkage_evidences.push_back("unspecified");
                    break;
                case CLinkage_evidence::eType_pcr:
                    gap_linkage_evidences.push_back("pcr");
                    break;
                case CLinkage_evidence::eType_other:
                    gap_linkage_evidences.push_back("other");
                    break;
                default:
                    gap_linkage_evidences.push_back("(UNRECOGNIZED LINKAGE EVIDENCE:" +
                        NStr::IntToString( evidence.GetType() ) + ")");
                    break;
                }
            }
        }
    }

    if( need_evidence && gap_linkage_evidences.empty() ) {
        gap_linkage_evidences.push_back("unspecified");
    } else if( ! need_evidence && ! gap_linkage_evidences.empty() ) {
        // This case shouldn't happen if the validator is checking
        // records first.
        gap_linkage_evidences.clear();
    }
}

/////////////////////////////////////////////////////////////////////////////
//
// sequence translation
//


template <class Container>
void x_Translate(const Container& seq,
                 string& prot,
                 int frame,
                 const CGenetic_code* code,
                 bool is_5prime_complete,
                 bool include_stop,
                 bool remove_trailing_X,
                 bool* alt_start)
{
    // reserve our space
    const size_t usable_size = seq.size() - frame;
    const size_t mod = usable_size % 3;
    prot.erase();
    prot.reserve(usable_size / 3 + (mod ? 1 : 0));

    // get appropriate translation table
    const CTrans_table & tbl =
        (code ? CGen_code_table::GetTransTable(*code) :
                CGen_code_table::GetTransTable(1));

    // main loop through bases
    typename Container::const_iterator start = seq.begin();
    {{
         for (int i = 0;  i < frame;  ++i) {
             ++start;
         }
     }}

    size_t i;
    size_t k;
    size_t state = 0;
    size_t start_state = 0;
    size_t length = usable_size / 3;
    bool check_start = (is_5prime_complete && frame == 0);
    bool first_time = true;

    for (i = 0;  i < length;  ++i) {

        // loop through one codon at a time
        for (k = 0;  k < 3;  ++k, ++start) {
            state = tbl.NextCodonState(state, *start);
        }

        if (first_time) {
            start_state = state;
        }

        // save translated amino acid
        if (first_time  &&  check_start) {
            prot.append(1, tbl.GetStartResidue(state));
        } else {
            prot.append(1, tbl.GetCodonResidue(state));
        }

        first_time = false;
    }

    if (mod) {
        for (k = 0;  k < mod;  ++k, ++start) {
            state = tbl.NextCodonState(state, *start);
        }

        for ( ;  k < 3;  ++k) {
            state = tbl.NextCodonState(state, 'N');
        }

        if (first_time) {
            start_state = state;
        }

        // save translated amino acid
        char c = tbl.GetCodonResidue(state);
        if (first_time  &&  check_start) {
            prot.append(1, tbl.GetStartResidue(state));
        } else if (c != 'X') {
            // if padding was needed, trim ambiguous last residue
            prot.append(1, tbl.GetCodonResidue(state));
        }
    }

    // check for alternative start codon
    if (alt_start && is_5prime_complete) {
        if ( tbl.IsAltStart(start_state) ) {
            *alt_start = true;
        } else {
            *alt_start = false;
        }
    }

    if ( !include_stop ) {
        SIZE_TYPE sz = prot.find_first_of("*");
        if (sz != string::npos) {
            prot.resize(sz);
        }
    }

    if (remove_trailing_X) {
        SIZE_TYPE sz;
        for (sz = prot.size();  sz > 0  &&  prot[sz - 1] == 'X';  --sz) {
        }
        prot.resize(sz);
    }

    /**
    cerr << "source: ";
    ITERATE (typename Container, it, seq) {
        cerr << *it;
    }
    cerr << endl;
    cerr << "xlate: ";
    ITERATE (string, it, prot) {
        cerr << *it;
    }
    cerr << endl;
    **/
}


static void AddAAToDeltaSeq (CRef<CBioseq> prot, char residue)
{
    if (prot->SetInst().SetExt().SetDelta().Set().empty()
        || prot->GetInst().GetExt().GetDelta().Get().back()->GetLiteral().GetSeq_data().IsGap()) {
        // either first seg or transitioning from gap, need new seg
        CRef<CDelta_seq> seg(new CDelta_seq());
        seg->SetLiteral().SetLength(0);
        prot->SetInst().SetExt().SetDelta().Set().push_back(seg);
    } 
       
    CRef<CDelta_seq> last = prot->SetInst().SetExt().SetDelta().Set().back();

    if (residue == '*') {
        if (last->IsLiteral() && last->GetLiteral().IsSetSeq_data() && last->GetLiteral().GetSeq_data().IsIupacaa()) {
            // convert to ncbieaa
            string current = last->GetLiteral().GetSeq_data().GetIupacaa().Get();
            last->SetLiteral().SetSeq_data().SetNcbieaa().Set(current);
        }
        // add *
        last->SetLiteral().SetSeq_data().SetNcbieaa().Set().append(1, residue);
    } else if (last->IsLiteral() && last->GetLiteral().IsSetSeq_data() && last->GetLiteral().GetSeq_data().IsNcbieaa()) {
        last->SetLiteral().SetSeq_data().SetNcbieaa().Set().append(1, residue);
    } else {
        last->SetLiteral().SetSeq_data().SetIupacaa().Set().append(1, residue);
    }
        
    TSeqPos len = last->GetLiteral().GetLength();
    last->SetLiteral().SetLength(len + 1);
}


static void AddGapToDeltaSeq (CRef<CBioseq>prot, bool unknown_length, size_t add_len)
{
    if (prot->SetInst().SetExt().SetDelta().Set().empty()) {
        // create new segment for gap
        CRef<CDelta_seq> new_seg(new CDelta_seq());
        new_seg->SetLiteral().SetSeq_data().SetGap().SetType(CSeq_gap::eType_unknown);
        new_seg->SetLiteral().SetLength(add_len);
        if (unknown_length) {
            new_seg->SetLiteral().SetFuzz().SetLim(CInt_fuzz::eLim_unk);
        }
        prot->SetInst().SetExt().SetDelta().Set().push_back(new_seg);
    } else {
        CRef<CDelta_seq> last = prot->SetInst().SetExt().SetDelta().Set().back();
        if (last->SetLiteral().GetSeq_data().IsGap()
            && ((unknown_length && last->SetLiteral().IsSetFuzz()) 
                 || (!unknown_length && !last->SetLiteral().IsSetFuzz()))) {
            // ok, already creating gap segment with correct fuzz
            size_t len = prot->GetInst().GetExt().GetDelta().Get().back()->GetLiteral().GetLength();
            prot->SetInst().SetExt().SetDelta().Set().back()->SetLiteral().SetLength(len + add_len);
        } else {
            // create new segment for gap
            CRef<CDelta_seq> new_seg(new CDelta_seq());
            new_seg->SetLiteral().SetSeq_data().SetGap().SetType(CSeq_gap::eType_unknown);
            new_seg->SetLiteral().SetLength(add_len);
            if (unknown_length) {
                new_seg->SetLiteral().SetFuzz().SetLim(CInt_fuzz::eLim_unk);
            }
            prot->SetInst().SetExt().SetDelta().Set().push_back(new_seg);
        }
    }
}


CRef<CBioseq> CSeqTranslator::TranslateToProtein(const CSeq_feat& cds,
                                                   CScope& scope)
{
    const CGenetic_code* code = NULL;
    int frame = 0;
    if (cds.GetData().IsCdregion()) {
        const CCdregion& cdr = cds.GetData().GetCdregion();
        if (cdr.IsSetFrame ()) {
            switch (cdr.GetFrame ()) {
            case CCdregion::eFrame_two :
                frame = 1;
                break;
            case CCdregion::eFrame_three :
                frame = 2;
                break;
            default :
                break;
            }
        }
        if (cdr.IsSetCode()) {
            code = &cdr.GetCode();
        }
    }
    bool is_5prime_complete = !cds.GetLocation().IsPartialStart(eExtreme_Biological);

    CSeqVector seq(cds.GetLocation(), scope, CBioseq_Handle::eCoding_Iupac);
    CConstRef<CSeqMap> map;
    map.Reset(&seq.GetSeqMap());

    CRef<CBioseq> prot(new CBioseq());

    prot->SetInst().SetRepr(CSeq_inst::eRepr_delta);
    prot->SetInst().SetMol(CSeq_inst::eMol_aa);
    prot->SetInst().SetLength(0);
    prot->SetInst().SetTopology(CSeq_inst::eTopology_linear);

    // reserve our space
    const size_t usable_size = seq.size() - frame;
    const size_t mod = usable_size % 3;

    // get appropriate translation table
    const CTrans_table & tbl =
        (code ? CGen_code_table::GetTransTable(*code) :
                CGen_code_table::GetTransTable(1));

    // main loop through bases
    CSeqVector::const_iterator start = seq.begin();
    for (int i = 0;  i < frame;  ++i) {
        ++start;
    }

    size_t i;
    size_t k;
    size_t state = 0;
    size_t length = usable_size / 3;
    bool check_start = (is_5prime_complete && frame == 0);
    bool first_time = true;

    for (i = 0;  i < length;  ++i) {
        bool is_gap = true;
        bool unknown_length = false;
        size_t pos = (i * 3) + frame;

        if (start.HasZeroGapBefore()) {
            AddGapToDeltaSeq (prot, true, 0);
        }

        // loop through one codon at a time
        for (k = 0;  k < 3;  ++k, ++start) {
            state = tbl.NextCodonState(state, *start);
            if (seq.IsInGap(pos + k )) {
                if (is_gap && !unknown_length) {
                    CSeqMap_CI map_iter(map, &scope, SSeqMapSelector(), pos + k);
                    if (map_iter.GetType() == CSeqMap::eSeqGap
                        && map_iter.IsUnknownLength()) {
                        unknown_length = true;
                    }
                }
            } else {
                is_gap = false;
            }
        }

        if (is_gap) {
            AddGapToDeltaSeq (prot, unknown_length, 1);
        } else {
            // save translated amino acid
            if (first_time  &&  check_start) { 
                AddAAToDeltaSeq(prot, tbl.GetStartResidue(state));
            } else {
                AddAAToDeltaSeq(prot, tbl.GetCodonResidue(state));
            }

        }

        first_time = false;
    }

    if (mod) {
        bool is_gap = true;
        bool unknown_length = false;
        size_t pos = (length * 3) + frame;
        for (k = 0;  k < mod;  ++k, ++start) {
            state = tbl.NextCodonState(state, *start);
            if (seq.IsInGap(pos + k)) {
                if (is_gap && !unknown_length) {
                    CSeqMap_CI map_iter(map, &scope, SSeqMapSelector(), pos + k);
                    if (map_iter.GetType() == CSeqMap::eSeqGap) {
                        if (map_iter.IsUnknownLength()) {
                            unknown_length = true;
                        }
                    }
                }
            } else {
                is_gap = false;
            }
        }

        CRef<CDelta_seq> last = prot->SetInst().SetExt().SetDelta().Set().back();
        if (is_gap) {
            AddGapToDeltaSeq (prot, unknown_length, 1);
        } else {
            for ( ;  k < 3;  ++k) {
                state = tbl.NextCodonState(state, 'N');
            }

            // save translated amino acid
            char c = tbl.GetCodonResidue(state);
            if (c != 'X') {
                if (first_time  &&  check_start) { 
                    AddAAToDeltaSeq(prot, tbl.GetStartResidue(state));
                } else {
                    AddAAToDeltaSeq(prot, tbl.GetCodonResidue(state));
                }
            }
        }
    }

    size_t prot_len = 0;
    ITERATE (CDelta_ext::Tdata, seg_it, prot->SetInst().SetExt().SetDelta().Set()) {
        prot_len += (*seg_it)->GetLiteral().GetLength();
    }

    // code break substitution
    if (cds.GetData().IsCdregion()  &&
        cds.GetData().GetCdregion().IsSetCode_break()) {
        const CCdregion& cdr = cds.GetData().GetCdregion();
        ITERATE (CCdregion::TCode_break, code_break, cdr.GetCode_break()) {
            const CRef <CCode_break> brk = *code_break;
            const CSeq_loc& cbk_loc = brk->GetLoc();
            TSeqPos seq_pos =
                sequence::LocationOffset(cds.GetLocation(), cbk_loc,
                                         sequence::eOffset_FromStart,
                                         &scope);
            seq_pos -= frame;
            string::size_type i = seq_pos / 3;
            if (i < prot_len) {
                const CCode_break::C_Aa& c_aa = brk->GetAa ();
                if (c_aa.IsNcbieaa ()) {
                    CDelta_ext::Tdata::iterator seg_it = prot->SetInst().SetExt().SetDelta().Set().begin();
                    string::size_type offset = 0;
                    while (seg_it != prot->SetInst().SetExt().SetDelta().Set().end()
                           && offset + (*seg_it)->GetLiteral().GetLength() < i) {
                        offset += (*seg_it)->GetLiteral().GetLength();
                        ++seg_it;
                    }
                    if (seg_it != prot->SetInst().SetExt().SetDelta().Set().end()
                        && !(*seg_it)->GetLiteral().GetSeq_data().IsGap()) {
                        if ((*seg_it)->GetLiteral().GetSeq_data().IsIupacaa()) {
                            (*seg_it)->SetLiteral().SetSeq_data().SetIupacaa().Set()[i - offset] = c_aa.GetNcbieaa ();
                        } else {
                            (*seg_it)->SetLiteral().SetSeq_data().SetNcbieaa().Set()[i - offset] = c_aa.GetNcbieaa ();
                        }
                    }
                }
            } else if (i == prot_len) {
                // add terminal exception
                const CCode_break::C_Aa& c_aa = brk->GetAa ();
                if (c_aa.IsNcbieaa () && c_aa.GetNcbieaa () == 42) {
                    AddAAToDeltaSeq(prot, c_aa.GetNcbieaa ());
                }
            }
        }
    }

    // remove stop codon from end
    CRef<CDelta_seq> end = prot->SetInst().SetExt().SetDelta().Set().back();
    if (end->IsLiteral() && end->GetLiteral().IsSetSeq_data()) {
        if (end->GetLiteral().GetSeq_data().IsIupacaa()) {
            string& last_seg = end->SetLiteral().SetSeq_data().SetIupacaa().Set();
            if (NStr::EndsWith(last_seg, "*")) {
                last_seg = last_seg.substr(0, last_seg.length() - 1);
                end->SetLiteral().SetLength(last_seg.length());
            }
        } else if (end->GetLiteral().GetSeq_data().IsNcbieaa()) {
            string& last_seg = end->SetLiteral().SetSeq_data().SetNcbieaa().Set();
            if (NStr::EndsWith(last_seg, "*")) {
                last_seg = last_seg.substr(0, last_seg.length() - 1);
                end->SetLiteral().SetLength(last_seg.length());
            }
        }
    }

    // recalculate protein length, check need for ncbieaa - may have been altered by removal of stop codon/transl_except
    prot_len = 0;
    NON_CONST_ITERATE (CDelta_ext::Tdata, seg_it, prot->SetInst().SetExt().SetDelta().Set()) {
        prot_len += (*seg_it)->GetLiteral().GetLength();
        if ((*seg_it)->GetLiteral().IsSetSeq_data()
            && (*seg_it)->GetLiteral().GetSeq_data().IsNcbieaa()) {
            string current = (*seg_it)->GetLiteral().GetSeq_data().GetNcbieaa();
            if (NStr::Find (current, "*") == string::npos) {
                (*seg_it)->SetLiteral().SetSeq_data().SetIupacaa().Set(current);
            }
        }            
    }
    prot->SetInst().SetLength(prot_len);


    if (prot->SetInst().SetExt().SetDelta().Set().size() == 1
        && prot->SetInst().SetExt().SetDelta().Set().front()->IsLiteral()
        && prot->SetInst().SetExt().SetDelta().Set().front()->GetLiteral().IsSetSeq_data()) {
        // only one segment, should be raw rather than delta
        if (prot->SetInst().SetExt().SetDelta().Set().front()->GetLiteral().GetSeq_data().IsIupacaa()) {
            string data = prot->SetInst().SetExt().SetDelta().Set().front()->GetLiteral().GetSeq_data().GetIupacaa().Get();
            prot->SetInst().ResetExt();
            prot->SetInst().SetSeq_data().SetIupacaa().Set(data);
            prot->SetInst().SetRepr(CSeq_inst::eRepr_raw);
        } else if (prot->SetInst().SetExt().SetDelta().Set().front()->GetLiteral().GetSeq_data().IsNcbieaa()) {
            string data = prot->SetInst().SetExt().SetDelta().Set().front()->GetLiteral().GetSeq_data().GetNcbieaa().Get();
            prot->SetInst().ResetExt();
            prot->SetInst().SetSeq_data().SetNcbieaa().Set(data);
            prot->SetInst().SetRepr(CSeq_inst::eRepr_raw);
        }
    }

    return prot;
}


bool CSeqTranslator::ChangeDeltaProteinToRawProtein(CRef<CBioseq> protein)
{
    if (!protein || !protein->IsAa() || !protein->IsSetInst() || !protein->GetInst().IsSetRepr()
        || protein->GetInst().GetRepr() != objects::CSeq_inst::eRepr_delta
        || !protein->GetInst().IsSetExt()
        || !protein->GetInst().GetExt().IsDelta()) {
        return false;
    }
    // can only do this if all elements are literal
    ITERATE (objects::CDelta_ext::Tdata, it, protein->GetInst().GetExt().GetDelta().Get()) {
        if (!(*it)->IsLiteral()) {
            return false;
        }
    }

    objects::CSeqVector prot_vec(*protein);
    prot_vec.SetCoding(objects::CSeq_data::e_Iupacaa);
    string buffer = "";
    prot_vec.GetSeqData(0, protein->GetLength(), buffer);
    protein->SetInst().ResetExt();
    protein->SetInst().SetRepr(objects::CSeq_inst::eRepr_raw);
    protein->SetInst().SetSeq_data().SetIupacaa().Set(buffer);
    protein->SetInst().SetLength(buffer.length());
    return true;
}


void CSeqTranslator::Translate(const string& seq, string& prot,
                               const CGenetic_code* code,
                               bool include_stop,
                               bool remove_trailing_X,
                               bool* alt_start,
                               bool is_5prime_complete)
{
    x_Translate(seq, prot, 0, code,
                is_5prime_complete, include_stop, remove_trailing_X, alt_start);
}


void CSeqTranslator::Translate(const string& seq,
                               string& prot,
                               TTranslationFlags flags,
                               const CGenetic_code* code,
                               bool* alt_start)
{
    x_Translate(seq, prot, 0, code,
                !(flags & fIs5PrimePartial), 
                !(flags & fNoStop), 
                flags & fRemoveTrailingX,
                alt_start);
}


void CSeqTranslator::Translate(const CSeqVector& seq, string& prot,
                               const CGenetic_code* code,
                               bool include_stop,
                               bool remove_trailing_X,
                               bool* alt_start,
                               bool is_5prime_complete)
{
    x_Translate(seq, prot, 0, code,
                is_5prime_complete, include_stop, remove_trailing_X, alt_start);
}


void CSeqTranslator::Translate(const CSeqVector& seq, string& prot,
                               TTranslationFlags flags,
                               const CGenetic_code* code,
                               bool* alt_start)
{
    x_Translate(seq, prot, 0, code,
                !(flags & fIs5PrimePartial), 
                !(flags & fNoStop), 
                flags & fRemoveTrailingX,
                alt_start);
}


void CSeqTranslator::Translate(const CSeq_loc& loc,
                               const CBioseq_Handle& handle,
                               string& prot,
                               const CGenetic_code* code,
                               bool include_stop,
                               bool remove_trailing_X,
                               bool* alt_start)
{
    CSeqVector seq(loc, handle.GetScope(), CBioseq_Handle::eCoding_Iupac);
    x_Translate(seq, prot, 0, code,
                !loc.IsPartialStart(eExtreme_Biological),
                include_stop, remove_trailing_X, alt_start);
}



void CSeqTranslator::Translate(const CSeq_loc& loc,
                               CScope& scope,
                               string& prot,
                               const CGenetic_code* code,
                               bool include_stop,
                               bool remove_trailing_X,
                               bool* alt_start)
{
    CSeqVector seq(loc, scope, CBioseq_Handle::eCoding_Iupac);
    x_Translate(seq, prot, 0, code,
                !loc.IsPartialStart(eExtreme_Biological),
                include_stop, remove_trailing_X, alt_start);
}


void CSeqTranslator::Translate(const CSeq_feat& feat,
                               CScope& scope,
                               string& prot,
                               bool include_stop,
                               bool remove_trailing_X,
                               bool* alt_start)
{
    const CGenetic_code* code = NULL;
    int frame = 0;
    if (feat.GetData().IsCdregion()) {
        const CCdregion& cdr = feat.GetData().GetCdregion();
        if (cdr.IsSetFrame ()) {
            switch (cdr.GetFrame ()) {
            case CCdregion::eFrame_two :
                frame = 1;
                break;
            case CCdregion::eFrame_three :
                frame = 2;
                break;
            default :
                break;
            }
        }
        if (cdr.IsSetCode()) {
            code = &cdr.GetCode();
        }
    }

    bool code_break_include_stop = include_stop;
    if (feat.GetData().IsCdregion()  &&
        feat.GetData().GetCdregion().IsSetCode_break()) {
        code_break_include_stop = true;
    }

    CSeqVector seq(feat.GetLocation(), scope, CBioseq_Handle::eCoding_Iupac);
    x_Translate(seq, prot, frame, code,
                !feat.GetLocation().IsPartialStart(eExtreme_Biological),
                code_break_include_stop, remove_trailing_X, alt_start);


    // code break substitution
    if (feat.GetData().IsCdregion()  &&
        feat.GetData().GetCdregion().IsSetCode_break()) {
        const CCdregion& cdr = feat.GetData().GetCdregion();
        string::size_type protlen = prot.size();
        ITERATE (CCdregion::TCode_break, code_break, cdr.GetCode_break()) {
            const CRef <CCode_break> brk = *code_break;
            const CSeq_loc& cbk_loc = brk->GetLoc();
            TSeqPos seq_pos =
                sequence::LocationOffset(feat.GetLocation(), cbk_loc,
                                         sequence::eOffset_FromStart,
                                         &scope);
            seq_pos -= frame;
            string::size_type i = seq_pos / 3;
            if (i < protlen) {
                const CCode_break::C_Aa& c_aa = brk->GetAa ();
                if (c_aa.IsNcbieaa ()) {
                    prot [i] = c_aa.GetNcbieaa ();
                }
            } else if (i == protlen) {
                // add terminal exception
                const CCode_break::C_Aa& c_aa = brk->GetAa ();
                if (c_aa.IsNcbieaa () && c_aa.GetNcbieaa () == 42) {
                    prot += c_aa.GetNcbieaa ();
                }
            }
        }

        if ( !include_stop ) {
            SIZE_TYPE sz = prot.find_first_of("*");
            if (sz != string::npos) {
                prot.resize(sz);
            }
        }
    }
}



void CCdregion_translate::TranslateCdregion (string& prot,
                                             const CBioseq_Handle& bsh,
                                             const CSeq_loc& loc,
                                             const CCdregion& cdr,
                                             bool include_stop,
                                             bool remove_trailing_X,
                                             bool* alt_start,
                                             ETranslationLengthProblemOptions options)
{
    CSeq_feat feat;
    feat.SetLocation(const_cast<CSeq_loc&>(loc));
    feat.SetData().SetCdregion(const_cast<CCdregion&>(cdr));
    CSeqTranslator::Translate(feat, bsh.GetScope(), prot,
                              include_stop, remove_trailing_X, alt_start);
}


void CCdregion_translate::TranslateCdregion
(string& prot,
 const CSeq_feat& cds,
 CScope& scope,
 bool include_stop,
 bool remove_trailing_X,
 bool* alt_start,
 ETranslationLengthProblemOptions options)
{
    _ASSERT(cds.GetData().IsCdregion());

    prot.erase();

    CBioseq_Handle bsh = scope.GetBioseqHandle(cds.GetLocation());
    if ( !bsh ) {
        return;
    }

    CSeqTranslator::Translate(cds, bsh.GetScope(), prot,
                              include_stop, remove_trailing_X, alt_start);
}


SRelLoc::SRelLoc(const CSeq_loc& parent, const CSeq_loc& child, CScope* scope,
                 SRelLoc::TFlags flags)
    : m_ParentLoc(&parent)
{
    typedef CSeq_loc::TRange TRange0;
    for (CSeq_loc_CI cit(child);  cit;  ++cit) {
        const CSeq_id& cseqid  = cit.GetSeq_id();
        TRange0        crange  = cit.GetRange();
        if (crange.IsWholeTo()  &&  scope) {
            // determine actual end
            crange.SetToOpen(sequence::GetLength(cit.GetSeq_id(), scope));
        }
        ENa_strand     cstrand = cit.GetStrand();
        TSeqPos        pos     = 0;
        for (CSeq_loc_CI pit(parent);  pit;  ++pit) {
            ENa_strand pstrand = pit.GetStrand();
            TRange0    prange  = pit.GetRange();
            if (prange.IsWholeTo()  &&  scope) {
                // determine actual end
                prange.SetToOpen(sequence::GetLength(pit.GetSeq_id(), scope));
            }
            if ( !sequence::IsSameBioseq(cseqid, pit.GetSeq_id(), scope) ) {
                pos += prange.GetLength();
                continue;
            }
            CRef<TRange>         intersection(new TRange);
            TSeqPos              abs_from, abs_to;
            CConstRef<CInt_fuzz> fuzz_from, fuzz_to;
            if (crange.GetFrom() >= prange.GetFrom()) {
                abs_from  = crange.GetFrom();
                fuzz_from = cit.GetFuzzFrom();
                if (abs_from == prange.GetFrom()) {
                    // subtract out parent fuzz, if any
                    const CInt_fuzz* pfuzz = pit.GetFuzzFrom();
                    if (pfuzz) {
                        if (fuzz_from) {
                            CRef<CInt_fuzz> f(new CInt_fuzz);
                            f->Assign(*fuzz_from);
                            f->Subtract(*pfuzz, abs_from, abs_from);
                            if (f->IsP_m()  &&  !f->GetP_m() ) {
                                fuzz_from.Reset(); // cancelled
                            } else {
                                fuzz_from = f;
                            }
                        } else {
                            fuzz_from = pfuzz->Negative(abs_from);
                        }
                    }
                }
            } else {
                abs_from  = prange.GetFrom();
                // fuzz_from = pit.GetFuzzFrom();
                CRef<CInt_fuzz> f(new CInt_fuzz);
                f->SetLim(CInt_fuzz::eLim_lt);
                fuzz_from = f;
            }
            if (crange.GetTo() <= prange.GetTo()) {
                abs_to  = crange.GetTo();
                fuzz_to = cit.GetFuzzTo();
                if (abs_to == prange.GetTo()) {
                    // subtract out parent fuzz, if any
                    const CInt_fuzz* pfuzz = pit.GetFuzzTo();
                    if (pfuzz) {
                        if (fuzz_to) {
                            CRef<CInt_fuzz> f(new CInt_fuzz);
                            f->Assign(*fuzz_to);
                            f->Subtract(*pfuzz, abs_to, abs_to);
                            if (f->IsP_m()  &&  !f->GetP_m() ) {
                                fuzz_to.Reset(); // cancelled
                            } else {
                                fuzz_to = f;
                            }
                        } else {
                            fuzz_to = pfuzz->Negative(abs_to);
                        }
                    }
                }
            } else {
                abs_to  = prange.GetTo();
                // fuzz_to = pit.GetFuzzTo();
                CRef<CInt_fuzz> f(new CInt_fuzz);
                f->SetLim(CInt_fuzz::eLim_gt);
                fuzz_to = f;
            }
            if (abs_from <= abs_to) {
                if (IsReverse(pstrand)) {
                    TSeqPos sigma = pos + prange.GetTo();
                    intersection->SetFrom(sigma - abs_to);
                    intersection->SetTo  (sigma - abs_from);
                    if (fuzz_from) {
                        intersection->SetFuzz_to().AssignTranslated
                            (*fuzz_from, intersection->GetTo(), abs_from);
                        intersection->SetFuzz_to().Negate
                            (intersection->GetTo());
                    }
                    if (fuzz_to) {
                        intersection->SetFuzz_from().AssignTranslated
                            (*fuzz_to, intersection->GetFrom(), abs_to);
                        intersection->SetFuzz_from().Negate
                            (intersection->GetFrom());
                    }
                    if (cstrand == eNa_strand_unknown) {
                        intersection->SetStrand(pstrand);
                    } else {
                        intersection->SetStrand(Reverse(cstrand));
                    }
                } else {
                    TSignedSeqPos delta = pos - prange.GetFrom();
                    intersection->SetFrom(abs_from + delta);
                    intersection->SetTo  (abs_to   + delta);
                    if (fuzz_from) {
                        intersection->SetFuzz_from().AssignTranslated
                            (*fuzz_from, intersection->GetFrom(), abs_from);
                    }
                    if (fuzz_to) {
                        intersection->SetFuzz_to().AssignTranslated
                            (*fuzz_to, intersection->GetTo(), abs_to);
                    }
                    if (cstrand == eNa_strand_unknown) {
                        intersection->SetStrand(pstrand);
                    } else {
                        intersection->SetStrand(cstrand);
                    }
                }
                // add to m_Ranges, combining with the previous
                // interval if possible
                if ( !(flags & fNoMerge)  &&  !m_Ranges.empty()
                    &&  SameOrientation(intersection->GetStrand(),
                                        m_Ranges.back()->GetStrand()) ) {
                    if (m_Ranges.back()->GetTo() == intersection->GetFrom() - 1
                        &&  !IsReverse(intersection->GetStrand()) ) {
                        m_Ranges.back()->SetTo(intersection->GetTo());
                        if (intersection->IsSetFuzz_to()) {
                            m_Ranges.back()->SetFuzz_to
                                (intersection->SetFuzz_to());
                        } else {
                            m_Ranges.back()->ResetFuzz_to();
                        }
                    } else if (m_Ranges.back()->GetFrom()
                               == intersection->GetTo() + 1
                               &&  IsReverse(intersection->GetStrand())) {
                        m_Ranges.back()->SetFrom(intersection->GetFrom());
                        if (intersection->IsSetFuzz_from()) {
                            m_Ranges.back()->SetFuzz_from
                                (intersection->SetFuzz_from());
                        } else {
                            m_Ranges.back()->ResetFuzz_from();
                        }
                    } else {
                        m_Ranges.push_back(intersection);
                    }
                } else {
                    m_Ranges.push_back(intersection);
                }
            }
            pos += prange.GetLength();
        }
    }
}


// Bother trying to merge?
CRef<CSeq_loc> SRelLoc::Resolve(const CSeq_loc& new_parent, CScope* scope,
                                SRelLoc::TFlags /* flags */)
    const
{
    typedef CSeq_loc::TRange TRange0;
    CRef<CSeq_loc> result(new CSeq_loc);
    CSeq_loc_mix&  mix = result->SetMix();
    ITERATE (TRanges, it, m_Ranges) {
        _ASSERT((*it)->GetFrom() <= (*it)->GetTo());
        TSeqPos pos = 0, start = (*it)->GetFrom();
        bool    keep_going = true;
        for (CSeq_loc_CI pit(new_parent);  pit;  ++pit) {
            TRange0 prange = pit.GetRange();
            if (prange.IsWholeTo()  &&  scope) {
                // determine actual end
                prange.SetToOpen(sequence::GetLength(pit.GetSeq_id(), scope));
            }
            TSeqPos length = prange.GetLength();
            if (start >= pos  &&  start < pos + length) {
                TSeqPos              from, to;
                CConstRef<CInt_fuzz> fuzz_from, fuzz_to;
                ENa_strand           strand;
                if (IsReverse(pit.GetStrand())) {
                    TSeqPos sigma = pos + prange.GetTo();
                    from = sigma - (*it)->GetTo();
                    to   = sigma - start;
                    if (from < prange.GetFrom()  ||  from > sigma) {
                        from = prange.GetFrom();
                        keep_going = true;
                    } else {
                        keep_going = false;
                    }
                    if ( !(*it)->IsSetStrand()
                        ||  (*it)->GetStrand() == eNa_strand_unknown) {
                        strand = pit.GetStrand();
                    } else {
                        strand = Reverse((*it)->GetStrand());
                    }
                    if (from == prange.GetFrom()) {
                        fuzz_from = pit.GetFuzzFrom();
                    }
                    if ( !keep_going  &&  (*it)->IsSetFuzz_to() ) {
                        CRef<CInt_fuzz> f(new CInt_fuzz);
                        if (fuzz_from) {
                            f->Assign(*fuzz_from);
                        } else {
                            f->SetP_m(0);
                        }
                        f->Subtract((*it)->GetFuzz_to(), from, (*it)->GetTo(),
                                    CInt_fuzz::eAmplify);
                        if (f->IsP_m()  &&  !f->GetP_m() ) {
                            fuzz_from.Reset(); // cancelled
                        } else {
                            fuzz_from = f;
                        }
                    }
                    if (to == prange.GetTo()) {
                        fuzz_to = pit.GetFuzzTo();
                    }
                    if (start == (*it)->GetFrom()
                        &&  (*it)->IsSetFuzz_from()) {
                        CRef<CInt_fuzz> f(new CInt_fuzz);
                        if (fuzz_to) {
                            f->Assign(*fuzz_to);
                        } else {
                            f->SetP_m(0);
                        }
                        f->Subtract((*it)->GetFuzz_from(), to,
                                    (*it)->GetFrom(), CInt_fuzz::eAmplify);
                        if (f->IsP_m()  &&  !f->GetP_m() ) {
                            fuzz_to.Reset(); // cancelled
                        } else {
                            fuzz_to = f;
                        }
                    }
                } else {
                    TSignedSeqPos delta = prange.GetFrom() - pos;
                    from = start          + delta;
                    to   = (*it)->GetTo() + delta;
                    if (to > prange.GetTo()) {
                        to = prange.GetTo();
                        keep_going = true;
                    } else {
                        keep_going = false;
                    }
                    if ( !(*it)->IsSetStrand()
                        ||  (*it)->GetStrand() == eNa_strand_unknown) {
                        strand = pit.GetStrand();
                    } else {
                        strand = (*it)->GetStrand();
                    }
                    if (from == prange.GetFrom()) {
                        fuzz_from = pit.GetFuzzFrom();
                    }
                    if (start == (*it)->GetFrom()
                        &&  (*it)->IsSetFuzz_from()) {
                        CRef<CInt_fuzz> f(new CInt_fuzz);
                        if (fuzz_from) {
                            f->Assign(*fuzz_from);
                            f->Add((*it)->GetFuzz_from(), from,
                                   (*it)->GetFrom());
                        } else {
                            f->AssignTranslated((*it)->GetFuzz_from(), from,
                                                (*it)->GetFrom());
                        }
                        if (f->IsP_m()  &&  !f->GetP_m() ) {
                            fuzz_from.Reset(); // cancelled
                        } else {
                            fuzz_from = f;
                        }
                    }
                    if (to == prange.GetTo()) {
                        fuzz_to = pit.GetFuzzTo();
                    }
                    if ( !keep_going  &&  (*it)->IsSetFuzz_to() ) {
                        CRef<CInt_fuzz> f(new CInt_fuzz);
                        if (fuzz_to) {
                            f->Assign(*fuzz_to);
                            f->Add((*it)->GetFuzz_to(), to, (*it)->GetTo());
                        } else {
                            f->AssignTranslated((*it)->GetFuzz_to(), to,
                                                (*it)->GetTo());
                        }
                        if (f->IsP_m()  &&  !f->GetP_m() ) {
                            fuzz_to.Reset(); // cancelled
                        } else {
                            fuzz_to = f;
                        }
                    }
                }
                if (from == to
                    &&  (fuzz_from == fuzz_to
                         ||  (fuzz_from.GetPointer()  &&  fuzz_to.GetPointer()
                              &&  fuzz_from->Equals(*fuzz_to)))) {
                    // just a point
                    CRef<CSeq_loc> loc(new CSeq_loc);
                    CSeq_point& point = loc->SetPnt();
                    point.SetPoint(from);
                    if (strand != eNa_strand_unknown) {
                        point.SetStrand(strand);
                    }
                    if (fuzz_from) {
                        point.SetFuzz().Assign(*fuzz_from);
                    }
                    point.SetId().Assign(pit.GetSeq_id());
                    mix.Set().push_back(loc);
                } else {
                    CRef<CSeq_loc> loc(new CSeq_loc);
                    CSeq_interval& ival = loc->SetInt();
                    ival.SetFrom(from);
                    ival.SetTo(to);
                    if (strand != eNa_strand_unknown) {
                        ival.SetStrand(strand);
                    }
                    if (fuzz_from) {
                        ival.SetFuzz_from().Assign(*fuzz_from);
                    }
                    if (fuzz_to) {
                        ival.SetFuzz_to().Assign(*fuzz_to);
                    }
                    ival.SetId().Assign(pit.GetSeq_id());
                    mix.Set().push_back(loc);
                }
                if (keep_going) {
                    start = pos + length;
                } else {
                    break;
                }
            }
            pos += length;
        }
        if (keep_going) {
            TSeqPos total_length;
            string  label;
            new_parent.GetLabel(&label);
            try {
                total_length = sequence::GetLength(new_parent, scope);
                ERR_POST_X(8, Warning << "SRelLoc::Resolve: Relative position "
                           << start << " exceeds length (" << total_length
                           << ") of parent location " << label);
            } catch (CObjmgrUtilException) {
                ERR_POST_X(9, Warning << "SRelLoc::Resolve: Relative position "
                           << start
                           << " exceeds length (?\?\?) of parent location "
                           << label);
            }
        }
    }
    // clean up output
    switch (mix.Get().size()) {
    case 0:
        result->SetNull();
        break;
    case 1:
    {{
        CRef<CSeq_loc> first = mix.Set().front();
        result = first;
        break;
    }}
    default:
        break;
    }
    return result;
}


//============================================================================//
//                                 SeqSearch                                  //
//============================================================================//

// Public:
// =======

// Constructors and Destructors:
CSeqSearch::CSeqSearch(IClient *client, TSearchFlags flags) :
    m_Client(client), m_Flags(flags), m_LongestPattern(0), m_Fsa(true)
{
}


CSeqSearch::~CSeqSearch(void)
{
}


typedef SStaticPair<Char, Char> TCharPair;
static const TCharPair sc_comp_tbl[32] = {
    // uppercase
    { 'A', 'T' },
    { 'B', 'V' },
    { 'C', 'G' },
    { 'D', 'H' },
    { 'G', 'C' },
    { 'H', 'D' },
    { 'K', 'M' },
    { 'M', 'K' },
    { 'N', 'N' },
    { 'R', 'Y' },
    { 'S', 'S' },
    { 'T', 'A' },
    { 'U', 'A' },
    { 'V', 'B' },
    { 'W', 'W' },
    { 'Y', 'R' },
    // lowercase
    { 'a', 'T' },
    { 'b', 'V' },
    { 'c', 'G' },
    { 'd', 'H' },
    { 'g', 'C' },
    { 'h', 'D' },
    { 'k', 'M' },
    { 'm', 'K' },
    { 'n', 'N' },
    { 'r', 'Y' },
    { 's', 'S' },
    { 't', 'A' },
    { 'u', 'A' },
    { 'v', 'B' },
    { 'w', 'W' },
    { 'y', 'R' },
};
typedef CStaticPairArrayMap<Char, Char> TComplement;
DEFINE_STATIC_ARRAY_MAP(TComplement, sc_Complement, sc_comp_tbl);


inline
static char s_GetComplement(char c)
{
    TComplement::const_iterator comp_it = sc_Complement.find(c);
    return (comp_it != sc_Complement.end()) ? comp_it->second : '\0';
}


static string s_GetReverseComplement(const string& sequence)
{
    string revcomp;
    revcomp.reserve(sequence.length());
    string::const_reverse_iterator rend = sequence.rend();

    for (string::const_reverse_iterator rit = sequence.rbegin(); rit != rend; ++rit) {
        revcomp += s_GetComplement(*rit);
    }

    return revcomp;
}


void CSeqSearch::AddNucleotidePattern
(const string& name,
 const string& sequence,
 Int2          cut_site,
 TSearchFlags  flags)
{
    if (NStr::IsBlank(name)  ||  NStr::IsBlank(sequence)) {
        NCBI_THROW(CUtilException, eNoInput, "Empty input value");
    }

    // cleanup pattern
    string pattern = sequence;
    NStr::TruncateSpaces(pattern);
    NStr::ToUpper(pattern);

    string revcomp = s_GetReverseComplement(pattern);
    bool symmetric = (pattern == revcomp);
    ENa_strand strand = symmetric ? eNa_strand_both : eNa_strand_plus;

    // record expansion of entered pattern
    x_AddNucleotidePattern(name, pattern, cut_site, strand, flags);

    // record expansion of reverse complement of asymmetric pattern
    if (!symmetric  &&  (!x_IsJustTopStrand(flags))) {
        size_t revcomp_cut_site = pattern.length() - cut_site;
        x_AddNucleotidePattern(name, revcomp, revcomp_cut_site,
            eNa_strand_minus, flags);
    }
}


// Program passes each character in turn to finite state machine.
int CSeqSearch::Search
(int  current_state,
 char ch,
 int  position,
 int  length)
{
    if (m_Client == NULL) {
        return 0;
    }

    // on first character, populate state transition table
    if (!m_Fsa.IsPrimed()) {
        m_Fsa.Prime();
    }
    
    int next_state = m_Fsa.GetNextState(current_state, ch);
    
    // report matches (if any)
    if (m_Fsa.IsMatchFound(next_state)) {
        ITERATE(vector<TPatternInfo>, it, m_Fsa.GetMatches(next_state)) {
            int start = position - it->GetSequence().length() + 1;

            // prevent multiple reports of patterns for circular sequences.
            if (start < length) {
                bool keep_going = m_Client->OnPatternFound(*it, start);
                if (!keep_going) {
                    break;
                }
            }
        }
    }

    return next_state;
}


// Search entire bioseq.
void CSeqSearch::Search(const CBioseq_Handle& bsh)
{
    if (!bsh  ||  m_Client == NULL) {
        return;
    }

    CSeqVector seq_vec = bsh.GetSeqVector(CBioseq_Handle::eCoding_Iupac);
    size_t seq_len = seq_vec.size();
    size_t search_len = seq_len;

    // handle circular bioseqs
    CSeq_inst::ETopology topology = bsh.GetInst_Topology();
    if (topology == CSeq_inst::eTopology_circular) {
        search_len += m_LongestPattern - 1;
    }
    
    int state = m_Fsa.GetInitialState();

    for (size_t i = 0; i < search_len; ++i) {
        state = Search(state, seq_vec[i % seq_len], i, seq_len);
    }
}


// Private:
// ========

/// translation finite state machine base codes - ncbi4na
enum EBaseCode {
    eBase_A = 1,  ///< A
    eBase_C,      ///< C
    eBase_M,      ///< AC
    eBase_G,      ///< G
    eBase_R,      ///< AG
    eBase_S,      ///< CG
    eBase_V,      ///< ACG
    eBase_T,      ///< T
    eBase_W,      ///< AT
    eBase_Y,      ///< CT
    eBase_H,      ///< ACT
    eBase_K,      ///< GT
    eBase_D,      ///< AGT
    eBase_B,      ///< CGT
    eBase_N       ///< ACGT
};

/// conversion table from Ncbi4na / Iupacna to EBaseCode
static const EBaseCode sc_CharToEnum[256] = {
    // Ncbi4na
    eBase_N, eBase_A, eBase_C, eBase_M,
    eBase_G, eBase_R, eBase_S, eBase_V,
    eBase_T, eBase_W, eBase_Y, eBase_H,
    eBase_K, eBase_D, eBase_B, eBase_N,

    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    // Iupacna (uppercase)
    eBase_N, eBase_A, eBase_B, eBase_C,
    eBase_D, eBase_N, eBase_N, eBase_G,
    eBase_H, eBase_N, eBase_N, eBase_K,
    eBase_N, eBase_M, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_R, eBase_S,
    eBase_T, eBase_T, eBase_V, eBase_W,
    eBase_N, eBase_Y, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    // Iupacna (lowercase)
    eBase_N, eBase_A, eBase_B, eBase_C,
    eBase_D, eBase_N, eBase_N, eBase_G,
    eBase_H, eBase_N, eBase_N, eBase_K,
    eBase_N, eBase_M, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_R, eBase_S,
    eBase_T, eBase_T, eBase_V, eBase_W,
    eBase_N, eBase_Y, eBase_N, eBase_N,

    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N
};

static const char sc_EnumToChar[16] = {
    '\0', 'A', 'C', 'M', 'G', 'R', 'S', 'V', 'T', 'W', 'Y', 'H', 'K', 'D', 'B', 'N'
};


void CSeqSearch::x_AddNucleotidePattern
(const string& name,
 string& pattern,
 Int2 cut_site,
 ENa_strand strand,
 TSearchFlags flags)
{
    if (pattern.length() > m_LongestPattern) {
        m_LongestPattern = pattern.length();
    }
    
    TPatternInfo pat_info(name, kEmptyStr, cut_site);
    pat_info.m_Strand = strand;

    if (!x_IsExpandPattern(flags)) {
        pat_info.m_Sequence = pattern;
        x_AddPattern(pat_info, pattern, flags);
    } else {
        string buffer;
        buffer.reserve(pattern.length());

        x_ExpandPattern(pattern, buffer, 0, pat_info, flags);
    }
}


void CSeqSearch::x_ExpandPattern
(string& sequence,
 string& buf,
 size_t pos,
 TPatternInfo& pat_info,
 TSearchFlags flags)
{
    static const EBaseCode expansion[] = { eBase_A, eBase_C, eBase_G, eBase_T };

    if (pos < sequence.length()) {
        Uint4 code = static_cast<Uint4>(sc_CharToEnum[static_cast<Uint1>(sequence[pos])]);

        for (int i = 0; i < 4; ++i) {
            if ((code & expansion[i]) != 0) {
                buf += sc_EnumToChar[expansion[i]];
                x_ExpandPattern(sequence, buf, pos + 1, pat_info, flags);
                buf.erase(pos);
            }
        }
    } else {
        // when position reaches pattern length, store one expanded string.
        x_AddPattern(pat_info, buf, flags);
    }
}


void CSeqSearch::x_AddPattern(TPatternInfo& pat_info, string& sequence, TSearchFlags flags)
{
    x_StorePattern(pat_info, sequence);

    if (x_IsAllowMismatch(flags)) {
        // put 'N' at every position if a single mismatch is allowed.
        char ch = 'N';
        NON_CONST_ITERATE (string, it, sequence) {
            swap(*it, ch);
        
            x_StorePattern(pat_info, sequence);

            // restore proper character, go on to put N in next position.
            swap(*it, ch);
        }
    }
}


void CSeqSearch::x_StorePattern(TPatternInfo& pat_info, string& sequence)
{
    pat_info.m_Sequence = sequence;
    m_Fsa.AddWord(sequence, pat_info);
}


void ReverseComplement(CSeq_inst& inst, CScope* scope)
{
    switch (inst.GetRepr()) {
        case CSeq_inst::eRepr_raw:
            CSeqportUtil::ReverseComplement(&(inst.SetSeq_data()), 0, inst.GetLength());
            break;
        case CSeq_inst::eRepr_delta:
            if (!inst.IsSetExt() || !inst.GetExt().IsDelta()) {
                NCBI_THROW(CObjmgrUtilException, eBadSequenceType,
                   "Sequence of this type cannot be reverse-complemented.");
            }
            // reverse order of segments
            inst.SetExt().SetDelta().Set().reverse();
            // reverse-complement individual segments
            NON_CONST_ITERATE(CSeq_inst::TExt::TDelta::Tdata, it, inst.SetExt().SetDelta().Set()) {
                switch ((*it)->Which()) {
                    case CDelta_seq::e_Literal:
                        if ((*it)->GetLiteral().IsSetSeq_data()) {    
                            CSeq_literal& lit = (*it)->SetLiteral();
                            if (!lit.GetSeq_data().IsGap()) {
                                CSeqportUtil::ReverseComplement(&(lit.SetSeq_data()), 0, lit.GetLength());
                            }
                        }
                        break;
                    case CDelta_seq::e_Loc:
                        {{
                            CSeq_loc* flip = sequence::SeqLocRevCmpl((*it)->SetLoc(), scope);
                            (*it)->SetLoc().Assign(*flip);
                        }}
                        break;
                    default:
                        // do nothing
                        break;
                }
            }
            break;
        default:
            NCBI_THROW(CObjmgrUtilException, eBadSequenceType,
                "Sequence of this type cannot be reverse-complemented.");
            break;
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE
