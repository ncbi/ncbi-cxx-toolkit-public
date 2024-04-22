/* $Id$
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
 * File Name: nucprot.cpp
 *
 * Author: Karl Sirotkin, Hsiu-Chuan Chen
 *
 * File Description:
 * -----------------
 *
 *      Take a Seq-entry or elements of a Bioseq-set, do orglookup,
 * protein translation lookup, then make a nucleic acid protein
 * sequence.
 *
 *      Get genetic code from either from Taxonomy database or from
 * guess rules (if the organism is different in the segment set or
 * Taxserver is not available)
 *
 *      orglookup includes
 *      - lookup taxname, common name
 *      - get lineage and division
 *      - get genetic codes
 *
 *      Protein translation lookup includes
 *      - lookup internal and end stop codon
 *      - compare two sequences, one from CdRegion, one from
 *        translation qualifier
 *
 *      Take our translation when the only diff is start codon.
 *
 *      This program only assign 3 different level of Bioseqset:
 *         class = nucprot, assign level = 1
 *         class = segset,  assign level = 2
 *         class = parts,   assign levle = 3
 *
 */

#include <ncbi_pch.hpp>

#include "ftacpp.hpp"

#include <objmgr/scope.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objects/seqfeat/Code_break.hpp>
#include <objmgr/util/seq_loc_util.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/Genetic_code.hpp>
#include <objmgr/util/sequence.hpp>
#include <objects/seq/Seq_data.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objtools/cleanup/cleanup.hpp>
#include <objects/general/User_object.hpp>
#include <objects/seqblock/EMBL_block.hpp>
#include <objects/seq/GIBB_method.hpp>
#include <objtools/edit/cds_fix.hpp>


#include "index.h"

#include <objtools/flatfile/flatfile_parser.hpp>
#include <objtools/flatfile/flatdefn.h>

#include "ftaerr.hpp"
#include "asci_blk.h"
#include "add.h"
#include "utilfeat.h"
#include "nucprot.h"
#include "fta_src.h"
#include "utilfun.h"
#include "indx_blk.h"
#include "xgbparint.h"

#ifdef THIS_FILE
#  undef THIS_FILE
#endif
#define THIS_FILE "nucprot.cpp"

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

typedef list<CRef<CCode_break>> TCodeBreakList;

const char* GBExceptionQualVals[] = {
    "RNA editing",
    "reasons given in citation",
    "rearrangement required for product",
    "annotated by transcript or proteomic data",
    nullptr
};

const char* RSExceptionQualVals[] = {
    "RNA editing",
    "reasons given in citation",
    "ribosomal slippage",
    "trans-splicing",
    "alternative processing",
    "artificial frameshift",
    "nonconsensus splice site",
    "rearrangement required for product",
    "modified codon recognition",
    "alternative start codon",
    "unclassified transcription discrepancy",
    "unclassified translation discrepancy",
    "mismatches in transcription",
    "mismatches in translation",
    nullptr
};

/**********************************************************
 *
 *   bool FindTheQual(qlist, qual):
 *
 *      Finds qual in the "qlist" return TRUE.
 *   Otherwise, return FALSE.
 *
 **********************************************************/
static bool FindTheQual(const CSeq_feat& feat, const Char* qual_to_find)
{
    for (const auto& qual : feat.GetQual()) {
        if (qual->IsSetQual() && qual->GetQual() == qual_to_find)
            return true;
    }

    return (false);
}

/**********************************************************
 *
 *   static char* CpTheQualValueNext(qlist, retq, qual):
 *
 *      Return qual's value if found the "qual" in the
 *   "qlist", and retq points to next available searching
 *   list; Otherwise, return NULL value and retq points
 *   to NULL.
 *
 **********************************************************/
static char* CpTheQualValueNext(TQualVector::iterator& cur_qual, const TQualVector::iterator& end_qual, const char* qual)
{
    string qvalue;

    for (; cur_qual != end_qual; ++cur_qual) {
        if (! (*cur_qual)->IsSetQual() || (*cur_qual)->GetQual() != qual || ! (*cur_qual)->IsSetVal())
            continue;

        qvalue = NStr::Sanitize((*cur_qual)->GetVal());

        ++cur_qual;
        break;
    }

    char* ret = nullptr;
    if (! qvalue.empty())
        ret = StringSave(qvalue);

    return ret;
}

/**********************************************************/
static Int4 fta_get_genetic_code(ParserPtr pp)
{
    IndexblkPtr ibp;
    ProtBlkPtr  pbp;
    Int4        gcode;

    if (pp->taxserver != 1)
        return (0);

    ibp = pp->entrylist[pp->curindx];
    if (ibp->gc_genomic < 1 && ibp->gc_mito < 1)
        return (0);

    pbp   = pp->pbp;
    gcode = ibp->gc_genomic;
    if (pbp->genome == 4 || pbp->genome == 5)
        gcode = ibp->gc_mito;
    pp->no_code = false;

    return (gcode);
}

/**********************************************************/
static void GuessGeneticCode(ParserPtr pp, const CSeq_descr& descrs)
{
    ProtBlkPtr pbp;
    Int4       gcode = 0;

    pbp = pp->pbp;

    for (const auto& descr : descrs.Get()) {
        if (! descr->IsModif())
            continue;

        for (EGIBB_mod modif : descr->GetModif()) {
            if (modif == eGIBB_mod_mitochondrial ||
                modif == eGIBB_mod_kinetoplast) {
                pbp->genome = 5; /* mitochondrion */
                break;
            }
        }
        break;
    }

    for (const auto& descr : descrs.Get()) {
        if (! descr->IsSource())
            continue;

        pbp->genome = descr->GetSource().IsSetGenome() ? descr->GetSource().GetGenome() : 0;
        break;
    }

    gcode = fta_get_genetic_code(pp);
    if (gcode <= 0)
        return;

    pbp->orig_gcode = gcode;
    pbp->gcode.SetId(gcode);
}

/**********************************************************/
static void GetGcode(const TEntryList& seq_entries, ParserPtr pp)
{
    if (pp && pp->pbp && ! pp->pbp->gcode.IsId()) {
        for (const auto& entry : seq_entries) {
            GuessGeneticCode(pp, GetDescrPointer(*entry));

            if (pp->pbp->gcode.IsId())
                break;
        }
    }
}

/**********************************************************/
static void ProtBlkFree(ProtBlkPtr pbp)
{
    pbp->gcode.Reset();
    pbp->feats.clear();

    pbp->entries.clear();
    // delete pbp->ibp;
    // pbp->ibp = nullptr;
    pbp->ibp->ids.clear();
}

/**********************************************************/
static void ProtBlkInit(ProtBlkPtr pbp)
{
    pbp->biosep = nullptr;

    pbp->gcode.Reset();
    pbp->segset = false;
    pbp->genome = 0;

    InfoBioseqPtr ibp = pbp->ibp;
    if (ibp) {
        ibp->ids.clear();
        ibp->mLocus.clear();
        ibp->mAccNum.clear();
    }
}

/**********************************************************/
static void AssignBioseqSetLevel(TEntryList& seq_entries)
{

    for (auto& entry : seq_entries) {
        for (CTypeIterator<CBioseq_set> bio_set(Begin(*entry)); bio_set; ++bio_set) {
            switch (bio_set->GetClass()) {
            case CBioseq_set::eClass_nuc_prot:
                bio_set->SetLevel(1);
                break;
            case CBioseq_set::eClass_segset:
                bio_set->SetLevel(2);
                break;
            case CBioseq_set::eClass_parts:
                bio_set->SetLevel(3);
                break;
            default:
                ErrPostEx(SEV_INFO, ERR_BIOSEQSETCLASS_NewClass, "BioseqSeq class %d not handled", (int)bio_set->GetClass());
            }
        }
    }
}

/**********************************************************
 *
 *   static bool check_short_CDS(pp, sfp, err_msg):
 *
 *      If CDS location contains one of the sequence ends
 *   return TRUE, e.g. it's short do not create protein
 *   bioseq, copy prot-ref to Xref.
 *
 **********************************************************/
static bool check_short_CDS(ParserPtr pp, const CSeq_feat& feat, bool err_msg)
{
    const CSeq_interval& interval = feat.GetLocation().GetInt();
    if (interval.GetFrom() == 0 || interval.GetTo() == (TSeqPos)(pp->entrylist[pp->curindx]->bases) - 1)
        return true;

    if (err_msg) {
        string loc = location_to_string(feat.GetLocation());
        ErrPostEx(SEV_WARNING, ERR_CDREGION_ShortProtein, "Short CDS (< 6 aa) located in the middle of the sequence: %s", loc.c_str());
    }
    return false;
}

