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
* Author:  Aaron Ucko, NCBI
*          Mati Shomrat
*
* File Description:
*   new (early 2003) flat-file generator -- context needed when (pre)formatting
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Seg_ext.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seqloc/Textseq_id.hpp>
#include <objects/seqloc/Patent_seq_id.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/misc/sequence_macros.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/seq_entry_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/seq_loc_mapper.hpp>
#include <objmgr/seq_map.hpp>
#include <objmgr/seq_map_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/annot_ci.hpp>

#include <objtools/format/context.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
USING_SCOPE(sequence);


/////////////////////////////////////////////////////////////////////////////
//
// CBioseqContext

// constructor
CBioseqContext::CBioseqContext
(const CBioseq_Handle& seq,
 CFlatFileContext& ffctx,
 CMasterContext* mctx,
 CTopLevelSeqEntryContext *tlsec) :
    m_Handle(seq),
    m_pOpticalMapPoints(NULL),
    m_Repr(CSeq_inst::eRepr_not_set),
    m_Mol(CSeq_inst::eMol_not_set),
    m_HasParts(false),
    m_IsPart(false),
    m_PartNumber(0),
    m_IsDeltaLitOnly(false),
    m_IsProt(false),
    m_IsInGPS(false),
    m_IsInNucProt(false),
    m_IsGED(false),
    m_IsGenbank(false),
    m_IsEMBL(false),
    m_IsDDBJ(false),
    m_IsPDB(false),
    m_IsSP(false),
    m_IsTPA(false),
    m_IsJournalScan(false),
    m_IsRefSeq(false),
    m_RefseqInfo(0),
    m_IsGbGenomeProject(false),  // GenBank Genome project data (AE)
    m_IsNcbiCONDiv(false),       // NCBI CON division (CH)
    m_IsNcbiGenomes(false),
    m_IsPatent(false),
    m_IsGI(false),
    m_IsWGS(false),
    m_IsWGSMaster(false),
    m_IsTSA(false),
    m_IsTSAMaster(false),
    m_IsHup(false),
    m_Gi(0),
    m_ShowGBBSource(false),
    m_PatSeqid(0),
    m_HasOperon(false),
    m_HasMultiIntervalGenes(true), // true is the safe choice if we're not sure
    m_IsGenomeAssembly(false),
    m_IsCrossKingdom(false),
    m_fUnverified(fUnverified_None),
    m_ShowAnnotCommentAsCOMMENT(false),
    m_FFCtx(ffctx),
    m_Master(mctx),
    m_TLSeqEntryCtx(tlsec)
{
    x_Init(seq, m_FFCtx.GetLocation());
}


CBioseqContext::CBioseqContext
(const CBioseq_Handle& prev_seq, 
 const CBioseq_Handle& seq,
 const CBioseq_Handle& next_seq,
 CFlatFileContext& ffctx,
 CMasterContext* mctx,
 CTopLevelSeqEntryContext *tlsec) :
    m_PrevHandle(prev_seq),
    m_Handle(seq),
    m_NextHandle(next_seq),
    m_pOpticalMapPoints(NULL),
    m_Repr(CSeq_inst::eRepr_not_set),
    m_Mol(CSeq_inst::eMol_not_set),
    m_HasParts(false),
    m_IsPart(false),
    m_PartNumber(0),
    m_IsDeltaLitOnly(false),
    m_IsProt(false),
    m_IsInGPS(false),
    m_IsInNucProt(false),
    m_IsGED(false),
    m_IsGenbank(false),
    m_IsEMBL(false),
    m_IsDDBJ(false),
    m_IsPDB(false),
    m_IsSP(false),
    m_IsTPA(false),
    m_IsJournalScan(false),
    m_IsRefSeq(false),
    m_RefseqInfo(0),
    m_IsGbGenomeProject(false),  // GenBank Genome project data (AE)
    m_IsNcbiCONDiv(false),       // NCBI CON division (CH)
    m_IsNcbiGenomes(false),
    m_IsPatent(false),
    m_IsGI(false),
    m_IsWGS(false),
    m_IsWGSMaster(false),
    m_IsTSA(false),
    m_IsTSAMaster(false),
    m_IsHup(false),
    m_Gi(0),
    m_ShowGBBSource(false),
    m_PatSeqid(0),
    m_HasOperon(false),
    m_HasMultiIntervalGenes(true), // true is the safe choice if we're not sure
    m_IsGenomeAssembly(false),
    m_IsCrossKingdom(false),
    m_fUnverified(fUnverified_None),
    m_ShowAnnotCommentAsCOMMENT(false),
    m_FFCtx(ffctx),
    m_Master(mctx),
    m_TLSeqEntryCtx(tlsec)
{
    x_Init(seq, m_FFCtx.GetLocation());
}


// destructor
CBioseqContext::~CBioseqContext(void)
{
    if (m_Virtual) {
        m_Virtual.GetEditHandle().Remove();
    }
}


