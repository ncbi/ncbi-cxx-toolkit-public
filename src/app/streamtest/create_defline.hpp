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

#ifndef __create_defline__hpp__
#define __create_defline__hpp__


// DEFINITION

class CDeflineGenerator
{
public:
    // constructor
    CDeflineGenerator (
        const CBioseq& bioseq,
        CScope& scope
    );

    // destructor
    ~CDeflineGenerator (void);

    // main method
    string GenerateDefline (
        bool ignoreExisting,
        bool allProteinNames
    );

private:
    // internal methods

    void x_SetFlags (void);
    void x_SetBioSrc (void);

    string x_DescribeClones (void);
    bool x_EndsWithStrain (void);
    void x_FlyCG_PtoR (
        string& s
    );
    string x_OrganelleName (
        bool has_plasmid,
        bool virus_or_phage,
        bool wgs_suffix
    );

    string x_TitleFromBioSrc (void);
    string x_TitleFromNC (void);
    string x_TitleFromNM (void);
    string x_TitleFromNR (void);
    string x_TitleFromPatent (void);
    string x_TitleFromPDB (void);
    string x_TitleFromProtein (void);
    string x_TitleFromSegSeq (void);
    string x_TitleFromWGS (void);

    string x_SetPrefix (void);
    string x_SetSuffix (
        const string& title
    );

private:
    // instance variables
    const CBioseq& m_bioseq;
    CScope& m_scope;

    // ignore existing title is forced for certain types
    bool m_reconstruct;
    bool m_allprotnames;

    // seq-inst fields
    bool m_is_na;
    bool m_is_aa;

    bool m_is_seg;
    bool m_is_delta;

    // seq-id fields
    bool m_is_nc;
    bool m_is_nm;
    bool m_is_nr;
    bool m_is_patent;
    bool m_is_pdb;
    bool m_third_party;
    bool m_wgs_master;

    string m_general_str;
    string m_patent_country;
    string m_patent_number;
    string m_patent_sequence;

    int m_pdb_chain;

    // molinfo fields
    TMOLINFO_BIOMOL m_mi_biomol;
    TMOLINFO_TECH m_mi_tech;
    TMOLINFO_COMPLETENESS m_mi_completeness;

    bool m_htg_tech;
    bool m_htgs_unfinished;
    bool m_is_tsa;
    bool m_is_wgs;

    bool m_use_biosrc;

    // genbank or embl block keyword fields
    bool m_htgs_cancelled;
    bool m_htgs_draft;
    bool m_htgs_pooled;
    bool m_tpa_exp;
    bool m_tpa_inf;

    // pdb block fields
    string m_pdb_compound;

    // biosource fields
    string m_taxname;
    TBIOSOURCE_GENOME m_genome;

    // subsource fields
    string m_chromosome;
    string m_clone;
    string m_map;
    string m_plasmid;
    string m_segment;

    // orgmod fields
    string m_isolate;
    string m_strain;
};


// IMPLEMENTATION

#include <objmgr/util/seq_loc_util.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/util/feature.hpp>
#include <objmgr/feat_ci.hpp>

// constructor
CDeflineGenerator::CDeflineGenerator (
    const CBioseq& bioseq,
    CScope& scope
)
    : m_bioseq (bioseq)
    , m_scope (scope)

{
    m_reconstruct = false;
    m_allprotnames = false;

    m_is_na = false;
    m_is_aa = false;

    m_is_seg = false;
    m_is_delta = false;

    m_is_nc = false;
    m_is_nm = false;
    m_is_nr = false;
    m_is_patent = false;
    m_is_pdb = false;
    m_third_party = false;
    m_wgs_master = false;

    m_pdb_chain = 0;

    m_mi_biomol = NCBI_BIOMOL(unknown);
    m_mi_tech = NCBI_TECH(unknown);
    m_mi_completeness = NCBI_COMPLETENESS(unknown);

    m_htg_tech = false;
    m_htgs_unfinished = false;
    m_is_tsa = false;
    m_is_wgs = false;

    m_use_biosrc = false;

    m_htgs_cancelled = false;
    m_htgs_draft = false;
    m_htgs_pooled = false;
    m_tpa_exp = false;
    m_tpa_inf = false;

    m_genome = NCBI_GENOME(unknown);
}