/**********************************************************/
static void GetProtRefSeqId(CBioseq::TId& ids, InfoBioseqPtr ibp, int* num, ParserPtr pp, CScope& scope, CSeq_feat& cds)
{
    const char* r;
    string      protacc;
    const char* p;
    const char* q;
    ErrSev      sev;

    CSeq_id::E_Choice cho;
    CSeq_id::E_Choice ncho;

    if (pp->mode == Parser::EMode::Relaxed) {
        protacc = CpTheQualValue(cds.SetQual(), "protein_id");
        if (protacc.empty()) {
            int    protein_id_counter = 0;
            string idLabel;
            auto   pProteinId =
                edit::GetNewProtId(scope.GetBioseqHandle(cds.GetLocation()), protein_id_counter, idLabel, false);
            cds.SetProduct().SetWhole().Assign(*pProteinId);
            ids.push_back(pProteinId);
            return;
        }
        CSeq_id::ParseIDs(ids, protacc);
        return;
    }

    if (pp->source == Parser::ESource::USPTO) {
        protacc = CpTheQualValue(cds.SetQual(), "protein_id");
        CRef<CSeq_id>        pat_seq_id(new CSeq_id);
        CRef<CPatent_seq_id> pat_id = MakeUsptoPatSeqId(protacc.c_str());
        pat_seq_id->SetPatent(*pat_id);
        ids.push_back(pat_seq_id);
        return;
    }

    const CTextseq_id* text_id = nullptr;
    for (const auto& id : ibp->ids) {
        if (! id->IsPatent()) {
            text_id = id->GetTextseq_Id();
            break;
        }
    }

    if (! text_id)
        return;

    if (pp->accver == false || (pp->source != Parser::ESource::EMBL &&
                                pp->source != Parser::ESource::NCBI && pp->source != Parser::ESource::DDBJ)) {
        ++(*num);
        string obj_id_str = text_id->GetAccession();
        obj_id_str += '_';
        obj_id_str += to_string(*num);

        CRef<CSeq_id> seq_id(new CSeq_id);
        seq_id->SetLocal().SetStr(obj_id_str);
        ids.push_back(seq_id);
        return;
    }

    protacc = CpTheQualValue(cds.SetQual(), "protein_id");
    if (protacc.empty()) {
        string loc = location_to_string(cds.GetLocation());
        ErrPostEx(SEV_FATAL, ERR_CDREGION_MissingProteinId, "/protein_id qualifier is missing for CDS feature: \"%s\".", loc.c_str());
        return;
    }

    if (pp->mode == Parser::EMode::HTGSCON) {
        ++(*num);
        string obj_id_str = text_id->GetAccession();
        obj_id_str += '_';
        obj_id_str += to_string(*num);

        CRef<CSeq_id> seq_id(new CSeq_id);
        seq_id->SetLocal().SetStr(obj_id_str);
        ids.push_back(seq_id);
        return;
    }

    p = StringChr(protacc.c_str(), '.');
    if (! p || *(p + 1) == '\0') {
        string loc = location_to_string(cds.GetLocation());
        ErrPostEx(SEV_FATAL, ERR_CDREGION_MissingProteinVersion, "/protein_id qualifier has missing version for CDS feature: \"%s\".", loc.c_str());
        return;
    }

    for (q = p + 1; *q >= '0' && *q <= '9';)
        q++;
    if (*q != '\0') {
        string loc = location_to_string(cds.GetLocation());
        ErrPostEx(SEV_FATAL, ERR_CDREGION_IncorrectProteinVersion, "/protein_id qualifier \"%s\" has incorrect version for CDS feature: \"%s\".", protacc.c_str(), loc.c_str());
        return;
    }

    const string protaccStr(protacc.c_str(), p);
    const int    protaccVer(atoi(p + 1));
    protacc.clear();

    cho = GetProtAccOwner(protaccStr);
    if (cho == CSeq_id::e_not_set) {
        string loc = location_to_string(cds.GetLocation());
        ErrPostEx(SEV_FATAL, ERR_CDREGION_IncorrectProteinAccession, "/protein_id qualifier has incorrect accession \"%s\" for CDS feature: \"%s\".", protaccStr.c_str(), loc.c_str());
        return;
    }

    r    = nullptr;
    ncho = cho;
    if (pp->source == Parser::ESource::EMBL && cho != CSeq_id::e_Embl && cho != CSeq_id::e_Tpe)
        r = "EMBL";
    else if (pp->source == Parser::ESource::DDBJ && cho != CSeq_id::e_Ddbj &&
             cho != CSeq_id::e_Tpd)
        r = "DDBJ";
    else if (pp->source == Parser::ESource::NCBI && cho != CSeq_id::e_Genbank &&
             cho != CSeq_id::e_Tpg)
        r = "NCBI";
    else {
        ncho = GetNucAccOwner(text_id->GetAccession().c_str());
        if (ncho == CSeq_id::e_Tpe && cho == CSeq_id::e_Embl)
            cho = ncho;
    }

    if (r || ncho != cho) {
        string loc = location_to_string(cds.GetLocation());
        if (pp->ign_prot_src == false)
            sev = SEV_FATAL;
        else
            sev = SEV_WARNING;
        if (r)
            ErrPostEx(sev, ERR_CDREGION_IncorrectProteinAccession, "/protein_id qualifier has incorrect accession prefix \"%s\" for source %s for CDS feature: \"%s\".", protaccStr.c_str(), r, loc.c_str());
        else
            ErrPostEx(sev, ERR_CDREGION_IncorrectProteinAccession, "Found mismatching TPA and non-TPA nucleotides's and protein's accessions in one nuc-prot set. Nuc = \"%s\", prot = \"%s\".", text_id->GetAccession().c_str(), protaccStr.c_str());
        if (pp->ign_prot_src == false) {
            return;
        }
    }

    CRef<CSeq_id> seq_id(new CSeq_id);

    CRef<CTextseq_id> new_text_id(new CTextseq_id);
    new_text_id->SetAccession(protaccStr);
    new_text_id->SetVersion(protaccVer);
    SetTextId(cho, *seq_id, *new_text_id);

    ids.push_back(seq_id);

    if ((pp->source != Parser::ESource::DDBJ && pp->source != Parser::ESource::EMBL) ||
        pp->entrylist[pp->curindx]->is_wgs == false || ibp->mAccNum.size() == 8) {
        return;
    }

    seq_id.Reset(new CSeq_id);
    seq_id->SetGeneral().SetTag().SetStr(protaccStr);

    string& db = seq_id->SetGeneral().SetDb();
    if (pp->entrylist[pp->curindx]->is_tsa != false)
        db = "TSA:";
    else if (pp->entrylist[pp->curindx]->is_tls != false)
        db = "TLS:";
    else
        db = "WGS:";

    db.append(ibp->mAccNum.substr(0, 4));
    ids.push_back(seq_id);
}

/**********************************************************/
static char* stripStr(char* base, const char* str)
{
    char* bptr;
    char* eptr;

    if (! base || ! str)
        return nullptr;
    bptr = StringStr(base, str);
    if (bptr) {
        eptr = bptr + StringLen(str);
        fta_StringCpy(bptr, eptr);
    }

    return (base);
}

/**********************************************************/
static void StripCDSComment(CSeq_feat& feat)
{
    static const char* strA[] = {
        "Author-given protein sequence is in conflict with the conceptual translation.",
        "direct peptide sequencing.",
        "Method: conceptual translation with partial peptide sequencing.",
        "Method: sequenced peptide, ordered by overlap.",
        "Method: sequenced peptide, ordered by homology.",
        "Method: conceptual translation supplied by author.",
        nullptr
    };

    if (! feat.IsSetComment())
        return;

    string comment = tata_save(feat.GetComment());

    if (! comment.empty()) {
        char* pchComment = StringSave(comment);
        for (const char** b = strA; *b; b++) {
            pchComment = stripStr(pchComment, *b);
        }
        comment = tata_save(pchComment);
        MemFree(pchComment);
        ShrinkSpaces(comment);
    }

    if (! comment.empty())
        feat.SetComment(comment);
    else
        feat.ResetComment();
}

static void GetProtRefAnnot(InfoBioseqPtr ibp, CSeq_feat& feat, CBioseq& bioseq)
{
    optional<string> qval;
    bool  partial5;
    bool  partial3;

    std::set<string> names;

    for (;;) {
        qval = GetTheQualValue(feat.SetQual(), "product");
        if (! qval)
            break;

        string qval_str = *qval;
        qval.reset();

        if (qval_str[0] == '\'')
            qval_str = qval_str.substr(1, qval_str.size() - 2);

        names.insert(qval_str);
    }

    string qval2;
    if (names.empty()) {
        qval2 = CpTheQualValue(feat.GetQual(), "gene");
        if (qval2.empty())
            qval2 = CpTheQualValue(feat.GetQual(), "standard_name");
        if (qval2.empty())
            qval2 = CpTheQualValue(feat.GetQual(), "label");
    }

    CRef<CProt_ref> prot_ref(new CProt_ref);

    if (names.empty() && qval2.empty()) {
        string prid = CpTheQualValue(feat.GetQual(), "protein_id");
        string loc = location_to_string(feat.GetLocation());
        if (! prid.empty()) {
            ErrPostEx(SEV_WARNING, ERR_PROTREF_NoNameForProtein, "No product, gene, or standard_name qualifier found for protein \"%s\" on CDS:%s", prid.c_str(), loc.c_str());
        } else
            ErrPostEx(SEV_WARNING, ERR_PROTREF_NoNameForProtein, "No product, gene, or standard_name qualifier found on CDS:%s", loc.c_str());
        prot_ref->SetDesc("unnamed protein product");
    } else {
        if (! names.empty()) {
            prot_ref->SetName().clear();
            std::copy(names.begin(), names.end(), std::back_inserter(prot_ref->SetName()));
            names.clear();
        } else
            prot_ref->SetDesc(qval2);
    }

    while ((qval = GetTheQualValue(feat.SetQual(), "EC_number"))) {
        prot_ref->SetEc().push_back(*qval);
    }

    while ((qval = GetTheQualValue(feat.SetQual(), "function"))) {
        prot_ref->SetActivity().push_back(*qval);
    }

    if (feat.GetQual().empty())
        feat.ResetQual();

    CRef<CSeq_feat> feat_prot(new CSeq_feat);
    feat_prot->SetData().SetProt(*prot_ref);
    feat_prot->SetLocation(*fta_get_seqloc_int_whole(*bioseq.SetId().front(), bioseq.GetLength()));

    if (feat.IsSetPartial())
        feat_prot->SetPartial(feat.GetPartial());

    partial5 = feat.GetLocation().IsPartialStart(eExtreme_Biological);
    partial3 = feat.GetLocation().IsPartialStop(eExtreme_Biological);

    if (partial5 || partial3) {
        CSeq_interval& interval = feat_prot->SetLocation().SetInt();

        if (partial5) {
            interval.SetFuzz_from().SetLim(CInt_fuzz::eLim_lt);
        }

        if (partial3) {
            interval.SetFuzz_to().SetLim(CInt_fuzz::eLim_gt);
        }
    }

    CRef<CSeq_annot> annot(new CSeq_annot);
    annot->SetData().SetFtable().push_back(feat_prot);

    bioseq.SetAnnot().push_back(annot);
}

