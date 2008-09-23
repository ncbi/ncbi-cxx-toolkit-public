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
* Author:
*
* File Description:
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <objmgr/util/create_defline.hpp>

#include <serial/iterator.hpp>

#include <objmgr/feat_ci.hpp>
#include <objmgr/seq_map_ci.hpp>

#include <objmgr/util/feature.hpp>
#include <objmgr/util/sequence.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(sequence);

// constructor
CDeflineGenerator::CDeflineGenerator (void)
    : m_HasBiosrcFeats(eSFS_Unknown)

{
}

// destructor
CDeflineGenerator::~CDeflineGenerator (void)

{
}

// set instance variables from Seq-inst, Seq-ids, MolInfo, etc., but not
//  BioSource
void CDeflineGenerator::x_SetFlags (
    const CBioseq& bioseq,
    CScope& scope,
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

    m_UseBiosrc = false;

    m_HTGSCancelled = false;
    m_HTGSDraft = false;
    m_HTGSPooled = false;
    m_TPAExp = false;
    m_TPAInf = false;

    m_PDBCompound.clear();

    m_Taxname.clear();
    m_Genome = NCBI_GENOME(unknown);

    m_Chromosome.clear();
    m_Clone.clear();
    m_Map.clear();
    m_Plasmid.clear();
    m_Segment.clear();

    m_Isolate.clear();
    m_Strain.clear();

    // now start setting member variables
    m_IsNA = bioseq.IsNa();
    m_IsAA = bioseq.IsAa();

    if (bioseq.IsSetInst()) {
        const CSeq_inst& inst = bioseq.GetInst();
        if (inst.IsSetRepr()) {
            TSEQ_REPR repr = inst.GetRepr();
            m_IsSeg = (repr == CSeq_inst::eRepr_seg);
            m_IsDelta = (repr == CSeq_inst::eRepr_delta);
        }
    }

    // process Seq-ids
    FOR_EACH_SEQID_ON_BIOSEQ (sid_itr, bioseq) {
        const CSeq_id& sid = **sid_itr;
        TSEQID_CHOICE chs = sid.Which();
         switch (chs) {
            case NCBI_SEQID(Other):
            case NCBI_SEQID(Genbank):
            case NCBI_SEQID(Embl):
            case NCBI_SEQID(Ddbj):
            {
                const CTextseq_id& tsid = *sid.GetTextseq_Id ();
                if (tsid.IsSetAccession()) {
                    const string& acc = tsid.GetAccession ();
                    TACCN_CHOICE type = CSeq_id::IdentifyAccession (acc);
                    if ((type & NCBI_ACCN(division_mask)) == NCBI_ACCN(wgs) &&
                            NStr::EndsWith(acc, "000000")) {
                        m_WGSMaster = true;
                    } else if (type == NCBI_ACCN(refseq_chromosome)) {
                        m_IsNC = true;
                    } else if (type == NCBI_ACCN(refseq_mrna)) {
                        m_IsNM = true;
                    } else if (type == NCBI_ACCN(refseq_ncrna)) {
                        m_IsNR = true;
                    }
                }
                break;
            }
            case NCBI_SEQID(General):
            {
                const CDbtag& gen_id = sid.GetGeneral ();
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
                const CPDB_seq_id& pdb_id = sid.GetPdb ();
                if (pdb_id.IsSetChain()) {
                    m_PDBChain = pdb_id.GetChain();
                }
                break;
            }
            case NCBI_SEQID(Patent):
            {
                m_IsPatent = true;
                const CPatent_seq_id& pat_id = sid.GetPatent();
                if (pat_id.IsSetSeqid()) {
                    m_PatentSequence = pat_id.GetSeqid();
                }
                if (pat_id.IsSetCit()) {
                    const CId_pat& cit = pat_id.GetCit();
                    m_PatentCountry = cit.GetCountry();
                    m_PatentNumber = cit.GetId().GetNumber();
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
    IF_EXISTS_CLOSEST_MOLINFO (mi_ref, bioseq, NULL) {
        const CMolInfo& molinf = (*mi_ref).GetMolinfo();
        m_MIBiomol = molinf.GetBiomol();
        m_MITech = molinf.GetTech();
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
    }

    if (m_HTGTech || m_ThirdParty) {

        // process keywords
        const list <string> *keywords = NULL;

        IF_EXISTS_CLOSEST_GENBANKBLOCK (gb_ref, bioseq, NULL) {
            const CGB_block& gbk = (*gb_ref).GetGenbank();
            if (gbk.IsSetKeywords()) {
                keywords = &gbk.GetKeywords();
            }
        }
        if (keywords == NULL) {
            IF_EXISTS_CLOSEST_EMBLBLOCK (eb_ref, bioseq, NULL) {
                const CEMBL_block& ebk = (*eb_ref).GetEmbl();
                if (ebk.IsSetKeywords()) {
                    keywords = &ebk.GetKeywords();
                }
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
                }
            }
        }
    }

    if (m_IsPDB) {

        // process PDB block
        IF_EXISTS_CLOSEST_PDBBLOCK (pb_ref, bioseq, NULL) {
            const CPDB_block& pbk = (*pb_ref).GetPdb();
            FOR_EACH_COMPOUND_ON_PDBBLOCK (cp_itr, pbk) {
                const string& str = *cp_itr;
                if (m_PDBCompound.empty()) {
                    m_PDBCompound = str;
                    // take first, then break to skip remainder
                    break;
                }
            }
        }
    }
}

// set instance variables from BioSource
void CDeflineGenerator::x_SetBioSrc (
    const CBioseq& bioseq,
    CScope& scope
)

{
    IF_EXISTS_CLOSEST_BIOSOURCE (bs_ref, bioseq, NULL) {
        const CBioSource& source = (*bs_ref).GetSource();

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
            if (sbs.IsSetSubtype()) {
                TSUBSRC_SUBTYPE sst = sbs.GetSubtype();
                if (sbs.IsSetName()) {
                    const string& str = sbs.GetName();
                    switch (sst) {
                        case NCBI_SUBSRC(chromosome):
                            m_Chromosome = str;
                            break;
                        case NCBI_SUBSRC(clone):
                            m_Clone = str;
                            break;
                        case NCBI_SUBSRC(map):
                            m_Map = str;
                            break;
                        case NCBI_SUBSRC(plasmid_name):
                            m_Plasmid = str;
                            break;
                        case NCBI_SUBSRC(segment):
                            m_Segment = str;
                            break;
                        default:
                            break;
                    }
                }
            }
        }

        // process OrgMod
        FOR_EACH_ORGMOD_ON_BIOSOURCE (omd_itr, source) {
            const COrgMod& omd = **omd_itr;
            if (omd.IsSetSubtype()) {
                TORGMOD_SUBTYPE omt = omd.GetSubtype();
                if (omd.IsSetSubname()) {
                    const string& str = omd.GetSubname();
                    switch (omt) {
                        case NCBI_ORGMOD(strain):
                            if (m_Strain.empty()) {
                                m_Strain = str;
                            }
                            break;
                        case NCBI_ORGMOD(isolate):
                            if (m_Isolate.empty()) {
                                m_Isolate = str;
                            }
                            break;
                        default:
                            break;
                    }
                }
            }
        }
    }
}

// generate title from BioSource fields
string CDeflineGenerator::x_DescribeClones (void)
{
    SIZE_TYPE count = 1;
    for (SIZE_TYPE pos = m_Clone.find(';'); pos != NPOS;
         pos = m_Clone.find(';', pos + 1)) {
        ++count;
    }
    if (m_HTGSUnfinished && m_HTGSPooled) {
        return ", pooled multiple clones";
    } else if (count > 3) {
        return ", " + NStr::IntToString(count) + " clones";
    } else {
        return " clone " + m_Clone;
    }
}

bool CDeflineGenerator::x_EndsWithStrain (void)
{
    // return NStr::EndsWith(m_Taxname, m_Strain, NStr::eNocase);
    if (m_Strain.size() >= m_Taxname.size()) {
        return false;
    }
    SIZE_TYPE pos = m_Taxname.find(' ');
    if (pos == NPOS) {
        return false;
    }
    pos = m_Taxname.find(' ', pos + 1);
    if (pos == NPOS) {
        return false;
    }
    // XXX - the C Toolkit looks for the first occurrence, which could
    // (at least in theory) lead to false negatives.
    pos = NStr::FindNoCase (m_Taxname, m_Strain, pos + 1, NPOS, NStr::eLast);
    if (pos == m_Taxname.size() - m_Strain.size()) {
        // check for space to avoid fortuitous match to end of taxname
        char ch = m_Taxname[pos - 1];
        if (ispunct (ch) || isspace (ch)) {
            return true;
        }
    } else if (pos == m_Taxname.size() - m_Strain.size() - 1
               &&  m_Taxname[pos - 1] == '\''
               &&  m_Taxname[m_Taxname.size() - 1] == '\'') {
        return true;
    }
    return false;
}

string CDeflineGenerator::x_TitleFromBioSrc (void)

{
    string chr, cln, mp, pls, stn, sfx;

    if (! m_Strain.empty()) {
        if (! x_EndsWithStrain ()) {
            stn = " strain " + m_Strain.substr (0, m_Strain.find(';'));
        }
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

    string title = NStr::TruncateSpaces
        (m_Taxname + stn + chr + cln + mp + pls + sfx);

    if (islower ((unsigned char) title[0])) {
        title [0] = toupper ((unsigned char) title [0]);
    }

    return title;
}

// generate title for NC
string CDeflineGenerator::x_OrganelleName (
    bool has_plasmid,
    bool virus_or_phage,
    bool wgs_suffix
)

{
    string result;

    switch (m_Genome) {
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
            if (! wgs_suffix) {
                result = "macronuclear";
            }
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

    orgnl = x_OrganelleName (has_plasmid, virus_or_phage, false);

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
void CDeflineGenerator::x_FlyCG_PtoR (
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
    const CBioseq& bioseq,
    CScope& scope
)

{
    unsigned int         genes = 0, cdregions = 0, prots = 0;
    CConstRef<CSeq_feat> gene(0);
    CConstRef<CSeq_feat> cdregion(0);
    string result;

    // require taxname to be set
    if (m_Taxname.empty()) return result;

    // !!! NOTE CALL TO OBJECT MANAGER !!!
    CBioseq_Handle hnd = scope.GetBioseqHandle (bioseq);

    for (CFeat_CI it(hnd); it; ++it) {
        switch (it->GetData().Which()) {
            case CSeqFeatData::e_Gene:
                ++genes;
                gene.Reset(&it->GetMappedFeature());
                break;
            case CSeqFeatData::e_Cdregion:
                ++cdregions;
                cdregion.Reset(&it->GetMappedFeature());
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
        feature::GetLabel (*cdregion, &cds_label, feature::eContent, &scope);
        if (NStr::EqualNocase (m_Taxname, "Drosophila melanogaster")) {
            x_FlyCG_PtoR (cds_label);
        }
        result += NStr::Replace (cds_label, "isoform ", "transcript variant ");
        result += " (";
        feature::GetLabel (*gene, &result, feature::eContent, &scope);
        result += "), mRNA";
    }

    return result;
}

// generate title for NR
string CDeflineGenerator::x_TitleFromNR (
    const CBioseq& bioseq,
    CScope& scope
)

{
    string result;

    // require taxname to be set
    if (m_Taxname.empty()) return result;

    // !!! NOTE CALL TO OBJECT MANAGER !!!
    CBioseq_Handle hnd = scope.GetBioseqHandle (bioseq);

    for (CTypeConstIterator<CSeq_feat> it(
          *hnd.GetTopLevelEntry().GetCompleteSeq_entry()); it; ++it) {
        if (it->GetData().IsGene()) {
            result = m_Taxname + " ";
            feature::GetLabel(*it, &result, feature::eContent);
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
            break;
        }
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
    const CBioseq& bioseq,
    CScope& scope
)

{
    bool                  go_on = true;
    TSeqPos               longest = 0;
    CProt_ref::EProcessed bestprocessed = CProt_ref::eProcessed_not_set;
    CProt_ref::EProcessed processed;
    CConstRef<CProt_ref>  prot;
    CConstRef<CSeq_feat>  prot_feat;
    CSeq_entry*           se;
    TSeqPos               seq_len = UINT_MAX;

    if (bioseq.IsSetInst ()) {
        const CSeq_inst& inst = bioseq.GetInst ();
        if (inst.IsSetLength ()) {
            seq_len = inst.GetLength ();
        }
    }
    
    se = bioseq.GetParentEntry();

    while (se && go_on) {
        const CSeq_entry& entry = *se;
        FOR_EACH_ANNOT_ON_SEQENTRY (sa_itr, entry) {
            const CSeq_annot& annot = **sa_itr;
            FOR_EACH_FEATURE_ON_ANNOT (sf_itr, annot) {
                const CSeq_feat& feat = **sf_itr;
                if (! feat.IsSetData ()) continue;
                const CSeqFeatData& sfdata = feat.GetData ();
                if (! sfdata.IsProt ()) continue;
                const CProt_ref& prp = sfdata.GetProt();
                processed = CProt_ref::eProcessed_not_set;
                if (prp.IsSetProcessed()) {
                    processed = prp.GetProcessed();
                }
                if (! feat.IsSetLocation ()) continue;
                const CSeq_loc& loc = feat.GetLocation ();
                TSeqPos prot_length = GetLength (loc, &scope);
                if (prot_length > longest) {
                    prot_feat = *sf_itr;
                    longest = prot_length;
                    bestprocessed = processed;
                } else if (prot_length == longest) {
                    // unprocessed 0 preferred over preprotein 1 preferred
                    // over mat peptide 2
                    if (processed < bestprocessed) {
                        prot_feat = *sf_itr;
                        longest = prot_length;
                        bestprocessed = processed;
                    }
                }
            }
        }

        if (longest == seq_len && prot_feat) return prot_feat;

        se = se->GetParentEntry();

        go_on = false;
        if (se) {
            if (se->IsSet ()) {
                const CBioseq_set& bss = se->GetSet ();
                if (bss.IsSetClass ()) {
                    CBioseq_set::EClass bss_class = bss.GetClass ();
                    if (bss_class == CBioseq_set::eClass_parts ||
                        bss_class == CBioseq_set::eClass_segset) {
                        go_on = true;
                    }
                }
            }
        }
    }

    if (prot_feat) {
        return prot_feat;
    }

    // !!! NOTE CALL TO OBJECT MANAGER !!!
    const CBioseq_Handle& hnd = scope.GetBioseqHandle (bioseq);

    CSeq_loc everywhere;
    everywhere.SetWhole().Assign(*hnd.GetSeqId());

    prot_feat = GetBestOverlappingFeat (everywhere, CSeqFeatData::e_Prot,
                                        eOverlap_Contained, scope);

    if (prot_feat) {
        return prot_feat;
    }

    return CConstRef<CSeq_feat> ();
}

CConstRef<CGene_ref> CDeflineGenerator::x_GetGeneRefViaCDS (
    const CBioseq& bioseq,
    CScope& scope
)

{
    CConstRef<CSeq_feat> cds_feat;
    CConstRef<CSeq_loc>  cds_loc;
    CConstRef<CSeq_feat> gene_feat;
    CConstRef<CGene_ref> gene_ref;

    // !!! NOTE CALL TO OBJECT MANAGER !!!
    const CBioseq_Handle& hnd = scope.GetBioseqHandle (bioseq);

    cds_feat = GetCDSForProduct (hnd);

    if (cds_feat) {
        const CSeq_feat& feat = (*cds_feat);
        FOR_EACH_SEQFEATXREF_ON_FEATURE (xf_itr, feat) {
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

        cds_loc = &cds_feat->GetLocation();
        if (cds_loc) {
            gene_feat = GetOverlappingGene (*cds_loc, scope);
            if (gene_feat) {
                gene_ref = &gene_feat->GetData().GetGene();
            }
        }
    }

    if (gene_ref) {
        return gene_ref;
    }

    return CConstRef<CGene_ref> ();
}

bool CDeflineGenerator::x_HasSourceFeats (
    const CBioseq& bioseq
)

{
    if (m_HasBiosrcFeats == eSFS_Unknown) {
        CSeq_entry* se;
        CSeq_entry* top;

        se = bioseq.GetParentEntry();
        top = se;

        while (se) {
            top = se;
            se = se->GetParentEntry();
        }

        if (top) {
            m_HasBiosrcFeats = eSFS_Absent;
            /*
            VISIT_ALL_FEATURES_WITHIN_SEQENTRY (ft_itr, *top) {
                const CSeq_feat& feat = *ft_itr;
                if (feat.IsSetData()) {
                    const CSeqFeatData& sfd = feat.GetData();
                    if (sfd.IsBiosrc()) {
                        m_HasBiosrcFeats = eSFS_Present;
                        return true;
                    }
                }
            }
            */
            VISIT_ALL_SEQENTRYS_WITHIN_SEQENTRY (se_itr, *top) {
                const CSeq_entry& entry = *se_itr;
                FOR_EACH_ANNOT_ON_SEQENTRY (an_itr, entry) {
                    const CSeq_annot& annot = **an_itr;
                    FOR_EACH_FEATURE_ON_ANNOT (ft_itr, annot) {
                        const CSeq_feat& feat = **ft_itr;
                        if (feat.IsSetData()) {
                            const CSeqFeatData& sfd = feat.GetData();
                            if (sfd.IsBiosrc()) {
                                m_HasBiosrcFeats = eSFS_Present;
                                return true;
                            }
                        }
                    }
                }
            }
        }
    }

    if (m_HasBiosrcFeats == eSFS_Present) return true;

    return false;
}

CConstRef<CBioSource> CDeflineGenerator::x_GetSourceFeatViaCDS  (
    const CBioseq& bioseq,
    CScope& scope
)

{
    CConstRef<CSeq_feat>   cds_feat;
    CConstRef<CSeq_loc>    cds_loc;
    CConstRef<CBioSource>  src_ref;

    // !!! NOTE CALL TO OBJECT MANAGER !!!
    const CBioseq_Handle& hnd = scope.GetBioseqHandle (bioseq);

    cds_feat = GetCDSForProduct (hnd);

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


string CDeflineGenerator::x_TitleFromProtein (
    const CBioseq& bioseq,
    CScope& scope
)

{
    CConstRef<CProt_ref>  prot;
    CConstRef<CSeq_feat>  prot_feat;
    CConstRef<CGene_ref>  gene;
    CConstRef<CBioSource> src;
    string                locus_tag;
    string                result;

    // gets longest protein on Bioseq, parts set, or seg set, even if not
    // full-length
    prot_feat = x_GetLongestProtein (bioseq, scope);

    if (prot_feat) {
        prot = &prot_feat->GetData().GetProt();
    }

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
            while (NStr::EndsWith (result, ".") ||
                   NStr::EndsWith (result, ",") ||
                   NStr::EndsWith (result, ";") ||
                   NStr::EndsWith (result, "~") ||
                   NStr::EndsWith (result, " ")) {
                result.erase (result.end() - 1);
            }

            if (NStr::CompareNocase (result, "hypothetical protein") == 0) {
                gene = x_GetGeneRefViaCDS (bioseq, scope);
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
        gene = x_GetGeneRefViaCDS (bioseq, scope);
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

    // strip trailing periods, commas, and spaces
    while (NStr::EndsWith (result, ".") ||
               NStr::EndsWith (result, ",") ||
               NStr::EndsWith (result, ";") ||
               NStr::EndsWith (result, "~") ||
               NStr::EndsWith (result, " ")) {
        result.erase (result.end() - 1);
    }

    string taxname;
    taxname = m_Taxname;

    // check for special taxname, go to overlapping source feature
    if (taxname.empty() ||
        (NStr::CompareNocase (taxname, "synthetic construct") != 0 &&
         NStr::CompareNocase (taxname, "artificial sequence") != 0 &&
         taxname.find ("vector") == NPOS &&
         taxname.find ("Vector") == NPOS)) {

        if (x_HasSourceFeats(bioseq)) {
            src = x_GetSourceFeatViaCDS (bioseq, scope);
            if (src) {
                const CBioSource& source = *src;
                if (source.IsSetTaxname()) {
                    const string& str = source.GetTaxname();
                    taxname = str;
                }
            }
        }
    }

    if (! taxname.empty() && result.find(taxname) == NPOS) {
        result += " [" + taxname + "]";
    }

    return result;
}

// generate title for segmented sequence
string CDeflineGenerator::x_TitleFromSegSeq  (
    const CBioseq& bioseq,
    CScope& scope
)

{
    string completeness = "complete";
    bool   cds_found    = false;
    string locus, product, result;

    // !!! NOTE CALL TO OBJECT MANAGER !!!
    const CBioseq_Handle& hnd = scope.GetBioseqHandle (bioseq);

    if (m_Taxname.empty()) {
        m_Taxname = "Unknown";
    }

    CSeq_loc everywhere;
    everywhere.SetMix().Set() = hnd.GetInst_Ext().GetSeg();

    CFeat_CI it(scope, everywhere, CSeqFeatData::e_Cdregion);
    for (; it; ++it) {
        cds_found = true;
        if ( !it->IsSetProduct() ) {
            continue;
        }
        const CSeq_loc& product_loc = it->GetProduct();

        if (it->IsSetPartial()) {
            completeness = "partial";
        }

        CConstRef<CSeq_feat> prot_feat
            = GetBestOverlappingFeat(product_loc, CSeqFeatData::e_Prot,
                                     eOverlap_Interval, scope);
        if (product.empty()  &&  prot_feat.NotEmpty() &&
            prot_feat->GetData().GetProt().IsSetName()) {
            product = *prot_feat->GetData().GetProt().GetName().begin();
        }

        CConstRef<CSeq_feat> gene_feat
            = GetOverlappingGene(it->GetLocation(), scope);
        if (locus.empty()  &&  gene_feat.NotEmpty()) {
            if (gene_feat->GetData().GetGene().IsSetLocus()) {
                locus = gene_feat->GetData().GetGene().GetLocus();
            } else if (gene_feat->GetData().GetGene().IsSetSyn()) {
                locus = *gene_feat->GetData().GetGene().GetSyn().begin();
            }
        }

        break;
    }

    result = m_Taxname;

    if ( !cds_found) {
        if ( (! m_Strain.empty()) && (! x_EndsWithStrain ())) {
            result += " strain " + m_Strain;
        } else if (! m_Clone.empty() && m_Clone.find(" clone ") != NPOS) {
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
    string chr, cln, mp, pls, stn, sfx;

    if (! m_Strain.empty()) {
        if (! x_EndsWithStrain ()) {
            stn = " strain " + m_Strain.substr (0, m_Strain.find(';'));
        }
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
    if (! m_GeneralStr.empty()) {
        sfx = " " + m_GeneralStr;
    }

    string title = NStr::TruncateSpaces
        (m_Taxname + stn + chr + cln + mp + pls + sfx);

    if (islower ((unsigned char) title[0])) {
        title [0] = toupper ((unsigned char) title [0]);
    }

    return title;
}

// generate TPA or TSA prefix
string CDeflineGenerator::x_SetPrefix (void)

{
    string prefix;

    if (m_IsTSA) {
        prefix = "TSA: ";
    } else if (m_ThirdParty) {
        if (m_TPAExp) {
            prefix = "TPA_exp: ";
        } else if (m_TPAInf) {
            prefix = "TPA_inf: ";
        } else {
            prefix = "TPA: ";
        }
    }

    return prefix;
}

// generate suffix if not already present
string CDeflineGenerator::x_SetSuffix (
    const CBioseq& bioseq,
    CScope& scope,
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
                // !!! NOTE CALL TO OBJECT MANAGER !!!
                const CBioseq_Handle& hnd = scope.GetBioseqHandle (bioseq);
                for (CSeqMap_CI it (hnd, CSeqMap::fFindGap); it; ++it) {
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
                string orgnl = x_OrganelleName (false, false, true);
                if (! orgnl.empty()) {
                    suffix = " " + orgnl;
                }
                suffix += ", whole genome shotgun sequence";
            }
            break;
        default:
            break;
    }

    return suffix;
}

// main method
string CDeflineGenerator::GenerateDefline (
    const CBioseq& bioseq,
    CScope& scope,
    TUserFlags flags
)

{
    string prefix, title, suffix;

    // set flags from record components
    x_SetFlags (bioseq, scope, flags);

    if (! m_Reconstruct) {
        // look for existing instantiated title
        int level = 0;
        IF_EXISTS_CLOSEST_TITLE (ttl_ref, bioseq, &level) {
            const string& str = (*ttl_ref).GetTitle();
            // for non-PDB proteins, title must be packaged on Bioseq
            if (m_IsNA || m_IsPDB || level == 0) {
                title = str;

                // strip trailing periods, commas, semicolons, and spaces
                while (NStr::EndsWith (title, ".") ||
                           NStr::EndsWith (title, ",") ||
                           NStr::EndsWith (title, ";") ||
                           NStr::EndsWith (title, "~") ||
                           NStr::EndsWith (title, " ")) {
                    title.erase (title.end() - 1);
                }
            }
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
            x_SetBioSrc (bioseq, scope);

            // several record types have specific methods
            if (m_IsNC) {
                title = x_TitleFromNC ();
            } else if (m_IsNM) {
                title = x_TitleFromNM (bioseq, scope);
            } else if (m_IsNR) {
                title = x_TitleFromNR (bioseq, scope);
            } else if (m_IsAA) {
                title = x_TitleFromProtein (bioseq, scope);
            } else if (m_IsSeg && (! m_IsEST_STS_GSS)) {
                title = x_TitleFromSegSeq (bioseq, scope);
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
            title = "No definition line found";
        }
    }

    // remove TPA or TSA prefix, will rely on other data in record to set
    if (NStr::StartsWith (title, "TPA:", NStr::eNocase)) {
        title.erase (0, 4);
    } else if (NStr::StartsWith (title, "TPA_exp:", NStr::eNocase)) {
        title.erase (0, 8);
    } else if (NStr::StartsWith (title, "TPA_inf:", NStr::eNocase)) {
        title.erase (0, 8);
    } else if (NStr::StartsWith (title, "TSA:", NStr::eNocase)) {
        title.erase (0, 4);
    }

    // strip leading spaces remaining after removal of old TPA or TSA prefixes
    while (NStr::StartsWith (title, " ")) {
        title.erase (0, 1);
    }

    // strip trailing commas, semicolons, and spaces (period may be an sp.
    // species)
    while (NStr::EndsWith (title, ",") ||
               NStr::EndsWith (title, ";") ||
               NStr::EndsWith (title, "~") ||
               NStr::EndsWith (title, " ")) {
        title.erase (title.end() - 1);
    }

    // calculate prefix
    prefix = x_SetPrefix ();

    // calculate suffix
    suffix = x_SetSuffix (bioseq, scope, title);

    return prefix + title + suffix;
}


#if 0
// PUBLIC FUNCTIONS - will probably remove

// preferred function only does feature indexing if necessary
string CreateDefLine (
    const CBioseq& bioseq,
    CScope& scope,
    CDeflineGenerator::TUserFlags flags
)

{
    CDeflineGenerator gen;

    return gen.GenerateDefline (bioseq, scope, flags);
}

// alternative provided for backward compatibility with existing function
string CreateDefLine (
    const CBioseq_Handle& hnd,
    CDeflineGenerator::TUserFlags flags
)

{
    string result;

    CConstRef<CBioseq> bs_ref = hnd.GetCompleteBioseq();
    if (bs_ref) {
        CScope& scope = hnd.GetScope();
        result = CreateDefLine (*bs_ref, scope, flags);
    }

    return result;
}
#endif