// destructor
CDeflineGenerator::~CDeflineGenerator (void)

{
}

// set instance variables from Seq-inst, Seq-ids, MolInfo, etc., but not BioSource
void CDeflineGenerator::x_SetFlags (void)

{
    m_is_na = m_bioseq.IsNa();
    m_is_aa = m_bioseq.IsAa();

    if (m_bioseq.IsSetInst()) {
        const CSeq_inst& inst = m_bioseq.GetInst();
        if (inst.IsSetRepr()) {
            TSEQ_REPR repr = inst.GetRepr();
            m_is_seg = (repr == CSeq_inst::eRepr_seg);
            m_is_delta = (repr == CSeq_inst::eRepr_delta);
        }
    }

    // process Seq-ids
    FOR_EACH_SEQID_ON_BIOSEQ (sid_itr, m_bioseq) {
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
                        m_wgs_master = true;
                    } else if (type == NCBI_ACCN(refseq_chromosome)) {
                        m_is_nc = true;
                    } else if (type == NCBI_ACCN(refseq_mrna)) {
                        m_is_nm = true;
                    } else if (type == NCBI_ACCN(refseq_ncrna)) {
                        m_is_nr = true;
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
                            m_general_str = oid.GetStr();
                        }
                    }
                }
                break;
            }
            case NCBI_SEQID(Tpg):
            case NCBI_SEQID(Tpe):
            case NCBI_SEQID(Tpd):
                m_third_party = true;
                break;
            case NCBI_SEQID(Pdb):
            {
                m_is_pdb = true;
                const CPDB_seq_id& pdb_id = sid.GetPdb ();
                if (pdb_id.IsSetChain()) {
                    m_pdb_chain = pdb_id.GetChain();
                }
                break;
            }
            case NCBI_SEQID(Patent):
            {
                m_is_patent = true;
                const CPatent_seq_id& pat_id = sid.GetPatent();
                if (pat_id.IsSetSeqid()) {
                    m_patent_sequence = NStr::IntToString(pat_id.GetSeqid());
                }
                if (pat_id.IsSetCit()) {
                    const CId_pat& cit = pat_id.GetCit();
                    m_patent_country = cit.GetCountry();
                    m_patent_number = cit.GetId().GetNumber();
                }
                break;
            }
            default:
                break;
        }
    }

    // process MolInfo tech
    IF_EXISTS_CLOSEST_MOLINFO (mi_ref, m_bioseq, NULL) {
        const CMolInfo& molinf = (*mi_ref).GetMolinfo();
        m_mi_biomol = molinf.GetBiomol();
        m_mi_tech = molinf.GetTech();
        switch (m_mi_tech) {
            case NCBI_TECH(htgs_0):
            case NCBI_TECH(htgs_1):
            case NCBI_TECH(htgs_2):
                m_htgs_unfinished = true;
                // manufacture all titles for unfinished HTG sequences
                m_reconstruct = true;
                // fall through
            case NCBI_TECH(htgs_3):
                m_htg_tech = true;
                // fall through
            case NCBI_TECH(est):
            case NCBI_TECH(sts):
            case NCBI_TECH(survey):
                m_use_biosrc = true;
                break;
            case NCBI_TECH(wgs):
                m_is_wgs = true;
                m_use_biosrc = true;
                break;
            case NCBI_TECH(tsa):
                m_is_tsa = true;
                m_use_biosrc = true;
                break;
            default:
                break;
        }
    }

    if (m_htg_tech || m_third_party) {

        // process keywords
        const list <string> *keywords = NULL;

        IF_EXISTS_CLOSEST_GENBANKBLOCK (gb_ref, m_bioseq, NULL) {
            const CGB_block& gbk = (*gb_ref).GetGenbank();
            if (gbk.IsSetKeywords()) {
                keywords = &gbk.GetKeywords();
            }
        }
        if (keywords == NULL) {
            IF_EXISTS_CLOSEST_EMBLBLOCK (eb_ref, m_bioseq, NULL) {
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
                    m_htgs_draft = true;
                } else if (NStr::EqualNocase (str, "HTGS_CANCELLED")) {
                    m_htgs_cancelled = true;
                } else if (NStr::EqualNocase (str, "HTGS_POOLED_MULTICLONE")) {
                    m_htgs_pooled = true;
                } else if (NStr::EqualNocase (str, "TPA:experimental")) {
                    m_tpa_exp = true;
                } else if (NStr::EqualNocase (str, "TPA:inferential")) {
                    m_tpa_inf = true;
                }
            }
        }
    }

    if (m_is_pdb) {

        // process PDB block
        IF_EXISTS_CLOSEST_PDBBLOCK (pb_ref, m_bioseq, NULL) {
            const CPDB_block& pbk = (*pb_ref).GetPdb();
            FOR_EACH_COMPOUND_ON_PDBBLOCK (cp_itr, pbk) {
                const string& str = *cp_itr;
                if (m_pdb_compound.empty()) {
                    m_pdb_compound = str;
                    // take first, then break to skip remainder
                    BREAK(cp_itr);
                }
            }
        }
    }
}