/**********************************************************/
static void GetProtRefDescr(CSeq_feat& feat, Uint1 method, const CBioseq& bioseq, TSeqdescList& descrs)
{
    char* p;
    char* q;
    bool  partial5;
    bool  partial3;
    Int4  diff_lowest;
    Int4  diff_current;
    Int4  cdslen;
    Int4  orglen;
    Uint1 strand;

    strand = feat.GetLocation().GetStrand();

    string organism;

    for (const auto& desc : bioseq.GetDescr().Get()) {
        if (! desc->IsSource())
            continue;

        const CBioSource& source = desc->GetSource();
        if (source.IsSetOrg() && source.GetOrg().IsSetTaxname()) {
            organism = source.GetOrg().GetTaxname();
            break;
        }
    }

    if (! fta_if_special_org(organism.c_str())) {
        diff_lowest = -1;
        cdslen      = sequence::GetLength(feat.GetLocation(), &GetScope());

        for (const auto& annot : bioseq.GetAnnot()) {
            if (! annot->IsFtable())
                continue;

            bool found = false;
            for (const auto& cur_feat : annot->GetData().GetFtable()) {
                if (! cur_feat->IsSetData() || ! cur_feat->GetData().IsBiosrc())
                    continue;

                orglen = sequence::GetLength(cur_feat->GetLocation(), &GetScope());

                const CBioSource& source = cur_feat->GetData().GetBiosrc();
                if (! source.IsSetOrg() || ! source.GetOrg().IsSetTaxname() ||
                    strand != cur_feat->GetLocation().GetStrand())
                    continue;

                sequence::ECompare cmp_res = sequence::Compare(feat.GetLocation(), cur_feat->GetLocation(), nullptr, sequence::fCompareOverlapping);
                if (cmp_res == sequence::eNoOverlap)
                    continue;

                if (cmp_res == sequence::eSame) {
                    organism = source.GetOrg().GetTaxname();
                    break;
                }

                if (cmp_res == sequence::eContained) {
                    diff_current = orglen - cdslen;
                    if (diff_lowest == -1 || diff_current < diff_lowest) {
                        diff_lowest = diff_current;
                        organism    = source.GetOrg().GetTaxname();
                    }
                } else if (cmp_res == sequence::eOverlap && diff_lowest < 0)
                    organism = source.GetOrg().GetTaxname();
            }

            if (found)
                break;
        }
    }

    CRef<CMolInfo> mol_info(new CMolInfo);
    mol_info->SetBiomol(CMolInfo::eBiomol_peptide); /* peptide */

    partial5 = feat.GetLocation().IsPartialStart(eExtreme_Biological);
    partial3 = feat.GetLocation().IsPartialStop(eExtreme_Biological);

    if (partial5 && partial3)
        mol_info->SetCompleteness(CMolInfo::eCompleteness_no_ends);
    else if (partial5)
        mol_info->SetCompleteness(CMolInfo::eCompleteness_no_left);
    else if (partial3)
        mol_info->SetCompleteness(CMolInfo::eCompleteness_no_right);
    else if (feat.IsSetPartial() && feat.GetPartial())
        mol_info->SetCompleteness(CMolInfo::eCompleteness_partial);

    if (method == eGIBB_method_concept_trans_a)
        mol_info->SetTech(CMolInfo::eTech_concept_trans_a);
    else if (method == eGIBB_method_concept_trans)
        mol_info->SetTech(CMolInfo::eTech_concept_trans);

    CRef<CSeqdesc> descr(new CSeqdesc);
    descr->SetMolinfo(*mol_info);
    descrs.push_back(descr);

    string s;
    for (const auto& qual : feat.GetQual()) {
        if (qual->GetQual() != "product")
            continue;

        if (! s.empty())
            s.append("; ");
        s.append(qual->GetVal());
    }

    if (s.empty())
        s = CpTheQualValue(feat.GetQual(), "gene");
    if (s.empty())
        s = CpTheQualValue(feat.GetQual(), "label");
    if (s.empty())
        s = CpTheQualValue(feat.GetQual(), "standard_name");
    if (s.empty())
        p = StringSave("unnamed protein product");
    else {
        p = StringSave(s);
        for (q = p; *q != '\0';)
            q++;
        if (q > p) {
            for (q--; *q == ' ' || *q == ','; q--)
                if (q == p)
                    break;
            if (*q != ' ' && *q != ',')
                q++;
            *q = '\0';
        }
    }

    if (StringLen(p) < 511 && ! organism.empty() && ! StringStr(p, organism.c_str())) {
        string s = p;
        s.append(" [");
        s.append(organism);
        s.append("]");
        MemFree(p);
        p = StringSave(s);
    }

    if (StringLen(p) > 511) {
        p[510] = '>';
        p[511] = '\0';
        q      = StringSave(p);
        MemFree(p);
    } else
        q = p;

    descr.Reset(new CSeqdesc);
    descr->SetTitle(q);
    descrs.push_back(descr);
    MemFree(q);
}

/**********************************************************
 *
 *   Function:
 *      static SeqIdPtr QualsToSeqID(pSeqFeat, source)
 *
 *   Purpose:
 *      Searches all /db_xref qualifiers make from them
 *      corresponding SeqId and removing found qualifiers
 *      from given SeqFeature.
 *
 *      Tatiana: /db_xref qualifiers were already processed
 *               sfp->dbxref in loadfeat.c
 *
 *   Parameters:
 *      pSeqFeat - pointer to SeqFeat structure which have
 *                 to be processed.
 *
 *   Return:
 *      Pointer to resultant SeqId chain if successful,
 *      otherwise NULL.
 *
 *   Note:
 *      Returned SeqId must be freed by caller.
 *
 **********************************************************/
static void QualsToSeqID(CSeq_feat& feat, Parser::ESource source, TSeqIdList& ids)
{
    char* p;

    if (! feat.IsSetQual())
        return;

    for (TQualVector::iterator qual = feat.SetQual().begin(); qual != feat.SetQual().end();) {
        if ((*qual)->GetQual() != "db_xref") {
            ++qual;
            continue;
        }

        CRef<CSeq_id> seq_id;
        p = StringIStr((*qual)->GetVal().c_str(), "pid:");
        if (p)
            seq_id = StrToSeqId(p + 4, true);

        if (seq_id.Empty()) {
            ErrPostEx(SEV_ERROR, ERR_CDREGION_InvalidDb_xref, "Invalid data format /db_xref = \"%s\", ignore it", (*qual)->GetVal().c_str());
        } else {
            if (p[4] == 'g' && (source == Parser::ESource::DDBJ || source == Parser::ESource::EMBL))
                seq_id.Reset();
            else
                ids.push_back(seq_id);
        }

        qual = feat.SetQual().erase(qual);
    }

    if (feat.GetQual().empty())
        feat.ResetQual();
}

/**********************************************************
 *
 *   Function:
 *      static SeqIdPtr ValidateQualSeqId(pSeqID)
 *
 *   Purpose:
 *      Validates consistency of SeqId list obtained from
 *      /db_xref in following maner. The number of SeqId
 *      must be not more then 3. Each SeqId must refer to
 *      a different GenBank. If two or more SeqId's refer
 *      to the same GenBank only first is taken in account
 *      and other ones are abandoned.
 *      During validating the function drop corresponding
 *      error messages if something wrong occured.
 *
 *   Parameters:
 *      pSeqFeat - pointer to SeqFeat structure which have
 *                 to be processed;
 *
 *   Return:
 *      Pointer to resultant SeqId chain. If pointer is
 *      NULL it means that there is no good SeqId.
 *
 **********************************************************/
static void ValidateQualSeqId(TSeqIdList& ids)
{
    bool abGenBanks[3] = { false, false, false };
    Int2 num;
    Char ch;

    if (ids.empty())
        return;

    ch = '\0';
    for (TSeqIdList::iterator id = ids.begin(); id != ids.end();) {
        num                   = -1;
        const Char* dbtag_str = (*id)->IsGeneral() ? (*id)->GetGeneral().GetTag().GetStr().c_str() : "";
        if ((*id)->IsGi()) {
            num = 1;
            ch  = 'g';
        } else if (*dbtag_str == 'e') {
            num = 0;
            ch  = 'e';
        } else if (*dbtag_str == 'd') {
            num = 2;
            ch  = 'd';
        }
        if (num == -1)
            continue;

        if (abGenBanks[num]) {
            /* PID of this type already exist, ignore it */
            ErrPostEx(SEV_WARNING, ERR_CDREGION_Multiple_PID, "/db_xref=\"pid:%c%i\" refer the same data base", ch, (*id)->GetGeneral().GetTag().GetId());

            id = ids.erase(id);
        } else {
            abGenBanks[num] = true;
            ++id;
        }
    }
}

/**********************************************************/
static void DbxrefToSeqID(CSeq_feat& feat, Parser::ESource source, TSeqIdList& ids)
{
    if (! feat.IsSetDbxref())
        return;

    for (CSeq_feat::TDbxref::iterator xref = feat.SetDbxref().begin(); xref != feat.SetDbxref().end();) {
        if (! (*xref)->IsSetTag() || ! (*xref)->IsSetDb()) {
            ++xref;
            continue;
        }

        CRef<CSeq_id> id;

        if ((*xref)->GetDb() == "PID") {
            const Char* tag_str = (*xref)->GetTag().GetStr().c_str();
            switch (tag_str[0]) {
            case 'g':
                if (source != Parser::ESource::DDBJ && source != Parser::ESource::EMBL) {
                    id.Reset(new CSeq_id);
                    id->SetGi(GI_FROM(long long, strtoll(tag_str + 1, nullptr, 10)));
                }
                break;

            case 'd':
            case 'e':
                id.Reset(new CSeq_id);
                id->SetGeneral(*(*xref));
                break;

            default:
                break;
            }

            xref = feat.SetDbxref().erase(xref);
        } else
            ++xref;

        if (id.NotEmpty())
            ids.push_back(id);
    }

    if (feat.GetDbxref().empty())
        feat.ResetDbxref();
}

/**********************************************************
 *
 *   Function:
 *      static void ProcessForDbxref(pBioseq, pSeqFeat,
 *                                   source)
 *
 *   Purpose:
 *      Finds all qualifiers corresponding to /db_xref,
 *      makes from them SeqId and remove them from further
 *      processing. Also the function looks for PID which
 *      can be found in /note (pSeqFeat->comment) and
 *      removes PID record from /note.
 *
 *   Parameters:
 *      pBioseq  - pointer or a Bioseq structure which will
 *                 hold resultant list of SeqId;
 *      pSeqFeat - pointer to SeqFeat structure which have
 *                 to processed.
 *      source   - source of sequence.
 *
 *   Return:
 *      void
 *
 **********************************************************/
static void ProcessForDbxref(CBioseq& bioseq, CSeq_feat& feat, Parser::ESource source)
{
    TSeqIdList ids;
    QualsToSeqID(feat, source, ids);
    if (! ids.empty()) {
        ValidateQualSeqId(ids);
        if (! ids.empty()) {
            bioseq.SetId().splice(bioseq.SetId().end(), ids);
            return;
        }
    }

    DbxrefToSeqID(feat, source, ids);
    if (feat.IsSetComment() && feat.GetComment().empty())
        feat.ResetComment();

    if (! ids.empty())
        bioseq.SetId().splice(bioseq.SetId().end(), ids);
}

/**********************************************************/
static CRef<CBioseq> BldProtRefSeqEntry(ProtBlkPtr pbp, CSeq_feat& feat, const string& seq_data, Uint1 method, ParserPtr pp, const CBioseq& bioseq, CBioseq::TId& ids)
{
    CRef<CBioseq> new_bioseq;

    if (ids.empty())
        return new_bioseq;

    new_bioseq.Reset(new CBioseq);

    new_bioseq->SetId().swap(ids);

    ProcessForDbxref(*new_bioseq, feat, pp->source);

    new_bioseq->SetInst().SetLength(static_cast<TSeqPos>(seq_data.size()));

    GetProtRefDescr(feat, method, bioseq, new_bioseq->SetDescr());
    GetProtRefAnnot(pbp->ibp, feat, *new_bioseq);

    new_bioseq->SetInst().SetRepr(CSeq_inst::eRepr_raw);
    new_bioseq->SetInst().SetMol(CSeq_inst::eMol_aa);

    /* Seq_code always ncbieaa  08.08.96 */
    CRef<CSeq_data> data(new CSeq_data(seq_data, CSeq_data::e_Ncbieaa));
    new_bioseq->SetInst().SetSeq_data(*data);

    return new_bioseq;
}

/**********************************************************/
static void AddProtRefSeqEntry(ProtBlkPtr pbp, CBioseq& bioseq)
{
    CRef<CSeq_entry> entry(new CSeq_entry);
    entry->SetSeq(bioseq);
    pbp->entries.push_back(entry);
}

/**********************************************************/
static char* SimpleValuePos(const char* qval)
{
    const char* bptr;
    const char* eptr;

    bptr = StringStr(qval, "(pos:");
    if (! bptr)
        return nullptr;

    bptr += 5;
    while (*bptr == ' ')
        bptr++;
    for (eptr = bptr; *eptr != ',' && *eptr != '\0';)
        eptr++;

    return StringSave(string(bptr, eptr));
}

