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
 * Authors:  Frank Ludwig
 *
 * File Description:  Write gff file
 *
 */

#include <ncbi_pch.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/seq/seqport_util.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objtools/writers/write_util.hpp>
#include <objtools/writers/feature_context.hpp>
#include <objmgr/util/sequence.hpp>

#include <objtools/writers/writer_exception.hpp>
#include <objtools/writers/genbank_id_resolve.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

//  ----------------------------------------------------------------------------
CRef<CUser_object> CWriteUtil::GetDescriptor(
    const CSeq_annot& annot,
    const string& strType )
//  ----------------------------------------------------------------------------
{
    CRef< CUser_object > pUser;
    if (!annot.IsSetDesc()) {
        return pUser;
    }

    const list<CRef<CAnnotdesc> > descriptors = annot.GetDesc().Get();
    list<CRef<CAnnotdesc> >::const_iterator it;
    for (it = descriptors.begin(); it != descriptors.end(); ++it) {
        if (!(*it)->IsUser()) {
            continue;
        }
        const CUser_object& user = (*it)->GetUser();
        if (user.GetType().GetStr() == strType) {
            pUser.Reset(new CUser_object);
            pUser->Assign(user);
            return pUser;
        }
    }
    return pUser;
}

//  ----------------------------------------------------------------------------
bool CWriteUtil::GetGenomeString(
    const CBioSource& bs,
    string& genome_str )
//  ----------------------------------------------------------------------------
{
#define EMIT(str) { genome_str = str; return true; }

    if (!bs.IsSetGenome()) {
        return false;
    }
    switch (bs.GetGenome()) {
        default:
            return false;
        case CBioSource::eGenome_apicoplast: EMIT("apicoplast");
        case CBioSource::eGenome_chloroplast: EMIT("chloroplast");
        case CBioSource::eGenome_chromatophore: EMIT("chromatophore");
        case CBioSource::eGenome_chromoplast: EMIT("chromoplast");
        case CBioSource::eGenome_chromosome: EMIT("chromosome");
        case CBioSource::eGenome_cyanelle: EMIT("cyanelle");
        case CBioSource::eGenome_endogenous_virus: EMIT("endogenous_virus");
        case CBioSource::eGenome_extrachrom: EMIT("extrachrom");
        case CBioSource::eGenome_genomic: EMIT("genomic");
        case CBioSource::eGenome_hydrogenosome: EMIT("hydrogenosome");
        case CBioSource::eGenome_insertion_seq: EMIT("insertion_seq");
        case CBioSource::eGenome_kinetoplast: EMIT("kinetoplast");
        case CBioSource::eGenome_leucoplast: EMIT("leucoplast");
        case CBioSource::eGenome_macronuclear: EMIT("macronuclear");
        case CBioSource::eGenome_mitochondrion: EMIT("mitochondrion");
        case CBioSource::eGenome_nucleomorph: EMIT("nucleomorph");
        case CBioSource::eGenome_plasmid: EMIT("plasmid");
        case CBioSource::eGenome_plastid: EMIT("plastid");
        case CBioSource::eGenome_proplastid: EMIT("proplastid");
        case CBioSource::eGenome_proviral: EMIT("proviral");
        case CBioSource::eGenome_transposon: EMIT("transposon");
        case CBioSource::eGenome_unknown: EMIT("unknown");
        case CBioSource::eGenome_virion: EMIT("virion");
    }
}
#undef EMIT

//  ----------------------------------------------------------------------------
bool CWriteUtil::GetIdType(
    const CSeq_id& seqId,
    string& idType )
//  ----------------------------------------------------------------------------
{
#define EMIT(str) { idType = str; return true; }
    switch(seqId.Which()) {
    default:
        idType = CSeq_id::SelectionName(seqId.Which());
        NStr::ToUpper(idType);
        break;

    case CSeq_id::e_Local: EMIT("Local");

    case CSeq_id::e_Gibbsq:
    case CSeq_id::e_Gibbmt:
    case CSeq_id::e_Giim:
    case CSeq_id::e_Gi: EMIT("GenInfo");

    case CSeq_id::e_Genbank: EMIT("Genbank");
    case CSeq_id::e_Swissprot: EMIT("SwissProt");
    case CSeq_id::e_Patent: EMIT("Patent");
    case CSeq_id::e_Other: EMIT("RefSeq");
    case CSeq_id::e_Ddbj: EMIT("DDBJ");
    case CSeq_id::e_Embl: EMIT("EMBL");
    case CSeq_id::e_Pir: EMIT("PIR");
    case CSeq_id::e_Prf: EMIT("PRF");
    case CSeq_id::e_Pdb: EMIT("PDB");
    case CSeq_id::e_Tpg: EMIT("tpg");
    case CSeq_id::e_Tpe: EMIT("tpe");
    case CSeq_id::e_Tpd: EMIT("tpd");
    case CSeq_id::e_Gpipe: EMIT("gpipe");
    case CSeq_id::e_Named_annot_track: EMIT("NADB");
    case CSeq_id::e_General:
        EMIT(seqId.GetGeneral().GetDb());
    }
#undef EMIT
    return true;
}

//  ----------------------------------------------------------------------------
bool CWriteUtil::GetIdType(
    CBioseq_Handle bsh,
    string& idType )
//  ----------------------------------------------------------------------------
{
    if (!bsh) {
        return false;
    }
    CSeq_id_Handle best_idh;
    try {
        best_idh = sequence::GetId(bsh, sequence::eGetId_Best);
        if ( !best_idh ) {
            best_idh = sequence::GetId(bsh, sequence::eGetId_Canonical);
        }
    }
    catch(...) {
        return false;
    }
    return GetIdType(*best_idh.GetSeqId(), idType);
}

//  ----------------------------------------------------------------------------
bool CWriteUtil::GetOrgModSubType(
    const COrgMod& mod,
    string& subtype,
    string& subname)
//  ----------------------------------------------------------------------------
{
    if (!mod.IsSetSubtype() || !mod.IsSetSubname()) {
        return false;
    }
    subtype = COrgMod::GetSubtypeName(mod.GetSubtype());
    subname = mod.GetSubname();
    return true;
}

//  ----------------------------------------------------------------------------
bool CWriteUtil::GetSubSourceSubType(
    const CSubSource& sub,
    string& subtype,
    string& subname)
//  ----------------------------------------------------------------------------
{
#define EMIT(str) { subname = str; return true; }
    if (!sub.IsSetSubtype() || !sub.IsSetName()) {
        return false;
    }
    subtype = CSubSource::GetSubtypeName(sub.GetSubtype());

    switch (sub.GetSubtype()) {
        default:
            if (sub.GetName().empty()) {
                EMIT("indeterminate");
            }
            EMIT(sub.GetName());
        case CSubSource::eSubtype_environmental_sample:
        case CSubSource::eSubtype_germline:
        case CSubSource::eSubtype_transgenic:
        case CSubSource::eSubtype_rearranged:
        case CSubSource::eSubtype_metagenomic:
            EMIT("true");
    }
    return true;
#undef EMIT
}

//  ----------------------------------------------------------------------------
bool CWriteUtil::GetAaName(
    const CCode_break& cb,
    string& aaName )
//  ----------------------------------------------------------------------------
{
    static const char* AANames[] = {
        "---", "Ala", "Asx", "Cys", "Asp", "Glu", "Phe", "Gly", "His", "Ile",
        "Lys", "Leu", "Met", "Asn", "Pro", "Gln", "Arg", "Ser", "Thr", "Val",
        "Trp", "Other", "Tyr", "Glx", "Sec", "TERM", "Pyl"
    };
    static const char* other = "OTHER";

    unsigned char aa(0);
    switch (cb.GetAa().Which()) {
        case CCode_break::C_Aa::e_Ncbieaa:
            aa = cb.GetAa().GetNcbieaa();
            aa = CSeqportUtil::GetMapToIndex(
                CSeq_data::e_Ncbieaa, CSeq_data::e_Ncbistdaa, aa);
            break;
        case CCode_break::C_Aa::e_Ncbi8aa:
            aa = cb.GetAa().GetNcbi8aa();
            break;
        case CCode_break::C_Aa::e_Ncbistdaa:
            aa = cb.GetAa().GetNcbistdaa();
            break;
        default:
            return false;
    }
    aaName = ((aa < sizeof(AANames)/sizeof(*AANames)) ? AANames[aa] : other);
    return true;
}

//  ----------------------------------------------------------------------------
bool CWriteUtil::GetCodeBreak(
    const CCode_break& cb,
    string& cbString )
//  ----------------------------------------------------------------------------
{
    string cb_str = ("(pos:");
    if ( cb.IsSetLoc() ) {
        const CCode_break::TLoc& loc = cb.GetLoc();
        switch( loc.Which() ) {
            default: {
                cb_str += NStr::IntToString( loc.GetStart(eExtreme_Positional)+1 );
                cb_str += "..";
                cb_str += NStr::IntToString( loc.GetStop(eExtreme_Positional)+1 );
                break;
            }
            case CSeq_loc::e_Int: {
                const CSeq_interval& intv = loc.GetInt();
                string intv_str;
                intv_str += NStr::IntToString( intv.GetFrom()+1 );
                intv_str += "..";
                intv_str += NStr::IntToString( intv.GetTo()+1 );
                if ( intv.IsSetStrand()  &&  intv.GetStrand() == eNa_strand_minus ) {
                    intv_str = "complement(" + intv_str + ")";
                }
                cb_str += intv_str;
                break;
            }
        }
    }
    cb_str += ",aa:";

    string aaName;
    if (!CWriteUtil::GetAaName(cb, aaName)) {
        return false;
    }
    cb_str += aaName + ")";
    cbString = cb_str;
    return true;
}

//  ----------------------------------------------------------------------------
bool CWriteUtil::GetTrnaCodons(
    const CTrna_ext& trna,
    string& codonStr )
//  ----------------------------------------------------------------------------
{
    if (!trna.IsSetCodon()) {
        return false;
    }
    const list<int>& values = trna.GetCodon();
    if (values.empty()) {
        return false;
    }
    list<int>::const_iterator cit = values.begin();
    string codons = NStr::IntToString(*cit);
    for (cit++; cit != values.end(); ++cit) {
        codons += ",";
        codons += NStr::IntToString(*cit);
    }
    codonStr = codons;
    return true;
}

//  ----------------------------------------------------------------------------
bool CWriteUtil::GetTrnaProductName(
    const CTrna_ext& trna,
    string& name )