CSeq_id_Handle CBioseqContext::GetPreferredSynonym(const CSeq_id& id) const
{
    if ( id.IsGi()  &&  id.GetGi() == m_Gi ) {
        return CSeq_id_Handle::GetHandle(*m_PrimaryId);
    }

    CSeq_id_Handle idh =
        sequence::GetId(id, m_Handle.GetScope(), sequence::eGetId_Best);
    return idh;
}



// initialization
void CBioseqContext::x_Init(const CBioseq_Handle& seq, const CSeq_loc* user_loc)
{
    _ASSERT(seq);
    _ASSERT(seq.IsSetInst());

    // NB: order of execution is important
    m_Repr = x_GetRepr();
    x_SetId();
    m_Mol  = seq.GetInst_Mol();
    m_Molinfo.Reset(x_GetMolInfo());

    if ( IsSegmented() ) {
        m_HasParts = x_HasParts();
    }
    m_IsPart = x_IsPart();
    if ( m_IsPart ) {
        _ASSERT(m_Master);
        m_PartNumber = x_GetPartNumber();
    }
    if ( IsDelta() ) {
        m_IsDeltaLitOnly = x_IsDeltaLitOnly();
    }

    m_IsProt = CSeq_inst::IsAa(seq.GetInst_Mol());

    m_IsInGPS     = x_IsInGPS();
    m_IsInNucProt = x_IsInNucProt();

    x_SetLocation(user_loc);
    
    x_SetDataFromUserObjects();

    x_SetDataFromAnnot();
    
    m_HasOperon = x_HasOperon();

    if (IsRefSeq()) {
        m_FFCtx.GetConfig().SetRefSeqConventions();
    }

    SAnnotSelector sel = SetAnnotSelector();
    sel.SetResolveAll();

    x_SetHasMultiIntervalGenes();

    x_SetTaxname();

    x_SetOpticalMapPoints();
}


void CBioseqContext::x_SetLocation(const CSeq_loc* user_loc)
{
    CRef<CSeq_loc> loc;

    if (user_loc != NULL) {
        // map the user location to the current bioseq
        CSeq_id_Handle idh1 = CSeq_id_Handle::GetHandle(*m_Handle.GetSeqId());
        CSeq_id_Handle idh2 = sequence::GetIdHandle(*user_loc, &m_Handle.GetScope());
        if ( !sequence::IsSameBioseq(idh1, idh2, &m_Handle.GetScope()) ) {
            CSeq_loc_Mapper mapper(m_Handle, CSeq_loc_Mapper::eSeqMap_Up);
            loc.Reset(mapper.Map(*user_loc));
        } else {
            loc.Reset(new CSeq_loc);
            loc->Assign(*user_loc);
        }

        if (loc) {
            if (loc->IsWhole()) {
                loc.Reset();
            } else if (loc->IsInt()) {
                CSeq_loc::TRange range = loc->GetTotalRange();
                if (range.GetFrom() == 0  &&  range.GetTo() == m_Handle.GetInst_Length() - 1) {
                    loc.Reset();
                }
            }
        }
    }

    // if no partial location specified do the entire bioseq
    if (!loc) {
        loc.Reset(new CSeq_loc);
        loc->SetWhole(*m_PrimaryId);
    } else {
        x_SetMapper(*loc);
    }

    m_Location = loc;
}


void CBioseqContext::x_SetMapper(const CSeq_loc& loc)
{
    _ASSERT(GetBioseqFromSeqLoc(loc, GetScope()) == m_Handle);

    // not covering the entire bioseq (may be multiple ranges)
    CRef<CBioseq> vseq(new CBioseq(loc, GetAccession()));
    vseq->SetInst().SetRepr(CSeq_inst::eRepr_virtual);
    CBioseq_Handle vseqh = GetScope().AddBioseq(*vseq);

    if (vseqh) {
        m_Mapper.Reset(new CSeq_loc_Mapper(vseqh, CSeq_loc_Mapper::eSeqMap_Up));
        m_Mapper->SetMergeAbutting();
        m_Mapper->SetGapRemove();
        //m_Mapper->KeepNonmappingRanges();
    }
}

void CBioseqContext::x_SetHasMultiIntervalGenes(void)
{
    m_HasMultiIntervalGenes = false;

    SAnnotSelector sel;
    sel.SetFeatType( CSeqFeatData::e_Gene );

    CFeat_CI gene_ci( m_Handle, sel );
    for( ; gene_ci ; ++gene_ci ) {
        switch( gene_ci->GetLocation().Which() ) {
            case CSeq_loc::e_Packed_int:
            case CSeq_loc::e_Packed_pnt:
            case CSeq_loc::e_Mix:
            case CSeq_loc::e_Equiv:
                m_HasMultiIntervalGenes = true;
                break;
            default:
                // do nothing
                break;
        }
        if( m_HasMultiIntervalGenes ) {
            break;
        }
    }
}