/**********************************************************
 *
 *   static CodeBreakPtr GetCdRegionCB(ibp, sfp, accver):
 *
 *      If protein translation (InternalStopCodon) O.K.,
 *   then this qualifier will be deleted ==> different from
 *   Karl's parser.
 *      For transl_except of type OTHER, use the ncibeaa
 *   code 'X' for Code-break.aa.
 *      CodeBreakPtr->aa.choice = 1  ==>  for ncbieaa;
 *      CodeBreakPtr->aa.value.intvalue = (Int4)'X'  ==>
 *   for unknown.
 *
 **********************************************************/
static void GetCdRegionCB(InfoBioseqPtr ibp, CSeq_feat& feat, TCodeBreakList& code_breaks, unsigned char* dif, bool accver)
{
    Int4 feat_start = -1;
    Int4 feat_stop  = -1;

    if (feat.IsSetLocation()) {
        feat_start = feat.GetLocation().GetStart(eExtreme_Positional);
        feat_stop  = feat.GetLocation().GetStop(eExtreme_Positional);
    }

    Uint1 res = 2;

    if (feat.IsSetQual()) {
        TQualVector::iterator cur_qual = feat.SetQual().begin(),
                              end_qual = feat.SetQual().end();

        char* qval = nullptr;
        while ((qval = CpTheQualValueNext(cur_qual, end_qual, "transl_except"))) {
            CRef<CCode_break> code_break(new CCode_break);

            int ncbieaa_val = GetQualValueAa(qval, false);

            code_break->SetAa().SetNcbieaa(ncbieaa_val);

            char* pos = SimpleValuePos(qval);

            int  num_errs = 0;
            bool locmap   = false;

            CRef<CSeq_loc> location = xgbparseint_ver(pos, locmap, num_errs, ibp->ids, accver);
            if (location.NotEmpty())
                code_break->SetLoc(*location);

            Int4 start = code_break->IsSetLoc() ? code_break->GetLoc().GetStart(eExtreme_Positional) : -1;
            Int4 stop  = code_break->IsSetLoc() ? code_break->GetLoc().GetStop(eExtreme_Positional) : -1;

            Uint1 i = (start > stop) ? 3 : (stop - start);

            if (i != 2)
                res = i;

            bool itis = false;
            if (ncbieaa_val == 42 &&
                (start == feat_start ||
                 stop == feat_stop))
                itis = true;

            bool pos_range = false;
            if (i == 2)
                pos_range = false;
            else if (i > 2)
                pos_range = true;
            else
                pos_range = ! itis;

            if (num_errs > 0 || pos_range) {
                ErrPostEx(SEV_WARNING, ERR_FEATURE_LocationParsing, "transl_except range is wrong, %s, drop the transl_except", pos);
                MemFree(pos);
                MemFree(qval);
                break;
            }

            if (code_break->IsSetLoc()) {
                if (feat.GetLocation().IsSetStrand())
                    code_break->SetLoc().SetStrand(feat.GetLocation().GetStrand());

                sequence::ECompare cmp_res = sequence::Compare(feat.GetLocation(), code_break->GetLoc(), nullptr, sequence::fCompareOverlapping);
                if (cmp_res != sequence::eContains) {
                    ErrPostEx(SEV_WARNING, ERR_FEATURE_LocationParsing, "/transl_except not in CDS: %s", qval);
                }
            }

            MemFree(pos);
            MemFree(qval);

            if (code_break.NotEmpty())
                code_breaks.push_back(code_break);
        }
    }

    *dif = 2 - res;
}

/**********************************************************/
static void CkEndStop(const CSeq_feat& feat, Uint1 dif)
{
    Int4 len;
    Int4 r;
    Int4 frm;

    len                       = sequence::GetLength(feat.GetLocation(), &GetScope());
    const CCdregion& cdregion = feat.GetData().GetCdregion();

    if (! cdregion.IsSetFrame() || cdregion.GetFrame() == 0)
        frm = 0;
    else
        frm = cdregion.GetFrame() - 1;

    r = (len - frm + (Int4)dif) % 3;
    if (r != 0 && (! feat.IsSetExcept() || feat.GetExcept() == false)) {
        string loc = location_to_string(feat.GetLocation());
        ErrPostEx(SEV_WARNING, ERR_CDREGION_UnevenLocation, "CDS: %s. Length is not divisable by 3, the remain is: %d", loc.c_str(), r);
    }
}

/**********************************************************/
static void check_end_internal(size_t protlen, const CSeq_feat& feat, Uint1 dif)
{
    Int4 frm;

    const CCdregion& cdregion = feat.GetData().GetCdregion();

    if (! cdregion.IsSetFrame() || cdregion.GetFrame() == 0)
        frm = 0;
    else
        frm = cdregion.GetFrame() - 1;

    size_t len = sequence::GetLength(feat.GetLocation(), &GetScope()) - frm + dif;

    if (protlen * 3 != len && (! feat.IsSetExcept() || feat.GetExcept() == false)) {
        string loc = location_to_string(feat.GetLocation());
        ErrPostEx(SEV_WARNING, ERR_CDREGION_LocationLength, "Length of the CDS: %s (%ld) disagree with the length calculated from the protein: %ld", loc.c_str(), len, protlen * 3);
    }
}

/**********************************************************
 *
 *   static void ErrByteStorePtr(ibp, sfp, bsp):
 *
 *      For debugging, put to error logfile, it needs
 *   to delete for "buildcds.c program.
 *
 **********************************************************/
static void ErrByteStorePtr(InfoBioseqPtr ibp, const CSeq_feat& feat, const string& prot)
{
    string qval = CpTheQualValue(feat.GetQual(), "translation");
    if (qval.empty())
        qval = "no translation qualifier";

    if (! feat.IsSetExcept() || feat.GetExcept() == false) {
        string loc = location_to_string(feat.GetLocation());
        ErrPostEx(SEV_WARNING, ERR_CDREGION_TranslationDiff, "Location: %s, translation: %s", loc.c_str(), qval.c_str());
    }

    ErrLogPrintStr(prot.c_str());
    ErrLogPrintStr("\n");
}

/**********************************************************
 *
 *   static ByteStorePtr CkProteinTransl(pp, ibp, bsp, sfp,
 *                                       qval, intercodon,
 *                                       gcode, method):
 *
 *      If bsp != translation's value, then take
 *   translation's value and print out warning message.
 *      If the only diff is start codon and bsp has "M"
 *   take bsp.
 *      If intercodon = TRUE, then no comparison.
 *
 **********************************************************/
static void CkProteinTransl(ParserPtr pp, InfoBioseqPtr ibp, string& prot, CSeq_feat& feat, const char* qval, bool intercodon, const char* gcode, unsigned char* method)
{
    const char* ptr;

    string msg2;
    Int2   residue;
    Int4   num = 0;
    size_t aa;
    bool   msgout = false;
    bool   first  = false;
    Int4   difflen;

    CCdregion& cdregion = feat.SetData().SetCdregion();
    size_t     len      = StringLen(qval);
    msg2.reserve(1100);

    string loc = location_to_string(feat.GetLocation());

    size_t blen = prot.size();

    if (len != blen && (! feat.IsSetExcept() || feat.GetExcept() == false)) {
        ErrPostEx(SEV_ERROR, ERR_CDREGION_ProteinLenDiff, "Lengths of conceptual translation and translation qualifier differ : %d : %d : %s", blen, len, loc.c_str());
    }

    difflen = 0;
    if (! intercodon) {
        if (len != blen)
            difflen = 1;
        if (len > blen)
            difflen = 2;

        size_t residue_idx = 0;
        for (ptr = qval, num = 0, aa = 1; residue_idx < blen; ++aa, ++residue_idx) {
            residue = prot[residue_idx];

            if (aa > len) {
                if (residue == '*')
                    continue;
                difflen = 2;
                break;
            }

            if (residue == *ptr) {
                ptr++;
                continue;
            }

            if (aa == 1 && residue == 'M')
                first = true;

            msgout = true;
            num++;
            if (num == 1)
                msg2 = "at AA # ";

            if (num < 11 && msg2.length() < 1000) {
                stringstream aastr;
                aastr << aa << '(' << (char)residue << ',' << (char)*ptr << "), ";
                msg2 += aastr.str();
            } else if (num == 11 && msg2.length() < 1000)
                msg2 += ", additional details suppressed, ";
            ptr++;
        }

        if (num > 0) {
            cdregion.SetConflict(true);
            stringstream aastr;
            aastr << "using genetic code " << gcode << ", total " << num << " difference";
            if (num > 1)
                aastr << 's'; // plural
            msg2 += aastr.str();
            if (! feat.IsSetExcept() || feat.GetExcept() == false) {
                ErrPostEx(SEV_WARNING, ERR_CDREGION_TranslationDiff, "%s:%s", msg2.c_str(), loc.c_str());
            }
        }

        if (! msgout) {
            cdregion.ResetConflict();
            ErrPostEx(SEV_INFO, ERR_CDREGION_TranslationsAgree, "Parser-generated conceptual translation agrees with input translation %s", loc.c_str());
        }

        if (difflen == 2) {
            cdregion.SetConflict(true);
            msgout = true;
        }
    } /* intercodon */
    else {
        msgout = true;
        if (! feat.IsSetExcept() || feat.GetExcept() == false) {
            ErrPostEx(SEV_WARNING, ERR_CDREGION_NoTranslationCompare, "Conceptual translation has internal stop codons, no comparison: %s", loc.c_str());
        }
    }

    if (msgout) {
        if (pp->debug) {
            ErrByteStorePtr(ibp, feat, prot);
        }
    }

    if (pp->accver == false) {
        if ((num == 1 && first) ||
            (pp->transl && ! prot.empty() && cdregion.IsSetConflict() && cdregion.GetConflict())) {
            cdregion.ResetConflict();
            ErrPostEx(SEV_WARNING, ERR_CDREGION_TranslationOverride, "Input translation is replaced with conceptual translation: %s", loc.c_str());
            *method = eGIBB_method_concept_trans;
        }
    }

    if ((cdregion.IsSetConflict() && cdregion.GetConflict() == true) || difflen != 0)
        *method = eGIBB_method_concept_trans_a;

    if (msgout && pp->transl == false && (! feat.IsSetExcept() || feat.GetExcept() == false)) {
        ErrPostEx(SEV_WARNING, ERR_CDREGION_SuppliedProteinUsed, "In spite of previously reported problems, the supplied protein translation will be used : %s", loc.c_str());
    }

    prot = qval;
}

/**********************************************************
 *
 *   static bool check_translation(bsp, qval):
 *
 *      If bsp != translation's value return FALSE.
 *
 **********************************************************/
static bool check_translation(string& prot, const char* qval)
{
    size_t len  = 0;
    size_t blen = prot.size();

    for (len = StringLen(qval); len != 0; len--) {
        if (qval[len - 1] != 'X') /* remove terminal X */
            break;
    }

    size_t cur_pos = blen;
    for (; cur_pos != 0; --cur_pos) {
        if (prot[cur_pos - 1] != 'X')
            break;
    }

    if (cur_pos != len)
        return false;

    cur_pos = 0;
    for (; *qval != '\0' && cur_pos < blen; qval++, ++cur_pos) {
        if (prot[cur_pos] != *qval)
            return false;
    }

    return true;
}