// set instance variables from BioSource
void CDeflineGenerator::x_SetBioSrc (void)

{
    IF_EXISTS_CLOSEST_BIOSOURCE (bs_ref, m_bioseq, NULL) {
        const CBioSource& source = (*bs_ref).GetSource();

        // get organism name
        if (source.IsSetTaxname()) {
            m_taxname = source.GetTaxname();
        }
        if (source.IsSetGenome()) {
            m_genome = source.GetGenome();
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
                            m_chromosome = str;
                            break;
                        case NCBI_SUBSRC(clone):
                            m_clone = str;
                            break;
                        case NCBI_SUBSRC(map):
                            m_map = str;
                            break;
                        case NCBI_SUBSRC(plasmid_name):
                            m_plasmid = str;
                            break;
                        case NCBI_SUBSRC(segment):
                            m_segment = str;
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
                            if (m_strain.empty()) {
                                m_strain = str;
                            }
                            break;
                        case NCBI_ORGMOD(isolate):
                            if (m_isolate.empty()) {
                                m_isolate = str;
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
    for (SIZE_TYPE pos = m_clone.find(';'); pos != NPOS;
         pos = m_clone.find(';', pos + 1)) {
        ++count;
    }
    if (m_htgs_unfinished && m_htgs_pooled) {
        return ", pooled multiple clones";
    } else if (count > 3) {
        return ", " + NStr::IntToString(count) + " clones";
    } else {
        return " clone " + m_clone;
    }
}

bool CDeflineGenerator::x_EndsWithStrain (void)
{
    // return NStr::EndsWith(m_taxname, m_strain, NStr::eNocase);
    if (m_strain.size() >= m_taxname.size()) {
        return false;
    }
    SIZE_TYPE pos = m_taxname.find(' ');
    if (pos == NPOS) {
        return false;
    }
    pos = m_taxname.find(' ', pos + 1);
    if (pos == NPOS) {
        return false;
    }
    // XXX - the C Toolkit looks for the first occurrence, which could
    // (at least in theory) lead to false negatives.
    pos = NStr::FindNoCase (m_taxname, m_strain, pos + 1, NPOS, NStr::eLast);
    if (pos == m_taxname.size() - m_strain.size()) {
        // check for space to avoid fortuitous match to end of taxname
        if (m_taxname[pos - 1] == ' ') {
            return true;
        }
    } else if (pos == m_taxname.size() - m_strain.size() - 1 &&
               m_taxname[pos - 1] == '\'' && m_taxname[m_taxname.size() - 1] == '\'') {
        return true;
    }
    return false;
}

string CDeflineGenerator::x_TitleFromBioSrc (void)

{
    string chr, cln, mp, pls, stn, sfx;

    if (! m_chromosome.empty()) {
        chr = " chromosome " + m_chromosome;
    }
    if (! m_clone.empty()) {
        cln = x_DescribeClones ();
    }
    if (! m_map.empty()) {
        mp = " map " + m_map;
    }
    if (! m_plasmid.empty()) {
        if (m_is_wgs) {
            pls = " plasmid " + m_plasmid;
        }
    }
    if (! m_strain.empty()) {
        if (! x_EndsWithStrain ()) {
            stn = " strain " + m_strain.substr (0, m_strain.find(';'));
        }
    }

    string title = NStr::TruncateSpaces (m_taxname + stn + chr + cln + mp + pls + sfx);

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

    switch (m_genome) {
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

     if (m_mi_biomol != NCBI_BIOMOL(genomic) &&
         m_mi_biomol != NCBI_BIOMOL(other_genetic)) return result;

    // require taxname to be set
    if (m_taxname.empty()) return result;

    string lc_name = m_taxname;
    NStr::ToLower (lc_name);

    if (lc_name.find("virus") != NPOS || lc_name.find("phage") != NPOS) {
        virus_or_phage = true;
    }

    if (! m_plasmid.empty()) {
        has_plasmid = true;
        string lc_plasmid = m_plasmid;
        NStr::ToLower (lc_plasmid);
        if (lc_plasmid.find("plasmid") == NPOS &&
            lc_plasmid.find("element") == NPOS) {
            pls = "plasmid " + m_plasmid;
        } else {
            pls = m_plasmid;
        }
    }

    orgnl = x_OrganelleName (has_plasmid, virus_or_phage, false);

    is_plasmid = (m_genome == NCBI_GENOME(plasmid));

    switch (m_mi_completeness) {
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
        result = m_taxname + seq_tag;        
    } else if (is_plasmid) {
        if (pls.empty()) {
            result = m_taxname + " unnamed plasmid" + seq_tag;
        } else {
            result = m_taxname + " " + pls + seq_tag;
        }
    } else if (! pls.empty() ) {
        if (orgnl.empty()) {
            result = m_taxname + " " + pls + seq_tag;
        } else {
            result = m_taxname + " " + orgnl + " " + pls + seq_tag;
        }
    } else if (! orgnl.empty() ) {
        if ( m_chromosome.empty() ) {
            result = m_taxname + " " + orgnl + gen_tag;
        } else {
            result = m_taxname + " " + orgnl + " chromosome " + m_chromosome + seq_tag;
        }
    } else if (! m_segment.empty()) {
        if (m_segment.find ("DNA") == NPOS &&
            m_segment.find ("RNA") == NPOS &&
            m_segment.find ("segment") == NPOS &&
            m_segment.find ("Segment") == NPOS) {
            result = m_taxname + " segment " + m_segment + seq_tag;
        } else {
            result = m_taxname + " " + m_segment + seq_tag;
        }
    } else if (! m_chromosome.empty() ) {
        result = m_taxname + " chromosome " + m_chromosome + seq_tag;
    } else {
        result = m_taxname + gen_tag;
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

string CDeflineGenerator::x_TitleFromNM (void)

{
    unsigned int         genes = 0, cdregions = 0, prots = 0;
    CConstRef<CSeq_feat> gene(0);
    CConstRef<CSeq_feat> cdregion(0);
    string result;

    // require taxname to be set
    if (m_taxname.empty()) return result;

    // !!! NOTE CALL TO OBJECT MANAGER !!!
    const CBioseq_Handle& hnd = m_scope.GetBioseqHandle (m_bioseq);

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

    if (genes == 1 && cdregions == 1 && (! m_taxname.empty())) {
        result = m_taxname + " ";
        string cds_label;
        feature::GetLabel (*cdregion, &cds_label, feature::eContent, &m_scope);
        if (NStr::EqualNocase (m_taxname, "Drosophila melanogaster")) {
            x_FlyCG_PtoR (cds_label);
        }
        result += NStr::Replace (cds_label, "isoform ", "transcript variant ");
        result += " (";
        feature::GetLabel (*gene, &result, feature::eContent, &m_scope);
        result += "), mRNA";
    }

    return result;
}

// generate title for NR
string CDeflineGenerator::x_TitleFromNR (void)

{
    string result;

    // require taxname to be set
    if (m_taxname.empty()) return result;

    // !!! NOTE CALL TO OBJECT MANAGER !!!
    const CBioseq_Handle& hnd = m_scope.GetBioseqHandle (m_bioseq);

    for (CTypeConstIterator<CSeq_feat> it(
          *hnd.GetTopLevelEntry().GetCompleteSeq_entry()); it; ++it) {
        if (it->GetData().IsGene()) {
            result = m_taxname + " ";
            feature::GetLabel(*it, &result, feature::eContent);
            result += ", ";
            switch (m_mi_biomol) {
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
            BREAK(it);
        }
    }

    return result;
}

// generate title for Patent
string CDeflineGenerator::x_TitleFromPatent (void)

{
    string result;

    result = "Sequence " + m_patent_sequence +
             " from Patent " + m_patent_country +
             " " + m_patent_number;

    return result;
}

// generate title for PDB
string CDeflineGenerator::x_TitleFromPDB (void)

{
    string result;

    if (isprint ((unsigned char) m_pdb_chain)) {
        result = string("Chain ") + (char) m_pdb_chain + ", ";
    }
    result += m_pdb_compound;

    return result;
}

// generate title for protein
string CDeflineGenerator::x_TitleFromProtein (void)

{
    CConstRef<CProt_ref> prot;
    CConstRef<CSeq_feat> cds_feat;
    CConstRef<CSeq_loc>  cds_loc;
    CConstRef<CGene_ref> gene;
    string               locus_tag;
    string               result;

    // !!! NOTE CALL TO OBJECT MANAGER !!!
    const CBioseq_Handle& hnd = m_scope.GetBioseqHandle (m_bioseq);

    CSeq_loc everywhere;
    everywhere.SetWhole().Assign(*hnd.GetSeqId());

    {{
        CConstRef<CSeq_feat> prot_feat
            = GetBestOverlappingFeat (everywhere, CSeqFeatData::e_Prot,
                                      eOverlap_Contained, m_scope);
        if (prot_feat) {
            prot = &prot_feat->GetData().GetProt();
        }
    }}

    {{
        cds_feat = GetCDSForProduct (hnd);
        if (cds_feat) {
            cds_loc = &cds_feat->GetLocation();
        }
    }}

    if (cds_loc) {
        CConstRef<CSeq_feat> gene_feat = GetOverlappingGene (*cds_loc, m_scope);
        if (gene_feat) {
            gene = &gene_feat->GetData().GetGene();
        }
    }

    if (prot) {
        const CProt_ref& prp = *prot;
        string prefix = "";
        FOR_EACH_NAME_ON_PROT (prp_itr, prp) {
            const string& str = *prp_itr;
            result += prefix;
            result += str;
            if (! m_allprotnames) {
                BREAK(prp_itr);
            }
            prefix = "; ";
        }
        if (! result.empty()) {
            if (NStr::CompareNocase (result, "hypothetical protein") == 0) {
                bool check_gene_feat = true;
                // first look for gene xref on CDS for locus_tag
                if (cds_feat) {
                    const CSeq_feat& feat = (*cds_feat);
                    FOR_EACH_SEQFEATXREF_ON_FEATURE (xf_itr, feat) {
                        const CSeqFeatXref& sfx = **xf_itr;
                        if (sfx.IsSetData()) {
                            const CSeqFeatData& sfd = sfx.GetData();
                            if (sfd.IsGene()) {
                                check_gene_feat = false;
                                const CGene_ref& grp = sfd.GetGene();
                                if (grp.IsSetLocus_tag()) {
                                    locus_tag = grp.GetLocus_tag();
                                }
                            }
                        }
                    }
                }
                // otherwise check overlapping gene feature for locus_tag
                if (check_gene_feat) {
                    if (gene && gene->IsSetLocus_tag()) {
                        locus_tag = gene->GetLocus_tag();
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
                BREAK(act_itr);
            }
        }
    }

    if (result.empty() && gene) {
        const CGene_ref& grp = *gene;
        if (grp.IsSetLocus()) {
            result = grp.GetLocus();
        }
        if (result.empty()) {
            FOR_EACH_SYNONYM_ON_GENE (syn_itr, grp) {
                const string& str = *syn_itr;
                result = str;
                BREAK(syn_itr);
            }
        }
        if (result.empty()) {
            if (grp.IsSetDesc()) {
                result = grp.GetDesc();
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
               NStr::EndsWith (result, " ")) {
        result.erase (result.end() - 1);
    }

    string taxname;
    taxname = m_taxname;

    // check for special taxname, go to overlapping source feature
    if (taxname.empty() ||
        (NStr::CompareNocase (taxname, "synthetic construct") != 0 &&
         NStr::CompareNocase (taxname, "artificial sequence") != 0 &&
         NStr::CompareNocase (taxname, "vector") != 0 &&
         NStr::CompareNocase (taxname, "Vector") != 0)) {
        if (cds_loc) {
            CConstRef<CSeq_feat> src_feat = GetOverlappingSource (*cds_loc, m_scope);
            if (src_feat) {
                const CSeq_feat& feat = *src_feat;
                if (feat.IsSetData()) {
                    const CSeqFeatData& fdata = feat.GetData();
                    if (fdata.IsBiosrc()) {
                        const CBioSource& source = fdata.GetBiosrc();
                        if (source.IsSetTaxname()) {
                            const string& str = source.GetTaxname();
                            taxname = str;
                        }
                    }
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
string CDeflineGenerator::x_TitleFromSegSeq (void)

{
    string completeness = "complete";
    bool   cds_found    = false;
    string locus, product, result;

    // !!! NOTE CALL TO OBJECT MANAGER !!!
    const CBioseq_Handle& hnd = m_scope.GetBioseqHandle (m_bioseq);

    if (m_taxname.empty()) {
        m_taxname = "Unknown";
    }

    CSeq_loc everywhere;
    everywhere.SetMix().Set() = hnd.GetInst_Ext().GetSeg();

    CFeat_CI it(m_scope, everywhere, CSeqFeatData::e_Cdregion);
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
                                     eOverlap_Interval, m_scope);
        if (product.empty()  &&  prot_feat.NotEmpty() &&
            prot_feat->GetData().GetProt().IsSetName()) {
            product = *prot_feat->GetData().GetProt().GetName().begin();
        }

        CConstRef<CSeq_feat> gene_feat
            = GetOverlappingGene(it->GetLocation(), m_scope);
        if (locus.empty()  &&  gene_feat.NotEmpty()) {
            if (gene_feat->GetData().GetGene().IsSetLocus()) {
                locus = gene_feat->GetData().GetGene().GetLocus();
            } else if (gene_feat->GetData().GetGene().IsSetSyn()) {
                locus = *gene_feat->GetData().GetGene().GetSyn().begin();
            }
        }

        BREAK(it);
    }

    result = m_taxname;

    if ( !cds_found) {
        if ( (! m_strain.empty()) && (! x_EndsWithStrain ())) {
            result += " strain " + m_strain;
        } else if (! m_clone.empty() && m_clone.find(" clone ") != NPOS) {
            result += x_DescribeClones ();
        } else if (! m_isolate.empty() ) {
            result += " isolate " + m_isolate;
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

    if (! m_chromosome.empty()) {
        chr = " chromosome " + m_chromosome;
    }
    if (! m_clone.empty()) {
        cln = x_DescribeClones ();
    }
    if (! m_map.empty()) {
        mp = " map " + m_map;
    }
    if (! m_plasmid.empty()) {
        if (m_is_wgs) {
            pls = " plasmid " + m_plasmid;
        }
    }
    if (! m_strain.empty()) {
        if (! x_EndsWithStrain ()) {
            stn = " strain " + m_strain.substr (0, m_strain.find(';'));
        }
    }
    if (! m_general_str.empty()) {
        sfx = " " + m_general_str;
    }

    string title = NStr::TruncateSpaces (m_taxname + stn + chr + cln + mp + pls + sfx);

    if (islower ((unsigned char) title[0])) {
        title [0] = toupper ((unsigned char) title [0]);
    }

    return title;
}

// generate TPA or TSA prefix
string CDeflineGenerator::x_SetPrefix (void)

{
    string prefix;

    if (m_is_tsa) {
        prefix = "TSA: ";
    } else if (m_third_party) {
        if (m_tpa_exp) {
            prefix = "TPA_exp: ";
        } else if (m_tpa_inf) {
            prefix = "TPA_inf: ";
        } else {
            prefix = "TPA: ";
        }
    }

    return prefix;
}

// generate suffix if not already present
string CDeflineGenerator::x_SetSuffix (
    const string& title
)

{
    string suffix;

    switch (m_mi_tech) {
        case NCBI_TECH(htgs_0):
            if (title.find ("LOW-PASS") == NPOS) {
                suffix = ", LOW-PASS SEQUENCE SAMPLING";
            }
            break;
        case NCBI_TECH(htgs_1):
        case NCBI_TECH(htgs_2):
        {
            if (m_htgs_draft && title.find ("WORKING DRAFT") == NPOS) {
                suffix = ", WORKING DRAFT SEQUENCE";
            } else if ( !m_htgs_draft && !m_htgs_cancelled &&
                       title.find ("SEQUENCING IN") == NPOS) {
                suffix = ", *** SEQUENCING IN PROGRESS ***";
            }

            string un;
            if (m_mi_tech == NCBI_TECH(htgs_1)) {
                un = "un";
            }
            if (m_is_delta) {
                unsigned int pieces = 1;
                // !!! NOTE CALL TO OBJECT MANAGER !!!
                const CBioseq_Handle& hnd = m_scope.GetBioseqHandle (m_bioseq);
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
            if (m_wgs_master) {
                if (title.find ("whole genome shotgun sequencing project") == NPOS){
                    suffix = ", whole genome shotgun sequencing project";
                }            
            } else if (title.find ("whole genome shotgun sequence") == NPOS) {
                if (! m_taxname.empty()) {
                    string orgnl = x_OrganelleName (false, false, true);
                    if (! orgnl.empty()) {
                        suffix = " " + orgnl;
                    }
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
    bool ignoreExisting,
    bool allProteinNames
)

{
    string prefix, title, suffix;

    // set flags from record components
    m_reconstruct = ignoreExisting;
    m_allprotnames = allProteinNames;

    // set flags from record components
    x_SetFlags ();

    if (! m_reconstruct) {
        // look for existing instantiated title
        int level = 0;
        IF_EXISTS_CLOSEST_TITLE (ttl_ref, m_bioseq, &level) {
            const string& str = (*ttl_ref).GetTitle();
            // for non-PDB proteins, title must be packaged on Bioseq
            if (m_is_na || m_is_pdb || level == 0) {
                title = str;

                // strip trailing periods, commas, semicolons, and spaces
                while (NStr::EndsWith (title, ".") ||
                           NStr::EndsWith (title, ",") ||
                           NStr::EndsWith (title, ";") ||
                           NStr::EndsWith (title, " ")) {
                    title.erase (title.end() - 1);
                }
            }
        }
    }

    // use appropriate algorithm if title needs to be generated
    if (title.empty()) {
        // PDB and patent records do not normally need source data
        if (m_is_pdb) {
            title = x_TitleFromPDB ();
        } else if (m_is_patent) {
            title = x_TitleFromPatent ();
        }

        if (title.empty()) {
            // set fields from source information
            x_SetBioSrc ();

            // several record types have specific methods
            if (m_is_nc) {
                title = x_TitleFromNC ();
            } else if (m_is_nm) {
                title = x_TitleFromNM ();
            } else if (m_is_nr) {
                title = x_TitleFromNR ();
            } else if (m_is_aa) {
                title = x_TitleFromProtein ();
            } else if (m_is_seg) {
                title = x_TitleFromSegSeq ();
            } else if (m_is_tsa || (m_is_wgs && (! m_wgs_master))) {
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

    // strip trailing commas, semicolons, and spaces (period may be an sp. species)
    while (NStr::EndsWith (title, ",") ||
               NStr::EndsWith (title, ";") ||
               NStr::EndsWith (title, " ")) {
        title.erase (title.end() - 1);
    }

    // calculate prefix
    prefix = x_SetPrefix ();

    // calculate suffix
    suffix = x_SetSuffix (title);

    return prefix + title + suffix;
}


// PUBLIC FUNCTIONS

// preferred function only does feature indexing if necessary
static string CreateDefLine (
    const CBioseq& bioseq,
    CScope& scope,
    bool ignoreExisting = false,
    bool allProteinNames = false
)

{
    CDeflineGenerator def (bioseq, scope);

    return def.GenerateDefline (ignoreExisting, allProteinNames);
}

// alternative provided for backward compatibility with existing function
static string CreateDefLine (
    const CBioseq_Handle& hnd,
    bool ignoreExisting = false,
    bool allProteinNames = false
)

{
    string result;

    CConstRef<CBioseq> bs_ref = hnd.GetCompleteBioseq();
    if (bs_ref) {
        CScope& scope = hnd.GetScope();
        result = CreateDefLine (*bs_ref, scope, ignoreExisting, allProteinNames);
    }

    return result;
}

#endif