void CBioseqContext::x_SetTaxname(void)
{
    // look for taxname in Seqdescs
    int num_super_kingdom = 0;
    bool super_kingdoms_different = false;
    string super_kingdom_name;
    CSeqdesc_CI desc_ci( m_Handle, CSeqdesc::e_Source );
    for( ; desc_ci; ++desc_ci ) {
        if( desc_ci->IsSource() ) {
            const CBioSource &bsrc = desc_ci->GetSource();
            if (bsrc.IsSetOrgname()) {
                const COrgName& onp = bsrc.GetOrgname();
                if (onp.IsSetName()) {
                    const COrgName::TName& nam = onp.GetName();
                    if (nam.IsPartial()) {
                        const CPartialOrgName& pon = nam.GetPartial();
                        if (pon.IsSet()) {
                            const CPartialOrgName::Tdata& tx = pon.Get();
                            ITERATE (CPartialOrgName::Tdata, itr, tx) {
                                const CTaxElement& te = **itr;
                                if (te.IsSetFixed_level()) {
                                    if (te.GetFixed_level() == 0 && te.IsSetLevel()) {
                                        const string& lvl = te.GetLevel();
                                        if (NStr::EqualNocase (lvl, "superkingdom")) {
                                            num_super_kingdom++;
                                            if (super_kingdom_name.empty() && te.IsSetName()) {
                                                super_kingdom_name = te.GetName();
                                            } else if (te.IsSetName() && ! NStr::EqualNocase (super_kingdom_name, te.GetName())) {
                                                super_kingdoms_different = true;
                                            }
                                            if (num_super_kingdom > 1 && super_kingdoms_different) {
                                                m_IsCrossKingdom = true;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            if( bsrc.IsSetTaxname() && ! bsrc.GetTaxname().empty() ) {
                // we found a taxname; but need to look at all descriptors to set m_IsCrossKingdom, so keep going
                m_Taxname = bsrc.GetTaxname();
                // return;
            }
        }
    }

    if (! m_Taxname.empty()) {
        return;
    }

    // fall back on the Seq-feats

    SAnnotSelector sel;
    sel.SetFeatType( CSeqFeatData::e_Biosrc );

    CFeat_CI biosrc_ci( m_Handle, sel );
    for( ; biosrc_ci ; ++biosrc_ci ) {
        CConstRef<CSeq_feat> seq_feat = biosrc_ci->GetSeq_feat();
        if( seq_feat && seq_feat->IsSetData() ) {
            const CSeqFeatData & seq_feat_data = seq_feat->GetData();
            if( seq_feat_data.IsBiosrc() ) {
                const CBioSource & bsrc = seq_feat_data.GetBiosrc();
                if( bsrc.IsSetTaxname() && ! bsrc.GetTaxname().empty() ) {
                    // we found a taxname; we're done
                    m_Taxname = bsrc.GetTaxname();
                    return;
                }
            }
        }
    }
}

void CBioseqContext::x_SetFiletrackURL(const CUser_object& uo)
{
    if ( ! FIELD_IS_SET_AND_IS(uo, Type, Str) || 
        ! NStr::EqualNocase(uo.GetType().GetStr(), "FileTrack")) 
    {
        return;
    }
    CConstRef<CUser_field> pFileTrackURLField =
        uo.GetFieldRef("FileTrackURL");
    if( ! pFileTrackURLField || 
        ! FIELD_IS_SET_AND_IS(*pFileTrackURLField, Data, Str) ||
        pFileTrackURLField->GetData().GetStr().empty() )
    {
        return;
    }
    m_FiletrackURL = pFileTrackURLField->GetData().GetStr();
}

void CBioseqContext::x_SetAuthorizedAccess(const CUser_object& uo)
{
    if ( ! FIELD_IS_SET_AND_IS(uo, Type, Str) || 
        ! NStr::EqualNocase(uo.GetType().GetStr(), "AuthorizedAccess")) 
    {
        return;
    }
    CConstRef<CUser_field> pAuthorizedAccessField =
        uo.GetFieldRef("Study");
    if( ! pAuthorizedAccessField || 
        ! FIELD_IS_SET_AND_IS(*pAuthorizedAccessField, Data, Str) ||
        pAuthorizedAccessField->GetData().GetStr().empty() )
    {
        return;
    }
    m_AuthorizedAccess = pAuthorizedAccessField->GetData().GetStr();
}

void CBioseqContext::x_SetOpticalMapPoints(void)
{  
    if( GetRepr() != CSeq_inst::eRepr_map || 
        ! FIELD_IS_SET_AND_IS(m_Handle, Inst_Ext, Map) )
    {
       return;
    }

    const CMap_ext & map_ext = m_Handle.GetInst_Ext().GetMap();
    FOR_EACH_SEQFEAT_ON_MAPEXT(feat_it, map_ext ) {
        const CSeq_feat & feat = **feat_it;
        if( ! FIELD_IS_SET_AND_IS(feat, Data, Rsite) || 
            ! feat.IsSetLocation() )
        {
            continue;
        }
        const CSeq_loc & feat_loc = feat.GetLocation();
        switch( feat_loc.Which() ) {
        case CSeq_loc::e_Pnt: {
            const CSeq_point & seq_point = feat_loc.GetPnt();

            if( seq_point.IsSetPoint() ) {
                m_pOpticalMapPointsDestroyer.reset( new CPacked_seqpnt );
                CLONE_IF_SET_ELSE_RESET(*m_pOpticalMapPointsDestroyer, Fuzz, 
                    seq_point, Fuzz);
                CLONE_IF_SET_ELSE_RESET(*m_pOpticalMapPointsDestroyer, Id,
                    seq_point, Id);
                ASSIGN_IF_SET_ELSE_RESET(*m_pOpticalMapPointsDestroyer, Strand,
                    seq_point, Strand);
                m_pOpticalMapPointsDestroyer->AddPoint( seq_point.GetPoint() );

                m_pOpticalMapPoints = m_pOpticalMapPointsDestroyer.get();
            }
            break;
        }
        case CSeq_loc::e_Packed_pnt:
            m_pOpticalMapPoints = & feat_loc.GetPacked_pnt();
            // in case a previous iteration set this
            m_pOpticalMapPointsDestroyer.reset(); 
            break;
        default:
            // ignore other types
            break;
        }
    }
}

void CBioseqContext::x_SetDataFromUserObjects(void)
{
    // translate finishing status
    typedef SStaticPair<const char *, const char *>  TFinStatElem;
    static const TFinStatElem sc_finstat_map[] = {
        { "Annotation-directed-improvement", "ANNOTATION_DIRECTED_IMPROVEMENT" },
        { "High-quality-draft",              "HIGH_QUALITY_DRAFT" },
        { "Improved-high-quality-draft",     "IMPROVED_HIGH_QUALITY_DRAFT" },
        { "Noncontiguous-finished",          "NONCONTIGUOUS_FINISHED" },
        { "Standard-draft",                  "STANDARD_DRAFT" }
    };
    typedef CStaticArrayMap<const char *, const char *, PNocase_CStr> TFinStatMap;
    DEFINE_STATIC_ARRAY_MAP(TFinStatMap, sc_FinStatMap, sc_finstat_map);

    for (CSeqdesc_CI it(m_Handle, CSeqdesc::e_User);  it;  ++it) {
        const CUser_object& uo = it->GetUser();
        if (uo.IsSetType()  &&  uo.GetType().IsStr()) {
            if (NStr::EqualNocase(uo.GetType().GetStr(), "StructuredComment")) {
                if( uo.IsSetData() ) {
                    ITERATE( CUser_object::TData, field_iter, uo.GetData() ) {
                        const CUser_field &field = **field_iter;
                        if( ! field.IsSetData() || ! field.GetData().IsStr() ||
                            ! field.IsSetLabel() || ! field.GetLabel().IsStr() ) {
                            continue;
                        }
                        if( field.GetLabel().GetStr() == "StructuredCommentPrefix" && 
                            field.GetData().GetStr() == "##Genome-Assembly-Data-START##" ) 
                        {
                            m_IsGenomeAssembly = true;
                        }
                        if( field.GetLabel().GetStr() == "Current Finishing Status" )
                        {
                            string asn_fin_stat = field.GetData().GetStr();
                            replace( asn_fin_stat.begin(), asn_fin_stat.end(), ' ', '-' ); 
                            TFinStatMap::const_iterator new_fin_stat_iter = sc_FinStatMap.find(asn_fin_stat.c_str());
                            if( new_fin_stat_iter != sc_FinStatMap.end() ) {
                                m_FinishingStatus = new_fin_stat_iter->second;
                            }
                        }
                    }
                }
            } else if(NStr::EqualNocase(uo.GetType().GetStr(), "Unverified")) {
                if( uo.IsSetData() ) {
                    ITERATE( CUser_object::TData, field_iter, uo.GetData() ) {
                        const CUser_field &field = **field_iter;
                        if( ! field.IsSetLabel() || ! field.GetLabel().IsStr() ||
                            field.GetLabel().GetStr() != "Type" ) 
                        {
                                continue;
                        }
                        if( ! field.IsSetData() || ! field.GetData().IsStr() )
                        {
                            continue;
                        }
                        if( NStr::EqualNocase( field.GetData().GetStr(), "Organism") ) {
                            m_fUnverified |= fUnverified_Organism;
                        } else if( NStr::EqualNocase( field.GetData().GetStr(), "Features") ) {
                            m_fUnverified |= fUnverified_SequenceOrAnnotation;
                        }
                    }
                }
                // default in the past was to use feature
                if( m_fUnverified == fUnverified_None ) {
                    m_fUnverified = fUnverified_SequenceOrAnnotation;
                }
            } else if( NStr::EqualNocase(uo.GetType().GetStr(), "FileTrack") ) {
                x_SetFiletrackURL(uo);
            } else if( NStr::EqualNocase(uo.GetType().GetStr(), "AuthorizedAccess") ) {
                x_SetAuthorizedAccess(uo);
            } else if( NStr::EqualNocase(uo.GetType().GetStr(), "ENCODE") ) {
                x_SetEncode(uo);
            }
        }
    }
}

void CBioseqContext::x_SetDataFromAnnot(void)
{
    if( GetRepr() == CSeq_inst::eRepr_map ) {
        // TODO: is this right?  Maybe handle it differently once
        // CAnnot_CI is able to handle CSeq_inst::eRepr_map.
        return;
    }

    CAnnot_CI annot_ci(m_Handle);
    for( ; annot_ci; ++annot_ci ) {
        if( ! annot_ci->Seq_annot_IsSetDesc() ) {
            continue;
        }

        const CSeq_annot::TDesc & desc = annot_ci->Seq_annot_GetDesc();
        ITERATE( CSeq_annot::TDesc::Tdata, one_desc_iter, desc.Get() ) {
            const CAnnotdesc & one_desc = **one_desc_iter;
            if( ! one_desc.IsUser() ) {
                continue;
            }

            // we finally got down to an annot desc user object.  See if it indicates any
            // relevant information
            const CUser_object & user_obj = one_desc.GetUser();
            if( ! user_obj.IsSetType() || ! user_obj.GetType().IsStr() || 
                ! user_obj.IsSetData() ||
                user_obj.GetType().GetStr() != "AnnotDescCommentPolicy" )
            {
                continue;
            }

            // check policy flags
            ITERATE( CUser_object::TData, policy_field_iter, user_obj.GetData() ) {
                const CUser_field & policy_field = **policy_field_iter;
                if( ! policy_field.IsSetLabel() || ! policy_field.GetLabel().IsStr() ||
                    ! policy_field.IsSetData() ||
                    policy_field.GetLabel().GetStr() != "Policy" )
                {
                    continue;
                }

                if( policy_field.GetData().IsStr() ) { 
                    const string & policy_str = policy_field.GetData().GetStr();
                    if( policy_str == "ShowInComment" ) {
                        m_ShowAnnotCommentAsCOMMENT = true;
                    }
                }
            }
        }
    }
}


bool CBioseqContext::x_HasOperon(void) const
{
    return CFeat_CI(m_Handle.GetScope(),
                    *m_Location,
                    CSeqFeatData::eSubtype_operon);
}


void CBioseqContext::x_SetId(void)
{
    m_PrimaryId.Reset(new CSeq_id);
    m_PrimaryId->Assign(*sequence::GetId(
        m_Handle, sequence::eGetId_Best).GetSeqId());

    m_Accession.erase();
    m_PrimaryId->GetLabel(&m_Accession, CSeq_id::eContent);

    //  -----------------------------------------------------------------------
    //  Look for TPA assembly:
    //  -----------------------------------------------------------------------
    bool bTpaAssemblyPresent = false;
    for (CSeqdesc_CI it(m_Handle, CSeqdesc::e_User);  it;  ++it) {
        const CUser_object& obj = it->GetUser();
        if ( !obj.GetType().IsStr() ) {
            continue;
        }
        if ( obj.GetType().GetStr() == "TpaAssembly" ) {
            bTpaAssemblyPresent = true;
            continue; 
        }
        if ( obj.GetType().GetStr() == "GenomeProjectsDB" ) {
            m_IsGbGenomeProject = true;
            continue; 
        }
    }

    ITERATE (CBioseq::TId, id_iter, m_Handle.GetBioseqCore()->GetId()) {
        const CSeq_id& id = **id_iter;
        const CTextseq_id* tsip = id.GetTextseq_Id();
        const string& acc = (tsip != 0  &&  tsip->CanGetAccession()) ?
            tsip->GetAccession() : kEmptyStr;

        CSeq_id::EAccessionInfo acc_info = id.IdentifyAccession();
        unsigned int acc_div =  acc_info & CSeq_id::eAcc_division_mask;

        switch ( id.Which() ) {
        // Genbank, Embl or Ddbj
        case CSeq_id::e_Embl:
            m_IsEMBL = true;
            break;
        case CSeq_id::e_Ddbj:
            m_IsDDBJ = true;
            break;
        case CSeq_id::e_Genbank:
            m_IsGenbank = true;
            switch (acc_info) {
            case CSeq_id::eAcc_gb_genome:
                m_IsGbGenomeProject = true;
                break;
            case CSeq_id::eAcc_gb_con:  case CSeq_id::eAcc_gb_segset:
                m_IsNcbiCONDiv = true;
                break;
            default:
                break;
            }
           break;
        // Patent
        case CSeq_id::e_Patent:
            m_IsPatent = true;
            if (id.GetPatent().IsSetSeqid()) {
                m_PatSeqid = id.GetPatent().GetSeqid();
            }
            break;
        // RefSeq
        case CSeq_id::e_Other:
            m_IsRefSeq = true;
            m_RefseqInfo = acc_info;
            break;
        // Gi
        case CSeq_id::e_Gi:
            m_IsGI = true;
            m_Gi = id.GetGi();
            break;
        // PDB
        case CSeq_id::e_Pdb:
            m_IsPDB = true;
            break;
        // TPA
        case CSeq_id::e_Tpg:
            m_IsTPA = true;
            m_IsGenbank = true;
            break;
        case CSeq_id::e_Tpe:
            m_IsTPA = true;
            m_IsEMBL = true;
            break;
        case CSeq_id::e_Tpd:
            m_IsTPA = true;
            m_IsDDBJ = true;
            break;
        case CSeq_id::e_General:
            if ( id.GetGeneral().CanGetDb() ) {
                if ( !NStr::CompareCase(id.GetGeneral().GetDb(), "BankIt") ) {
                    m_IsTPA = bTpaAssemblyPresent;
                }
                if( NStr::Equal(id.GetGeneral().GetDb(), "NCBI_GENOMES") ) {
                    m_IsNcbiGenomes = true;
                }
            }
            break;
        case CSeq_id::e_Gibbsq:
        case CSeq_id::e_Gibbmt:
        case CSeq_id::e_Giim:
            m_IsJournalScan = true;
            break;
        case CSeq_id::e_Swissprot:
            m_IsSP = true;
            break;
        // nothing special
        case CSeq_id::e_Pir:
        case CSeq_id::e_not_set:
        case CSeq_id::e_Local:
        case CSeq_id::e_Prf:
        default:
            break;
        }

        // WGS
        m_IsWGS = m_IsWGS  ||  (acc_div == CSeq_id::eAcc_wgs);
        
        if ( m_IsWGS  &&  !acc.empty() ) {
            size_t len = acc.length();
            m_IsWGSMaster = 
                ((len == 12  ||  len == 15)  &&  NStr::EndsWith(acc, "000000"))  ||
                ((len == 14)  &&  NStr::EndsWith(acc, "00000000")) ||
                ((len == 13 || len == 16 || len == 17)  &&  NStr::EndsWith(acc, "0000000"));
            if ( m_IsWGSMaster ) {
                m_WGSMasterAccn = acc;
                m_WGSMasterName = tsip->CanGetName() ? tsip->GetName() : kEmptyStr;
            }
        } 

        // TSA
        m_IsTSA = m_IsTSA  ||  (acc_div == CSeq_id::eAcc_tsa);
        
        if ( m_IsTSA  &&  !acc.empty() ) {
            size_t len = acc.length();
            m_IsTSAMaster = 
                ( ((len == 12  ||  len == 15)  &&  NStr::EndsWith(acc, "000000"))  ||
                  (len == 13  &&  NStr::EndsWith(acc, "0000000")) ) &&
                ( m_Repr == CSeq_inst::eRepr_virtual );
            if ( m_IsTSAMaster ) {
                m_TSAMasterAccn = acc;
                m_TSAMasterName = tsip->CanGetName() ? tsip->GetName() : kEmptyStr;
            }
        } 

        // GBB source
        m_ShowGBBSource = m_ShowGBBSource  ||  (acc_info == CSeq_id::eAcc_gsdb_dirsub);
    }

    // Genbank/Embl/Ddbj (GED)
    m_IsGED = m_IsEMBL  ||  m_IsDDBJ  ||  m_IsGenbank;
}


CSeq_inst::TRepr CBioseqContext::x_GetRepr(void) const
{
    return m_Handle.IsSetInst_Repr() ?
        m_Handle.GetInst_Repr() : CSeq_inst::eRepr_not_set;
}


const CMolInfo* CBioseqContext::x_GetMolInfo(void) const
{
    CSeqdesc_CI desc(m_Handle, CSeqdesc::e_Molinfo);
    return desc ? &desc->GetMolinfo() : 0;
}


bool CBioseqContext::x_IsPart() const
{
    if ( m_Repr == CSeq_inst::eRepr_raw    ||
         m_Repr == CSeq_inst::eRepr_const  ||
         m_Repr == CSeq_inst::eRepr_delta  ||
         m_Repr == CSeq_inst::eRepr_virtual ) {
         const CSeq_entry_Handle& fftse = GetTopLevelEntry();
         CSeq_entry_Handle eh = m_Handle.GetParentEntry();
         _ASSERT(eh  &&  eh.IsSeq());

         if (fftse != eh) {
             eh = eh.GetParentEntry();
             if ( eh  &&  eh.IsSet() ) {
                 CBioseq_set_Handle bsst = eh.GetSet();
                 if ( bsst.IsSetClass()  &&
                     bsst.GetClass() == CBioseq_set::eClass_parts ) {
                     return true;
                 }
            }
        }
    }
    return false;
}


bool CBioseqContext::x_HasParts(void) const
{
    _ASSERT(IsSegmented());

    CSeq_entry_Handle h =
        m_Handle.GetExactComplexityLevel(CBioseq_set::eClass_segset);
    if ( !h ) {
        return false;
    }
    
    // make sure the segmented set contains our bioseq
    {{
        bool has_seq = false;
        for ( CSeq_entry_CI it(h); it; ++it ) {
            if ( it->IsSeq()  &&  it->GetSeq() == m_Handle ) {
                has_seq = true;
                break;
            }
        }
        if ( !has_seq ) {
            return false;
        }
    }}

    // find the parts set
    {{
        for ( CSeq_entry_CI it(h); it; ++it ) {
            if ( it->IsSet()  &&  it->GetSet().IsSetClass()  &&
                it->GetSet().GetClass() == CBioseq_set::eClass_parts ) {
                return true;
            }
        }
    }}

    return false;
}


bool CBioseqContext::x_IsDeltaLitOnly(void) const
{
    _ASSERT(IsDelta());

    if ( m_Handle.IsSetInst_Ext() ) {
        const CBioseq_Handle::TInst_Ext& ext = m_Handle.GetInst_Ext();
        if ( ext.IsDelta() ) {
            ITERATE (CDelta_ext::Tdata, it, ext.GetDelta().Get()) {
                if ( (*it)->IsLoc() ) {
                    return false;
                }
            }
        }
    }
    return true;
}


SIZE_TYPE CBioseqContext::x_GetPartNumber(void)
{
    return m_Master ? m_Master->GetPartNumber(m_Handle) : 0;
}


bool CBioseqContext::x_IsInGPS(void) const
{
    CSeq_entry_Handle e = 
        m_Handle.GetExactComplexityLevel(CBioseq_set::eClass_gen_prod_set);
    return e;
}


bool CBioseqContext::x_IsInNucProt(void) const
{
    CSeq_entry_Handle e = 
        m_Handle.GetExactComplexityLevel(CBioseq_set::eClass_nuc_prot);
    return e;
}


bool CBioseqContext::DoContigStyle(void) const
{
    const CFlatFileConfig& cfg = Config();
    if ( cfg.IsStyleContig() ) {
        return true;
    } else if ( cfg.IsStyleNormal() ) {
        if ( (IsSegmented()  &&  !HasParts())  ||
             (IsDelta()  &&  !IsDeltaLitOnly()) ) {
            return true;
        }
    }

    return false;
}


void CBioseqContext::x_SetEncode(const CUser_object& uo)
{
    if (uo.IsSetType()  &&  uo.GetType().IsStr()) {
        if (NStr::EqualNocase(uo.GetType().GetStr(), "ENCODE")) {
            m_Encode.Reset(&uo);
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
//
// CMasterContext


CMasterContext::CMasterContext(const CBioseq_Handle& seq) :
    m_Handle(seq)
{
    _ASSERT(seq);
    _ASSERT(seq.GetInst_Ext().IsSeg());

    x_SetNumParts();
    x_SetBaseName();
}


CMasterContext::~CMasterContext(void)
{
}


SIZE_TYPE CMasterContext::GetPartNumber(const CBioseq_Handle& part)
{
    if ( !part ) {
        return 0;
    }
    CScope& scope = m_Handle.GetScope();

    SIZE_TYPE serial = 1;
    ITERATE (CSeg_ext::Tdata, it, m_Handle.GetInst_Ext().GetSeg().Get()) {
        if ((*it)->IsNull()) {
            continue;
        }
        const CSeq_id& id = GetId(**it, &m_Handle.GetScope());
        CBioseq_Handle bsh = scope.GetBioseqHandleFromTSE(id, m_Handle);
        if (bsh  &&
            bsh.IsSetInst_Repr()  &&
            bsh.GetInst_Repr() != CSeq_inst::eRepr_virtual) {
            if (bsh == part) {
                return serial;
            }
            ++serial;
        }
    }

    return 0;
}


void CMasterContext::x_SetNumParts(void)
{
    CScope& scope = m_Handle.GetScope();
    SIZE_TYPE count = 0;

    // count only non-gap and non-virtual parts
    ITERATE (CSeg_ext::Tdata, it, m_Handle.GetInst_Ext().GetSeg().Get()) {
        const CSeq_loc& loc = **it;
        if (loc.IsNull()) { // skip gaps
            continue;
        }
        // count only non-virtual
        const CSeq_id_Handle id = CSeq_id_Handle::GetHandle(GetId(loc, &scope));
        CBioseq_Handle part = scope.GetBioseqHandleFromTSE(id, m_Handle);
        if (part  &&
            part.IsSetInst_Repr()  &&
            part.GetInst_Repr() != CSeq_inst::eRepr_virtual) {
            ++count;    
        }
    }
    m_NumParts = count;
}


static void s_GetNameForBioseq(const CBioseq_Handle& seq, string& name)
{
    name.erase();

    CConstRef<CSeq_id> sip;
    ITERATE (CBioseq_Handle::TId, it, seq.GetId()) {
        CConstRef<CSeq_id> id = it->GetSeqId();
        if (id->IsGenbank()  ||  id->IsEmbl()    ||  id->IsDdbj()  ||
            id->IsTpg()      ||  id->IsTpe()     ||  id->IsTpd()) {
            sip = id;
            break;
        }
    }

    if (sip) {
        const CTextseq_id* tsip = sip->GetTextseq_Id();
        if (tsip != NULL  &&  tsip->CanGetName()) {
            name = tsip->GetName();
        }
    }
}


void CMasterContext::x_SetBaseName(void)
{
    string parent_name;
    s_GetNameForBioseq(m_Handle, parent_name);

    // if there's no "SEG_" prefix just use the master's name
    if (!NStr::StartsWith(parent_name, "SEG_")) {
        m_BaseName = parent_name;
        return;
    }

    // otherwise, eliminate the prefix ...
    parent_name = parent_name.substr(4);

    // ... and calculate a base name

    // find the first segment
    CScope* scope = &m_Handle.GetScope();
    CBioseq_Handle segment;
    const CSeqMap& seqmap = m_Handle.GetSeqMap();
    CSeqMap_CI it = seqmap.BeginResolved(scope,
                                         SSeqMapSelector()
                                         .SetResolveCount(1)
                                         .SetFlags(CSeqMap::fFindRef));
    while (it) {
        CSeq_id_Handle id = it.GetRefSeqid();
        segment = scope->GetBioseqHandleFromTSE(id, m_Handle);
        if (segment) {
            break;
        }
    }
    string seg_name;
    if (segment) {
        s_GetNameForBioseq(segment, seg_name);
    }

    if (!seg_name.empty()  &&  NStr::EndsWith(seg_name, '1')  &&
        parent_name.length() == seg_name.length()  &&
        NStr::EndsWith(parent_name, '1')) {
        size_t pos  = parent_name.length() - 2;
        for ( /*noop*/; pos > 0; --pos) {
            if (parent_name[pos] != '0') {
                break;
            }
        }
        parent_name.erase(pos + 1);
    }

    m_BaseName = parent_name;
}

/////////////////////////////////////////////////////////////////////////////
//
// CTopLevelSeqEntryContext

CTopLevelSeqEntryContext::CTopLevelSeqEntryContext( const CSeq_entry_Handle &entry_handle )
{
    m_CanSourcePubsBeFused = false;
    m_HasSmallGenomeSet = false;

    CBioseq_CI bioseq_iter( entry_handle.GetScope(), *entry_handle.GetSeq_entryCore() );
    for( ; bioseq_iter; ++bioseq_iter ) {
        ITERATE( CBioseq_Handle::TId, it, bioseq_iter->GetId() )  {
            CConstRef<CSeq_id> seqId = (*it).GetSeqIdOrNull();
            if( ! seqId.IsNull() ) {
                switch( seqId->Which() ) {
                        case CSeq_id_Base::e_Gibbsq:
                        case CSeq_id_Base::e_Gibbmt:
                        case CSeq_id_Base::e_Embl:
                        case CSeq_id_Base::e_Pir:
                        case CSeq_id_Base::e_Swissprot:
                        case CSeq_id_Base::e_Patent:        
                        case CSeq_id_Base::e_Ddbj:
                        case CSeq_id_Base::e_Prf:
                        case CSeq_id_Base::e_Pdb:
                        case CSeq_id_Base::e_Tpe:
                        case CSeq_id_Base::e_Tpd:
                        case CSeq_id_Base::e_Gpipe:
                            // with some types, it's okay to merge
                            m_CanSourcePubsBeFused = true;
                            break;                        
                        case CSeq_id_Base::e_Genbank:
                        case CSeq_id_Base::e_Tpg:
                            // Genbank allows merging only if it's the old-style 1 + 5 accessions
                            if( NULL != seqId->GetTextseq_Id() &&
                                seqId->GetTextseq_Id()->GetAccession().length() == 6 ) {
                                    m_CanSourcePubsBeFused = true;
                            }
                            break;
                        case CSeq_id_Base::e_not_set:
                        case CSeq_id_Base::e_Local:
                        case CSeq_id_Base::e_Other:
                        case CSeq_id_Base::e_General:
                        case CSeq_id_Base::e_Giim:                        
                        case CSeq_id_Base::e_Gi:
                            break;
                        default:
                            break;
                }
            }
        }
    }

    // check all Bioseq-sets, if any
    if( entry_handle.IsSet() ) {
        if( entry_handle.GetSet().CanGetClass() &&
            entry_handle.GetSet().GetClass() == CBioseq_set::eClass_small_genome_set ) 
        {
            m_HasSmallGenomeSet = true;
        } else {
            CSeq_entry_CI seq_entry_ci( entry_handle, CSeq_entry_CI::eRecursive );
            for( ; seq_entry_ci && ! m_HasSmallGenomeSet; ++seq_entry_ci ) {
                if( seq_entry_ci->IsSet() && seq_entry_ci->GetSet().CanGetClass() &&
                    seq_entry_ci->GetSet().GetClass() == CBioseq_set::eClass_small_genome_set ) 
                {
                    m_HasSmallGenomeSet = true;
                    break;
                }
            }
        }
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE
