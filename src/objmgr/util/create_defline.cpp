/*
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
* Author: Jonathan Kans, Aaron Ucko
*
* File Description:
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <objmgr/util/create_defline.hpp>

#include <serial/iterator.hpp>

#include <objects/misc/sequence_macros.hpp>

#include <objmgr/feat_ci.hpp>
#include <objmgr/seq_map_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/mapped_feat.hpp>
#include <objmgr/seq_entry_ci.hpp>

#include <objmgr/util/feature.hpp>
#include <objmgr/util/sequence.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(sequence);
USING_SCOPE(feature);

// constructor
CDeflineGenerator::CDeflineGenerator (void)
{
    m_ConstructedFeatTree = false;
    m_InitializedFeatTree = false;
    m_Low_Quality_Fsa = 0;

    m_Low_Quality_Fsa.AddWord ("heterogeneous population sequenced", 1);
    m_Low_Quality_Fsa.AddWord ("low-quality sequence region", 2);
    m_Low_Quality_Fsa.AddWord ("unextendable partial coding region", 3);
    m_Low_Quality_Fsa.Prime ();
}

// constructor
CDeflineGenerator::CDeflineGenerator (const CSeq_entry_Handle& tseh)
{
    // first call default constructor
    CDeflineGenerator ();

    // then store top SeqEntry Handle for building CFeatTree when first needed
    m_TopSEH = tseh;
    m_ConstructedFeatTree = true;
    m_InitializedFeatTree = false;
}

// destructor
CDeflineGenerator::~CDeflineGenerator (void)

{
}

// macros

// SEQENTRY_HANDLE_ON_SEQENTRY_HANDLE_ITERATOR
// FOR_EACH_SEQENTRY_HANDLE_ON_SEQENTRY_HANDLE
// CSeq_entry_Handle as input,
//  dereference with CSeq_entry_Handle var = *Itr;

#define SEQENTRY_HANDLE_ON_SEQENTRY_HANDLE_ITERATOR(Itr, Var) \
CSeq_entry_CI Itr(Var)

#define FOR_EACH_SEQENTRY_HANDLE_ON_SEQENTRY_HANDLE(Itr, Var) \
for (SEQENTRY_HANDLE_ON_SEQENTRY_HANDLE_ITERATOR(Itr, Var); Itr;  ++Itr)

// FOR_EACH_SEQID_ON_BIOSEQ_HANDLE
// CBioseq_Handle& as input,
//  dereference with CSeq_id_Handle sid = *Itr;

#define FOR_EACH_SEQID_ON_BIOSEQ_HANDLE(Itr, Var) \
ITERATE (CBioseq_Handle::TId, Itr, Var.GetId())

// SEQDESC_ON_BIOSEQ_HANDLE_ITERATOR
// FOR_EACH_SEQDESC_ON_BIOSEQ_HANDLE
// CBioseq_Handle& as input,
//  dereference with const C##Chs& sdc = Itr->Get##Chs();

#define SEQDESC_ON_BIOSEQ_HANDLE_ITERATOR(Itr, Var, Chs) \
CSeqdesc_CI Itr(Var, CSeqdesc::e_##Chs)

#define FOR_EACH_SEQDESC_ON_BIOSEQ_HANDLE(Itr, Var, Chs) \
for (SEQDESC_ON_BIOSEQ_HANDLE_ITERATOR(Itr, Var, Chs); Itr;  ++Itr)

// SEQFEAT_ON_BIOSEQ_HANDLE_ITERATOR
// FOR_EACH_SEQFEAT_ON_BIOSEQ_HANDLE
// CBioseq_Handle& as input,
//  dereference with const CSeq_feat& sft = Itr->GetOriginalFeature();

#define SEQFEAT_ON_BIOSEQ_HANDLE_ITERATOR(Itr, Var, Chs) \
CFeat_CI Itr(Var, CSeqFeatData::e_##Chs)

#define FOR_EACH_SEQFEAT_ON_BIOSEQ_HANDLE(Itr, Var, Chs) \
for (SEQFEAT_ON_BIOSEQ_HANDLE_ITERATOR(Itr, Var, Chs); Itr;  ++Itr)

// SEQFEAT_ON_SCOPE_ITERATOR
// FOR_EACH_SEQFEAT_ON_SCOPE
// CScope& as input,
//  dereference with const CSeq_feat& sft = Itr->GetOriginalFeature();

#define SEQFEAT_ON_SCOPE_ITERATOR(Itr, Var, Loc, Chs) \
CFeat_CI Itr(Var, Loc, CSeqFeatData::e_##Chs)

#define FOR_EACH_SEQFEAT_ON_SCOPE(Itr, Var, Loc, Chs) \
for (SEQFEAT_ON_SCOPE_ITERATOR(Itr, Var, Loc, Chs); Itr;  ++Itr)

// SELECTED_SEQFEAT_ON_BIOSEQ_HANDLE_ITERATOR
// FOR_SELECTED_SEQFEAT_ON_BIOSEQ_HANDLE
// CBioseq_Handle& and SAnnotSelector as input,
//  dereference with const CSeq_feat& sft = Itr->GetOriginalFeature();

#define SELECTED_SEQFEAT_ON_BIOSEQ_HANDLE_ITERATOR(Itr, Var, Sel) \
CFeat_CI Itr(Var, Sel)

#define FOR_SELECTED_SEQFEAT_ON_BIOSEQ_HANDLE(Itr, Var, Sel) \
for (SELECTED_SEQFEAT_ON_BIOSEQ_HANDLE_ITERATOR(Itr, Var, Sel); Itr;  ++Itr)

// set instance variables from Seq-inst, Seq-ids, MolInfo, etc., but not
//  BioSource
void CDeflineGenerator::x_SetFlags (
    const CBioseq_Handle& bsh,
    TUserFlags flags
)

{
    // set flags from record components
    m_Reconstruct = (flags & fIgnoreExisting) != 0;
    m_AllProtNames = (flags & fAllProteinNames) != 0;

    // reset member variables to cleared state
    m_IsNA = false;
    m_IsAA = false;

    m_IsSeg = false;
    m_IsDelta = false;

    m_IsNC = false;
    m_IsNM = false;
    m_IsNR = false;
    m_IsPatent = false;
    m_IsPDB = false;
    m_ThirdParty = false;
    m_WGSMaster = false;
    m_TSAMaster = false;

    m_GeneralStr.clear();
    m_PatentCountry.clear();
    m_PatentNumber.clear();

    m_PatentSequence = 0;

    m_PDBChain = 0;

    m_MIBiomol = NCBI_BIOMOL(unknown);
    m_MITech = NCBI_TECH(unknown);
    m_MICompleteness = NCBI_COMPLETENESS(unknown);

    m_HTGTech = false;
    m_HTGSUnfinished = false;
    m_IsTSA = false;
    m_IsWGS = false;
    m_IsEST_STS_GSS = false;

    m_UseBiosrc = false;

    m_HTGSCancelled = false;
    m_HTGSDraft = false;
    m_HTGSPooled = false;
    m_TPAExp = false;
    m_TPAInf = false;
    m_TPAReasm = false;

    m_PDBCompound.clear();

    m_Taxname.clear();
    m_Genome = NCBI_GENOME(unknown);

    m_Chromosome.clear();
    m_Clone.clear();
    m_has_clone = false;
    m_Map.clear();
    m_Plasmid.clear();
    m_Segment.clear();

    m_Breed.clear();
    m_Cultivar.clear();
    m_Isolate.clear();
    m_Strain.clear();

    m_IsUnverified = false;

    // now start setting member variables
    m_IsNA = bsh.IsNa();
    m_IsAA = bsh.IsAa();

    if (bsh.IsSetInst()) {
        if (bsh.IsSetInst_Repr()) {
            TSEQ_REPR repr = bsh.GetInst_Repr();
            m_IsSeg = (repr == CSeq_inst::eRepr_seg);
            m_IsDelta = (repr == CSeq_inst::eRepr_delta);
            m_IsVirtual = (repr == CSeq_inst::eRepr_virtual);
        }
    }

    // process Seq-ids
    FOR_EACH_SEQID_ON_BIOSEQ_HANDLE (sid_itr, bsh) {
        CSeq_id_Handle sid = *sid_itr;
        switch (sid.Which()) {
            case NCBI_SEQID(Other):
            case NCBI_SEQID(Genbank):
            case NCBI_SEQID(Embl):
            case NCBI_SEQID(Ddbj):
            {
                CConstRef<CSeq_id> id = sid.GetSeqId();
                const CTextseq_id& tsid = *id->GetTextseq_Id ();
                if (tsid.IsSetAccession()) {
                    const string& acc = tsid.GetAccession ();
                    TACCN_CHOICE type = CSeq_id::IdentifyAccession (acc);
                    if ( (type & NCBI_ACCN(division_mask)) == NCBI_ACCN(wgs) ) 
                    {
                        if( (type & CSeq_id::fAcc_master) != 0 ) {
                            m_WGSMaster = true;
                        }
                    } else if ( (type & NCBI_ACCN(division_mask)) == NCBI_ACCN(tsa) ) 
                    {
                        if( (type & CSeq_id::fAcc_master) != 0 && m_IsVirtual ) {
                            m_TSAMaster = true;
                        }
                    } else if ((type & NCBI_ACCN(division_mask)) == NCBI_ACCN(refseq_chromosome)) {
                        m_IsNC = true;
                    } else if ((type & NCBI_ACCN(division_mask)) == NCBI_ACCN(refseq_mrna)) {
                        m_IsNM = true;
                    } else if ((type & NCBI_ACCN(division_mask)) == NCBI_ACCN(refseq_mrna_predicted)) {
                        m_IsNM = true;
                    } else if ((type & NCBI_ACCN(division_mask)) == NCBI_ACCN(refseq_ncrna)) {
                        m_IsNR = true;
                    }
                }
                break;
            }
            case NCBI_SEQID(General):
            {
                CConstRef<CSeq_id> id = sid.GetSeqId();
                const CDbtag& gen_id = id->GetGeneral ();
                if (! gen_id.IsSkippable ()) {
                    if (gen_id.IsSetTag ()) {
                        const CObject_id& oid = gen_id.GetTag();
                        if (oid.IsStr()) {
                            m_GeneralStr = oid.GetStr();
                        }
                    }
                }
                break;
            }
            case NCBI_SEQID(Tpg):
            case NCBI_SEQID(Tpe):
            case NCBI_SEQID(Tpd):
                m_ThirdParty = true;
                break;
            case NCBI_SEQID(Pdb):
            {
                m_IsPDB = true;
                CConstRef<CSeq_id> id = sid.GetSeqId();
                const CPDB_seq_id& pdb_id = id->GetPdb ();
                if (pdb_id.IsSetChain()) {
                    m_PDBChain = pdb_id.GetChain();
                }
                break;
            }
            case NCBI_SEQID(Patent):
            {
                m_IsPatent = true;
                CConstRef<CSeq_id> id = sid.GetSeqId();
                const CPatent_seq_id& pat_id = id->GetPatent();
                if (pat_id.IsSetSeqid()) {
                    m_PatentSequence = pat_id.GetSeqid();
                }
                if (pat_id.IsSetCit()) {
                    const CId_pat& cit = pat_id.GetCit();
                    m_PatentCountry = cit.GetCountry();
                    m_PatentNumber = cit.GetSomeNumber();
                }
                break;
            }
            case NCBI_SEQID(Gpipe):
                break;
            default:
                break;
        }
    }

    // process MolInfo tech
    {
        FOR_EACH_SEQDESC_ON_BIOSEQ_HANDLE (desc_it, bsh, Molinfo) {
            const CMolInfo& molinf = desc_it->GetMolinfo();
            m_MIBiomol = molinf.GetBiomol();
            m_MITech = molinf.GetTech();
            m_MICompleteness = molinf.GetCompleteness();
            switch (m_MITech) {
                case NCBI_TECH(htgs_0):
                case NCBI_TECH(htgs_1):
                case NCBI_TECH(htgs_2):
                    m_HTGSUnfinished = true;
                    // manufacture all titles for unfinished HTG sequences
                    m_Reconstruct = true;
                    // fall through
                case NCBI_TECH(htgs_3):
                    m_HTGTech = true;
                    m_UseBiosrc = true;
                    break;
                case NCBI_TECH(est):
                case NCBI_TECH(sts):
                case NCBI_TECH(survey):
                    m_IsEST_STS_GSS = true;
                    m_UseBiosrc = true;
                    break;
                case NCBI_TECH(wgs):
                    m_IsWGS = true;
                    m_UseBiosrc = true;
                    break;
                case NCBI_TECH(tsa):
                    m_IsTSA = true;
                    m_UseBiosrc = true;
                    break;
                default:
                    break;
            }

            // take first, then break to skip remainder
            break;
        }
    }

    // process Unverified user object
    {
        FOR_EACH_SEQDESC_ON_BIOSEQ_HANDLE (desc_it, bsh, User) {
            const CUser_object& user_obj = desc_it->GetUser();
            if (FIELD_IS_SET_AND_IS(user_obj, Type, Str) && user_obj.GetType().GetStr() == "Unverified" ) {
                m_IsUnverified = true;
                break;
            }
        }
    }

    if (m_HTGTech || m_ThirdParty) {

        // process keywords
        const list <string> *keywords = NULL;

        FOR_EACH_SEQDESC_ON_BIOSEQ_HANDLE (desc_it, bsh, Genbank) {
            const CGB_block& gbk = desc_it->GetGenbank();
            if (gbk.IsSetKeywords()) {
                keywords = &gbk.GetKeywords();
            }

            // take first, then break to skip remainder
            break;
        }
        if (keywords == NULL) {
            FOR_EACH_SEQDESC_ON_BIOSEQ_HANDLE (desc_it, bsh, Embl) {
                const CEMBL_block& ebk = desc_it->GetEmbl();
                if (ebk.IsSetKeywords()) {
                    keywords = &ebk.GetKeywords();
                }

                // take first, then break to skip remainder
                break;
            }
        }
        if (keywords != NULL) {
            FOR_EACH_STRING_IN_LIST (kw_itr, *keywords) {
                const string& str = *kw_itr;
                if (NStr::EqualNocase (str, "HTGS_DRAFT")) {
                    m_HTGSDraft = true;
                } else if (NStr::EqualNocase (str, "HTGS_CANCELLED")) {
                    m_HTGSCancelled = true;
                } else if (NStr::EqualNocase (str, "HTGS_POOLED_MULTICLONE")) {
                    m_HTGSPooled = true;
                } else if (NStr::EqualNocase (str, "TPA:experimental")) {
                    m_TPAExp = true;
                } else if (NStr::EqualNocase (str, "TPA:inferential")) {
                    m_TPAInf = true;
                } else if (NStr::EqualNocase (str, "TPA:reassembly")) {
                    m_TPAReasm = true;
                }
            }
        }
    }

    if (m_IsPDB) {

        // process PDB block
        FOR_EACH_SEQDESC_ON_BIOSEQ_HANDLE (desc_it, bsh, Pdb) {
            const CPDB_block& pbk = desc_it->GetPdb();
            FOR_EACH_COMPOUND_ON_PDBBLOCK (cp_itr, pbk) {
                const string& str = *cp_itr;
                if (m_PDBCompound.empty()) {
                    m_PDBCompound = str;

                    // take first, then break to skip remainder
                    break;
                }
            }
            break;
        }
    }
}

// set instance variables from BioSource
void CDeflineGenerator::x_SetBioSrc (
    const CBioseq_Handle& bsh
)

{
    FOR_EACH_SEQDESC_ON_BIOSEQ_HANDLE (desc_it, bsh, Source) {
        const CBioSource& source = desc_it->GetSource();

        // get organism name
        if (source.IsSetTaxname()) {
            m_Taxname = source.GetTaxname();
        }
        if (source.IsSetGenome()) {
            m_Genome = source.GetGenome();
        }

        // process SubSource
        FOR_EACH_SUBSOURCE_ON_BIOSOURCE (sbs_itr, source) {
            const CSubSource& sbs = **sbs_itr;
            if (! sbs.IsSetName()) continue;
            const string& str = sbs.GetName();
            SWITCH_ON_SUBSOURCE_CHOICE (sbs) {
                case NCBI_SUBSOURCE(chromosome):
                    m_Chromosome = str;
                    break;
                case NCBI_SUBSOURCE(clone):
                    m_Clone = str;
                    m_has_clone = true;
                    break;
                case NCBI_SUBSOURCE(map):
                    m_Map = str;
                    break;
                case NCBI_SUBSOURCE(plasmid_name):
                    m_Plasmid = str;
                    break;
                case NCBI_SUBSOURCE(segment):
                    m_Segment = str;
                    break;
                default:
                    break;
            }
        }

        // process OrgMod
        FOR_EACH_ORGMOD_ON_BIOSOURCE (omd_itr, source) {
            const COrgMod& omd = **omd_itr;
            if (! omd.IsSetSubname()) continue;
            const string& str = omd.GetSubname();
            SWITCH_ON_ORGMOD_CHOICE (omd) {
                case NCBI_ORGMOD(strain):
                    if (m_Strain.empty()) {
                        m_Strain = str;
                    }
                    break;
                case NCBI_ORGMOD(cultivar):
                    if (m_Cultivar.empty()) {
                        m_Cultivar = str;
                    }
                    break;
                case NCBI_ORGMOD(isolate):
                    if (m_Isolate.empty()) {
                        m_Isolate = str;
                    }
                    break;
                case NCBI_ORGMOD(breed):
                    if (m_Breed.empty()) {
                        m_Breed = str;
                    }
                    break;
                default:
                    break;
            }
        }

        // take first, then break to skip remainder
        break;
    }

    if (m_has_clone) return;

    FOR_EACH_SEQFEAT_ON_BIOSEQ_HANDLE (feat_it, bsh, Biosrc) {
        const CSeq_feat& feat = feat_it->GetOriginalFeature();
        if (! feat.IsSetData ()) continue;
        const CSeqFeatData& sfdata = feat.GetData ();
        const CBioSource& source = sfdata.GetBiosrc();

        // process SubSource
        FOR_EACH_SUBSOURCE_ON_BIOSOURCE (sbs_itr, source) {
            const CSubSource& sbs = **sbs_itr;
            if (! sbs.IsSetName()) continue;
            SWITCH_ON_SUBSOURCE_CHOICE (sbs) {
                case NCBI_SUBSOURCE(clone):
                    m_has_clone = true;
                    break;
                default:
                    break;
            }
        }
    }
}

// generate title from BioSource fields
string CDeflineGenerator::x_DescribeClones (void)

{
    if (m_HTGSUnfinished && m_HTGSPooled && m_has_clone) {
        return ", pooled multiple clones";
    }

    if( m_Clone.empty() ) {
        return kEmptyStr;
    }

    SIZE_TYPE count = 1;
    for (SIZE_TYPE pos = m_Clone.find(';'); pos != NPOS;
         pos = m_Clone.find(';', pos + 1)) {
        ++count;
    }
    if (count > 3) {
        return ", " + NStr::SizetToString(count) + " clones";
    } else {
        return " clone " + m_Clone;
    }
}

static bool x_EndsWithStrain (
    string taxname,
    string strain
)

{
    // return NStr::EndsWith(taxname, strain, NStr::eNocase);
    if (strain.size() >= taxname.size()) {
        return false;
    }
    SIZE_TYPE pos = taxname.find(' ');
    if (pos == NPOS) {
        return false;
    }
    pos = taxname.find(' ', pos + 1);
    if (pos == NPOS) {
        return false;
    }

    pos = NStr::FindNoCase (taxname, strain, 0, taxname.size() - 1, NStr::eLast);
    if (pos == taxname.size() - strain.size()) {
        // check for space to avoid fortuitous match to end of taxname
        char ch = taxname[pos - 1];
        if (ispunct (ch) || isspace (ch)) {
            return true;
        }
    } else if (pos == taxname.size() - strain.size() - 1
               &&  taxname[pos - 1] == '\''
               &&  taxname[taxname.size() - 1] == '\'') {
        return true;
    }
    return false;
}

string CDeflineGenerator::x_TitleFromBioSrc (void)

{
    string chr, cln, mp, pls, stn, sfx;

    if (! m_Strain.empty()) {
        if (! x_EndsWithStrain (m_Taxname, m_Strain)) {
            stn = " strain " + m_Strain.substr (0, m_Strain.find(';'));
        }
    }
    if (! m_Chromosome.empty()) {
        chr = " chromosome " + m_Chromosome;
    }
    if (m_has_clone) {
        cln = x_DescribeClones ();
    }
    if (! m_Map.empty()) {
        mp = " map " + m_Map;
    }
    if (! m_Plasmid.empty()) {
        if (m_IsWGS) {
            pls = " plasmid " + m_Plasmid;
        }
    }

    string title = NStr::TruncateSpaces
        (m_Taxname + stn + chr + cln + mp + pls + sfx);

    if (islower ((unsigned char) title[0])) {
        title [0] = toupper ((unsigned char) title [0]);
    }

    return title;
}

// generate title for NC
static string x_OrganelleName (
    TBIOSOURCE_GENOME genome,
    bool has_plasmid,
    bool virus_or_phage,
    bool wgs_suffix
)

{
    string result;

    switch (genome) {
        case NCBI_GENOME(chloroplast):
            result = "chloroplast";
            break;
        case NCBI_GENOME(chromoplast):
            result = "chromoplast";
            break;
        case NCBI_GENOME(kinetoplast):
            result = "kinetoplast";
            break;
        case NCBI_GENOME(mitochondrion):
        {
            if (has_plasmid || wgs_suffix) {
                result = "mitochondrial";
            } else {
                result = "mitochondrion";
            }
            break;
        }
        case NCBI_GENOME(plastid):
            result = "plastid";
            break;
        case NCBI_GENOME(macronuclear):
        {
            result = "macronuclear";
            break;
        }
        case NCBI_GENOME(extrachrom):
        {
            if (! wgs_suffix) {
                result = "extrachromosomal";
            }
            break;
        }
        case NCBI_GENOME(plasmid):
        {
            if (! wgs_suffix) {
                result = "plasmid";
            }
            break;
        }
        // transposon and insertion-seq are obsolete
        case NCBI_GENOME(cyanelle):
            result = "cyanelle";
            break;
        case NCBI_GENOME(proviral):
        {
            if (! virus_or_phage) {
                if (has_plasmid || wgs_suffix) {
                    result = "proviral";
                } else {
                    result = "provirus";
                }
            }
            break;
        }
        case NCBI_GENOME(virion):
        {
            if (! virus_or_phage) {
                result = "virus";
            }
            break;
        }
        case NCBI_GENOME(nucleomorph):
        {
            if (! wgs_suffix) {
                result = "nucleomorph";
            }
           break;
        }
        case NCBI_GENOME(apicoplast):
            result = "apicoplast";
            break;
        case NCBI_GENOME(leucoplast):
            result = "leucoplast";
            break;
        case NCBI_GENOME(proplastid):
            result = "proplastid";
            break;
        case NCBI_GENOME(endogenous_virus):
            result = "endogenous virus";
            break;
        case NCBI_GENOME(hydrogenosome):
            result = "hydrogenosome";
            break;
        case NCBI_GENOME(chromosome):
            result = "chromosome";
            break;
        case NCBI_GENOME(chromatophore):
            result = "chromatophore";
            break;
    }

    return result;
}

string CDeflineGenerator::x_TitleFromNC (void)

{
    bool   has_plasmid = false, virus_or_phage = false, is_plasmid = false;
    string orgnl, pls, seq_tag, gen_tag;
    string result;

     if (m_MIBiomol != NCBI_BIOMOL(genomic) &&
         m_MIBiomol != NCBI_BIOMOL(other_genetic)) return result;

    // require taxname to be set
    if (m_Taxname.empty()) return result;

    string lc_name = m_Taxname;
    NStr::ToLower (lc_name);

    if (lc_name.find("virus") != NPOS || lc_name.find("phage") != NPOS) {
        virus_or_phage = true;
    }

    if (! m_Plasmid.empty()) {
        has_plasmid = true;
        string lc_plasmid = m_Plasmid;
        NStr::ToLower (lc_plasmid);
        if (lc_plasmid.find("plasmid") == NPOS &&
            lc_plasmid.find("element") == NPOS) {
            pls = "plasmid " + m_Plasmid;
        } else {
            pls = m_Plasmid;
        }
    }

    orgnl = x_OrganelleName (m_Genome, has_plasmid, virus_or_phage, false);

    is_plasmid = (m_Genome == NCBI_GENOME(plasmid));

    switch (m_MICompleteness) {
        case NCBI_COMPLETENESS(partial):
        case NCBI_COMPLETENESS(no_left):
        case NCBI_COMPLETENESS(no_right):
        case NCBI_COMPLETENESS(no_ends):
            seq_tag = ", partial sequence";
            gen_tag = ", genome";
            break;
        default:
            seq_tag = ", complete sequence";
            gen_tag = ", complete genome";
            break;
    }

    if (lc_name.find ("plasmid") != NPOS) {
        result = m_Taxname + seq_tag;        
    } else if (is_plasmid) {
        if (pls.empty()) {
            result = m_Taxname + " unnamed plasmid" + seq_tag;
        } else {
            result = m_Taxname + " " + pls + seq_tag;
        }
    } else if (! pls.empty() ) {
        if (orgnl.empty()) {
            result = m_Taxname + " " + pls + seq_tag;
        } else {
            result = m_Taxname + " " + orgnl + " " + pls + seq_tag;
        }
    } else if (! orgnl.empty() ) {
        if ( m_Chromosome.empty() ) {
            result = m_Taxname + " " + orgnl + gen_tag;
        } else {
            result = m_Taxname + " " + orgnl + " chromosome " + m_Chromosome
                + seq_tag;
        }
    } else if (! m_Segment.empty()) {
        if (m_Segment.find ("DNA") == NPOS &&
            m_Segment.find ("RNA") == NPOS &&
            m_Segment.find ("segment") == NPOS &&
            m_Segment.find ("Segment") == NPOS) {
            result = m_Taxname + " segment " + m_Segment + seq_tag;
        } else {
            result = m_Taxname + " " + m_Segment + seq_tag;
        }
    } else if (! m_Chromosome.empty() ) {
        result = m_Taxname + " chromosome " + m_Chromosome + seq_tag;
    } else {
        result = m_Taxname + gen_tag;
    }

    result = NStr::Replace (result, "Plasmid", "plasmid");
    result = NStr::Replace (result, "Element", "element");
    if (! result.empty()) {
        result[0] = toupper ((unsigned char) result[0]);
    }

    return result;
}

// generate title for NM
static void x_FlyCG_PtoR (
    string& s
)

{
    // s =~ s/\b(CG\d*-)P([[:alpha:]])\b/$1R$2/g, more or less.
    SIZE_TYPE pos = 0, len = s.size();
    while ((pos = NStr::FindCase (s, "CG", pos)) != NPOS) {
        if (pos > 0  &&  !isspace((unsigned char)s[pos - 1]) ) {
            continue;
        }
        pos += 2;
        while (pos + 3 < len && isdigit((unsigned char)s[pos])) {
            ++pos;
        }
        if (s[pos] == '-'  &&  s[pos + 1] == 'P' &&
            isalpha((unsigned char)s[pos + 2]) &&
            (pos + 3 == len  ||  strchr(" ,;", s[pos + 3])) ) {
            s[pos + 1] = 'R';
        }
    }
}

string CDeflineGenerator::x_TitleFromNM (
    const CBioseq_Handle& bsh
)

{
    unsigned int         genes = 0, cdregions = 0, prots = 0;
    CConstRef<CSeq_feat> gene(0);
    CConstRef<CSeq_feat> cdregion(0);
    string result;

    // require taxname to be set
    if (m_Taxname.empty()) return result;

    CScope& scope = bsh.GetScope();

    SAnnotSelector sel;
    sel.SetFeatType(CSeqFeatData::e_Gene);
    sel.IncludeFeatType(CSeqFeatData::e_Cdregion);
    sel.IncludeFeatType(CSeqFeatData::e_Prot);
    sel.SetResolveTSE();

    FOR_SELECTED_SEQFEAT_ON_BIOSEQ_HANDLE (feat_it, bsh, sel) {
        const CSeq_feat& sft = feat_it->GetOriginalFeature();
        SWITCH_ON_FEATURE_CHOICE (sft) {
            case CSeqFeatData::e_Gene:
                ++genes;
                gene.Reset(&sft);
                break;
            case CSeqFeatData::e_Cdregion:
                ++cdregions;
                cdregion.Reset(&sft);
                break;
            case CSeqFeatData::e_Prot:
                ++prots;
                break;
            default:
                break;
        }
    }

    if (genes == 1 && cdregions == 1 && (! m_Taxname.empty())) {
        result = m_Taxname + " ";
        string cds_label;
        feature::GetLabel(*cdregion, &cds_label, feature::fFGL_Content, &scope);
        if (NStr::EqualNocase (m_Taxname, "Drosophila melanogaster")) {
            x_FlyCG_PtoR (cds_label);
        }
        result += NStr::Replace (cds_label, "isoform ", "transcript variant ");
        result += " (";
        feature::GetLabel(*gene, &result, feature::fFGL_Content, &scope);
        result += "), mRNA";
    }

    return result;
}

// generate title for NR
string CDeflineGenerator::x_TitleFromNR (
    const CBioseq_Handle& bsh
)

{
    string result;

    // require taxname to be set
    if (m_Taxname.empty()) return result;

    FOR_EACH_SEQFEAT_ON_BIOSEQ_HANDLE (feat_it, bsh, Gene) {
        const CSeq_feat& sft = feat_it->GetOriginalFeature();
        result = m_Taxname + " ";
        feature::GetLabel(sft, &result, feature::fFGL_Content);
        result += ", ";
        switch (m_MIBiomol) {
            case NCBI_BIOMOL(pre_RNA):
                result += "precursorRNA";
                break;
            case NCBI_BIOMOL(mRNA):
                result += "mRNA";
                break;
            case NCBI_BIOMOL(rRNA):
                result += "rRNA";
                break;
            case NCBI_BIOMOL(tRNA):
                result += "tRNA";
                break;
            case NCBI_BIOMOL(snRNA):
                result += "snRNA";
                break;
            case NCBI_BIOMOL(scRNA):
                result += "scRNA";
                break;
            case NCBI_BIOMOL(cRNA):
                result += "cRNA";
                break;
            case NCBI_BIOMOL(snoRNA):
                result += "snoRNA";
                break;
            case NCBI_BIOMOL(transcribed_RNA):
                result+="miscRNA";
                break;
            case NCBI_BIOMOL(ncRNA):
                result += "ncRNA";
                break;
            case NCBI_BIOMOL(tmRNA):
                result += "tmRNA";
                break;
            default:
                break;
        }

        // take first, then break to skip remainder
        break;
    }

    return result;
}

// generate title for Patent
string CDeflineGenerator::x_TitleFromPatent (void)

{
    string result;

    result = "Sequence " + NStr::IntToString(m_PatentSequence) +
             " from Patent " + m_PatentCountry +
             " " + m_PatentNumber;

    return result;
}

// generate title for PDB
string CDeflineGenerator::x_TitleFromPDB (void)

{
    string result;

    if (isprint ((unsigned char) m_PDBChain)) {
        result = string("Chain ") + (char) m_PDBChain + ", ";
    }
    result += m_PDBCompound;

    return result;
}

// generate title for protein
CConstRef<CSeq_feat> CDeflineGenerator::x_GetLongestProtein (
    const CBioseq_Handle& bsh
)

{
    TSeqPos               longest = 0;
    CProt_ref::EProcessed bestprocessed = CProt_ref::eProcessed_not_set;
    CProt_ref::EProcessed processed;
    CConstRef<CProt_ref>  prot;
    CConstRef<CSeq_feat>  prot_feat;
    TSeqPos               seq_len = UINT_MAX;

    CScope& scope = bsh.GetScope();

    if (bsh.IsSetInst ()) {
        if (bsh.IsSetInst_Length ()) {
            seq_len = bsh.GetInst_Length ();
        }
    }

    FOR_EACH_SEQFEAT_ON_BIOSEQ_HANDLE (feat_it, bsh, Prot) {
        const CSeq_feat& feat = feat_it->GetOriginalFeature();
        if (! feat.IsSetData ()) continue;
        const CSeqFeatData& sfdata = feat.GetData ();
        const CProt_ref& prp = sfdata.GetProt();
        processed = CProt_ref::eProcessed_not_set;
        if (prp.IsSetProcessed()) {
            processed = prp.GetProcessed();
        }
        if (! feat.IsSetLocation ()) continue;
        const CSeq_loc& loc = feat.GetLocation ();
        TSeqPos prot_length = GetLength (loc, &scope);
        if (prot_length > longest) {
            prot_feat = &feat;
            longest = prot_length;
            bestprocessed = processed;
        } else if (prot_length == longest) {
            // unprocessed 0 preferred over preprotein 1 preferred
            // over mat peptide 2
            if (processed < bestprocessed) {
                prot_feat = &feat;
                longest = prot_length;
                bestprocessed = processed;
            }
        }
    }

    if (longest == seq_len && prot_feat) {
        return prot_feat;
    }

    // confirm that this will automatically check features on
    // parts and segset in pathological segmented protein ???

    if (prot_feat) {
        return prot_feat;
    }

    CSeq_loc everywhere;
    everywhere.SetWhole().Assign(*bsh.GetSeqId());

    prot_feat = GetBestOverlappingFeat (everywhere, CSeqFeatData::e_Prot,
                                        eOverlap_Contained, scope);

    if (prot_feat) {
        return prot_feat;
    }

    return CConstRef<CSeq_feat> ();
}

CConstRef<CGene_ref> CDeflineGenerator::x_GetGeneRefViaCDS (
    const CMappedFeat& mapped_cds)

{
    CConstRef<CGene_ref> gene_ref;

    if (mapped_cds) {
        const CSeq_feat& cds_feat = mapped_cds.GetOriginalFeature();
        FOR_EACH_SEQFEATXREF_ON_FEATURE (xf_itr, cds_feat) {
            const CSeqFeatXref& sfx = **xf_itr;
            if (sfx.IsSetData()) {
                const CSeqFeatData& sfd = sfx.GetData();
                if (sfd.IsGene()) {
                    gene_ref = &sfd.GetGene();
                }
            }
        }

        if (gene_ref) {
            return gene_ref;
        }

        if (m_ConstructedFeatTree) {
            if (! m_InitializedFeatTree) {
                CFeat_CI iter (m_TopSEH);
                m_Feat_Tree.Reset (new CFeatTree (iter));
                m_InitializedFeatTree = true;
            }
        }
        if (m_Feat_Tree.Empty ()) {
            m_Feat_Tree.Reset (new CFeatTree);
        }
        if (! m_ConstructedFeatTree) {
            m_Feat_Tree->AddGenesForCds (mapped_cds);
        }

        CMappedFeat mapped_gene = GetBestGeneForCds (mapped_cds, m_Feat_Tree);
        if (mapped_gene) {
            const CSeq_feat& gene_feat = mapped_gene.GetOriginalFeature();
            gene_ref = &gene_feat.GetData().GetGene();
        }
    }

    return gene_ref;
}

static CConstRef<CBioSource> x_GetSourceFeatViaCDS  (
    const CBioseq_Handle& bsh
)

{
    CConstRef<CSeq_feat>   cds_feat;
    CConstRef<CSeq_loc>    cds_loc;
    CConstRef<CBioSource>  src_ref;

    CScope& scope = bsh.GetScope();

    cds_feat = GetCDSForProduct (bsh);

    if (cds_feat) {
        /*
        const CSeq_feat& feat = *cds_feat;
        */
        cds_loc = &cds_feat->GetLocation();
        if (cds_loc) {
            CConstRef<CSeq_feat> src_feat
                = GetOverlappingSource (*cds_loc, scope);
            if (src_feat) {
                const CSeq_feat& feat = *src_feat;
                if (feat.IsSetData()) {
                    const CSeqFeatData& sfd = feat.GetData();
                    if (sfd.IsBiosrc()) {
                        src_ref = &sfd.GetBiosrc();
                    }
                }
            }
        }
    }

    if (src_ref) {
        return src_ref;
    }

    return CConstRef<CBioSource> ();
}

bool CDeflineGenerator::x_CDShasLowQualityException (
    const CSeq_feat& sft
)

{
    if (! FEATURE_CHOICE_IS (sft, NCBI_SEQFEAT(Cdregion))) return false;
    if (! sft.IsSetExcept()) return false;
    if (! sft.GetExcept()) return false;
    if (! sft.IsSetExcept_text()) return false;

    const string& str = sft.GetExcept_text();
    int current_state = 0;
    FOR_EACH_CHAR_IN_STRING (str_itr, str) {
        const char ch = *str_itr;
        int next_state = m_Low_Quality_Fsa.GetNextState (current_state, ch);
        if (m_Low_Quality_Fsa.IsMatchFound (next_state)) {
            return true;
        }
        current_state = next_state;
    }


    return false;
}

/*
static const char* s_proteinOrganellePrefix [] = {
  "",
  "",
  "chloroplast",
  "chromoplast",
  "kinetoplast",
  "mitochondrion",
  "plastid",
  "macronuclear",
  "extrachromosomal",
  "plasmid",
  "",
  "",
  "cyanelle",
  "proviral",
  "virus",
  "nucleomorph",
  "apicoplast",
  "leucoplast",
  "protoplast",
  "endogenous virus",
  "hydrogenosome",
  "chromosome",
  "chromatophore"
};
*/

static const char* s_proteinOrganellePrefix [] = {
  "",
  "",
  "chloroplast",
  "",
  "",
  "mitochondrion",
  "",
  "",
  "",
  "",
  "",
  "",
  "",
  "",
  "",
  "",
  "",
  "",
  "",
  "",
  "",
  "",
  "",
};

string CDeflineGenerator::x_TitleFromProtein (
    const CBioseq_Handle& bsh
)

{
    CConstRef<CSeq_feat>  cds_feat;
    CConstRef<CProt_ref>  prot;
    CConstRef<CSeq_feat>  prot_feat;
    CConstRef<CGene_ref>  gene;
    CConstRef<CBioSource> src;
    string                locus_tag;
    bool                  partial = false;
    string                result;

    // gets longest protein on Bioseq, parts set, or seg set, even if not
    // full-length

    prot_feat = x_GetLongestProtein (bsh);

    if (prot_feat) {
        prot = &prot_feat->GetData().GetProt();
    }

    switch (m_MICompleteness) {
        case NCBI_COMPLETENESS(partial):
        case NCBI_COMPLETENESS(no_left):
        case NCBI_COMPLETENESS(no_right):
        case NCBI_COMPLETENESS(no_ends):
            partial = true;
            break;
        default:
            break;
    }

    const CMappedFeat& mapped_cds = GetMappedCDSForProduct (bsh);

    if (prot) {
        const CProt_ref& prp = *prot;
        string prefix = "";
        FOR_EACH_NAME_ON_PROT (prp_itr, prp) {
            const string& str = *prp_itr;
            result += prefix;
            result += str;
            if (! m_AllProtNames) {
                break;
            }
            prefix = "; ";
        }

        if (! result.empty()) {
            // strip trailing periods, commas, and spaces
            size_t pos = result.find_last_not_of (".,;~ ");
            if (pos != string::npos) {
                result.erase (pos + 1);
            }
            /*
            while (NStr::EndsWith (result, ".") ||
                   NStr::EndsWith (result, ",") ||
                   NStr::EndsWith (result, ";") ||
                   NStr::EndsWith (result, "~") ||
                   NStr::EndsWith (result, " ")) {
                result.erase (result.end() - 1);
            }
            */

            if (NStr::CompareNocase (result, "hypothetical protein") == 0) {
                gene = x_GetGeneRefViaCDS (mapped_cds);
                if (gene) {
                    const CGene_ref& grp = *gene;
                    if (grp.IsSetLocus_tag()) {
                        locus_tag = grp.GetLocus_tag();
                    }
                }
                if (! locus_tag.empty()) {
                    result += " " + locus_tag;
                }
            }
        }
        if (result.empty()) {
            if (prp.IsSetDesc()) {
                result = prp.GetDesc();
            }
        }
        if (result.empty()) {
            FOR_EACH_ACTIVITY_ON_PROT (act_itr, prp) {
                const string& str = *act_itr;
                result = str;
                break;
            }
        }
    }

    if (result.empty()) {
        gene = x_GetGeneRefViaCDS (mapped_cds);
        if (gene) {
            const CGene_ref& grp = *gene;
            if (grp.IsSetLocus()) {
                result = grp.GetLocus();
            }
            if (result.empty()) {
                FOR_EACH_SYNONYM_ON_GENE (syn_itr, grp) {
                    const string& str = *syn_itr;
                    result = str;
                    break;
                }
            }
            if (result.empty()) {
                if (grp.IsSetDesc()) {
                    result = grp.GetDesc();
                }
            }
        }
        if (! result.empty()) {
            result += " gene product";
        }
    }

    if (result.empty()) {
        result = "unnamed protein product";
    }

    if (mapped_cds) {
        const CSeq_feat& cds = mapped_cds.GetOriginalFeature();
        if (x_CDShasLowQualityException (cds)) {
          const string& low_qual = "LOW QUALITY PROTEIN: ";
          if (NStr::FindNoCase (result, low_qual, 0) == NPOS) {
              string tmp = result;
              result = low_qual + tmp;
          }
        }
    }

    // strip trailing periods, commas, and spaces
    size_t pos = result.find_last_not_of (".,;~ ");
    if (pos != string::npos) {
        result.erase (pos + 1);
    }
    /*
    while (NStr::EndsWith (result, ".") ||
               NStr::EndsWith (result, ",") ||
               NStr::EndsWith (result, ";") ||
               NStr::EndsWith (result, "~") ||
               NStr::EndsWith (result, " ")) {
        result.erase (result.end() - 1);
    }
    */

    if (partial /* && result.find(", partial") == NPOS */) {
        result += ", partial";
    }

    string taxname;
    taxname = m_Taxname;

    if (m_Genome >= NCBI_GENOME(chloroplast) && m_Genome <= NCBI_GENOME(chromatophore)) {
        CTempString organelle(s_proteinOrganellePrefix [m_Genome]);
        if (! organelle.empty() && ! taxname.empty() /* && taxname.find(organelle) == NPOS */) {
            result += " (";
            result += organelle;
            result += ")";
        }
    }

    // check for special taxname, go to overlapping source feature
    if (taxname.empty() ||
        (NStr::CompareNocase (taxname, "synthetic construct") != 0 &&
         NStr::CompareNocase (taxname, "artificial sequence") != 0 &&
         taxname.find ("vector") == NPOS &&
         taxname.find ("Vector") == NPOS)) {

        src = x_GetSourceFeatViaCDS (bsh);
        if (src) {
            const CBioSource& source = *src;
            if (source.IsSetTaxname()) {
                const string& str = source.GetTaxname();
                taxname = str;
            }
        }
    }

    if (! taxname.empty() /* && result.find(taxname) == NPOS */) {
        result += " [" + taxname + "]";
    }

    return result;
}

// generate title for segmented sequence
string CDeflineGenerator::x_TitleFromSegSeq  (
    const CBioseq_Handle& bsh
)

{
    string completeness = "complete";
    bool   cds_found    = false;
    string locus, product, result;

    CScope& scope = bsh.GetScope();

    if (m_Taxname.empty()) {
        m_Taxname = "Unknown";
    }

    // check C toolkit code to understand what is happening here ???

    CSeq_loc everywhere;
    everywhere.SetMix().Set() = bsh.GetInst_Ext().GetSeg();

    FOR_EACH_SEQFEAT_ON_SCOPE (it, scope, everywhere, Cdregion) {
        const CSeq_feat& cds = it->GetOriginalFeature();
        if (! cds.IsSetLocation ()) continue;
        const CSeq_loc& cds_loc = cds.GetLocation();

        cds_found = true;

        GetLabel (cds, &product, feature::fFGL_Content, &scope);

        if (cds.IsSetPartial()) {
            completeness = "partial";
        }

        FOR_EACH_SEQFEATXREF_ON_SEQFEAT (xr_itr, cds) {
            const CSeqFeatXref& sfx = **xr_itr;
            if (! FIELD_IS_SET (sfx, Data)) continue;
            const CSeqFeatData& sfd = GET_FIELD (sfx, Data);
            if (! FIELD_IS (sfd, Gene)) continue;
            const CGene_ref& gr = GET_FIELD (sfd, Gene);
            if (FIELD_IS_SET (gr, Locus)) {
                locus = GET_FIELD (gr, Locus);
            } else {
                FOR_EACH_SYNONYM_ON_GENEREF (syn_itr, gr) {
                    locus = *syn_itr;
                    // take first, then break to skip remainder
                    break;
                }
            }
        }

        if (locus.empty()) {
            CConstRef<CSeq_feat> gene_feat
                = GetBestOverlappingFeat(cds_loc,
                                         CSeqFeatData::eSubtype_gene,
                                         eOverlap_Contained,
                                         scope);
            if (gene_feat.NotEmpty()) {
                const CSeq_feat& gene = *gene_feat;
                GetLabel (gene, &locus, feature::fFGL_Content, &scope);
                /*
                if (gene_feat->GetData().GetGene().IsSetLocus()) {
                    locus = gene_feat->GetData().GetGene().GetLocus();
                } else if (gene_feat->GetData().GetGene().IsSetSyn()) {
                    locus = *gene_feat->GetData().GetGene().GetSyn().begin();
                }
                */
            }
        }

        // take first, then break to skip remainder
        break;
    }

    result = m_Taxname;

    if ( !cds_found) {
        if ( (! m_Strain.empty()) && (! x_EndsWithStrain (m_Taxname, m_Strain))) {
            result += " strain " + m_Strain;
        } else if (! m_Clone.empty() /* && m_Clone.find(" clone ") != NPOS */) {
            result += x_DescribeClones ();
        } else if (! m_Isolate.empty() ) {
            result += " isolate " + m_Isolate;
        }
    }
    if (! product.empty()) {
        result += " " + product;
    }
    if (! locus.empty()) {
        result += " (" + locus + ")";
    }
    if ((! product.empty()) || (! locus.empty())) {
        result += " gene, " + completeness + " cds";
    }
    return NStr::TruncateSpaces(result);
}

// generate title for TSA or non-master WGS
string CDeflineGenerator::x_TitleFromWGS (void)

{
    string chr, cln, mp, pls, mod, sfx;

    if (! m_Strain.empty()) {
        if (! x_EndsWithStrain (m_Taxname, m_Strain)) {
            mod = " strain " + m_Strain.substr (0, m_Strain.find(';'));
        }
    } else if (! m_Breed.empty()) {
        mod = " breed " + m_Breed.substr (0, m_Breed.find(';'));
    } else if (! m_Cultivar.empty()) {
        mod = " cultivar " + m_Cultivar.substr (0, m_Cultivar.find(';'));
    }
    if (! m_Chromosome.empty()) {
        chr = " chromosome " + m_Chromosome;
    }
    if (! m_Clone.empty()) {
        cln = x_DescribeClones ();
    }
    if (! m_Map.empty()) {
        mp = " map " + m_Map;
    }
    if (! m_Plasmid.empty()) {
        if (m_IsWGS) {
            pls = " plasmid " + m_Plasmid;
        }
    }
    if (! m_GeneralStr.empty()  &&  m_GeneralStr != m_Chromosome
        &&  (! m_IsWGS  ||  m_GeneralStr != m_Plasmid)) {
        sfx = " " + m_GeneralStr;
    }

    string title = NStr::TruncateSpaces
        (m_Taxname + mod + chr + cln + mp + pls + sfx);

    if (islower ((unsigned char) title[0])) {
        title [0] = toupper ((unsigned char) title [0]);
    }

    return title;
}

