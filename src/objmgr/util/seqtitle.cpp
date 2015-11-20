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
* Author:  Aaron Ucko
*
* File Description:
*   Obtains or constructs a sequence's title.  (Corresponds to
*   CreateDefLine in the C toolkit.)
*/

#include <ncbi_pch.hpp>
#include <serial/iterator.hpp>

#include <objects/biblio/Id_pat.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seq/Seg_ext.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seqblock/EMBL_block.hpp>
#include <objects/seqblock/GB_block.hpp>
#include <objects/seqblock/PDB_block.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Gene_ref.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/Prot_ref.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqloc/PDB_seq_id.hpp>
#include <objects/seqloc/PDB_seq_id.hpp>
#include <objects/seqloc/Patent_seq_id.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_loc_mix.hpp>
#include <objects/seqloc/Textseq_id.hpp>
#include <objects/seqset/Seq_entry.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/seq_map_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/util/feature.hpp>
#include <objmgr/util/sequence.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(sequence)

static string s_TitleFromBioSource (const CBioSource&    source,
                                          CMolInfo::TTech tech,
                                    const string&        suffix = kEmptyStr,
                                          bool           pooled_clones = false);


static string s_TitleFromChromosome(const CBioSource&    source,
                                    const CMolInfo&      mol_info);


static string s_TitleFromProtein   (const CBioseq_Handle& handle,
                                          CScope&        scope,
                                          string&        organism,
                                          TGetTitleFlags flags);
static string s_TitleFromSegment   (const CBioseq_Handle& handle,
                                          CScope&        scope,
                                          TGetTitleFlags flags);

static void s_FlyCG_PtoR(string& s);
                                          

enum EOrganelleNameFlags {
    fON_with_plasmid = 0x1,
    fON_virus        = 0x2,
    fON_wgs          = 0x4
};
typedef int TOrganelleNameFlags; // binary OR of EOrganelleNameFlags


static const char* s_OrganelleName(CBioSource::TGenome genome,
                                   TOrganelleNameFlags flags);


