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
                string intv_str = "";
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
bool CWriteUtil::GetGeneRefGene(
    const CGene_ref& generef,
    string& gene )
//  ----------------------------------------------------------------------------
{
#define EMIT(str) { gene = str; return true; }
   if (generef.IsSetLocus()) {
        EMIT(generef.GetLocus());
    }
    if (generef.IsSetSyn()  && generef.GetSyn().size() > 0) {
        EMIT(generef.GetSyn().front());
    }
    if (generef.IsSetDesc()) {
        EMIT(generef.GetDesc());
    }
    return false;
#undef EMIT
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
string CWriteUtil::UrlEncode(
    const string& raw)
//  ----------------------------------------------------------------------------
{
    static const char s_Table[256][4] = {
        "%00", "%01", "%02", "%03", "%04", "%05", "%06", "%07", "%08", "%09", 
        "%0A", "%0B", "%0C", "%0D", "%0E", "%0F", "%10", "%11", "%12", "%13", 
        "%14", "%15", "%16", "%17", "%18", "%19", "%1A", "%1B", "%1C", "%1D", 
        "%1E", "%1F", " ",   "!",   "%22", "%23", "$",   "%25", "%26", "%27",
        "%28", "%29", "%2A", "%2B", "%2C", "-",   ".",   "%2F", "0",   "1",   
        "2",   "3",   "4",   "5",   "6",   "7",   "8",   "9",   ":",   "%3B", 
        "%3C", "%3D", "%3E", "%3F", "@",   "A",   "B",   "C",   "D",   "E",   
        "F",   "G",   "H",   "I",   "J",   "K",   "L",   "M",   "N",   "O",
        "P",   "Q",   "R",   "S",   "T",   "U",   "V",   "W",   "X",   "Y",   
        "Z",   "%5B", "%5C", "%5D", "^",   "_",   "%60", "a",   "b",   "c",   
        "d",   "e",   "f",   "g",   "h",   "i",   "j",   "k",   "l",   "m",   
        "n",   "o",   "p",   "q",   "r",   "s",   "t",   "u",   "v",   "w",
        "x",   "y",   "z",   "%7B", "%7C", "%7D", "%7E", "%7F", "%80", "%81", 
        "%82", "%83", "%84", "%85", "%86", "%87", "%88", "%89", "%8A", "%8B", 
        "%8C", "%8D", "%8E", "%8F", "%90", "%91", "%92", "%93", "%94", "%95", 
        "%96", "%97", "%98", "%99", "%9A", "%9B", "%9C", "%9D", "%9E", "%9F",
        "%A0", "%A1", "%A2", "%A3", "%A4", "%A5", "%A6", "%A7", "%A8", "%A9", 
        "%AA", "%AB", "%AC", "%AD", "%AE", "%AF", "%B0", "%B1", "%B2", "%B3", 
        "%B4", "%B5", "%B6", "%B7", "%B8", "%B9", "%BA", "%BB", "%BC", "%BD", 
        "%BE", "%BF", "%C0", "%C1", "%C2", "%C3", "%C4", "%C5", "%C6", "%C7",
        "%C8", "%C9", "%CA", "%CB", "%CC", "%CD", "%CE", "%CF", "%D0", "%D1", 
        "%D2", "%D3", "%D4", "%D5", "%D6", "%D7", "%D8", "%D9", "%DA", "%DB", 
        "%DC", "%DD", "%DE", "%DF", "%E0", "%E1", "%E2", "%E3", "%E4", "%E5", 
        "%E6", "%E7", "%E8", "%E9", "%EA", "%EB", "%EC", "%ED", "%EE", "%EF",
        "%F0", "%F1", "%F2", "%F3", "%F4", "%F5", "%F6", "%F7", "%F8", "%F9", 
        "%FA", "%FB", "|", "%FD", "%FE", "%FF"
    };

    string encoded;
    for ( size_t i = 0;  i < raw.size();  ++i ) {
        encoded += s_Table[static_cast<unsigned char>( raw[i] )];
    }
    return encoded;
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
    string& best_id )
//  ----------------------------------------------------------------------------
{
    if (!idh) {
        return false;
    }
    CSeq_id_Handle best_idh = sequence::GetId(idh, scope, sequence::eGetId_Best);
    if (!best_idh) {
        best_idh = idh;
    }
    string backup = best_id;
    try {
        best_idh.GetSeqId()->GetLabel(&best_id, CSeq_id::eContent);
    }
    catch (...) {
        best_id = backup;
        return false;
    }
    return true;
}
    
//  ----------------------------------------------------------------------------
bool CWriteUtil::GetBestId(
    const CMappedFeat& mf,
    string& best_id)
//  ----------------------------------------------------------------------------
{
	CSeq_id_Handle idh = mf.GetLocationId();
    if (idh) {
        return GetBestId(idh, mf.GetScope(), best_id);
    }
    const CSeq_loc& loc = mf.GetLocation();
    idh = sequence::GetIdHandle(loc, &mf.GetScope());
    return  GetBestId(idh, mf.GetScope(), best_id);

    /**
    if (loc.IsInt()) {
        return  GetBestId( 
            CSeq_id_Handle::GetHandle(loc.GetInt().GetId()), 
            mf.GetScope(), best_id);
    }
    if (loc.IsMix()) {
        try {
            const CSeq_loc& sub = *loc.GetMix().Get().front();
            if (sub.IsInt()) {
                return  GetBestId( 
                    CSeq_id_Handle::GetHandle(sub.GetInt().GetId()), 
                    mf.GetScope(), best_id);
            }
            else
            if (sub.IsMix())
            {
                const CSeq_id* pid = sub.GetMix().GetFirstLoc()->GetId();
                return GetBestId(CSeq_id_Handle::GetHandle(*pid), mf.GetScope(),
                    best_id);
            }
            const CSeq_id* pid = sub.GetId();
            return GetBestId(CSeq_id_Handle::GetHandle(*pid), mf.GetScope(),
                best_id);

        }
        catch (...) {};
    }
    best_id = mf.GetLocationId().AsString();
    **/
    return true;
}

//  ----------------------------------------------------------------------------
bool CWriteUtil::IsNucProtSet(
    CSeq_entry_Handle seh)
//  ----------------------------------------------------------------------------
{
    return (seh.IsSet()  &&  seh.GetSet().IsSetClass()  &&  
        seh.GetSet().GetClass() == CBioseq_set::eClass_nuc_prot);
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
    if (!m_bsh || !m_bsh.IsSetDescr()) {
        m_bSequenceIsGenomicRecord = false;
        return;
    }
    const CSeq_descr& descr = m_bsh.GetDescr();
    if (!descr.CanGet()) {
        m_bSequenceIsGenomicRecord = false;
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
    m_bSequenceIsGenomicRecord = false;
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

    //CMappedFeat gene;
    if (mf.GetFeatSubtype() == CSeqFeatData::eSubtype_mRNA) {
        m_mfLastOut = feature::GetBestGeneForMrna(mf, &m_ft);
        if (!m_mfLastOut) {
            m_mfLastOut = feature::GetBestGeneForMrna(mf, &m_ft, 0,
                feature::CFeatTree::eBestGene_AllowOverlapped);
        }
    }
    else {
        m_mfLastOut = feature::GetBestGeneForFeat(mf, &m_ft);
        if (!m_mfLastOut) {
            m_mfLastOut = feature::GetBestGeneForFeat(mf, &m_ft, 0,
                feature::CFeatTree::eBestGene_AllowOverlapped);
        }
    }
    if (!m_mfLastOut) {
        CSeq_loc loc;
        loc.Add(mf.GetLocation());
        loc.SetStrand(objects::eNa_strand_unknown);
        CConstRef<CSeq_feat> pOverlap = sequence::GetBestOverlappingFeat(
            loc,
            CSeqFeatData::eSubtype_gene,
            sequence::eOverlap_Interval,
            mf.GetScope());
        CFeat_CI ci(mf.GetScope(), loc);
        if (ci) {
            m_mfLastOut = *ci;
        }
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

    CSeqFeatData::ESubtype st = mf.GetFeatSubtype();
    CNcbiOstrstream text;
    //text << "Derived by automated computational analysis";
    //if (!NStr::IsBlank(method)) {
    //    text << " using gene prediction method: " << method;
    //}
    //text << ".";

    if (numRna > 0 || numEst > 0 || numProtein > 0 || numLongSra > 0 ||
        rnaseqBaseCoverage > 0)
    {
        text << " Supporting evidence includes similarity to:";
    }
    string section_prefix = " ";
    // The countable section
    if (numRna > 0 || numEst > 0 || numProtein > 0 || numLongSra > 0)
    {
        text << section_prefix;
        string prefix = "";
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

END_NCBI_SCOPE