//  ----------------------------------------------------------------------------
{
    static const string sTrnaList[] = {
        "tRNA-Gap", "tRNA-Ala", "tRNA-Asx", "tRNA-Cys", "tRNA-Asp", "tRNA-Glu",
        "tRNA-Phe", "tRNA-Gly", "tRNA-His", "tRNA-Ile", "tRNA-Xle", "tRNA-Lys",
        "tRNA-Leu", "tRNA-Met", "tRNA-Asn", "tRNA-Pyl", "tRNA-Pro", "tRNA-Gln",
        "tRNA-Arg", "tRNA-Ser", "tRNA-Thr", "tRNA-Sec", "tRNA-Val", "tRNA-Trp",
        "tRNA-OTHER", "tRNA-Tyr", "tRNA-Glx", "tRNA-TERM"
    };
    static int AACOUNT = sizeof(sTrnaList)/sizeof(string);

    if (!trna.IsSetAa()  ||  !trna.GetAa().IsNcbieaa()) {
        return false;
    }
    int aa = trna.GetAa().GetNcbieaa();
    (aa == '*') ? (aa = 25) : (aa -= 64);
    name = ((0 < aa  &&  aa < AACOUNT) ? sTrnaList[aa] : "");
    return true;
}

//  ----------------------------------------------------------------------------
bool CWriteUtil::GetTrnaAntiCodon(
    const CTrna_ext& trna,
    string& acStr )
//  ----------------------------------------------------------------------------
{
    if (!trna.IsSetAnticodon()) {
        return false;
    }
    const CSeq_loc& loc = trna.GetAnticodon();
    string anticodon;
    switch( loc.Which() ) {
        default: {
            anticodon += NStr::IntToString( loc.GetStart(eExtreme_Positional)+1 );
            anticodon += "..";
            anticodon += NStr::IntToString( loc.GetStop(eExtreme_Positional)+1 );
            break;
        }
        case CSeq_loc::e_Int: {
            const CSeq_interval& intv = loc.GetInt();
            anticodon += NStr::IntToString( intv.GetFrom()+1 );
            anticodon += "..";
            anticodon += NStr::IntToString( intv.GetTo()+1 );
            if ( intv.IsSetStrand()  &&  intv.GetStrand() == eNa_strand_minus ) {
                anticodon = "complement(" + anticodon + ")";
            }
            break;
        }
    }
    acStr = string("(pos:") + anticodon + ")";
    return true;
}

//  ----------------------------------------------------------------------------
bool CWriteUtil::GetDbTag(
    const CDbtag& dbtag,
    string& dbTagStr )
//
//  Note: Different from CDbtag::GetLabel()
//  ----------------------------------------------------------------------------
{
    string str;
    if ( dbtag.IsSetDb() ) {
        str += dbtag.GetDb();
    }
    else {
        str += "NoDB";
    }
    if ( dbtag.IsSetTag() ) {
        if (!str.empty()) {
            str += ":";
        }
        if (dbtag.GetTag().IsId() ) {
            str += NStr::UIntToString( dbtag.GetTag().GetId() );
        }
        if ( dbtag.GetTag().IsStr() ) {
            str += dbtag.GetTag().GetStr();
        }
    }
    if (str.empty()) {
        return false;
    }
    dbTagStr = str;
    return true;
}

//  ----------------------------------------------------------------------------
bool CWriteUtil::GetBiomol(
    CBioseq_Handle bsh,
    string& mol_str)
//  ----------------------------------------------------------------------------
{
#define EMIT(str) { mol_str = str; return true; }
    CSeqdesc_CI md(bsh.GetParentEntry(), CSeqdesc::e_Molinfo, 0);
    if (!md) {
        return false;
    }
    const CMolInfo& molinfo = md->GetMolinfo();
    if (!molinfo.IsSetBiomol()) {
        return false;
    }

    int inst = bsh.GetInst_Mol();
    int mol = molinfo.GetBiomol();

    switch( mol ) {
        default:
            break;
        case CMolInfo::eBiomol_genomic: {
            switch (inst) {
                default:
                    EMIT("genomic");
                case CSeq_inst::eMol_dna:
                    EMIT("genomic DNA");
                case CSeq_inst::eMol_rna:
                    EMIT("genomic RNA");
            }
        }
        case CMolInfo::eBiomol_mRNA:
            EMIT("mRNA");
        case CMolInfo::eBiomol_rRNA:
            EMIT("rRNA");
        case CMolInfo::eBiomol_tRNA:
            EMIT("tRNA");
        case CMolInfo::eBiomol_pre_RNA:
        case CMolInfo::eBiomol_snRNA:
        case CMolInfo::eBiomol_scRNA:
        case CMolInfo::eBiomol_snoRNA:
        case CMolInfo::eBiomol_ncRNA:
        case CMolInfo::eBiomol_tmRNA:
        case CMolInfo::eBiomol_transcribed_RNA:
            EMIT("transcribed RNA");
        case CMolInfo::eBiomol_other_genetic:
        case CMolInfo::eBiomol_other: {
            switch (inst) {
                default:
                    EMIT("other");
                case CSeq_inst::eMol_dna:
                    EMIT("other DNA");
                case CSeq_inst::eMol_rna:
                    EMIT("other RNA");
            }
        }
        case CMolInfo::eBiomol_cRNA:
            EMIT("viral cRNA");

        case CMolInfo::eBiomol_genomic_mRNA:
            EMIT("genomic RNA");
    }
    switch (inst) {
        default:
            EMIT("unassigned");
        case CSeq_inst::eMol_dna:
            EMIT("unassigned DNA");
        case CSeq_inst::eMol_rna:
            EMIT("unassigned RNA");
    }
    return false;
#undef EMIT
}