// Workaround for translation function.
// This is needed because feat->location usually does not have a 'version' in its ID,
// but feat->cdregion->code_break->location has.
// In this case both locations is treated as they are from different sequences, but they definitely
// belong to the same one.
// Therefore, this function changes all IDs of the codebreaks to feat->location->id before translation,
// and return all this stuff back after translation.
// TODO: it probably should be organized in another way
static bool Translate(CSeq_feat& feat, string& prot)
{
    std::list<CRef<CSeq_id>> orig_ids;
    const CSeq_id*           feat_loc_id = nullptr;
    if (feat.IsSetLocation())
        feat_loc_id = feat.GetLocation().GetId();

    bool change = feat_loc_id && feat.GetData().GetCdregion().IsSetCode_break();
    if (change) {
        for (auto& code_break : feat.SetData().SetCdregion().SetCode_break()) {
            orig_ids.push_back(CRef<CSeq_id>(new CSeq_id));
            orig_ids.back()->Assign(*code_break->GetLoc().GetId());
            code_break->SetLoc().SetId(*feat_loc_id);
        }
    }

    bool ret = true;
    try {
        CSeqTranslator::Translate(feat, GetScope(), prot);
    } catch (CSeqMapException& e) {
        ErrPostEx(SEV_REJECT, 0, 0, "%s", e.GetMsg().c_str());
        ret = false;
    }

    if (change) {
        std::list<CRef<CSeq_id>>::iterator it = orig_ids.begin();
        for (auto& code_break : feat.SetData().SetCdregion().SetCode_break()) {
            code_break->SetLoc().SetId(**it);
            ++it;
        }
    }

    return ret;
}

/**********************************************************
 *
 *   static Int2 EndAdded(sfp, gene):
 *
 *      From EndStopCodonBaseAdded:
 *      Return 0 if no bases added, no end stop codon found
 *   after added one or two bases, or can not add
 *   (substract) any bases at end because it is really
 *   a partial.
 *      Return -1 if something bad.
 *      Return +1 if it found end stop codon after bases
 *   added.
 *
 **********************************************************/
static Int2 EndAdded(CSeq_feat& feat, GeneRefFeats& gene_refs)
{
    Int4 pos;
    Int4 pos1;
    Int4 pos2;
    Int4 len;
    Int4 len2;

    Uint4 remainder;

    Uint4 oldfrom;
    Uint4 oldto;

    size_t i;

    string transl;
    string name;

    CCdregion& cdregion = feat.SetData().SetCdregion();
    len                 = sequence::GetLength(feat.GetLocation(), &GetScope());
    len2                = len;

    int frame = cdregion.IsSetFrame() ? cdregion.GetFrame() : 0;
    if (frame > 1 && frame < 4)
        len -= frame - 1;

    remainder = 3 - len % 3;
    if (remainder == 0)
        return (0);

    if (cdregion.IsSetCode_break()) {
        bool ret_condition = false;
        for (const auto& code_break : cdregion.GetCode_break()) {
            pos1 = numeric_limits<int>::max();
            pos2 = -10;

            for (CSeq_loc_CI loc(code_break->GetLoc()); loc; ++loc) {
                pos = sequence::LocationOffset(*loc.GetRangeAsSeq_loc(), feat.GetLocation(), sequence::eOffset_FromStart);
                if (pos < pos1)
                    pos1 = pos;
                pos = sequence::LocationOffset(*loc.GetRangeAsSeq_loc(), feat.GetLocation(), sequence::eOffset_FromEnd);
                if (pos > pos2)
                    pos2 = pos;
            }
            pos = pos2 - pos1;
            if (pos == 2 || (pos >= 0 && pos <= 1 && pos2 == len2 - 1)) {
                ret_condition = true;
                break;
            }
        }

        if (ret_condition)
            return (0);
    }

    CSeq_interval* last_interval = nullptr;
    for (CTypeIterator<CSeq_interval> loc(Begin(feat.SetLocation())); loc; ++loc) {
        last_interval = &(*loc);
    }

    if (! last_interval)
        return (0);

    const CBioseq_Handle& bioseq_h = GetScope().GetBioseqHandle(last_interval->GetId());
    if (bioseq_h.GetState() != CBioseq_Handle::fState_none)
        return (0);

    oldfrom = last_interval->GetFrom();
    oldto   = last_interval->GetTo();

    if (last_interval->IsSetStrand() && last_interval->GetStrand() == eNa_strand_minus) {
        if (last_interval->GetFrom() < remainder || last_interval->IsSetFuzz_from())
            return (0);
        last_interval->SetFrom(oldfrom - remainder);
    } else {
        if (last_interval->GetTo() >= bioseq_h.GetBioseqLength() - remainder || last_interval->IsSetFuzz_to())
            return (0);
        last_interval->SetTo(oldto + remainder);
    }

    string newprot;
    Translate(feat, newprot);

    if (newprot.empty()) {
        last_interval->SetFrom(oldfrom);
        last_interval->SetTo(oldto);
        return (0);
    }

    size_t protlen = newprot.size();
    if (protlen != (size_t)(len + remainder) / 3) {
        last_interval->SetFrom(oldfrom);
        last_interval->SetTo(oldto);
        return (0);
    }

    if (newprot[protlen - 1] != '*') {
        last_interval->SetFrom(oldfrom);
        last_interval->SetTo(oldto);
        return (0);
    }

    transl = CpTheQualValue(feat.GetQual(), "translation");
    if (transl.empty()) {
        last_interval->SetFrom(oldfrom);
        last_interval->SetTo(oldto);
        return (0);
    }

    protlen--;
    newprot = newprot.substr(0, newprot.size() - 1);

    for (i = 0; i < protlen; i++) {
        if (transl[i] != newprot[i])
            break;
    }
    transl.clear();

    if (i < protlen) {
        last_interval->SetFrom(oldfrom);
        last_interval->SetTo(oldto);
        return (0);
    }

    if (! gene_refs.valid)
        return (remainder);

    name = CpTheQualValue(feat.GetQual(), "gene");
    if (name.empty())
        return (remainder);

    for (TSeqFeatList::iterator gene = gene_refs.first; gene != gene_refs.last; ++gene) {
        if (! (*gene)->IsSetData() || ! (*gene)->GetData().IsGene())
            continue;

        int cur_strand  = (*gene)->GetLocation().IsSetStrand() ? (*gene)->GetLocation().GetStrand() : 0,
            last_strand = last_interval->IsSetStrand() ? last_interval->GetStrand() : 0;
        if (cur_strand != last_strand)
            continue;

        const CGene_ref& cur_gene_ref = (*gene)->GetData().GetGene();
        if (NStr::CompareNocase(cur_gene_ref.GetLocus(), name) != 0) {
            if (! cur_gene_ref.IsSetSyn())
                continue;

            bool found = false;
            for (const string& syn : cur_gene_ref.GetSyn()) {
                if (NStr::CompareNocase(name, syn) == 0) {
                    found = true;
                    break;
                }
            }

            if (! found)
                continue;
        }

        for (CTypeIterator<CSeq_interval> loc(Begin((*gene)->SetLocation())); loc; ++loc) {
            if (! loc->GetId().Match(last_interval->GetId()))
                continue;

            int cur_strand = loc->IsSetStrand() ? loc->GetStrand() : 0;
            if (cur_strand == eNa_strand_minus && loc->GetFrom() == oldfrom)
                loc->SetFrom(last_interval->GetFrom());
            else if (cur_strand != eNa_strand_minus && loc->GetTo() == oldto)
                loc->SetTo(last_interval->GetTo());
        }
    }

    return (remainder);
}

/**********************************************************/
static void fta_check_codon_quals(CSeq_feat& feat)
{
    if (! feat.IsSetQual())
        return;

    for (TQualVector::iterator qual = feat.SetQual().begin(); qual != feat.SetQual().end();) {
        if (! (*qual)->IsSetQual() || ! (*qual)->IsSetVal() ||
            (*qual)->GetQual() != "codon") {
            ++qual;
            continue;
        }

        string loc = location_to_string(feat.GetLocation());
        ErrPostEx(SEV_ERROR, ERR_CDREGION_CodonQualifierUsed, "Encountered /codon qualifier for \"CDS\" feature at \"%s\". Code-breaks (/transl_except) should be used instead.", loc.c_str());

        qual = feat.SetQual().erase(qual);
    }

    if (feat.GetQual().empty())
        feat.ResetQual();
}

/**********************************************************
 *
 *   static ByteStorePtr InternalStopCodon(pp, ibp, sfp,
 *                                         method, gene):
 *
 *      Return NULL if there is no protein sequence, or
 *   there is no translation qualifier and protein sequence
 *   has internal stop codon; otherwise, return a protein
 *   sequence.
 *      In the embl format, it may not have "translation"
 *   qualifier, take protein sequence (without
 *   end_stop_codon) instead.
 *
 **********************************************************/