// generate TPA or TSA prefix
string CDeflineGenerator::x_SetPrefix (
    const string& title
)

{
    string prefix;

    if (m_IsUnverified) {
        if (title.find ("UNVERIFIED") == NPOS) {
            prefix = "UNVERIFIED: ";
        }
    } else if (m_IsTSA) {
        prefix = "TSA: ";
    } else if (m_ThirdParty) {
        if (m_TPAExp) {
            prefix = "TPA_exp: ";
        } else if (m_TPAInf) {
            prefix = "TPA_inf: ";
        } else if (m_TPAReasm) {
            prefix = "TPA_reasm: ";
        } else {
            prefix = "TPA: ";
        }
    }

    return prefix;
}

// generate suffix if not already present
string CDeflineGenerator::x_SetSuffix (
    const CBioseq_Handle& bsh,
    const string& title
)

{
    string suffix;

    switch (m_MITech) {
        case NCBI_TECH(htgs_0):
            if (title.find ("LOW-PASS") == NPOS) {
                suffix = ", LOW-PASS SEQUENCE SAMPLING";
            }
            break;
        case NCBI_TECH(htgs_1):
        case NCBI_TECH(htgs_2):
        {
            if (m_HTGSDraft && title.find ("WORKING DRAFT") == NPOS) {
                suffix = ", WORKING DRAFT SEQUENCE";
            } else if ( !m_HTGSDraft && !m_HTGSCancelled &&
                       title.find ("SEQUENCING IN") == NPOS) {
                suffix = ", *** SEQUENCING IN PROGRESS ***";
            }

            string un;
            if (m_MITech == NCBI_TECH(htgs_1)) {
                un = "un";
            }
            if (m_IsDelta) {
                unsigned int pieces = 1;
                for (CSeqMap_CI it (bsh, CSeqMap::fFindGap); it; ++it) {
                    ++pieces;
                }
                if (pieces == 1) {
                    // suffix += (", 1 " + un + "ordered piece");
                } else {
                    suffix += (", " + NStr::IntToString (pieces)
                               + " " + un + "ordered pieces");
                }
            } else {
                // suffix += ", in " + un + "ordered pieces";
            }
            break;
        }
        case NCBI_TECH(htgs_3):
            if (title.find ("complete sequence") == NPOS) {
                suffix = ", complete sequence";
            }
            break;
        case NCBI_TECH(est):
            if (title.find ("mRNA sequence") == NPOS) {
                suffix = ", mRNA sequence";
            }
            break;
        case NCBI_TECH(sts):
            if (title.find ("sequence tagged site") == NPOS) {
                suffix = ", sequence tagged site";
            }
            break;
        case NCBI_TECH(survey):
            if (title.find ("genomic survey sequence") == NPOS) {
                suffix = ", genomic survey sequence";
            }
            break;
        case NCBI_TECH(wgs):
            if (m_WGSMaster) {
                if (title.find ("whole genome shotgun sequencing project")
                    == NPOS){
                    suffix = ", whole genome shotgun sequencing project";
                }            
            } else if (title.find ("whole genome shotgun sequence") == NPOS) {
                string orgnl = x_OrganelleName (m_Genome, false, false, true);
                if (! orgnl.empty()  &&  title.find(orgnl) == NPOS) {
                    suffix = " " + orgnl;
                }
                suffix += ", whole genome shotgun sequence";
            }
            break;
        case NCBI_TECH(tsa):
            if (m_MIBiomol == NCBI_BIOMOL(mRNA)) {
                if (m_TSAMaster) {
                    if (title.find ("whole genome shotgun sequencing project")
                        == NPOS){
                        suffix = ", whole genome shotgun sequencing project";
                    }            
                } else {
                    if (title.find ("mRNA sequence") == NPOS){
                        suffix = ", mRNA sequence";
                    }            
                }
            }
            break;
        default:
            break;
    }

    return suffix;
}