string GetTitle(const CBioseq_Handle& hnd, TGetTitleFlags flags)
{
    string                    prefix, title, suffix;
    string                    organism;
    CConstRef<CTextseq_id>    tsid(NULL);
    CConstRef<CPDB_seq_id>    pdb_id(NULL);
    CConstRef<CPatent_seq_id> pat_id(NULL);
    CConstRef<CDbtag>         general_id(NULL);
    CConstRef<CBioSource>     source(NULL);
    CConstRef<CMolInfo>       mol_info(NULL);
    bool                      third_party = false;
    bool                      tpa_exp     = false;
    bool                      tpa_inf     = false;
    bool                      is_nc       = false;
    bool                      is_nm       = false;
    bool                      is_nr       = false;
    bool                      is_tsa      = false;
    bool                      wgs_master  = false;
    bool                      tsa_master  = false;
    CMolInfo::TTech           tech        = CMolInfo::eTech_unknown;
    bool                      htg_tech    = false;
    bool                      htgs_draft  = false;
    bool                      htgs_cancelled = false;
    bool                      htgs_pooled = false;
    bool                      htgs_unfinished = false;
    bool                      use_biosrc  = false;
    CScope&                   scope       = hnd.GetScope();

    ITERATE (CBioseq_Handle::TId, idh, hnd.GetId()) {
        CConstRef<CSeq_id> id = idh->GetSeqId();
        if ( !tsid ) {
            tsid = id->GetTextseq_Id();
        }
        switch (id->Which()) {
        case CSeq_id::e_Other:
        case CSeq_id::e_Genbank:
        case CSeq_id::e_Embl:
        case CSeq_id::e_Ddbj:
        {
            const CTextseq_id& t = *id->GetTextseq_Id();
            if (t.IsSetAccession()) {
                const string& acc = t.GetAccession();
                CSeq_id::EAccessionInfo type = CSeq_id::IdentifyAccession(acc);
                if ((type & CSeq_id::eAcc_division_mask) == CSeq_id::eAcc_wgs
                    &&  NStr::EndsWith(acc, "000000")) {
                    wgs_master = true;
                } else if ((type & CSeq_id::eAcc_division_mask) == CSeq_id::eAcc_tsa
                    &&  NStr::EndsWith(acc, "000000")) {
                    tsa_master = true;
                } else if (type == CSeq_id::eAcc_refseq_chromosome) {
                    is_nc = true;
                } else if (type == CSeq_id::eAcc_refseq_mrna) {
                    is_nm = true;
                } else if (type == CSeq_id::eAcc_refseq_ncrna) {
                    is_nr = true;
                }
            }
            break;
        }
        case CSeq_id::e_General:
            if ( !id->GetGeneral().IsSkippable() ) {
                general_id = &id->GetGeneral();
            }
            break;
        case CSeq_id::e_Tpg:
        case CSeq_id::e_Tpe:
        case CSeq_id::e_Tpd:
            third_party = true;
            break;
        case CSeq_id::e_Pdb:
            pdb_id = &id->GetPdb();
            break;
        case CSeq_id::e_Patent:
            pat_id = &id->GetPatent();
            break;
        default:
            break;
        }
    }

    {
        CSeqdesc_CI::TDescChoices choices;
        choices.push_back(CSeqdesc::e_Source);
        choices.push_back(CSeqdesc::e_Molinfo);
        int found = 0;
        for ( CSeqdesc_CI it(hnd, choices); it; ++it ) {
            if ( it->Which() == CSeqdesc::e_Source ) {
                if ( !source ) {
                    source = &it->GetSource();
                    if ( (found |= 1) == 3 ) {
                        break;
                    }
                }
            }
            else {
                if ( !mol_info ) {
                    mol_info = &it->GetMolinfo();
                    tech = mol_info->GetTech();
                    if ( (found |= 2) == 3 ) {
                        break;
                    }
                }
            }
        }            
    }

    switch (tech) {
    case CMolInfo::eTech_htgs_0:
    case CMolInfo::eTech_htgs_1:
    case CMolInfo::eTech_htgs_2:
        htgs_unfinished = true;
        // manufacture all titles for unfinished HTG sequences
        flags |= fGetTitle_Reconstruct;
        // fall through
    case CMolInfo::eTech_htgs_3:
        htg_tech = true;
        // fall through
    case CMolInfo::eTech_est:
    case CMolInfo::eTech_sts:
    case CMolInfo::eTech_survey:
    case CMolInfo::eTech_wgs:
        use_biosrc = true;
        break;
    case CMolInfo::eTech_tsa:
        is_tsa = true;
        use_biosrc = true;
        break;
    default:
        break;
    }

    if (htg_tech  ||  third_party) {
        const CGB_block::TKeywords* keywords = 0;
        for (CSeqdesc_CI gb(hnd, CSeqdesc::e_Genbank);  gb;  ++gb) {
            if (gb->GetGenbank().IsSetKeywords()) {
                keywords = &gb->GetGenbank().GetKeywords();
            }
            BREAK(gb);
        }
        if ( !keywords ) {
            for (CSeqdesc_CI embl(hnd, CSeqdesc::e_Embl);  embl;  ++embl) {
                if (embl->GetEmbl().IsSetKeywords()) {
                    keywords = &embl->GetEmbl().GetKeywords();
                }
                BREAK(embl);
            }
        }
        if (keywords) {
            ITERATE (CGB_block::TKeywords, it, *keywords) {
                if (NStr::EqualNocase(*it, "HTGS_DRAFT")) {
                    htgs_draft = true;
                } else if (NStr::EqualNocase(*it, "HTGS_CANCELLED")) {
                    htgs_cancelled = true;
                } else if (NStr::EqualNocase(*it, "HTGS_POOLED_MULTICLONE")) {
                    htgs_pooled = true;
                } else if (NStr::EqualNocase(*it, "TPA:experimental")) {
                    tpa_exp = true;
                } else if (NStr::EqualNocase(*it, "TPA:inferential")) {
                    tpa_inf = true;
                }
            }
        }
    }

    if (!(flags & fGetTitle_Reconstruct)) {
        size_t search_depth = 0; // no limit
        // Ignore parents' titles for non-PDB proteins.
        if (hnd.GetBioseqMolType() == CSeq_inst::eMol_aa
            &&  pdb_id.IsNull()) {
            search_depth = 1; // only Bioseq's descriptors
        }
        CSeqdesc_CI it(hnd, CSeqdesc::e_Title, search_depth);
        if (it) {
            title = it->GetTitle();
        }
    }

    if (title.empty()  &&  use_biosrc  &&  source.NotEmpty()) {
        if (((tech == CMolInfo::eTech_wgs  &&  !wgs_master)  ||  is_tsa)
            &&  general_id.NotEmpty()  &&  general_id->GetTag().IsStr()) {
            title = s_TitleFromBioSource(*source, tech,
                                         general_id->GetTag().GetStr());
        } else {
            title = s_TitleFromBioSource(*source, tech, kEmptyStr,
                                         htgs_unfinished && htgs_pooled);
        }
        flags &= ~fGetTitle_Organism;
    }

    if (title.empty()  &&  is_nc  &&  source.NotEmpty()
        &&  mol_info.NotEmpty()) {
        switch (mol_info->GetBiomol()) {
        case CMolInfo::eBiomol_genomic:
        case CMolInfo::eBiomol_other_genetic:
            title = s_TitleFromChromosome(*source, *mol_info);
            if (!title.empty()) {
                flags &= ~fGetTitle_Organism;
            }
            break;
        }
    } else if (title.empty()  &&  is_nm  &&  source.NotEmpty()
               &&  (flags & fGetTitle_NoExpensive) == 0) {
        unsigned int         genes = 0, cdregions = 0, prots = 0;
        CConstRef<CSeq_feat> gene(0),   cdregion(0);
        for (CFeat_CI it(hnd);
             it;  ++it) {
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
        if (genes == 1  &&  cdregions == 1  // &&  prots >= 1
            &&  source->GetOrg().IsSetTaxname()) {
            title = source->GetOrg().GetTaxname() + ' ';
            string cds_label;
            feature::GetLabel(*cdregion, &cds_label, feature::fFGL_Content,
                              &scope);
            if (NStr::EqualNocase(source->GetOrg().GetTaxname(),
                                  "Drosophila melanogaster")) {
                s_FlyCG_PtoR(cds_label);
            }
            title += NStr::Replace(cds_label, "isoform ",
                                   "transcript variant ");
            title += " (";
            feature::GetLabel(*gene, &title, feature::fFGL_Content,
                              &scope);
            title += "), mRNA";
        }
    } else if (title.empty()  &&  is_nr  &&  source.NotEmpty()
               &&  source->GetOrg().IsSetTaxname()  &&  mol_info.NotEmpty()) {
        for (CTypeConstIterator<CSeq_feat> it(
                 *hnd.GetTopLevelEntry().GetCompleteSeq_entry());
             it;  ++it) {
            if (it->GetData().IsGene()) {
                title = source->GetOrg().GetTaxname() + ' ';
                feature::GetLabel(*it, &title, feature::fFGL_Content);
                title += ", ";
                switch (mol_info->GetBiomol()) {
                case CMolInfo::eBiomol_pre_RNA: title += "precursorRNA"; break;
                case CMolInfo::eBiomol_mRNA:    title += "mRNA";         break;
                case CMolInfo::eBiomol_rRNA:    title += "rRNA";         break;
                case CMolInfo::eBiomol_tRNA:    title += "tRNA";         break;
                case CMolInfo::eBiomol_snRNA:   title += "snRNA";        break;
                case CMolInfo::eBiomol_scRNA:   title += "scRNA";        break;
                case CMolInfo::eBiomol_cRNA:    title += "cRNA";         break;
                case CMolInfo::eBiomol_snoRNA:  title += "snoRNA";       break;
                case CMolInfo::eBiomol_transcribed_RNA: title+="miscRNA"; break;
                case CMolInfo::eBiomol_ncRNA:   title += "ncRNA";        break;
                case CMolInfo::eBiomol_tmRNA:   title += "tmRNA";        break;
                default:                        break;
                }
                BREAK(it);
            }
        }
    }

    // originally further down, but moved up to match the C version
    while (NStr::EndsWith(title, ".")  ||  NStr::EndsWith(title, " ")) {
        title.erase(title.end() - 1);
    }

    if (title.empty()  &&  pdb_id.NotEmpty()) {
        CSeqdesc_CI it(hnd, CSeqdesc::e_Pdb);
        for (;  it;  ++it) {
            if ( !it->GetPdb().GetCompound().empty() ) {
                if (isprint((unsigned char) pdb_id->GetChain())) {
                    title = string("Chain ") + (char)pdb_id->GetChain() + ", ";
                }
                title += it->GetPdb().GetCompound().front();
                BREAK(it);
            }
        }
    }

    if (title.empty()  &&  pat_id.NotEmpty()) {
        title = "Sequence " + NStr::IntToString(pat_id->GetSeqid())
            + " from Patent " + pat_id->GetCit().GetCountry()
            + ' ' + pat_id->GetCit().GetSomeNumber();
    }

    if (title.empty()  &&  hnd.GetBioseqMolType() == CSeq_inst::eMol_aa) {
        title = s_TitleFromProtein(hnd, scope, organism, flags);
        if ( !title.empty() ) {
            flags |= fGetTitle_Organism;
        }
    }

    if (title.empty()  &&  !htg_tech
        &&  hnd.GetInst_Repr() == CSeq_inst::eRepr_seg) {
        title = s_TitleFromSegment(hnd, scope, flags);
    }

    if (title.empty()  &&  !htg_tech  &&  source.NotEmpty()) {
        title = s_TitleFromBioSource(*source, tech);
        if (title.empty()) {
            title = "No definition line found";
        }
    }

    if (is_tsa  &&  !title.empty() ) {
        prefix = "TSA: ";
    } else if (third_party  &&  !title.empty() ) {
        bool tpa_start = NStr::StartsWith(title, "TPA: ", NStr::eNocase);
        if (tpa_exp) {
            if ( !NStr::StartsWith(title, "TPA_exp:", NStr::eNocase) ) {
                prefix = "TPA_exp: ";
                if (tpa_start) {
                    title.erase(0, 5);
                }
            }
        } else if (tpa_inf) {
            if ( !NStr::StartsWith(title, "TPA_inf:", NStr::eNocase) ) {
                prefix = "TPA_inf: ";
                if (tpa_start) {
                    title.erase(0, 5);
                }
            }
        } else if ( !tpa_start ) {
            prefix = "TPA: ";
        }
    }

    switch (tech) {
    case CMolInfo::eTech_htgs_0:
        if (title.find("LOW-PASS") == NPOS) {
            suffix = ", LOW-PASS SEQUENCE SAMPLING";
        }
        break;
    case CMolInfo::eTech_htgs_1:
    case CMolInfo::eTech_htgs_2:
    {
        if (htgs_draft  &&  title.find("WORKING DRAFT") == NPOS) {
            suffix = ", WORKING DRAFT SEQUENCE";
        } else if ( !htgs_draft  &&  !htgs_cancelled
                    &&  title.find("SEQUENCING IN") == NPOS) {
            suffix = ", *** SEQUENCING IN PROGRESS ***";
        }
        
        string un;
        if (tech == CMolInfo::eTech_htgs_1) {
            un = "un";
        }
        if (hnd.GetInst_Repr() == CSeq_inst::eRepr_delta) {
            unsigned int pieces = 1;
            for (CSeqMap_CI it(hnd, CSeqMap::fFindGap); it;  ++it) {
                ++pieces;
            }
            if (pieces == 1) {
                // suffix += (", 1 " + un + "ordered piece");
            } else {
                suffix += (", " + NStr::IntToString(pieces)
                           + ' ' + un + "ordered pieces");
            }
        } else {
            // suffix += ", in " + un + "ordered pieces";
        }
        break;
    }
    case CMolInfo::eTech_htgs_3:
        if (title.find("complete sequence") == NPOS) {
            suffix = ", complete sequence";
        }
        break;

    case CMolInfo::eTech_est:
        if (title.find("mRNA sequence") == NPOS) {
            suffix = ", mRNA sequence";
        }
        break;

    case CMolInfo::eTech_sts:
        if (title.find("sequence tagged site") == NPOS) {
            suffix = ", sequence tagged site";
        }
        break;

    case CMolInfo::eTech_survey:
        if (title.find("genomic survey sequence") == NPOS) {
            suffix = ", genomic survey sequence";
        }
        break;

    case CMolInfo::eTech_wgs:
        if (wgs_master) {
            if (title.find("whole genome shotgun sequencing project") == NPOS){
                suffix = ", whole genome shotgun sequencing project";
            }            
        } else if (title.find("whole genome shotgun sequence") == NPOS) {
            if (source.NotEmpty()) {
                const char* orgnl = s_OrganelleName(source->GetGenome(),
                                                    fON_wgs);
                if (orgnl[0]  &&  title.find(orgnl) == NPOS) {
                    suffix = string(1, ' ') + orgnl;
                }
            }
            suffix += ", whole genome shotgun sequence";
        }
        break;

    case CMolInfo::eTech_tsa:
        if (tsa_master) {
            if (title.find("transcriptome shotgun assembly project") == NPOS){
                suffix = ", transcriptome shotgun assembly project";
            }            
        } else if (title.find("transcriptome shotgun assembly project") == NPOS) {
            suffix += ", transcriptome shotgun assembly project";
        }
        break;
    }

    if (flags & fGetTitle_Organism) {
        CConstRef<COrg_ref> org;
        if (source) {
            org = &source->GetOrg();
        } else {
            CSeqdesc_CI it(hnd, CSeqdesc::e_Org);
            for (;  it;  ++it) {
                org = &it->GetOrg();
                BREAK(it);
            }
        }

        if (organism.empty()  &&  org.NotEmpty()  &&  org->IsSetTaxname()) {
            organism = org->GetTaxname();
        }
        if ( !organism.empty()  &&  title.find(organism) == NPOS) {
            suffix += " [" + organism + ']';
        }
    }

    return prefix + title + suffix;
}


bool GetTitle(const CBioseq& seq, string* title_ptr, TGetTitleFlags flags)
{
    string                    prefix, title, suffix;
    string                    organism;
    CConstRef<CTextseq_id>    tsid(NULL);
    CConstRef<CPDB_seq_id>    pdb_id(NULL);
    CConstRef<CPatent_seq_id> pat_id(NULL);
    CConstRef<CDbtag>         general_id(NULL);
    CConstRef<CBioSource>     source(NULL);
    CConstRef<CMolInfo>       mol_info(NULL);
    bool                      third_party = false;
    bool                      tpa_exp     = false;
    bool                      tpa_inf     = false;
    bool                      is_nc       = false;
    bool                      is_nm       = false;
    bool                      is_nr       = false;
    bool                      is_tsa      = false;
    bool                      wgs_master  = false;
    bool                      tsa_master  = false;
    CMolInfo::TTech           tech        = CMolInfo::eTech_unknown;
    bool                      htg_tech    = false;
    bool                      htgs_draft  = false;
    bool                      htgs_cancelled = false;
    bool                      htgs_pooled = false;
    bool                      htgs_unfinished = false;
    bool                      use_biosrc  = false;

    ITERATE (CBioseq::TId, it, seq.GetId()) {
        CConstRef<CSeq_id> id = *it;
        if ( !tsid ) {
            tsid = id->GetTextseq_Id();
        }
        switch (id->Which()) {
        case CSeq_id::e_Other:
        case CSeq_id::e_Genbank:
        case CSeq_id::e_Embl:
        case CSeq_id::e_Ddbj:
        {
            const CTextseq_id& t = *id->GetTextseq_Id();
            if (t.IsSetAccession()) {
                const string& acc = t.GetAccession();
                CSeq_id::EAccessionInfo type = CSeq_id::IdentifyAccession(acc);
                if ((type & CSeq_id::eAcc_division_mask) == CSeq_id::eAcc_wgs
                    &&  NStr::EndsWith(acc, "000000")) {
                    wgs_master = true;
                } else if ((type & CSeq_id::eAcc_division_mask) == CSeq_id::eAcc_tsa
                    &&  NStr::EndsWith(acc, "000000")) {
                    tsa_master = true;
                } else if (type == CSeq_id::eAcc_refseq_chromosome) {
                    is_nc = true;
                } else if (type == CSeq_id::eAcc_refseq_mrna) {
                    is_nm = true;
                } else if (type == CSeq_id::eAcc_refseq_ncrna) {
                    is_nr = true;
                }
            }
            break;
        }
        case CSeq_id::e_General:
            if ( !id->GetGeneral().IsSkippable() ) {
                general_id = &id->GetGeneral();
            }
            break;
        case CSeq_id::e_Tpg:
        case CSeq_id::e_Tpe:
        case CSeq_id::e_Tpd:
            third_party = true;
            break;
        case CSeq_id::e_Pdb:
            pdb_id = &id->GetPdb();
            break;
        case CSeq_id::e_Patent:
            pat_id = &id->GetPatent();
            break;
        default:
            break;
        }
    }

    {
        if ( CConstRef<CSeqdesc> desc =
             seq.GetClosestDescriptor(CSeqdesc::e_Source) ) {
            source = &desc->GetSource();
        }
        if ( CConstRef<CSeqdesc> desc =
             seq.GetClosestDescriptor(CSeqdesc::e_Molinfo) ) {
            mol_info = &desc->GetMolinfo();
            tech = mol_info->GetTech();
        }
    }

    switch (tech) {
    case CMolInfo::eTech_htgs_0:
    case CMolInfo::eTech_htgs_1:
    case CMolInfo::eTech_htgs_2:
        htgs_unfinished = true;
        // manufacture all titles for unfinished HTG sequences
        flags |= fGetTitle_Reconstruct;
        // fall through
    case CMolInfo::eTech_htgs_3:
        htg_tech = true;
        // fall through
    case CMolInfo::eTech_est:
    case CMolInfo::eTech_sts:
    case CMolInfo::eTech_survey:
    case CMolInfo::eTech_wgs:
        use_biosrc = true;
        break;
    case CMolInfo::eTech_tsa:
        is_tsa = true;
        use_biosrc = true;
        break;
    default:
        break;
    }

    if (htg_tech  ||  third_party) {
        return false;
    }

    if (!(flags & fGetTitle_Reconstruct)) {
        int max_level = kMax_Int; // no limit
        // Ignore parents' titles for non-PDB proteins.
        if (seq.IsAa()
            &&  pdb_id.IsNull()) {
            max_level = 0; // only Bioseq's descriptors
        }
        int level = 0;
        if ( CConstRef<CSeqdesc> desc =
             seq.GetClosestDescriptor(CSeqdesc::e_Title, &level) ) {
            if ( level <= max_level ) {
                title = desc->GetTitle();
            }
        }
    }

    if (title.empty()  &&  use_biosrc  &&  source.NotEmpty()) {
        if (((tech == CMolInfo::eTech_wgs  &&  !wgs_master)  ||  is_tsa)
            &&  general_id.NotEmpty()  &&  general_id->GetTag().IsStr()) {
            title = s_TitleFromBioSource(*source, tech,
                                         general_id->GetTag().GetStr());
        } else {
            title = s_TitleFromBioSource(*source, tech, kEmptyStr,
                                         htgs_unfinished && htgs_pooled);
        }
        flags &= ~fGetTitle_Organism;
    }

    if (title.empty()  &&  is_nc  &&  source.NotEmpty()
        &&  mol_info.NotEmpty()) {
        switch (mol_info->GetBiomol()) {
        case CMolInfo::eBiomol_genomic:
        case CMolInfo::eBiomol_other_genetic:
            title = s_TitleFromChromosome(*source, *mol_info);
            if (!title.empty()) {
                flags &= ~fGetTitle_Organism;
            }
            break;
        }
    } else if (title.empty()  &&  is_nm  &&  source.NotEmpty()) {
        return false;
    } else if (title.empty()  &&  is_nr  &&  source.NotEmpty()
               &&  source->GetOrg().IsSetTaxname()) {
        return false;
    }

    // originally further down, but moved up to match the C version
    while (NStr::EndsWith(title, ".")  ||  NStr::EndsWith(title, " ")) {
        title.erase(title.end() - 1);
    }

    if (title.empty()  &&  pdb_id.NotEmpty()) {
        return false;
    }

    if (title.empty()  &&  pat_id.NotEmpty()) {
        title = "Sequence " + NStr::IntToString(pat_id->GetSeqid())
            + " from Patent " + pat_id->GetCit().GetCountry()
            + ' ' + pat_id->GetCit().GetSomeNumber();
    }

    if (title.empty()  &&  seq.IsAa()) {
        return false;
    }

    if (title.empty()  &&  !htg_tech  &&
        (!seq.IsSetInst() || seq.GetInst().GetRepr() == CSeq_inst::eRepr_seg)) {
        return false;
    }

    if (title.empty()  &&  !htg_tech  &&  source.NotEmpty()) {
        title = s_TitleFromBioSource(*source, tech);
        if (title.empty()) {
            title = "No definition line found";
        }
    }

    if (is_tsa  &&  !title.empty() ) {
        prefix = "TSA: ";
    } else if (third_party  &&  !title.empty() ) {
        bool tpa_start = NStr::StartsWith(title, "TPA: ", NStr::eNocase);
        if (tpa_exp) {
            if ( !NStr::StartsWith(title, "TPA_exp:", NStr::eNocase) ) {
                prefix = "TPA_exp: ";
                if (tpa_start) {
                    title.erase(0, 5);
                }
            }
        } else if (tpa_inf) {
            if ( !NStr::StartsWith(title, "TPA_inf:", NStr::eNocase) ) {
                prefix = "TPA_inf: ";
                if (tpa_start) {
                    title.erase(0, 5);
                }
            }
        } else if ( !tpa_start ) {
            prefix = "TPA: ";
        }
    }

    switch (tech) {
    case CMolInfo::eTech_htgs_0:
        if (title.find("LOW-PASS") == NPOS) {
            suffix = ", LOW-PASS SEQUENCE SAMPLING";
        }
        break;
    case CMolInfo::eTech_htgs_1:
    case CMolInfo::eTech_htgs_2:
    {
        if (htgs_draft  &&  title.find("WORKING DRAFT") == NPOS) {
            suffix = ", WORKING DRAFT SEQUENCE";
        } else if ( !htgs_draft  &&  !htgs_cancelled
                    &&  title.find("SEQUENCING IN") == NPOS) {
            suffix = ", *** SEQUENCING IN PROGRESS ***";
        }
        
        string un;
        if (tech == CMolInfo::eTech_htgs_1) {
            un = "un";
        }
        if ((!seq.IsSetInst() || seq.GetInst().GetRepr() == CSeq_inst::eRepr_delta)) {
            return false;
        } else {
            // suffix += ", in " + un + "ordered pieces";
        }
        break;
    }
    case CMolInfo::eTech_htgs_3:
        if (title.find("complete sequence") == NPOS) {
            suffix = ", complete sequence";
        }
        break;

    case CMolInfo::eTech_est:
        if (title.find("mRNA sequence") == NPOS) {
            suffix = ", mRNA sequence";
        }
        break;

    case CMolInfo::eTech_sts:
        if (title.find("sequence tagged site") == NPOS) {
            suffix = ", sequence tagged site";
        }
        break;

    case CMolInfo::eTech_survey:
        if (title.find("genomic survey sequence") == NPOS) {
            suffix = ", genomic survey sequence";
        }
        break;

    case CMolInfo::eTech_wgs:
        if (wgs_master) {
            if (title.find("whole genome shotgun sequencing project") == NPOS){
                suffix = ", whole genome shotgun sequencing project";
            }            
        } else if (title.find("whole genome shotgun sequence") == NPOS) {
            if (source.NotEmpty()) {
                const char* orgnl = s_OrganelleName(source->GetGenome(),
                                                    fON_wgs);
                if (orgnl[0]  &&  title.find(orgnl) == NPOS) {
                    suffix = string(1, ' ') + orgnl;
                }
            }
            suffix += ", whole genome shotgun sequence";
        }
        break;

    case CMolInfo::eTech_tsa:
        if (tsa_master) {
            if (title.find("transcriptome shotgun assembly project") == NPOS){
                suffix = ", transcriptome shotgun assembly project";
            }            
        } else if (title.find("transcriptome shotgun assembly project") == NPOS) {
            suffix += ", transcriptome shotgun assembly project";
        }
        break;
    }

    if (flags & fGetTitle_Organism) {
        CConstRef<COrg_ref> org;
        if (source) {
            org = &source->GetOrg();
        } else {
            if ( CConstRef<CSeqdesc> desc =
                 seq.GetClosestDescriptor(CSeqdesc::e_Org) ) {
                org = &desc->GetOrg();
            }
        }

        if (organism.empty()  &&  org.NotEmpty()  &&  org->IsSetTaxname()) {
            organism = org->GetTaxname();
        }
        if ( !organism.empty()  &&  title.find(organism) == NPOS) {
            suffix += " [" + organism + ']';
        }
    }

    *title_ptr = prefix + title + suffix;
    return true;
}


static string s_DescribeClones(const string& clone, bool pooled)
{
    SIZE_TYPE count = 1;
    for (SIZE_TYPE pos = clone.find(';');  pos != NPOS;
         pos = clone.find(';', pos + 1)) {
        ++count;
    }
    if (pooled) {
        return ", pooled multiple clones";
    } else if (count > 3) {
        return ", " + NStr::SizetToString(count) + " clones,";
    } else {
        return " clone " + clone;
    }
}


static bool s_EndsWithStrain(const string& name, const string& strain)
{
    // return NStr::EndsWith(name, strain, NStr::eNocase);
    if (strain.size() >= name.size()) {
        return false;
    }
    SIZE_TYPE pos = name.find(' ');
    if (pos == NPOS) {
        return false;
    }
    pos = name.find(' ', pos + 1);
    if (pos == NPOS  ||  pos + strain.size() >= name.size()) {
        return false;
    }
    // XXX - the C Toolkit starts looking unnecessarily early, which could
    // (at least in theory) lead to false negatives.
    pos = NStr::FindNoCase(name, strain, name.size() - strain.size() - 1);
    if (pos == name.size() - strain.size()) {
        return true;
    } else if (pos == name.size() - strain.size() - 1
               &&  name[pos - 1] == '\''  &&  name[name.size() - 1] == '\'') {
        return true;
    } else {
        return false;
    }
}


static string s_TitleFromBioSource(const CBioSource& source,
                                   CMolInfo::TTech   tech,
                                   const string&     suffix,
                                   bool              pooled_clones)
{
    string          name, chromosome, clone, map_, plasmid, strain, sfx;
    const COrg_ref& org = source.GetOrg();

    if (org.IsSetTaxname()) {
        name = org.GetTaxname();
    }

    if (suffix.size() > 0) {
        sfx = ' ' + suffix;
    }

    if (source.IsSetSubtype()) {
        ITERATE (CBioSource::TSubtype, it, source.GetSubtype()) {
            switch ((*it)->GetSubtype()) {
            case CSubSource::eSubtype_chromosome:
                chromosome = " chromosome " + (*it)->GetName();
                if (suffix == (*it)->GetName()) {
                    sfx.clear();
                }
                break;
            case CSubSource::eSubtype_clone:
                clone = s_DescribeClones((*it)->GetName(), pooled_clones);
                break;
            case CSubSource::eSubtype_map:
                map_ = " map " + (*it)->GetName();
                break;
            case CSubSource::eSubtype_plasmid_name:
                if (tech == CMolInfo::eTech_wgs) { // omit otherwise
                    plasmid = " plasmid " + (*it)->GetName();
                    if (suffix == (*it)->GetName()) {
                        sfx.clear();
                    }
                }
                break;
            }
        }
    }

    if (org.IsSetOrgname()  &&  org.GetOrgname().IsSetMod()) {
        ITERATE (COrgName::TMod, it, org.GetOrgname().GetMod()) {
            const string& subname = (*it)->GetSubname();
            if ((*it)->GetSubtype() == COrgMod::eSubtype_strain
                &&  !s_EndsWithStrain(name, subname)) {
                strain = " strain " + subname.substr(0, subname.find(';'));
            }
        }
    }

    string title = NStr::TruncateSpaces(name + strain + chromosome + clone
                                        + map_ + plasmid + sfx);
    if ( !title.empty()  &&  islower((unsigned char) title[0])) {
        title[0] = toupper((unsigned char) title[0]);
    }

    return title;
}


static const char* s_OrganelleName(CBioSource::TGenome genome,
                                   TOrganelleNameFlags flags)
{
    switch (genome) {
        // unknown, genomic
    case CBioSource::eGenome_chloroplast:
        return "chloroplast";
    case CBioSource::eGenome_chromoplast:
        return "chromoplast";
    case CBioSource::eGenome_kinetoplast:
        return "kinetoplast";
    case CBioSource::eGenome_mitochondrion:
        if ((flags & (fON_with_plasmid | fON_wgs)) == 0) {
            return "mitochondrion";
        } else {
            return "mitochondrial";
        }
    case CBioSource::eGenome_plastid:
        return "plastid";
    case CBioSource::eGenome_macronuclear:
        if ((flags & fON_wgs) == 0) {
            return "macronuclear";
        }
        break;
    case CBioSource::eGenome_extrachrom:
        if ((flags & fON_wgs) == 0) {
            return "extrachromosomal";
        }
        break;
    case CBioSource::eGenome_plasmid:
        if ((flags & fON_wgs) == 0) {
            return "plasmid";
        }
        break;
        // transposon, insertion-seq
    case CBioSource::eGenome_cyanelle:
        return "cyanelle";
    case CBioSource::eGenome_proviral:
        if ((flags & fON_virus) == 0) {
            if ((flags & (fON_with_plasmid | fON_wgs)) == 0) {
                return "provirus";
            } else {
                return "proviral";
            }
        }
        break;
    case CBioSource::eGenome_virion:
        if ((flags & fON_virus) == 0) {
            return "virus";
        }
        break;
    case CBioSource::eGenome_nucleomorph:
        if ((flags & fON_wgs) == 0) {
            return "nucleomorph";
        }
        break;
    case CBioSource::eGenome_apicoplast:
        return "apicoplast";
    case CBioSource::eGenome_leucoplast:
        return "leucoplast";
    case CBioSource::eGenome_proplastid:
        if ((flags & fON_wgs) == 0) {
            return "protoplast";
        } else {
            return "proplastid";
        }
        break;
    case CBioSource::eGenome_endogenous_virus:
        if ((flags & fON_wgs) != 0) {
            return "endogenous virus";
        }
        break;
    case CBioSource::eGenome_hydrogenosome:
        if ((flags & fON_wgs) != 0) {
            return "hydrogenosome";
        }
        break;
    case CBioSource::eGenome_chromosome:
        if ((flags & fON_wgs) != 0) {
            return "chromosome";
        }
        break;
    case CBioSource::eGenome_chromatophore:
        if ((flags & fON_wgs) != 0) {
            return "chromatophore";
        }
        break;
    }
    return kEmptyCStr;
}


static string x_TitleFromChromosome(const CBioSource& source,
                                    const CMolInfo&   mol_info)
{
    string name, chromosome, segment, plasmid_name, orgnl;
    string seq_tag, gen_tag;
    bool   is_plasmid = false;
    TOrganelleNameFlags flags = 0;

    if (source.GetOrg().IsSetTaxname()) {
        name = source.GetOrg().GetTaxname();
    } else {
        return kEmptyStr;
    }

    string lc_name = name;
    NStr::ToLower(lc_name);

    if (lc_name.find("virus") != NPOS  ||  lc_name.find("phage") != NPOS) {
        flags |= fON_virus;
    }

    if (source.IsSetSubtype()) {
        ITERATE (CBioSource::TSubtype, it, source.GetSubtype()) {
            switch ((*it)->GetSubtype()) {
            case CSubSource::eSubtype_chromosome:
                chromosome = (*it)->GetName();
                break;
            case CSubSource::eSubtype_segment:
                segment = (*it)->GetName();
                break;
            case CSubSource::eSubtype_plasmid_name:
            {
                plasmid_name = (*it)->GetName();
                string lc_plasmid = plasmid_name;
                NStr::ToLower(lc_plasmid);
                if (lc_plasmid.find("plasmid") == NPOS
                    &&  lc_plasmid.find("element") == NPOS) {
                    plasmid_name = "plasmid " + plasmid_name;
                }
                flags |= fON_with_plasmid;
                break;
            }
            }
        }
    }

    orgnl = s_OrganelleName(source.GetGenome(), flags);
    if (source.GetGenome() == CBioSource::eGenome_plasmid) {
        is_plasmid = true;
    }

    switch (mol_info.GetCompleteness()) {
    case CMolInfo::eCompleteness_partial:
    case CMolInfo::eCompleteness_no_left:
    case CMolInfo::eCompleteness_no_right:
    case CMolInfo::eCompleteness_no_ends:
        seq_tag = ", partial sequence";
        gen_tag = ", genome";
        break;
    default:
        seq_tag = ", complete sequence";
        gen_tag = ", complete genome";
        break;
    }

    if (lc_name.find("plasmid") != NPOS) {
        return name + seq_tag;        
    } else if (is_plasmid) {
        if (plasmid_name.empty()) {
            return name + " unnamed plasmid" + seq_tag;
        } else {
            return name + ' ' + plasmid_name + seq_tag;
        }
    } else if ( !plasmid_name.empty() ) {
        if (orgnl.empty()) {
            return name + ' ' + plasmid_name + seq_tag;
        } else {
            return name + ' ' + orgnl + ' ' + plasmid_name + seq_tag;
        }
    } else if ( !orgnl.empty() ) {
        if ( chromosome.empty() ) {
            return name + ' ' + orgnl + gen_tag;
        } else {
            return name + ' ' + orgnl + " chromosome " + chromosome + seq_tag;
        }
    } else if ( !segment.empty() ) {
        if (segment.find("DNA") == NPOS  &&  segment.find("RNA") == NPOS
            &&  segment.find("segment") == NPOS
            &&  segment.find("Segment") == NPOS) {
            return name + " segment " + segment + seq_tag;
        } else {
            return name + ' ' + segment + seq_tag;
        }
    } else if ( !chromosome.empty() ) {
        return name + " chromosome " + chromosome + seq_tag;
    } else {
        return name + gen_tag;
    }
}


static string s_TitleFromChromosome(const CBioSource& source,
                                    const CMolInfo&   mol_info)
{
    string result = x_TitleFromChromosome(source, mol_info);
    result = NStr::Replace(result, "Plasmid", "plasmid");
    result = NStr::Replace(result, "Element", "element");
    if (!result.empty()) {
        result[0] = toupper((unsigned char) result[0]);
    }
    return result;
}


static string s_GetProteinName(const CBioseq_Handle& handle, CScope& scope,
                               CConstRef<CSeq_loc>& cds_loc,
                               TGetTitleFlags flags)
{
    CConstRef<CProt_ref> prot;
    CConstRef<CGene_ref> gene;

    CSeq_loc everywhere;
    everywhere.SetWhole().Assign(*handle.GetSeqId());

    {{
        CConstRef<CSeq_feat> prot_feat
            = GetBestOverlappingFeat(everywhere, CSeqFeatData::e_Prot,
                                     eOverlap_Contained, scope);
        if (prot_feat) {
            prot = &prot_feat->GetData().GetProt();
        }
    }}

    {{
        CConstRef<CSeq_feat> cds_feat(GetCDSForProduct(handle));
        if (cds_feat) {
            cds_loc = &cds_feat->GetLocation();
        }
    }}

    if (cds_loc) {
        CConstRef<CSeq_feat> gene_feat = GetOverlappingGene(*cds_loc, scope);
        if (gene_feat) {
            gene = &gene_feat->GetData().GetGene();
        }
    }

    if (prot.NotEmpty()  &&  prot->IsSetName()  &&  !prot->GetName().empty()) {
        string result;
        bool   first = true;
        ITERATE (CProt_ref::TName, it, prot->GetName()) {
            if ( !first ) {
                result += "; ";
            }
            result += *it;
            first = false;
            if ((flags & fGetTitle_AllProteins) == 0) {
                break; // just give the first
            }
        }
        if (NStr::CompareNocase(result, "hypothetical protein") == 0) {
            // XXX - gene_feat might not always be exactly what we want
            if (gene && gene->IsSetLocus_tag()) {
                result += ' ' + gene->GetLocus_tag();
            }
        }
        return result;
    } else if (prot.NotEmpty()  &&  prot->IsSetDesc()
               &&  !prot->GetDesc().empty()) {
        return prot->GetDesc();
    } else if (prot.NotEmpty()  &&  prot->IsSetActivity()
               &&  !prot->GetActivity().empty()) {
        return prot->GetActivity().front();
    } else if (gene) {
        string gene_name;
        if (gene->IsSetLocus()  &&  !gene->GetLocus().empty()) {
            gene_name = gene->GetLocus();
        } else if (gene->IsSetSyn()  &&  !gene->GetSyn().empty()) {
            gene_name = *gene->GetSyn().begin();
        } else if (gene->IsSetDesc()  &&  !gene->GetDesc().empty()) {
            gene_name = gene->GetDesc();
        }
        if ( !gene_name.empty() ) {
            return gene_name + " gene product";
        }
    }

    return "unnamed protein product";
}


static string s_TitleFromProtein(const CBioseq_Handle& handle, CScope& scope,
                                 string& organism, TGetTitleFlags flags)
{
    string              result;
    CConstRef<CSeq_loc> cds_loc;

    if ((flags & fGetTitle_NoExpensive) == 0) {
        result = s_GetProteinName(handle, scope, cds_loc, flags);
    } else {
        result = "unnamed protein product";
    }

    {{ // Find organism name (must be specifically associated with this Bioseq)
        CConstRef<COrg_ref> org;
        for (CSeqdesc_CI it(handle, CSeqdesc::e_Source, 1);  it;  ++it) {
            org = &it->GetSource().GetOrg();
            BREAK(it);
        }
        if (org.Empty()  &&  cds_loc.NotEmpty()) {
            for (CFeat_CI it(scope, *cds_loc, CSeqFeatData::e_Biosrc);
                 it;  ++it) {
                org = &it->GetData().GetBiosrc().GetOrg();
                BREAK(it);
            }
        }
        if (org.NotEmpty()  &&  org->IsSetTaxname()) {
            organism = org->GetTaxname();
        }
    }}

    return result;
}


static string s_TitleFromSegment(const CBioseq_Handle& handle, CScope& scope,
                                 TGetTitleFlags flags)
{
    string   organism, product, locus, strain, clone, isolate;
    string   completeness = "complete";
    bool     cds_found    = false;

    {
        CSeqdesc_CI it(handle, CSeqdesc::e_Source);
        for (;  it;  ++it) {
            const CBioSource& src = it->GetSource();
            const COrg_ref& org = src.GetOrg();
            if (org.IsSetTaxname()) {
                organism = org.GetTaxname();
                if (org.IsSetOrgname()) {
                    const COrgName& orgname = org.GetOrgname();
                    if (orgname.IsSetMod()) {
                        ITERATE (COrgName::TMod, mod, orgname.GetMod()) {
                            COrgMod::TSubtype subtype = (*mod)->GetSubtype();
                            const string&     subname = (*mod)->GetSubname();
                            if (subtype == COrgMod::eSubtype_strain) {
                                if ( !NStr::EndsWith(organism, subname) ) {
                                    strain = subname;
                                }
                                break;
                            } else if (subtype == COrgMod::eSubtype_isolate) {
                                isolate = subname;
                                break;
                            }
                        }
                    }
                }
            }
            if (src.IsSetSubtype()) {
                ITERATE (CBioSource::TSubtype, ssrc, src.GetSubtype()) {
                    if ((*ssrc)->GetSubtype() == CSubSource::eSubtype_clone) {
                        clone = s_DescribeClones((*ssrc)->GetName(), false);
                    }
                }
            }
            BREAK(it);
        }
    }

    if (organism.empty()) {
        organism = "Unknown";
    }

    CSeq_loc everywhere;
    everywhere.SetMix().Set() = handle.GetInst_Ext().GetSeg();

    if ((flags & fGetTitle_NoExpensive) == 0) {
        CFeat_CI it(scope, everywhere, CSeqFeatData::e_Cdregion);
        for (; it;  ++it) {
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
            if (product.empty()  &&  prot_feat.NotEmpty()
                &&  prot_feat->GetData().GetProt().IsSetName()) {
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

            BREAK(it);
        }
    }

    string result = organism;
    if ( !cds_found) {
        if ( !strain.empty() ) {
            result += " strain " + strain;
        } else if ( !clone.empty()  &&  clone.find(" clone ") != NPOS) {
            result += clone;
        } else if ( !isolate.empty() ) {
            result += " isolate " + isolate;
        }
    }
    if ( !product.empty() ) {
        result += ' ' + product;
    }
    if ( !locus.empty() ) {
        result += " (" + locus + ')';
    }
    if ( !product.empty()  ||  !locus.empty() ) {
        result += " gene, " + completeness + " cds";
    }
    return NStr::TruncateSpaces(result);
}


static void s_FlyCG_PtoR(string& s)
{
    // s =~ s/\b(CG\d*-)P([[:alpha:]])\b/$1R$2/g, more or less.
    SIZE_TYPE pos = 0, len = s.size();
    while ((pos = NStr::FindCase(s, "CG", pos)) != NPOS) {
        if (pos > 0  &&  !isspace((unsigned char)s[pos - 1]) ) {
            continue;
        }
        pos += 2;
        while (pos + 3 < len  &&  isdigit((unsigned char)s[pos])) {
            ++pos;
        }
        if (s[pos] == '-'  &&  s[pos + 1] == 'P'
            &&  isalpha((unsigned char)s[pos + 2])
            &&  (pos + 3 == len  ||  strchr(" ,;", s[pos + 3])) ) {
            s[pos + 1] = 'R';
        }
    }
}


END_SCOPE(sequence)
END_SCOPE(objects)
END_NCBI_SCOPE