static void InternalStopCodon(ParserPtr pp, InfoBioseqPtr ibp, CSeq_feat& feat, unsigned char* method, Uint1 dif, GeneRefFeats& gene_refs, string& seq_data)
{
    string qval;
    bool  intercodon = false;
    bool  again      = true;

    ErrSev sev;

    Uint1  m = 0;
    string gcode_str;
    string stopmsg;
    size_t protlen;
    Int4   aa;
    Int4   num = 0;
    Int2   residue;
    Int2   r;

    CCdregion& cdregion = feat.SetData().SetCdregion();

    if (! cdregion.IsSetCode())
        return;

    CGenetic_code&      gen_code = cdregion.SetCode();
    CGenetic_code::C_E* cur_code = nullptr;

    for (auto& gcode : gen_code.Set()) {
        if (gcode->IsId()) {
            cur_code = gcode;
            break;
        }
    }

    if (! cur_code)
        return;

    if (pp->no_code) {
        int orig_code_id = cur_code->GetId();
        for (int cur_code_id = cur_code->GetId(); again && cur_code_id < 14; ++cur_code_id) {
            cur_code->SetId(cur_code_id);

            string prot;
            if (! Translate(feat, prot))
                pp->entrylist[pp->curindx]->drop = true;

            if (prot.empty())
                continue;

            protlen = prot.size();
            residue = prot[protlen - 1];

            if (residue == '*')
                prot = prot.substr(0, prot.size() - 1);

            intercodon = prot.find('*') != string::npos;

            if (! intercodon) {
                qval = CpTheQualValue(feat.GetQual(), "translation");
                if (! qval.empty()) /* compare protein sequence */
                {
                    if (check_translation(prot, qval.c_str())) {
                        sev = (pp->taxserver == 0) ? SEV_INFO : SEV_WARNING;
                        ErrPostEx(sev, ERR_CDREGION_GeneticCodeAssumed, "No genetic code from TaxArch, trying to guess, code %d assumed", cur_code_id);
                        again = false;
                    }
                    qval.clear();
                } else
                    break;
            }
        }

        if (again) {
            sev = (pp->taxserver == 0) ? SEV_INFO : SEV_WARNING;
            ErrPostEx(sev, ERR_CDREGION_GeneticCodeAssumed, "Can't guess genetic code, code %d assumed", orig_code_id);
            cur_code->SetId(orig_code_id);
        }
    }

    string prot;
    if (! Translate(feat, prot))
        pp->entrylist[pp->curindx]->drop = true;

    if (cur_code)
        gcode_str = to_string(cur_code->GetId());
    else
        gcode_str = "unknown";

    qval       = CpTheQualValue(feat.GetQual(), "translation");
    intercodon = false;

    string loc = location_to_string(feat.GetLocation());

    if (! prot.empty()) {
        protlen = prot.size();
        residue = prot[protlen - 1];
        if ((! feat.IsSetPartial() || feat.GetPartial() == false) && ! SeqLocHaveFuzz(feat.GetLocation())) {
            CkEndStop(feat, dif);
        }

        if (residue != '*') {
            r = EndAdded(feat, gene_refs);
            if (r > 0 && (! feat.IsSetExcept() || feat.GetExcept() == false)) {
                ErrPostEx(SEV_WARNING, ERR_CDREGION_TerminalStopCodonMissing, "CDS: %s |found end stop codon after %d bases added", loc.c_str(), r);
            }

            if ((! feat.IsSetPartial() || feat.GetPartial() == false) && ! SeqLocHaveFuzz(feat.GetLocation())) {
                /* if there is no partial qualifier and location
                 * doesn't have "fuzz" then output message
                 */
                if (! feat.IsSetExcept() || feat.GetExcept() == false) {
                    ErrPostEx(SEV_ERROR, ERR_CDREGION_TerminalStopCodonMissing, "No end stop codon found for CDS: %s", loc.c_str());
                }
            }
        } else /* remove termination codon from protein */
        {
            check_end_internal(protlen, feat, dif);
            prot = prot.substr(0, prot.size() - 1);
        }

        /* check internal stop codon */
        size_t residue_idx = 0;
        protlen            = prot.size();
        for (stopmsg.reserve(550), aa = 1; residue_idx < protlen; ++residue_idx) {
            residue = prot[residue_idx];
            if (aa == 1 && residue == '-') {
                /* if unrecognized start of translation,
                 * a ncbigap character is inserted
                 */
                if (! feat.IsSetExcept() || feat.GetExcept() == false) {
                    ErrPostEx(SEV_WARNING, ERR_CDREGION_IllegalStart, "unrecognized initiation codon from CDS: %s", loc.c_str());
                }
                if (qval.empty()) /* no /translation */
                {
                    return;
                }
            }

            if (residue == '*') /* only report 10 internal stop codons */
            {
                intercodon = true;
                ++num;

                if (num < 11 && stopmsg.length() < 500) {
                    stopmsg += to_string(aa);
                    stopmsg += ' ';
                } else if (num == 11 && stopmsg.length() < 500) {
                    stopmsg += ", only report 10 positions";
                }
            }

            aa++;
        }

        if (intercodon) {
            if (! feat.IsSetExcept() || feat.GetExcept() == false) {
                ErrPostEx(SEV_ERROR, ERR_CDREGION_InternalStopCodonFound, "Found %d internal stop codon, at AA # %s, on feature key, CDS, frame # %d, genetic code %s:%s", (int)num, stopmsg.c_str(), cdregion.GetFrame(), gcode_str.c_str(), loc.c_str());
            }

            if (pp->debug) {
                ErrByteStorePtr(ibp, feat, prot);
            }
        }
    } else if (! feat.IsSetExcept() || feat.GetExcept() == false) {
        ErrPostEx(SEV_WARNING, ERR_CDREGION_NoProteinSeq, "No protein sequence found:%s", loc.c_str());
    }

    if (! qval.empty()) /* compare protein sequence */
    {
        CkProteinTransl(pp, ibp, prot, feat, qval.c_str(), intercodon, gcode_str.c_str(), &m);
        *method = m;
        seq_data.swap(prot);
        return;
    }

    if (! prot.empty() && ! intercodon) {
        if (prot.size() > 6 || ! check_short_CDS(pp, feat, false)) {
            ErrPostEx(SEV_INFO, ERR_CDREGION_TranslationAdded, "input CDS lacks a translation: %s", loc.c_str());
        }
        *method = eGIBB_method_concept_trans;
        seq_data.swap(prot);
        return;
    }

    /* no translation qual and internal stop codon */
    if (intercodon) {
        cdregion.SetStops(num);
        if (! feat.IsSetExcept() || feat.GetExcept() == false) {
            ErrPostEx(SEV_WARNING, ERR_CDREGION_NoProteinSeq, "internal stop codons, and no translation qualifier CDS:%s", loc.c_str());
        }
    }
}

/**********************************************************/
static void check_gen_code(const char* qval, ProtBlkPtr pbp, Uint1 taxserver)
{
    ErrSev sev;
    Uint1  gcpvalue;
    Uint1  genome;
    Uint1  value;

    if (! pbp || ! pbp->gcode.IsId())
        return;

    gcpvalue = pbp->gcode.GetId();
    value    = (Uint1)atoi(qval);
    genome   = pbp->genome;

    if (value == gcpvalue)
        return;

    if (value == 7 || value == 8) {
        ErrPostEx(SEV_WARNING, ERR_CDREGION_InvalidGcodeTable, "genetic code table is obsolete /transl_table = %d", value);
        pbp->gcode.SetId(pbp->orig_gcode);
        return;
    }

    if (value != 11 || (genome != 2 && genome != 3 && genome != 6 &&
                        genome != 12 && genome != 16 && genome != 17 &&
                        genome != 18 && genome != 22)) {
        sev = (taxserver == 0) ? SEV_INFO : SEV_ERROR;
        ErrPostEx(sev, ERR_CDREGION_GeneticCodeDiff, "Genetic code from Taxonomy server: %d, from /transl_table: %d", gcpvalue, value);
    }

    pbp->gcode.SetId(value);
}

/**********************************************************/
static bool CpGeneticCodePtr(CGenetic_code& code, const CGenetic_code::C_E& gcode)
{
    if (! gcode.IsId())
        return false;

    CRef<CGenetic_code::C_E> ce(new CGenetic_code::C_E);
    ce->SetId(gcode.GetId());
    code.Set().push_back(ce);

    return true;
}

/**********************************************************/
static Int4 IfOnlyStopCodon(const CBioseq& bioseq, const CSeq_feat& feat, bool transl)
{
    Uint1 strand;
    Int4  len;
    Int4  i;

    if (! feat.IsSetLocation() || transl)
        return (0);

    const CSeq_loc& loc   = feat.GetLocation();
    TSeqPos         start = loc.GetStart(eExtreme_Positional),
            stop          = loc.GetStop(eExtreme_Positional) + 1;

    if (start == kInvalidSeqPos || stop == kInvalidSeqPos)
        return (0);

    len = stop - start;
    if (len < 1 || len > 5)
        return (0);

    strand = loc.IsSetStrand() ? loc.GetStrand() : 0;

    string loc_str = location_to_string(loc);
    if (loc_str.empty()) {
        loc_str = "???";
    }
    if ((strand == 2 && stop == bioseq.GetLength()) || (strand != 2 && start == 0)) {
        ErrPostEx(SEV_INFO, ERR_CDREGION_StopCodonOnly, "Assuming coding region at \"%s\" annotates the stop codon of an upstream or downstream coding region.", loc_str.c_str());
        i = 1;
    } else {
        ErrPostEx(SEV_REJECT, ERR_CDREGION_StopCodonBadInterval, "Coding region at \"%s\" appears to annotate a stop codon, but its location does not include a sequence endpoint.", loc_str.c_str());
        i = -1;
    }
    return (i);
}

/**********************************************************/
static void fta_concat_except_text(CSeq_feat& feat, const Char* text)
{
    if (! text)
        return;

    if (feat.IsSetExcept_text()) {
        feat.SetExcept_text() += ", ";
        feat.SetExcept_text() += text;
    } else
        feat.SetExcept_text() = text;
}

/**********************************************************/
static bool fta_check_exception(CSeq_feat& feat, Parser::ESource source)
{
    const char** b;
    ErrSev       sev;
    char*        p;

    if (! feat.IsSetQual())
        return true;

    bool stopped = false;
    for (TQualVector::iterator qual = feat.SetQual().begin(); qual != feat.SetQual().end();) {
        if (! (*qual)->IsSetQual()) {
            ++qual;
            continue;
        }

        if ((*qual)->GetQual() == "ribosomal_slippage")
            p = (char*)"ribosomal slippage";
        else if ((*qual)->GetQual() == "trans_splicing")
            p = (char*)"trans-splicing";
        else
            p = nullptr;

        if (p) {
            feat.SetExcept(true);

            qual = feat.SetQual().erase(qual);
            fta_concat_except_text(feat, p);
            continue;
        }

        if ((*qual)->GetQual() != "exception" || ! (*qual)->IsSetVal()) {
            ++qual;
            continue;
        }

        if (source == Parser::ESource::Refseq)
            b = RSExceptionQualVals;
        else
            b = GBExceptionQualVals;

        const Char* cur_val = (*qual)->GetVal().c_str();
        for (; *b; b++) {
            if (NStr::CompareNocase(*b, cur_val) == 0)
                break;
        }

        if (! *b) {
            sev = (source == Parser::ESource::Refseq) ? SEV_ERROR : SEV_REJECT;

            string loc = location_to_string(feat.GetLocation());
            ErrPostEx(sev, ERR_QUALIFIER_InvalidException, "/exception value \"%s\" on feature \"CDS\" at location \"%s\" is invalid.", cur_val, loc.empty() ? "Unknown" : loc.c_str());
            if (source != Parser::ESource::Refseq) {
                stopped = true;
                break;
            }
        } else {
            feat.SetExcept(true);
            fta_concat_except_text(feat, cur_val);
        }

        qual = feat.SetQual().erase(qual);
    }

    if (feat.GetQual().empty())
        feat.ResetQual();

    if (stopped)
        return false;

    return true;
}

/**********************************************************
 *
 *   static Int2 CkCdRegion(pp, sfp, bsq, num, gene):
 *
 *      Routine returns 0, and
 *   sfp->data.choice = SEQFEAT_IMP :
 *   - if ambiguous frame number and no "codon_start"
 *     qualifier;
 *   - if there is a "pseudo" qualifier;
 *   - if the range of the "transl_except" qualifier is
 *     wrong;
 *      Otherwise return 1, and
 *   sfp->data.choice = SEQFEAT_CDREGION
 *
 **********************************************************/