// main method
string CDeflineGenerator::GenerateDefline (
    const CBioseq_Handle& bsh,
    TUserFlags flags
)

{
    string prefix, title, suffix;

    // set flags from record components
    x_SetFlags (bsh, flags);

    if (! m_Reconstruct) {
        // look for existing instantiated title
        int level = 0;
        FOR_EACH_SEQDESC_ON_BIOSEQ_HANDLE (desc_it, bsh, Title) {
            const string& str = desc_it->GetTitle();
            // for non-PDB proteins, title must be packaged on Bioseq
            if (m_IsNA || m_IsPDB || level == 0) {
                title = str;

                // strip trailing periods, commas, semicolons, and spaces
                size_t pos = title.find_last_not_of (".,;~ ");
                if (pos != string::npos) {
                    title.erase (pos + 1);
                }
                /*
                while (NStr::EndsWith (title, ".") ||
                           NStr::EndsWith (title, ",") ||
                           NStr::EndsWith (title, ";") ||
                           NStr::EndsWith (title, "~") ||
                           NStr::EndsWith (title, " ")) {
                    title.erase (title.end() - 1);
                }
                */
            }

            // take first, then break to skip remainder
            break;
        }
    }

    // use appropriate algorithm if title needs to be generated
    if (title.empty()) {
        // PDB and patent records do not normally need source data
        if (m_IsPDB) {
            title = x_TitleFromPDB ();
        } else if (m_IsPatent) {
            title = x_TitleFromPatent ();
        }

        if (title.empty()) {
            // set fields from source information
            x_SetBioSrc (bsh);

            // several record types have specific methods
            if (m_IsNC) {
                title = x_TitleFromNC ();
            } else if (m_IsNM) {
                title = x_TitleFromNM (bsh);
            } else if (m_IsNR) {
                title = x_TitleFromNR (bsh);
            } else if (m_IsAA) {
                title = x_TitleFromProtein (bsh);
            } else if (m_IsSeg && (! m_IsEST_STS_GSS)) {
                title = x_TitleFromSegSeq (bsh);
            } else if (m_IsTSA || (m_IsWGS && (! m_WGSMaster))) {
                title = x_TitleFromWGS ();
            }
        }

        if (title.empty()) {
            // default title using source fields
            title = x_TitleFromBioSrc ();
        }

        if (title.empty()) {
            // last resort title created here
            //title = "No definition line found";
        }
    }

    // remove TPA or TSA prefix, will rely on other data in record to set
    if (NStr::StartsWith (title, "TPA:", NStr::eNocase)) {
        title.erase (0, 4);
    } else if (NStr::StartsWith (title, "TPA_exp:", NStr::eNocase)) {
        title.erase (0, 8);
    } else if (NStr::StartsWith (title, "TPA_inf:", NStr::eNocase)) {
        title.erase (0, 8);
    } else if (NStr::StartsWith (title, "TPA_reasm:", NStr::eNocase)) {
        title.erase (0, 10);
    } else if (NStr::StartsWith (title, "TSA:", NStr::eNocase)) {
        title.erase (0, 4);
    } else if (NStr::StartsWith (title, "UNVERIFIED:", NStr::eNocase)) {
        title.erase (0, 11);
    }

    // strip leading spaces remaining after removal of old TPA or TSA prefixes
    while (NStr::StartsWith (title, " ")) {
        title.erase (0, 1);
    }

    // strip trailing commas, semicolons, and spaces (period may be an sp.
    // species)
    size_t pos = title.find_last_not_of (",;~ ");
    if (pos != string::npos) {
        title.erase (pos + 1);
    }
    /*
    while (NStr::EndsWith (title, ",") ||
               NStr::EndsWith (title, ";") ||
               NStr::EndsWith (title, "~") ||
               NStr::EndsWith (title, " ")) {
        title.erase (title.end() - 1);
    }
    */

    // calculate prefix
    prefix = x_SetPrefix (title);

    // calculate suffix
    suffix = x_SetSuffix (bsh, title);

    return prefix + title + suffix;
}

string CDeflineGenerator::GenerateDefline (
    const CBioseq& bioseq,
    CScope& scope,
    TUserFlags flags
)

{
    CBioseq_Handle bsh = scope.AddBioseq(bioseq,
                                         CScope::kPriority_Default,
                                         CScope::eExist_Get);
    return GenerateDefline(bsh, flags);
}