//  ----------------------------------------------------------------------------
bool CWriteUtil::IsLocationOrdered(
    const CSeq_loc& loc)
//  Look whether the given location contains any eNull intervals. If so, the
//  location is ordered, otherwise not.
//  ----------------------------------------------------------------------------
{
    switch ( loc.Which() ) {
    case CSeq_loc::e_Null:
        return true;
    case CSeq_loc::e_Mix: {
            ITERATE (CSeq_loc_mix::Tdata, sub_loc, loc.GetMix().Get()) {
                if (IsLocationOrdered(**sub_loc)) {
                    return true;
                }
            }
            return false;
        }
    default:
        return false;
    }
}

//  ----------------------------------------------------------------------------
bool CWriteUtil::IsSequenceCircular(
    CBioseq_Handle bsh)
//  ----------------------------------------------------------------------------
{
    if (!bsh  ||  !bsh.IsSetInst_Topology()
              ||  bsh.GetInst_Topology() != CSeq_inst::eTopology_circular) {
        return false;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CWriteUtil::NeedsQuoting(
    const string& str )
//  ----------------------------------------------------------------------------
{
    if(str.empty())
        return true;

    for (size_t u=0; u < str.length(); ++u) {
        if (str[u] == '\"')
            return false;
        if (str[u] == ' ' || str[u] == ';' || str[u] == ':' || str[u] == '=') {
            return true;
        }
    }
    return false;
}

//  ----------------------------------------------------------------------------
void CWriteUtil::ChangeToPackedInt(
    CSeq_loc& loc)
//  Special mission:
//  Filter out eNull intervals before submitting the location to the "normal"
//  ChangeToPackedInt() method.
//  ----------------------------------------------------------------------------
{
    switch ( loc.Which() ) {
    case CSeq_loc::e_Null:
        loc.SetPacked_int();
        return;
    case CSeq_loc::e_Mix: {
            vector<CRef<CSeq_loc> > sub_locs;
            sub_locs.reserve(loc.GetMix().Get().size());
            ITERATE (CSeq_loc_mix::Tdata, orig_sub_loc, loc.GetMix().Get()) {
                if ((*orig_sub_loc)->Which() == CSeq_loc::e_Null) {
                    continue;
                }
                CRef<CSeq_loc> new_sub_loc(new CSeq_loc);
                new_sub_loc->Assign(**orig_sub_loc);
                ChangeToPackedInt(*new_sub_loc);
                sub_locs.push_back(new_sub_loc);
            }
            loc.SetPacked_int();  // in case there are zero intervals
            ITERATE (vector<CRef<CSeq_loc> >, sub_loc, sub_locs) {
                copy((*sub_loc)->GetPacked_int().Get().begin(),
                     (*sub_loc)->GetPacked_int().Get().end(),
                     back_inserter(loc.SetPacked_int().Set()));
            }
        }
        return;
    default:
        loc.ChangeToPackedInt();
        return;
    }
}

//  ----------------------------------------------------------------------------
bool CWriteUtil::GetBestId(
    CSeq_id_Handle idh,
    CScope& scope,
    string& best_id)
//  ----------------------------------------------------------------------------
{
    return CGenbankIdResolve::Get().GetBestId(idh, scope, best_id);
}

//  ----------------------------------------------------------------------------
bool CWriteUtil::GetBestId(
    const CMappedFeat& mf,
    string& best_id)
//  ----------------------------------------------------------------------------
{
    return CGenbankIdResolve::Get().GetBestId(mf, best_id);
}

//  ----------------------------------------------------------------------------
bool CWriteUtil::GetQualifier(
    CMappedFeat mf,
    const string& key,
    string& value)
//  ----------------------------------------------------------------------------
{
    if (!mf.IsSetQual()) {
        return false;
    }
    const vector<CRef<CGb_qual> >& quals = mf.GetQual();
    vector<CRef<CGb_qual> >::const_iterator it = quals.begin();
    for (; it != quals.end(); ++it) {
        if (!(*it)->CanGetQual() || !(*it)->CanGetVal()) {
            continue;
        }
        if ((*it)->GetQual() == key) {
            value = (*it)->GetVal();
            return true;
        }
    }
    return false;
}

//  ---------------------------------------------------------------------------
void CGffFeatureContext::xAssignSequenceIsGenomicRecord()
//  ---------------------------------------------------------------------------
{
    m_bSequenceIsGenomicRecord = false;
    if (!m_bsh) {
        return;
    }
    if (!m_bsh || !m_bsh.IsSetDescr()) {
        return;
    }
    const CSeq_descr& descr = m_bsh.GetDescr();
    if (!descr.CanGet()) {
        return;
    }
    const list< CRef< CSeqdesc > >& listDescr = descr.Get();
    for (list< CRef< CSeqdesc > >::const_iterator cit = listDescr.begin();
        cit != listDescr.end(); ++cit) {
        const CSeqdesc& desc = **cit;
        if (!desc.IsMolinfo()) {
            continue;
        }
        const CMolInfo& molInfo = desc.GetMolinfo();
        if (!molInfo.IsSetBiomol()) {
            continue;
        }
        CMolInfo::TBiomol bioMol = molInfo.GetBiomol();
        m_bSequenceIsGenomicRecord = (
            (bioMol == CMolInfo::eBiomol_genomic) ||
            (bioMol == CMolInfo::eBiomol_cRNA));
        return;
    }
    return;
}

//  ---------------------------------------------------------------------------
void CGffFeatureContext::xAssignSequenceHasBioSource()
//  ---------------------------------------------------------------------------
{
    m_bSequenceHasBioSource = false;
    if (!m_bsh) {
        return;
    }
    if (m_bsh.IsSetDescr()) {
        const CSeq_descr& descr = m_bsh.GetDescr();
        if (descr.CanGet()) {
            const list< CRef< CSeqdesc > >& listDescr = descr.Get();
            for (list< CRef< CSeqdesc > >::const_iterator cit = listDescr.begin();
                    cit != listDescr.end(); ++cit) {
                const CSeqdesc& desc = **cit;
                if (desc.IsSource()) {
                    m_bSequenceHasBioSource = true;
                    return;
                }
            }
        }
    }
    CBioseq_set_Handle setH;
    setH = m_bsh.GetParentBioseq_set();
    if (setH  &&  setH.IsSetDescr()) {
        const CSeq_descr& descr = setH.GetDescr();
        if (descr.CanGet()) {
            const list< CRef< CSeqdesc > >& listDescr = descr.Get();
            for (list< CRef< CSeqdesc > >::const_iterator cit = listDescr.begin();
                    cit != listDescr.end(); ++cit) {
                const CSeqdesc& desc = **cit;
                if (desc.IsSource()) {
                    m_bSequenceHasBioSource = true;
                    return;
                }
            }
        }
    }
    return;
}

// ----------------------------------------------------------------------------
CMappedFeat CGffFeatureContext::FindBestGeneParent(const CMappedFeat& mf)
// ----------------------------------------------------------------------------
{
    if (mf == m_mfLastIn) {
        return m_mfLastOut;
    }
    m_mfLastIn = mf;

    CSeqFeatData::ESubtype subType = mf.GetFeatSubtype();
    if (subType == CSeqFeatData::eSubtype_mobile_element) {
        m_mfLastOut = CMappedFeat();
        return m_mfLastOut;
    }

    if (mf.GetFeatSubtype() == CSeqFeatData::eSubtype_mRNA) {
        m_mfLastOut = feature::GetBestGeneForMrna(mf, &m_ft);
    }
    else {
        m_mfLastOut = feature::GetBestGeneForFeat(mf, &m_ft);
    }
    return m_mfLastOut;
}

//  ----------------------------------------------------------------------------
CConstRef<CUser_object> CWriteUtil::GetUserObjectByType(
    const CUser_object& uo,
    const string& strType)
//  ----------------------------------------------------------------------------
{
    if (uo.IsSetType() && uo.GetType().IsStr() &&
        uo.GetType().GetStr() == strType) {
        return CConstRef<CUser_object>(&uo);
    }
    const CUser_object::TData& fields = uo.GetData();
    for (CUser_object::TData::const_iterator it = fields.begin();
        it != fields.end();
        ++it) {
        const CUser_field& field = **it;
        if (field.IsSetData()) {
            const CUser_field::TData& data = field.GetData();
            if (data.Which() == CUser_field::TData::e_Object) {
                CConstRef<CUser_object> recur = CWriteUtil::GetUserObjectByType(
                    data.GetObject(), strType);
                if (recur) {
                    return recur;
                }
            }
        }
    }
    return CConstRef<CUser_object>();
}

//  ----------------------------------------------------------------------------
CConstRef<CUser_object> CWriteUtil::GetUserObjectByType(
    const list<CRef<CUser_object > >& uos,
    const string& strType)
    //  ----------------------------------------------------------------------------
{
    CConstRef<CUser_object> pResult;
    typedef list<CRef<CUser_object > >::const_iterator CIT;
    for (CIT cit = uos.begin(); cit != uos.end(); ++cit) {
        const CUser_object& uo = **cit;
        pResult = CWriteUtil::GetUserObjectByType(uo, strType);
        if (pResult) {
            return pResult;
        }
    }
    return CConstRef<CUser_object>();
}

//  ----------------------------------------------------------------------------
CConstRef<CUser_object> CWriteUtil::GetModelEvidence(
    CMappedFeat mf)
//  ----------------------------------------------------------------------------
{
    CConstRef<CUser_object> me;
    if (mf.IsSetExt()) {
        me = CWriteUtil::GetUserObjectByType(mf.GetExt(), "ModelEvidence");
    }
    if (!me  &&  mf.IsSetExts()) {
        me = CWriteUtil::GetUserObjectByType(mf.GetExts(), "ModelEvidence");
    }
    return me;
}

//  -----------------------------------------------------------------------------
size_t
s_CountAccessions(
    const CUser_field& field)
//  -----------------------------------------------------------------------------
{
    size_t count = 0;
    if (!field.IsSetData() || !field.GetData().IsFields()) {
        return 0;
    }

    //
    //  Each accession consists of yet another block of "Fields" one of which carries
    //  a label named "accession":
    //
    ITERATE(CUser_field::TData::TFields, it, field.GetData().GetFields()) {
        const CUser_field& uf = **it;
        if (uf.CanGetData() && uf.GetData().IsFields()) {

            ITERATE(CUser_field::TData::TFields, it2, uf.GetData().GetFields()) {
                const CUser_field& inner = **it2;
                if (inner.IsSetLabel() && inner.GetLabel().IsStr()) {
                    if (inner.GetLabel().GetStr() == "accession") {
                        ++count;
                    }
                }
            }
        }
    }
    return count;
}


//  ----------------------------------------------------------------------------
bool CWriteUtil::GetStringForModelEvidence(
    CMappedFeat mf,
    string& mestr)
//  ----------------------------------------------------------------------------
{
    CConstRef<CUser_object> me = CWriteUtil::GetModelEvidence(mf);
    if (!me) {
        return false;
    }

    size_t numRna(0), numEst(0), numProtein(0), numLongSra(0),
        rnaseqBaseCoverage(0), rnaseqBiosamplesIntronsFull(0);
    string method;
    const CUser_object::TData& fields = me->GetData();
    ITERATE(CUser_object::TData, it, fields) {
        const CUser_field& field = **it;
        if (!field.IsSetLabel()  ||  !field.GetLabel().IsStr()) {
            continue;
        }
        if (!field.IsSetData()) {
            continue;
        }
        const string& label = field.GetLabel().GetStr();
        if (label == "Method") {
            method = field.GetData().GetStr();
            continue;
        }
        if (label == "Counts") {
            ITERATE(CUser_field::TData::TFields, inner, field.GetData().GetFields()) {
                const CUser_field& field = **inner;
                if (!field.IsSetLabel() || !field.GetLabel().IsStr()) {
                    continue;
                }
                if (!field.IsSetData()) {
                    continue;
                }
                const string& label = field.GetLabel().GetStr();
                if (label == "mRNA") {
                    numRna = field.GetData().GetInt();
                    continue;
                }
                if (label == "EST") {
                    numEst = field.GetData().GetInt();
                    continue;
                }
                if (label == "Protein") {
                    numProtein = field.GetData().GetInt();
                    continue;
                }
                if (label == "long SRA read") {
                    numLongSra = field.GetData().GetInt();
                    continue;
                }
            }
        }
        if (label == "mRNA") {
            numRna = s_CountAccessions(field);
            continue;
        }
        if (label == "EST") {
            numEst = s_CountAccessions(field);
            continue;
        }
        if (label == "Protein") {
            numProtein = s_CountAccessions(field);
            continue;
        }
        if (label == "long SRA read") {
            numLongSra = s_CountAccessions(field);
            continue;
        }
        if (label == "rnaseq_base_coverage") {
            if (field.CanGetData()  &&  field.GetData().IsInt()) {
                rnaseqBaseCoverage = field.GetData().GetInt();
            }
            continue;
        }
        if (label == "rnaseq_biosamples_introns_full") {
            if (field.CanGetData() && field.GetData().IsInt()) {
                rnaseqBiosamplesIntronsFull = field.GetData().GetInt();
            }
            continue;
        }
    }

    //CSeqFeatData::ESubtype st = mf.GetFeatSubtype();
    CNcbiOstrstream text;
    //text << "Derived by automated computational analysis";
    //if (!NStr::IsBlank(method)) {
    //    text << " using gene prediction method: " << method;
    //}
    //text << ".";

    if (numRna > 0 || numEst > 0 || numProtein > 0 || numLongSra > 0 ||
        rnaseqBaseCoverage > 0)
    {
        text << "Supporting evidence includes similarity to:";
    }
    string section_prefix = " ";
    // The countable section
    if (numRna > 0 || numEst > 0 || numProtein > 0 || numLongSra > 0)
    {
        text << section_prefix;
        string prefix;
        if (numRna > 0) {
            text << prefix << numRna << " mRNA";
            if (numRna > 1) {
                text << 's';
            }
            prefix = ", ";
        }
        if (numEst > 0) {
            text << prefix << numEst << " EST";
            if (numEst > 1) {
                text << 's';
            }
            prefix = ", ";
        }
        if (numProtein > 0) {
            text << prefix << numProtein << " Protein";
            if (numProtein > 1) {
                text << 's';
            }
            prefix = ", ";
        }
        if (numLongSra > 0) {
            text << prefix << numLongSra << " long SRA read";
            if (numLongSra > 1) {
                text << 's';
            }
            prefix = ", ";
        }
        section_prefix = ", and ";
    }
    // The RNASeq section
    if (rnaseqBaseCoverage > 0)
    {
        text << section_prefix;

        text << rnaseqBaseCoverage << "% coverage of the annotated genomic feature by RNAseq alignments";
        if (rnaseqBiosamplesIntronsFull > 0) {
            text << ", including " << rnaseqBiosamplesIntronsFull;
            text << " sample";
            if (rnaseqBiosamplesIntronsFull > 1) {
                text << 's';
            }
            text << " with support for all annotated introns";
        }

        section_prefix = ", and ";
    }
    mestr = CNcbiOstrstreamToString(text);
    return true;
}


//  ----------------------------------------------------------------------------
bool CWriteUtil::GetThreeFeatType(
    const CSeq_feat& feat,
    string& threeFeatType)
//  ----------------------------------------------------------------------------
{
    if (!feat.IsSetExts()) {
        return false;
    }
    auto pUo = CWriteUtil::GetUserObjectByType(feat.GetExts(), "BED");
    if (!pUo  ||  !pUo->HasField("location")) {
        return false;
    }
    threeFeatType = pUo->GetField("location").GetString();
    return true;
}


//  ----------------------------------------------------------------------------
bool CWriteUtil::GetThreeFeatScore(
    const CSeq_feat& feat,
    int& score)
//  ----------------------------------------------------------------------------
{
    if (!feat.IsSetExts()) {
        return false;
    }
    auto pUo = CWriteUtil::GetUserObjectByType(feat.GetExts(), "DisplaySettings");
    if (!pUo  ||  !pUo->HasField("score")) {
        return false;
    }
    score = pUo->GetField("score").GetInt();
    return true;
}


//  ----------------------------------------------------------------------------
bool CWriteUtil::GetThreeFeatRgb(
    const CSeq_feat& feat,
    string& color)
//  ----------------------------------------------------------------------------
{
    if (!feat.IsSetExts()) {
        return false;
    }
    auto pUo = CWriteUtil::GetUserObjectByType(feat.GetExts(), "DisplaySettings");
    if (!pUo  ||  !pUo->HasField("color")) {
        return false;
    }
    color = pUo->GetField("color").GetString();
    return true;
}


//  ----------------------------------------------------------------------------
bool CWriteUtil::IsThreeFeatFormat(
    const CSeq_annot& annot)
//  ----------------------------------------------------------------------------
{
    using FTABLE = list<CRef<CSeq_feat> >;

    if (!annot.IsFtable()) {
        return false;
    }
    const FTABLE& ftable = annot.GetData().GetFtable();
    auto remainingTests = 100;
    for (auto pFeat: ftable) {
        string dummy;
        if (!CWriteUtil::GetThreeFeatType(*pFeat, dummy)) {
            return false;
        }
        if (--remainingTests == 0) {
            break;
        }
    }
    return true;
}


//  ----------------------------------------------------------------------------
bool CWriteUtil::GetStringForGoMarkup(
    const vector<CRef<CUser_field > >& fields,
    string& goMarkup)
//  ----------------------------------------------------------------------------
{
    vector<string> strings;
    if (! CWriteUtil::GetStringsForGoMarkup(fields, strings)) {
        return false;
    }
    goMarkup = NStr::Join(strings, ",");
    return true;
}

//  ----------------------------------------------------------------------------
bool CWriteUtil::GetStringsForGoMarkup(
    const vector<CRef<CUser_field > >& fields,
    vector<string>& goMarkup)
//  ----------------------------------------------------------------------------
{
    goMarkup.clear();
    for (const auto& field: fields) {
        if (!field->IsSetLabel()  ||  !field->GetLabel().IsId()
                ||  field->GetLabel().GetId() != 0) {
            continue;
        }
        if (!field->IsSetData()  ||  !field->GetData().IsFields()) {
            continue;
        }
        string descriptive, goId, pubmedId, evidence;
        const auto& subFields = field->GetData().GetFields();
        for (const auto& subField: subFields) {
            if (!subField->IsSetLabel()  ||  ! subField->GetLabel().IsStr()) {
                continue;
            }
            const auto& subLabel = subField->GetLabel().GetStr();
            if (subLabel == "text string") {
                descriptive = subField->GetData().GetStr();
                continue;
            }
            if (subLabel == "go id") {
                goId = subField->GetData().GetStr();
                continue;
            }
            if (subLabel == "pubmed id") {
                pubmedId = NStr::IntToString(subField->GetData().GetInt());
                continue;
            }
            if (subLabel == "evidence") {
                evidence = subField->GetData().GetStr();
                continue;
            }
        }
        goMarkup.push_back(descriptive + "|" + goId + "|" + pubmedId + "|" + evidence);
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CWriteUtil::GetListOfGoIds(
    const vector<CRef<CUser_field > >& fields,
    list<std::string>& goIds)
//  ----------------------------------------------------------------------------
{
    for (const auto& field: fields) {
        if (!field->IsSetLabel()  ||  !field->GetLabel().IsId()
                ||  field->GetLabel().GetId() != 0) {
            continue;
        }
        if (!field->IsSetData()  ||  !field->GetData().IsFields()) {
            continue;
        }
        string descriptive, goId, pubmedId, evidence;
        const auto& subFields = field->GetData().GetFields();
        for (const auto& subField: subFields) {
            if (!subField->IsSetLabel()  ||  ! subField->GetLabel().IsStr()) {
                continue;
            }
            const auto& subLabel = subField->GetLabel().GetStr();
            if (subLabel == "go id") {
                goId = subField->GetData().GetStr();
                goIds.push_back(string("GO:")+goId);
                continue;
            }
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CWriteUtil::CompareLocations(
    const CMappedFeat& lhs,
    const CMappedFeat& rhs)
//  ----------------------------------------------------------------------------
{
    const CSeq_loc& lhl = lhs.GetLocation();
    const CSeq_loc& rhl = rhs.GetLocation();

    //test1: id, alphabetical
    string lhs_id = CWriteUtil::GetStringId(lhl);
    string rhs_id = CWriteUtil::GetStringId(rhl);
    if (lhs_id != rhs_id) {
        return (lhs_id < rhs_id);
    }

    //test2: loc-start ascending
    size_t lhs_start = lhl.GetStart(ESeqLocExtremes::eExtreme_Positional);
    size_t rhs_start = rhl.GetStart(ESeqLocExtremes::eExtreme_Positional);
    if (lhs_start != rhs_start) {
        return (lhs_start < rhs_start);
    }
    //test3: loc-stop decending
    size_t lhs_stop = lhl.GetStop(ESeqLocExtremes::eExtreme_Positional);
    size_t rhs_stop = rhl.GetStop(ESeqLocExtremes::eExtreme_Positional);
    return (lhs_stop > rhs_stop);
}


//  ----------------------------------------------------------------------------
string CWriteUtil::GetStringId(
    const CSeq_loc& loc)
//  ----------------------------------------------------------------------------
{
    if (loc.GetId()) {
        return loc.GetId()->AsFastaString();
    }
    return "";
}

//  ----------------------------------------------------------------------------
bool CWriteUtil::IsNucleotideSequence(CBioseq_Handle bsh)
//  ----------------------------------------------------------------------------
{
    if (bsh.CanGetInst_Mol()) {
        const auto& mol = bsh.GetBioseqMolType();
        switch (mol) {
        default:
            break;
        case CSeq_inst::eMol_dna:
        case CSeq_inst::eMol_na:
        case CSeq_inst::eMol_rna:
            return true;
        case CSeq_inst::eMol_aa:
            return false;
        }
    }
    if (bsh.CanGetDescr()) {
        const auto& descrs = bsh.GetDescr().Get();
        for (const auto& pDescr: descrs) {
            if (pDescr->IsMolinfo()  &&  pDescr->GetMolinfo().CanGetBiomol()) {
                switch(pDescr->GetMolinfo().GetBiomol()) {
                case CMolInfo::eBiomol_unknown:
                case CMolInfo::eBiomol_other:
                    break;
                case CMolInfo::eBiomol_peptide:
                    return false;
                default:
                    return true;
                }
            }
        }
    }
    return false;
}


//  ----------------------------------------------------------------------------
bool CWriteUtil::IsProteinSequence(CBioseq_Handle bsh)
//  ----------------------------------------------------------------------------
{
    if (bsh.CanGetInst_Mol()) {
        const auto& mol = bsh.GetBioseqMolType();
        switch (mol) {
        default:
            break;
        case CSeq_inst::eMol_dna:
        case CSeq_inst::eMol_na:
        case CSeq_inst::eMol_rna:
            return false;
        case CSeq_inst::eMol_aa:
            return true;
        }
    }
    if (bsh.CanGetDescr()) {
        const auto& descrs = bsh.GetDescr().Get();
        for (const auto& pDescr: descrs) {
            if (pDescr->IsMolinfo()  &&  pDescr->GetMolinfo().CanGetBiomol()) {
                switch(pDescr->GetMolinfo().GetBiomol()) {
                case CMolInfo::eBiomol_unknown:
                case CMolInfo::eBiomol_other:
                    break;
                case CMolInfo::eBiomol_peptide:
                    return true;
                default:
                    return false;
                }
            }
        }
    }
    return false;
}

//  ----------------------------------------------------------------------------
bool CWriteUtil::IsTransspliced(const CSeq_feat& feature)
//  ----------------------------------------------------------------------------
{
    return (feature.IsSetExcept_text() && feature.GetExcept_text() == "trans-splicing");
}


//  ----------------------------------------------------------------------------
bool CWriteUtil::IsTransspliced(const CMappedFeat& mf)
//  ----------------------------------------------------------------------------
{
    return CWriteUtil::IsTransspliced(mf.GetMappedFeature());
    //return (mf.IsSetExcept_text()  &&  mf.GetExcept_text() == "trans-splicing");
}


//  ----------------------------------------------------------------------------
bool CWriteUtil::GetTranssplicedEndpoints(
//  ----------------------------------------------------------------------------
    const CSeq_loc& loc,
    unsigned int& inPoint,
    unsigned int& outPoint)
//  start determined by the minimum start of any sub interval
//  stop determined by the maximum stop of any sub interval
//  ----------------------------------------------------------------------------
{
    typedef list<CRef<CSeq_interval> >::const_iterator CIT;

    CSeq_loc testLoc;
    testLoc.Assign(loc);
    if (testLoc.IsMix()) {
        testLoc.ChangeToPackedInt();
    }
    if (!testLoc.IsPacked_int()) {
        return false;
    }
    const CPacked_seqint& packedInt = testLoc.GetPacked_int();
    inPoint = packedInt.GetStart(eExtreme_Biological);
    outPoint = packedInt.GetStop(eExtreme_Biological);
    const list<CRef<CSeq_interval> >& intvs = packedInt.Get();
    for (CIT cit = intvs.begin(); cit != intvs.end(); cit++) {
        const CSeq_interval& intv = **cit;
        if (intv.GetFrom() < inPoint) {
            inPoint = intv.GetFrom();
        }
        if (intv.GetTo() > outPoint) {
            outPoint = intv.GetTo();
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
ENa_strand CWriteUtil::GetEffectiveStrand(
    const CSeq_interval& interval)
//  ----------------------------------------------------------------------------
{
    // if it's not explicitely minus, then it's plus
    //  (not true for other location types)
    return (interval.IsSetStrand() && interval.GetStrand() == eNa_strand_minus)
        ?
        eNa_strand_minus :
        eNa_strand_plus;
}



END_NCBI_SCOPE