static Int2 CkCdRegion(ParserPtr pp, CScope& scope, CSeq_feat& cds, CBioseq& bioseq, int* num, GeneRefFeats& gene_refs)
{
    ProtBlkPtr  pbp;
    const char* r;

    optional<string> qval;
    bool  is_pseudo;
    bool  is_stop;
    bool  is_transl;

    ErrSev sev;

    Uint1 method = 0;
    Uint1 dif;
    Uint1 codon_start;
    Int2  frame;
    Int2  i;

    pbp = pp->pbp;
    if (pp->buf)
        MemFree(pp->buf);
    pp->buf = nullptr;

    TCodeBreakList code_breaks;
    GetCdRegionCB(pbp->ibp, cds, code_breaks, &dif, pp->accver);

    is_pseudo = cds.IsSetPseudo() ? cds.GetPseudo() : false;
    is_transl = FindTheQual(cds, "translation");

    CCode_break* first_code_break = nullptr;
    if (! code_breaks.empty())
        first_code_break = *code_breaks.begin();

    if (first_code_break && first_code_break->GetAa().GetNcbieaa() == 42)
        is_stop = true;
    else if (is_pseudo)
        is_stop = false;
    else {
        i = IfOnlyStopCodon(bioseq, cds, is_transl);
        if (i < 0)
            return (-1);
        is_stop = (i != 0);
    }

    if (! is_transl) {
        bool found = false;
        for (const auto& qual : cds.GetQual()) {
            if (! qual->IsSetQual() || ! qual->IsSetVal())
                continue;

            if (qual->GetQual() == "product" ||
                qual->GetQual() == "function" ||
                qual->GetQual() == "EC_number") {
                found = true;
                break;
            }
        }

        if (found) {
            CRef<CSeqFeatXref> xfer(new CSeqFeatXref);
            CProt_ref&         prot_ref = xfer->SetData().SetProt();
            for (const auto& qual : cds.GetQual()) {
                if (! qual->IsSetQual() || ! qual->IsSetVal())
                    continue;

                if (qual->GetQual() == "product")
                    prot_ref.SetName().push_back(qual->GetVal());
                else if (qual->GetQual() == "EC_number")
                    prot_ref.SetEc().push_back(qual->GetVal());
                else if (qual->GetQual() == "function")
                    prot_ref.SetActivity().push_back(qual->GetVal());
            }

            DeleteQual(cds.SetQual(), "product");
            DeleteQual(cds.SetQual(), "EC_number");
            DeleteQual(cds.SetQual(), "function");

            if (cds.GetQual().empty())
                cds.ResetQual();

            cds.SetXref().push_back(xfer);
        }
    }

    if (pp->accver && is_transl && is_pseudo) {
        string loc = location_to_string(cds.GetLocation());
        ErrPostEx(SEV_ERROR, ERR_CDREGION_PseudoWithTranslation, "Coding region flagged as /pseudo has a /translation qualifier : \"%s\".", loc.c_str());
        return (-1);
    }

    if (pp->mode != Parser::EMode::Relaxed &&
        pp->accver && is_transl == false && FindTheQual(cds, "protein_id")) {
        string loc = location_to_string(cds.GetLocation());
        ErrPostEx(SEV_ERROR, ERR_CDREGION_UnexpectedProteinId, "CDS without /translation should not have a /protein_id qualifier. CDS = \"%s\".", loc.c_str());
        return (-1);
    }

    if (pp->mode != Parser::EMode::Relaxed &&
        is_transl == false && is_pseudo == false && is_stop == false) {
        string loc = location_to_string(cds.GetLocation());
        if (pp->accver == false) {
            r = "Feature and protein bioseq";
            i = -2;
        } else {
            r = "Record";
            i = -1;
        }
        sev = (i == -1) ? SEV_REJECT : SEV_ERROR;
        ErrPostEx(sev, ERR_CDREGION_MissingTranslation, "Missing /translation qualifier for CDS \"%s\". %s rejected.", loc.c_str(), r);
        return (i);
    }

    /* check exception qualifier */
    if (! fta_check_exception(cds, pp->source))
        return (-1);

    CRef<CImp_feat> imp_feat(new CImp_feat);
    if (cds.IsSetData() && cds.GetData().IsImp())
        imp_feat->Assign(cds.GetData().GetImp());

    codon_start = 1;
    qval        = GetTheQualValue(cds.SetQual(), "codon_start");

    if (! qval) {
        if (pp->source == Parser::ESource::EMBL)
            frame = 1;
        else {
            frame                       = 0;
            CCdregion::EFrame loc_frame = CCdregion::eFrame_not_set;
            if (CCleanup::SetFrameFromLoc(loc_frame, cds.GetLocation(), scope))
                frame = loc_frame;

            if (frame == 0 && is_pseudo == false) {
                string loc = location_to_string(cds.GetLocation());
                sev        = (pp->source == Parser::ESource::DDBJ) ? SEV_INFO : SEV_ERROR;
                ErrPostEx(sev, ERR_CDREGION_MissingCodonStart, "CDS feature \"%s\" is lacking /codon_start qualifier; assuming frame = 1.", loc.c_str());
                frame = 1;
            }
        }
    } else {
        frame = (Uint1)atoi(qval->c_str());
        qval.reset();
    }

    CRef<CCdregion> cdregion(new CCdregion);

    if (frame > 0)
        cdregion->SetFrame(static_cast<CCdregion::EFrame>(frame));

    qval = GetTheQualValue(cds.SetQual(), "transl_table");

    if (qval) {
        check_gen_code(qval->c_str(), pbp, pp->taxserver);
        pp->no_code = false;
        qval.reset();
    } else if (pbp && pbp->gcode.IsId())
        pbp->gcode.SetId(pbp->orig_gcode);

    if (! code_breaks.empty())
        cdregion->SetCode_break().swap(code_breaks);

    if (! CpGeneticCodePtr(cdregion->SetCode(), pbp->gcode))
        cdregion->ResetCode();

    cds.SetData().SetCdregion(*cdregion);

    if (cds.GetQual().empty())
        cds.ResetQual();

    if (! is_transl) {
        imp_feat.Reset();
        return (0);
    }

    CBioseq::TId ids;
    GetProtRefSeqId(ids, pbp->ibp, num, pp, scope, cds);

    if (! ids.empty())
        fta_check_codon_quals(cds);

    string sequence_data;
    InternalStopCodon(pp, pbp->ibp, cds, &method, dif, gene_refs, sequence_data);

    if (cds.GetQual().empty())
        cds.ResetQual();

    if (cdregion->IsSetConflict() && cdregion->GetConflict() && codon_start == 0) {
        string loc = location_to_string(cds.GetLocation());
        ErrPostEx(SEV_ERROR, ERR_CDREGION_TooBad, "Input translation does not agree with parser generated one, cdregion \"%s\" is lacking /codon_start, frame not set, - so sequence will be rejected.", loc.c_str());
        return (-1);
    }

    if (! sequence_data.empty()) {
        imp_feat.Reset();
        CRef<CBioseq> new_bioseq = BldProtRefSeqEntry(pbp, cds, sequence_data, method, pp, bioseq, ids);

        if (new_bioseq.Empty()) {
            return (-1);
        }

        scope.AddBioseq(*new_bioseq);

        /* remove qualifiers which were processed before */
        DeleteQual(cds.SetQual(), "codon_start");
        DeleteQual(cds.SetQual(), "transl_except");
        DeleteQual(cds.SetQual(), "translation");
        DeleteQual(cds.SetQual(), "protein_id");

        if (cds.GetQual().empty())
            cds.ResetQual();

        if (sequence_data.size() < 6 && pp->accver == false && check_short_CDS(pp, cds, true)) {
            /* make xref from prot-ref for short CDS only */
            if (new_bioseq->IsSetAnnot()) {
                for (const auto& annot : new_bioseq->GetAnnot()) {
                    if (! annot->IsFtable())
                        continue;

                    for (const auto& cur_feat : annot->GetData().GetFtable()) {
                        if (! cur_feat->IsSetData() || ! cur_feat->GetData().IsProt())
                            continue;

                        CRef<CSeqFeatXref> new_xref(new CSeqFeatXref);
                        new_xref->SetData().SetProt().Assign(cur_feat->GetData().GetProt());

                        cds.SetXref().push_back(new_xref);
                    }
                }
            }
            return (0);
        }

        CSeq_id& first_id = *(*new_bioseq->SetId().begin());
        cds.SetProduct().SetWhole(first_id);

        AddProtRefSeqEntry(pbp, *new_bioseq);

        return (1);
    }

    /* no protein sequence, or there is no translation qualifier
     * and protein sequence has internal stop codon
     */

    cds.SetExcept(false);
    if (cds.IsSetExcept_text())
        cds.ResetExcept_text();

    cds.ResetData();
    if (imp_feat.NotEmpty())
        cds.SetData().SetImp(*imp_feat);

    if (! is_pseudo) {
        string loc = location_to_string(cds.GetLocation());
        ErrPostEx(SEV_ERROR, ERR_CDREGION_ConvertToImpFeat, "non-pseudo CDS with data problems is converted to ImpFeat%s", loc.c_str());
    }
    return (0);
}

/**********************************************************
 *
 *   static SeqFeatPtr SrchCdRegion(pp, bsp, sfp, gene):
 *
 *      Return a link list of SeqFeatPtr of type CDREGION.
 *
 **********************************************************/
static void SrchCdRegion(ParserPtr pp, CScope& scope, CBioseq& bioseq, CSeq_annot& annot, GeneRefFeats& gene_refs)
{
    Int4 num = 0;
    Int2 i;

    if (! annot.IsSetData() || ! annot.GetData().IsFtable())
        return;

    for (CSeq_annot::C_Data::TFtable::iterator feat = annot.SetData().SetFtable().begin();
         feat != annot.SetData().SetFtable().end();) {
        if (! (*feat)->IsSetData() || ! (*feat)->GetData().IsImp()) {
            ++feat;
            continue;
        }

        const CImp_feat& imp_feat = (*feat)->GetData().GetImp();
        if (! imp_feat.IsSetKey() || imp_feat.GetKey() != "CDS") {
            ++feat;
            continue;
        }

        /* remove asn2ff_generated comments */
        StripCDSComment(*(*feat));

        const CSeq_loc& loc = (*feat)->GetLocation();
        if (loc.IsEmpty() || loc.IsEquiv() || loc.IsBond()) {
            string loc_str = location_to_string(loc);
            ErrPostEx(SEV_REJECT, ERR_CDREGION_BadLocForTranslation, "Coding region feature has a location that cannot be processed: \"%s\".", loc_str.c_str());
            pp->entrylist[pp->curindx]->drop = true;
            break;
        }

        i = CkCdRegion(pp, scope, *(*feat), bioseq, &num, gene_refs);

        if (i == -2) {
            feat = annot.SetData().SetFtable().erase(feat);
            continue;
        }

        if (i == -1) {
            pp->entrylist[pp->curindx]->drop = true;
            break;
        }

        if (i != 1) {
            ++feat;
            continue;
        }

        /* prepare cdregion to link list, for nuc-prot level */
        pp->pbp->feats.push_back(*feat);

        feat = annot.SetData().SetFtable().erase(feat);
    }
}

/**********************************************************/
static void FindCd(TEntryList& seq_entries, CScope& scope, ParserPtr pp, GeneRefFeats& gene_refs)
{
    ProtBlkPtr pbp = pp->pbp;

    for (auto& entry : seq_entries) {
        for (CTypeIterator<CBioseq_set> bio_set(Begin(*entry)); bio_set; ++bio_set) {
            pbp->segset = true;
            pbp->biosep = entry;
            break;
        }

        if (pbp->segset)
            break;
    }

    for (auto& entry : seq_entries) {
        for (CTypeIterator<CBioseq> bioseq(Begin(*entry)); bioseq; ++bioseq) {
            const CSeq_id& first_id = *(*bioseq->GetId().begin());
            if (IsSegBioseq(first_id))
                continue;

            if (pp->source != Parser::ESource::USPTO)
                CpSeqId(pbp->ibp, first_id);

            if (bioseq->IsSetAnnot()) {
                for (CBioseq::TAnnot::iterator annot = bioseq->SetAnnot().begin(); annot != bioseq->SetAnnot().end();) {
                    if (! (*annot)->IsFtable()) {
                        ++annot;
                        continue;
                    }

                    SrchCdRegion(pp, scope, *bioseq, *(*annot), gene_refs);
                    if (! (*annot)->GetData().GetFtable().empty()) {
                        ++annot;
                        continue;
                    }

                    annot = bioseq->SetAnnot().erase(annot);
                }

                if (bioseq->GetAnnot().empty())
                    bioseq->ResetAnnot();
            }

            if (! pbp->segset) {
                pbp->biosep = entry;
            }
        }
    }
}

/**********************************************************/
static bool check_GIBB(TSeqdescList& descrs)
{
    if (descrs.empty())
        return false;

    const CSeqdesc* descr_modif = nullptr;
    for (const auto& descr : descrs) {
        if (descr->IsModif()) {
            descr_modif = descr;
            break;
        }
    }
    if (! descr_modif)
        return true;

    if (! descr_modif->GetModif().empty()) {
        EGIBB_mod gmod = *descr_modif->GetModif().begin();
        if (gmod == eGIBB_mod_dna || gmod == eGIBB_mod_rna ||
            gmod == eGIBB_mod_est)
            return false;
    }
    return true;
}

/**********************************************************/
static void ValNodeExtractUserObject(TSeqdescList& descrs_from, TSeqdescList& descrs_to, const Char* tag)
{
    for (TSeqdescList::iterator descr = descrs_from.begin(); descr != descrs_from.end();) {
        if ((*descr)->IsUser() && (*descr)->GetUser().IsSetData() &&
            (*descr)->GetUser().IsSetType() && (*descr)->GetUser().GetType().IsStr() &&
            (*descr)->GetUser().GetType().GetStr() == tag) {
            descrs_to.push_back(*descr);
            descr = descrs_from.erase(descr);
            break;
        } else
            ++descr;
    }
}

/**********************************************************/
void ExtractDescrs(TSeqdescList& descrs_from, TSeqdescList& descrs_to, CSeqdesc::E_Choice choice)
{
    for (TSeqdescList::iterator descr = descrs_from.begin(); descr != descrs_from.end();) {
        if ((*descr)->Which() == choice) {
            descrs_to.push_back(*descr);
            descr = descrs_from.erase(descr);
        } else
            ++descr;
    }
}

/**********************************************************/
static void GetBioseqSetDescr(ProtBlkPtr pbp, TSeqdescList& descrs)
{
    TSeqdescList* descrs_from = nullptr;
    if (pbp->segset) {
        if (! pbp->biosep->GetSet().GetDescr().Get().empty())
            descrs_from = &pbp->biosep->SetSet().SetDescr().Set();
    } else {
        if (! pbp->biosep->GetSeq().GetDescr().Get().empty())
            descrs_from = &pbp->biosep->SetSeq().SetDescr().Set();
    }

    if (! descrs_from)
        return;

    ExtractDescrs(*descrs_from, descrs, CSeqdesc::e_Org);

    if (check_GIBB(*descrs_from)) {
        ExtractDescrs(*descrs_from, descrs, CSeqdesc::e_Modif);
    }

    ExtractDescrs(*descrs_from, descrs, CSeqdesc::e_Comment);
    ExtractDescrs(*descrs_from, descrs, CSeqdesc::e_Pub);
    ExtractDescrs(*descrs_from, descrs, CSeqdesc::e_Update_date);

    ValNodeExtractUserObject(*descrs_from, descrs, "GenomeProjectsDB");
    ValNodeExtractUserObject(*descrs_from, descrs, "DBLink");
    ValNodeExtractUserObject(*descrs_from, descrs, "FeatureFetchPolicy");
}

/**********************************************************/
static void BuildProtBioseqSet(ProtBlkPtr pbp, TEntryList& entries)
{
    CRef<CSeq_entry> entry(new CSeq_entry);
    CBioseq_set&     seq_set = entry->SetSet();
    seq_set.SetClass(CBioseq_set::eClass_nuc_prot);

    /* add descr if nuc-prot */
    GetBioseqSetDescr(pbp, seq_set.SetDescr().Set()); /* get from ASN.1 tree */
    if (seq_set.GetDescr().Get().empty())
        seq_set.ResetDescr();

    seq_set.SetSeq_set().splice(seq_set.SetSeq_set().end(), entries);

    CRef<CSeq_annot> annot(new CSeq_annot);

    if (! pbp->feats.empty())
        annot->SetData().SetFtable().swap(pbp->feats);

    seq_set.SetAnnot().push_back(annot);

    entries.push_back(entry);
}

/**********************************************************/
void ProcNucProt(ParserPtr pp, TEntryList& seq_entries, GeneRefFeats& gene_refs)
{
    ProtBlkPtr pbp;
    ErrSev     sev;
    Int4       gcode = 0;

    pbp = pp->pbp;
    ProtBlkInit(pbp);

    GetGcode(seq_entries, pp);

    if (! pbp->gcode.IsId()) {
        gcode       = (pbp->genome == 4 || pbp->genome == 5) ? 2 : 1;
        pp->no_code = true;
        sev         = (pp->taxserver == 0) ? SEV_INFO : SEV_WARNING;
        ErrPostEx(sev, ERR_CDREGION_GeneticCodeAssumed, "No %sgenetic code from TaxArch, code %d assumed", (gcode == 2) ? "mitochondrial " : "", gcode);
        pbp->gcode.SetId(gcode);
        pbp->orig_gcode = gcode;
    }

    FindCd(seq_entries, GetScope(), pp, gene_refs);

    if (pp->entrylist[pp->curindx]->drop) {
        ProtBlkFree(pbp);
        seq_entries.clear();
        return;
    }

    if (! pbp->entries.empty()) {
        seq_entries.splice(seq_entries.end(), pbp->entries);

        BuildProtBioseqSet(pbp, seq_entries);
        AssignBioseqSetLevel(seq_entries);
    }

    ProtBlkFree(pbp);
}

/**********************************************************/
static const CDate* GetDateFromDescrs(const TSeqdescList& descrs, CSeqdesc::E_Choice what)
{
    const CDate* set_date = nullptr;
    for (const auto& descr : descrs) {
        if (descr->Which() == what) {
            if (what == CSeqdesc::e_Create_date)
                set_date = &descr->GetCreate_date();
            else if (what == CSeqdesc::e_Update_date)
                set_date = &descr->GetUpdate_date();

            if (set_date)
                break;
        }
    }

    return set_date;
}

/**********************************************************/
static void FixDupDates(CBioseq_set& bio_set, CSeqdesc::E_Choice what)
{
    if (! bio_set.IsSetSeq_set() || ! bio_set.IsSetDescr())
        return;

    for (auto& entry : bio_set.SetSeq_set()) {
        for (CTypeIterator<CBioseq> bioseq(Begin(*entry)); bioseq; ++bioseq) {
            if (! bioseq->IsSetInst() || ! bioseq->GetInst().IsSetMol() || ! bioseq->GetInst().IsNa() || ! bioseq->IsSetDescr())
                continue;

            const CDate* set_date = GetDateFromDescrs(bio_set.GetDescr().Get(), what);

            TSeqdescList&          cur_descrs = bioseq->SetDescr().Set();
            TSeqdescList::iterator cur_descr  = cur_descrs.begin();

            for (; cur_descr != cur_descrs.end(); ++cur_descr) {
                if ((*cur_descr)->Which() == what)
                    break;
            }

            if (cur_descr == cur_descrs.end())
                continue;

            const CDate* seq_date = nullptr;
            if (what == CSeqdesc::e_Create_date)
                seq_date = &(*cur_descr)->GetCreate_date();
            else if (what == CSeqdesc::e_Update_date)
                seq_date = &(*cur_descr)->GetUpdate_date();

            if (! seq_date)
                continue;

            if (set_date && seq_date->Compare(*set_date) == CDate::eCompare_same)
                cur_descrs.erase(cur_descr);

            if (! set_date) {
                bio_set.SetDescr().Set().push_back(*cur_descr);
                cur_descrs.erase(cur_descr);
            }
        }
    }
}

/**********************************************************/
static void FixCreateDates(CBioseq_set& bio_set)
{
    FixDupDates(bio_set, CSeqdesc::e_Create_date);
}

/**********************************************************/
static void FixUpdateDates(CBioseq_set& bio_set)
{
    FixDupDates(bio_set, CSeqdesc::e_Update_date);
}

/**********************************************************/
static void FixEmblUpdateDates(CBioseq_set& bio_set)
{
    if (! bio_set.IsSetSeq_set() || ! bio_set.IsSetDescr())
        return;

    for (auto& entry : bio_set.SetSeq_set()) {
        for (CTypeIterator<CBioseq> bioseq(Begin(*entry)); bioseq; ++bioseq) {
            if (! bioseq->IsSetInst() || ! bioseq->GetInst().IsSetMol() || ! bioseq->GetInst().IsNa() || ! bioseq->IsSetDescr())
                continue;

            const CDate* set_date = GetDateFromDescrs(bio_set.GetDescr().Get(), CSeqdesc::e_Update_date);

            const CEMBL_block* embl_block = nullptr;
            for (const auto& descr : bioseq->GetDescr().Get()) {
                if (descr->IsEmbl()) {
                    embl_block = &descr->GetEmbl();
                    break;
                }
            }

            const CDate* seq_date = nullptr;
            if (embl_block && embl_block->IsSetUpdate_date())
                seq_date = &embl_block->GetUpdate_date();

            if (! seq_date)
                continue;

            if (set_date && seq_date->Compare(*set_date) == CDate::eCompare_same)
                continue;

            if (! set_date) {
                CRef<CSeqdesc> new_descr(new CSeqdesc);
                new_descr->SetUpdate_date().Assign(*seq_date);
                bio_set.SetDescr().Set().push_back(new_descr);
            }
        }
    }
}

/**********************************************************/
void CheckDupDates(TEntryList& seq_entries)
{
    for (auto& entry : seq_entries) {
        for (CTypeIterator<CBioseq_set> bio_set(Begin(*entry)); bio_set; ++bio_set) {
            if (bio_set->IsSetClass() && bio_set->GetClass() == CBioseq_set::eClass_nuc_prot) {
                FixCreateDates(*bio_set);
                FixUpdateDates(*bio_set);
                FixEmblUpdateDates(*bio_set);
            }
        }
    }
}

END_NCBI_SCOPE
