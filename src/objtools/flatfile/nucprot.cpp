/* nucprot.c
*
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
* File Name:  nucprot.c
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
#    undef THIS_FILE
#endif
#define THIS_FILE "nucprot.cpp"

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

typedef list<CRef<objects::CCode_break> > TCodeBreakList;

const char *GBExceptionQualVals[] = {
    "RNA editing",
    "reasons given in citation",
    "rearrangement required for product",
    "annotated by transcript or proteomic data",
    NULL
};

const char *RSExceptionQualVals[] = {
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
    NULL
};

/**********************************************************
*
*   bool FindTheQual(qlist, qual):
*
*      Finds qual in the "qlist" return TRUE.
*   Otherwise, return FALSE.
*
**********************************************************/
static bool FindTheQual(const objects::CSeq_feat& feat, const Char *qual_to_find)
{
    ITERATE(TQualVector, qual, feat.GetQual())
    {
        if ((*qual)->IsSetQual() && (*qual)->GetQual() == qual_to_find)
            return true;
    }

    return(false);
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
static char* CpTheQualValueNext(TQualVector::iterator& cur_qual, const TQualVector::iterator& end_qual,
                                  const char *qual)
{
    std::string qvalue;

    for (; cur_qual != end_qual; ++cur_qual) {
        if (!(*cur_qual)->IsSetQual() || (*cur_qual)->GetQual() != qual || !(*cur_qual)->IsSetVal())
            continue;

        qvalue = NStr::Sanitize((*cur_qual)->GetVal());

        ++cur_qual;
        break;
    }

    char* ret = NULL;
    if (!qvalue.empty())
        ret = StringSave(qvalue.c_str());

    return ret;
}

/**********************************************************/
static Int4 fta_get_genetic_code(ParserPtr pp)
{
    IndexblkPtr ibp;
    ProtBlkPtr  pbp;
    Int4        gcode;

    if (pp->taxserver != 1)
        return(0);

    ibp = pp->entrylist[pp->curindx];
    if (ibp->gc_genomic < 1 && ibp->gc_mito < 1)
        return(0);

    pbp = pp->pbp;
    gcode = ibp->gc_genomic;
    if (pbp->genome == 4 || pbp->genome == 5)
        gcode = ibp->gc_mito;
    pp->no_code = false;

    return(gcode);
}

/**********************************************************/
static void GuessGeneticCode(ParserPtr pp, const objects::CSeq_descr& descrs)
{
    ProtBlkPtr   pbp;
    Int4         gcode = 0;

    pbp = pp->pbp;

    ITERATE(TSeqdescList, descr, descrs.Get())
    {
        if (!(*descr)->IsModif())
            continue;

        ITERATE(objects::CSeqdesc::TModif, modif, (*descr)->GetModif())
        {
            if (*modif == objects::eGIBB_mod_mitochondrial ||
                *modif == objects::eGIBB_mod_kinetoplast) {
                pbp->genome = 5;        /* mitochondrion */
                break;
            }
        }
        break;
    }

    ITERATE(TSeqdescList, descr, descrs.Get())
    {
        if (!(*descr)->IsSource())
            continue;

        pbp->genome = (*descr)->GetSource().IsSetGenome() ? (*descr)->GetSource().GetGenome() : 0;
        break;
    }

    gcode = fta_get_genetic_code(pp);
    if (gcode <= 0)
        return;

    pbp->orig_gcode = gcode;
    pbp->gcode.SetId(gcode);
}

/**********************************************************/
static void GetGcode(TEntryList& seq_entries, ParserPtr pp)
{
    if (pp != NULL && pp->pbp != NULL && !pp->pbp->gcode.IsId()) {
        ITERATE(TEntryList, entry, seq_entries)
        {
            GuessGeneticCode(pp, GetDescrPointer(*(*entry)));

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
    InfoBioseqFree(pbp->ibp);
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
        ibp->locus = NULL;
        ibp->acnum = NULL;
    }
}

/**********************************************************/
static void AssignBioseqSetLevel(TEntryList& seq_entries)
{

    NON_CONST_ITERATE(TEntryList, entry, seq_entries)
    {
        for (CTypeIterator<objects::CBioseq_set> bio_set(Begin(*(*entry))); bio_set; ++bio_set) {
            switch (bio_set->GetClass()) {
            case objects::CBioseq_set::eClass_nuc_prot:
                bio_set->SetLevel(1);
                break;
            case objects::CBioseq_set::eClass_segset:
                bio_set->SetLevel(2);
                break;
            case objects::CBioseq_set::eClass_parts:
                bio_set->SetLevel(3);
                break;
            default:
                ErrPostEx(SEV_INFO, ERR_BIOSEQSETCLASS_NewClass,
                          "BioseqSeq class %d not handled", (int)bio_set->GetClass());
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
static bool check_short_CDS(ParserPtr pp, const objects::CSeq_feat& feat, bool err_msg)
{
    const objects::CSeq_interval& interval = feat.GetLocation().GetInt();
    if (interval.GetFrom() == 0 || interval.GetTo() == (TSeqPos)(pp->entrylist[pp->curindx]->bases) - 1)
        return true;

    if (err_msg) {
        char* p = location_to_string(feat.GetLocation());
        ErrPostEx(SEV_WARNING, ERR_CDREGION_ShortProtein,
                  "Short CDS (< 6 aa) located in the middle of the sequence: %s",
                  (p == NULL) ? "" : p);
        MemFree(p);
    }
    return false;
}

/**********************************************************/
static void GetProtRefSeqId(objects::CBioseq::TId& ids, InfoBioseqPtr ibp, int* num,
                            ParserPtr pp, CScope& scope, objects::CSeq_feat& cds)
{
    const char   *r;

    char*      protacc;
    char*      p;
    char*      q;
    ErrSev       sev;
    Uint1        cho;
    Uint1        ncho;
    Char         str[100];

    if (pp->mode == Parser::EMode::Relaxed) {
        protacc = CpTheQualValue(cds.SetQual(), "protein_id");
        if (protacc == NULL || *protacc == '\0') {
            if (protacc)
                MemFree(protacc);
            int protein_id_counter=0;
            string idLabel;
            auto pProteinId =
                edit::GetNewProtId(scope.GetBioseqHandle(cds.GetLocation()), protein_id_counter, idLabel, false);
            cds.SetProduct().SetWhole().Assign(*pProteinId);
            ids.push_back(pProteinId);
            return;
        }
        CSeq_id::ParseIDs(ids, protacc);
        MemFree(protacc);
        return; 
    }

    if(pp->source == Parser::ESource::USPTO)
    {
        protacc = CpTheQualValue(cds.SetQual(), "protein_id");
        CRef<objects::CSeq_id> pat_seq_id(new objects::CSeq_id);
        CRef<objects::CPatent_seq_id> pat_id = MakeUsptoPatSeqId(protacc);
        pat_seq_id->SetPatent(*pat_id);
        ids.push_back(pat_seq_id);
        return;
    }

    const objects::CTextseq_id* text_id = nullptr;
    ITERATE(TSeqIdList, id, ibp->ids)
    {
        if (!(*id)->IsPatent()) {
            text_id = (*id)->GetTextseq_Id();
            break;
        }
    }

    if (text_id == nullptr)
        return;

    if (pp->accver == false || (pp->source != Parser::ESource::EMBL &&
        pp->source != Parser::ESource::NCBI && pp->source != Parser::ESource::DDBJ)) {
        ++(*num);
        sprintf(str, "%d", (int)*num);

        CRef<objects::CSeq_id> seq_id(new objects::CSeq_id);
        std::string& obj_id_str = seq_id->SetLocal().SetStr();
        obj_id_str = text_id->GetAccession();
        obj_id_str += "_";
        obj_id_str += str;
        ids.push_back(seq_id);
        return;
    }

    protacc = CpTheQualValue(cds.SetQual(), "protein_id");
    if (protacc == NULL || *protacc == '\0') {
        p = location_to_string(cds.GetLocation());
        ErrPostEx(SEV_FATAL, ERR_CDREGION_MissingProteinId,
                  "/protein_id qualifier is missing for CDS feature: \"%s\".",
                  (p == NULL) ? "" : p);
        if (p != NULL)
            MemFree(p);
        if (protacc != NULL)
            MemFree(protacc);

        return;
    }

    if (pp->mode == Parser::EMode::HTGSCON) {
        MemFree(protacc);
        ++(*num);
        sprintf(str, "%d", (int)*num);

        CRef<objects::CSeq_id> seq_id(new objects::CSeq_id);
        std::string& obj_id_str = seq_id->SetLocal().SetStr();
        obj_id_str = text_id->GetAccession();
        obj_id_str += "_";
        obj_id_str += str;
        ids.push_back(seq_id);
        return;
    }

    p = StringChr(protacc, '.');
    if (p == NULL || *(p + 1) == '\0') {
        p = location_to_string(cds.GetLocation());
        ErrPostEx(SEV_FATAL, ERR_CDREGION_MissingProteinVersion,
                  "/protein_id qualifier has missing version for CDS feature: \"%s\".",
                  (p == NULL) ? "" : p);
        if (p != NULL)
            MemFree(p);
        MemFree(protacc);
        return;
    }

    for (q = p + 1; *q >= '0' && *q <= '9';)
        q++;
    if (*q != '\0') {
        p = location_to_string(cds.GetLocation());
        ErrPostEx(SEV_FATAL, ERR_CDREGION_IncorrectProteinVersion,
                  "/protein_id qualifier \"%s\" has incorrect version for CDS feature: \"%s\".",
                  protacc, (p == NULL) ? "" : p);
        if (p != NULL)
            MemFree(p);
        MemFree(protacc);
        return;
    }
    q = p + 1;
    *p = '\0';
    cho = GetProtAccOwner(protacc);
    if (cho == 0) {
        p = location_to_string(cds.GetLocation());
        ErrPostEx(SEV_FATAL, ERR_CDREGION_IncorrectProteinAccession,
                  "/protein_id qualifier has incorrect accession \"%s\" for CDS feature: \"%s\".",
                  protacc, (p == NULL) ? "" : p);
        if (p != NULL)
            MemFree(p);
        MemFree(protacc);
        return;
    }

    r = NULL;
    ncho = cho;
    if (pp->source == Parser::ESource::EMBL && cho != objects::CSeq_id::e_Embl && cho != objects::CSeq_id::e_Tpe)
        r = "EMBL";
    else if (pp->source == Parser::ESource::DDBJ && cho != objects::CSeq_id::e_Ddbj &&
             cho != objects::CSeq_id::e_Tpd)
             r = "DDBJ";
    else if (pp->source == Parser::ESource::NCBI && cho != objects::CSeq_id::e_Genbank &&
             cho != objects::CSeq_id::e_Tpg)
             r = "NCBI";
    else {
        ncho = GetNucAccOwner(text_id->GetAccession().c_str(), pp->entrylist[pp->curindx]->is_tpa);
        if (ncho == objects::CSeq_id::e_Tpe && cho == objects::CSeq_id::e_Embl)
            cho = ncho;
    }

    if (r != NULL || ncho != cho) {
        p = location_to_string(cds.GetLocation());
        if (pp->ign_prot_src == false)
            sev = SEV_FATAL;
        else
            sev = SEV_WARNING;
        if (r != NULL)
            ErrPostEx(sev, ERR_CDREGION_IncorrectProteinAccession,
            "/protein_id qualifier has incorrect accession prefix \"%s\" for source %s for CDS feature: \"%s\".",
            protacc, r, (p == NULL) ? "" : p);
        else
            ErrPostEx(sev, ERR_CDREGION_IncorrectProteinAccession,
            "Found mismatching TPA and non-TPA nucleotides's and protein's accessions in one nuc-prot set. Nuc = \"%s\", prot = \"%s\".",
            text_id->GetAccession().c_str(), protacc);
        if (p != NULL)
            MemFree(p);
        if (pp->ign_prot_src == false) {
            MemFree(protacc);
            return;
        }
    }

    CRef<objects::CSeq_id> seq_id(new objects::CSeq_id);

    CRef<objects::CTextseq_id> new_text_id(new objects::CTextseq_id);
    new_text_id->SetAccession(protacc);
    new_text_id->SetVersion(atoi(q));
    SetTextId(cho, *seq_id, *new_text_id);

    ids.push_back(seq_id);

    if ((pp->source != Parser::ESource::DDBJ && pp->source != Parser::ESource::EMBL) ||
        pp->entrylist[pp->curindx]->is_wgs == false ||
        StringLen(ibp->acnum) == 8) {
        return;
    }

    seq_id.Reset(new objects::CSeq_id);
    seq_id->SetGeneral().SetTag().SetStr(protacc);

    std::string& db = seq_id->SetGeneral().SetDb();
    if (pp->entrylist[pp->curindx]->is_tsa != false)
        db = "TSA:";
    else if (pp->entrylist[pp->curindx]->is_tls != false)
        db = "TLS:";
    else
        db = "WGS:";

    db.append(ibp->acnum, ibp->acnum + 4);
    ids.push_back(seq_id);
}

/**********************************************************/
static char* stripStr(char* base, char* str)
{
    char* bptr;
    char* eptr;

    if (base == NULL || str == NULL)
        return(NULL);
    bptr = StringStr(base, str);
    if (bptr != NULL) {
        eptr = bptr + StringLen(str);
        fta_StringCpy(bptr, eptr);
    }

    return(base);
}

/**********************************************************/
static void StripCDSComment(objects::CSeq_feat& feat)
{
    static const char *strA[] = {
        "Author-given protein sequence is in conflict with the conceptual translation.",
        "direct peptide sequencing.",
        "Method: conceptual translation with partial peptide sequencing.",
        "Method: sequenced peptide, ordered by overlap.",
        "Method: sequenced peptide, ordered by homology.",
        "Method: conceptual translation supplied by author.",
        NULL
    };
    const char        **b;
    char*           pchComment;
    char*           comment;

    if (!feat.IsSetComment())
        return;

    comment = StringSave(feat.GetComment().c_str());
    pchComment = tata_save(comment);
    if (comment != NULL && comment[0] != 0)
        feat.SetComment(comment);
    else
        feat.ResetComment();

    MemFree(comment);

    for (b = strA; *b != NULL; b++) {
        pchComment = stripStr(pchComment, (char*)*b);
    }
    comment = tata_save(pchComment);
    if (comment != NULL && *comment != '\0')
        ShrinkSpaces(comment);
    MemFree(pchComment);

    if (comment != NULL && comment[0] != 0)
        feat.SetComment(comment);
    else
        feat.ResetComment();

    MemFree(comment);
}

/**********************************************************
*
*   static SeqAnnotPtr GetProtRefAnnot(ibp, sfp, seqid,
*                                      length):
*
*      "product" qualifier ==> prp->name.
*      "note" or "gene" or "standard_name" or
*   "label" ==> prp->desc.
*      "EC_number" qualifier ==> a ValNodePtr in
*   ProtRefPtr, prp->ec.
*      "function" qualifier ==> a ValNodePtr in
*   ProtRefPtr, prp->activity.
*
**********************************************************/
static void GetProtRefAnnot(InfoBioseqPtr ibp, objects::CSeq_feat& feat,
                            objects::CBioseq& bioseq)
{
    char*     qval;
    char*     prid;
    bool     partial5;
    bool     partial3;

    std::set<string> names;

    for (;;) {
        qval = GetTheQualValue(feat.SetQual(), "product");
        if (qval == NULL)
            break;

        std::string qval_str = qval;
        MemFree(qval);
        qval = NULL;

        if (qval_str[0] == '\'')
            qval_str = qval_str.substr(1, qval_str.size() - 2);

        names.insert(qval_str);
    }

    if (names.empty()) {
        qval = CpTheQualValue(feat.GetQual(), "gene");
        if (qval == NULL)
            qval = CpTheQualValue(feat.GetQual(), "standard_name");
        if (qval == NULL)
            qval = CpTheQualValue(feat.GetQual(), "label");
    }

    CRef<objects::CProt_ref> prot_ref(new objects::CProt_ref);

    if (names.empty() && qval == NULL) {
        prid = CpTheQualValue(feat.GetQual(), "protein_id");
        qval = location_to_string(feat.GetLocation());
        if (prid != NULL) {
            ErrPostEx(SEV_WARNING, ERR_PROTREF_NoNameForProtein,
                      "No product, gene, or standard_name qualifier found for protein \"%s\" on CDS:%s",
                      prid, (qval == NULL) ? "" : qval);
            MemFree(prid);
        }
        else
            ErrPostEx(SEV_WARNING, ERR_PROTREF_NoNameForProtein,
            "No product, gene, or standard_name qualifier found on CDS:%s",
            (qval == NULL) ? "" : qval);
        MemFree(qval);
        prot_ref->SetDesc("unnamed protein product");
    }
    else {
        if (!names.empty()) {
            prot_ref->SetName().clear();
            std::copy(names.begin(), names.end(), std::back_inserter(prot_ref->SetName()));
            names.clear();
        }
        else
            prot_ref->SetDesc(qval);
    }

    while ((qval = GetTheQualValue(feat.SetQual(), "EC_number")) != NULL) {
        prot_ref->SetEc().push_back(qval);
    }

    while ((qval = GetTheQualValue(feat.SetQual(), "function")) != NULL) {
        prot_ref->SetActivity().push_back(qval);
    }

    if (feat.GetQual().empty())
        feat.ResetQual();

    CRef<objects::CSeq_feat> feat_prot(new objects::CSeq_feat);
    feat_prot->SetData().SetProt(*prot_ref);
    feat_prot->SetLocation(*fta_get_seqloc_int_whole(*bioseq.SetId().front(), bioseq.GetLength()));

    if (feat.IsSetPartial())
        feat_prot->SetPartial(feat.GetPartial());

    partial5 = feat.GetLocation().IsPartialStart(objects::eExtreme_Biological);
    partial3 = feat.GetLocation().IsPartialStop(objects::eExtreme_Biological);

    if (partial5 || partial3) {
        objects::CSeq_interval& interval = feat_prot->SetLocation().SetInt();

        if (partial5) {
            interval.SetFuzz_from().SetLim(objects::CInt_fuzz::eLim_lt);
        }

        if (partial3) {
            interval.SetFuzz_to().SetLim(objects::CInt_fuzz::eLim_gt);
        }
    }

    CRef<objects::CSeq_annot> annot(new objects::CSeq_annot);
    annot->SetData().SetFtable().push_back(feat_prot);

    bioseq.SetAnnot().push_back(annot);
}

/**********************************************************/
static void GetProtRefDescr(objects::CSeq_feat& feat, Uint1 method, const objects::CBioseq& bioseq, TSeqdescList& descrs)
{
    char*      p;
    char*      q;
    bool      partial5;
    bool      partial3;
    Int4         diff_lowest;
    Int4         diff_current;
    Int4         cdslen;
    Int4         orglen;
    Uint1        strand;

    strand = feat.GetLocation().GetStrand();

    std::string organism;

    ITERATE(TSeqdescList, desc, bioseq.GetDescr().Get())
    {
        if (!(*desc)->IsSource())
            continue;

        const objects::CBioSource& source = (*desc)->GetSource();
        if (source.IsSetOrg() && source.GetOrg().IsSetTaxname()) {
            organism = source.GetOrg().GetTaxname();
            break;
        }
    }

    if (!fta_if_special_org(organism.c_str())) {
        diff_lowest = -1;
        cdslen = objects::sequence::GetLength(feat.GetLocation(), &GetScope());

        ITERATE(objects::CBioseq::TAnnot, annot, bioseq.GetAnnot())
        {
            if (!(*annot)->IsFtable())
                continue;

            bool found = false;
            ITERATE(TSeqFeatList, cur_feat, (*annot)->GetData().GetFtable())
            {
                if (!(*cur_feat)->IsSetData() || !(*cur_feat)->GetData().IsBiosrc())
                    continue;

                orglen = objects::sequence::GetLength((*cur_feat)->GetLocation(), &GetScope());

                const objects::CBioSource& source = (*cur_feat)->GetData().GetBiosrc();
                if (!source.IsSetOrg() || !source.GetOrg().IsSetTaxname() ||
                    strand != (*cur_feat)->GetLocation().GetStrand())
                    continue;

                objects::sequence::ECompare cmp_res = objects::sequence::Compare(feat.GetLocation(), (*cur_feat)->GetLocation(), nullptr, objects::sequence::fCompareOverlapping);
                if (cmp_res == objects::sequence::eNoOverlap)
                    continue;

                if (cmp_res == objects::sequence::eSame) {
                    organism = source.GetOrg().GetTaxname();
                    break;
                }

                if (cmp_res == objects::sequence::eContained) {
                    diff_current = orglen - cdslen;
                    if (diff_lowest == -1 || diff_current < diff_lowest) {
                        diff_lowest = diff_current;
                        organism = source.GetOrg().GetTaxname();
                    }
                }
                else if (cmp_res == objects::sequence::eOverlap && diff_lowest < 0)
                    organism = source.GetOrg().GetTaxname();
            }

            if (found)
                break;
        }
    }

    CRef<objects::CMolInfo> mol_info(new objects::CMolInfo);
    mol_info->SetBiomol(objects::CMolInfo::eBiomol_peptide);             /* peptide */

    partial5 = feat.GetLocation().IsPartialStart(objects::eExtreme_Biological);
    partial3 = feat.GetLocation().IsPartialStop(objects::eExtreme_Biological);

    if (partial5 && partial3)
        mol_info->SetCompleteness(objects::CMolInfo::eCompleteness_no_ends);
    else if (partial5)
        mol_info->SetCompleteness(objects::CMolInfo::eCompleteness_no_left);
    else if (partial3)
        mol_info->SetCompleteness(objects::CMolInfo::eCompleteness_no_right);
    else if (feat.IsSetPartial() && feat.GetPartial())
        mol_info->SetCompleteness(objects::CMolInfo::eCompleteness_partial);

    if (method == objects::eGIBB_method_concept_trans_a)
        mol_info->SetTech(objects::CMolInfo::eTech_concept_trans_a);
    else if (method == objects::eGIBB_method_concept_trans)
        mol_info->SetTech(objects::CMolInfo::eTech_concept_trans);

    CRef<objects::CSeqdesc> descr(new objects::CSeqdesc);
    descr->SetMolinfo(*mol_info);
    descrs.push_back(descr);

    p = NULL;
    ITERATE(TQualVector, qual, feat.GetQual())
    {
        if ((*qual)->GetQual() != "product")
            continue;

        const std::string& val_str = (*qual)->GetVal();
        if (p == NULL) {
            p = StringSave((*qual)->GetVal().c_str());
            continue;
        }

        q = (char*)MemNew(StringLen(p) + val_str.size() + 3);
        StringCpy(q, p);
        StringCat(q, "; ");
        StringCat(q, val_str.c_str());
        MemFree(p);
        p = q;
    }

    if (p == NULL)
        p = CpTheQualValue(feat.GetQual(), "gene");

    if (p == NULL)
        p = CpTheQualValue(feat.GetQual(), "label");

    if (p == NULL)
        p = CpTheQualValue(feat.GetQual(), "standard_name");

    if (p == NULL)
        p = StringSave("unnamed protein product");
    else {
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

    if (StringLen(p) < 511 && !organism.empty() && StringStr(p, organism.c_str()) == NULL) {
        q = (char*)MemNew(StringLen(p) + organism.size() + 4);
        StringCpy(q, p);
        StringCat(q, " [");
        StringCat(q, organism.c_str());
        StringCat(q, "]");
        MemFree(p);
        p = q;
    }

    if (StringLen(p) > 511) {
        p[510] = '>';
        p[511] = '\0';
        q = StringSave(p);
        MemFree(p);
    }
    else
        q = p;

    descr.Reset(new objects::CSeqdesc);
    descr->SetTitle(q);
    descrs.push_back(descr);
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
static void QualsToSeqID(objects::CSeq_feat& feat, Parser::ESource source, TSeqIdList& ids)
{
    char*   p;

    if (!feat.IsSetQual())
        return;

    for (TQualVector::iterator qual = feat.SetQual().begin(); qual != feat.SetQual().end();) {
        if ((*qual)->GetQual() != "db_xref") {
            ++qual;
            continue;
        }

        CRef<objects::CSeq_id> seq_id;
        p = StringIStr((*qual)->GetVal().c_str(), "pid:");
        if (p != NULL)
            seq_id = StrToSeqId(p + 4, true);

        if (seq_id.Empty()) {
            ErrPostEx(SEV_ERROR, ERR_CDREGION_InvalidDb_xref,
                      "Invalid data format /db_xref = \"%s\", ignore it",
                      (*qual)->GetVal().c_str());
        }
        else {
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
    bool     abGenBanks[3] = { false, false, false };
    Int2     num;
    Char     ch;

    if (ids.empty())
        return;

    ch = '\0';
    for (TSeqIdList::iterator id = ids.begin(); id != ids.end();) {
        num = -1;
        const Char* dbtag_str = (*id)->IsGeneral() ? (*id)->GetGeneral().GetTag().GetStr().c_str() : "";
        if ((*id)->IsGi()) {
            num = 1;
            ch = 'g';
        }
        else if (*dbtag_str == 'e') {
            num = 0;
            ch = 'e';
        }
        else if (*dbtag_str == 'd') {
            num = 2;
            ch = 'd';
        }
        if (num == -1)
            continue;

        if (abGenBanks[num]) {
            /* PID of this type already exist, ignore it
            */
            ErrPostEx(SEV_WARNING, ERR_CDREGION_Multiple_PID,
                      "/db_xref=\"pid:%c%i\" refer the same data base",
                      ch, (*id)->GetGeneral().GetTag().GetId());

            id = ids.erase(id);
        }
        else {
            abGenBanks[num] = true;
            ++id;
        }
    }
}

/**********************************************************/
static void DbxrefToSeqID(objects::CSeq_feat& feat, Parser::ESource source, TSeqIdList& ids)
{
    if (!feat.IsSetDbxref())
        return;

    for (objects::CSeq_feat::TDbxref::iterator xref = feat.SetDbxref().begin(); xref != feat.SetDbxref().end();) {
        if (!(*xref)->IsSetTag() || !(*xref)->IsSetDb()) {
            ++xref;
            continue;
        }

        CRef<objects::CSeq_id> id;

        if ((*xref)->GetDb() == "PID") {
            const Char* tag_str = (*xref)->GetTag().GetStr().c_str();
            switch (tag_str[0]) {
            case 'g':
                if (source != Parser::ESource::DDBJ && source != Parser::ESource::EMBL) {
                    id.Reset(new objects::CSeq_id);
                    id->SetGi(GI_FROM(long long, strtoll(tag_str + 1, NULL, 10)));
                }
                break;

            case 'd':
            case 'e':
                id.Reset(new objects::CSeq_id);
                id->SetGeneral(*(*xref));
                break;

            default:
                break;
            }

            xref = feat.SetDbxref().erase(xref);
        }
        else
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
static void ProcessForDbxref(objects::CBioseq& bioseq, objects::CSeq_feat& feat,
                             Parser::ESource source)
{
    TSeqIdList ids;
    QualsToSeqID(feat, source, ids);
    if (!ids.empty()) {
        ValidateQualSeqId(ids);
        if (!ids.empty()) {
            bioseq.SetId().splice(bioseq.SetId().end(), ids);
            return;
        }
    }

    DbxrefToSeqID(feat, source, ids);
    if (feat.IsSetComment() && feat.GetComment().empty())
        feat.ResetComment();

    if (!ids.empty())
        bioseq.SetId().splice(bioseq.SetId().end(), ids);
}

/**********************************************************/
static CRef<objects::CBioseq> BldProtRefSeqEntry(ProtBlkPtr pbp, objects::CSeq_feat& feat,
                                                             std::string& seq_data, Uint1 method,
                                                             ParserPtr pp, const objects::CBioseq& bioseq,
                                                             objects::CBioseq::TId& ids)
{
    CRef<objects::CBioseq> new_bioseq;

    if (ids.empty())
        return new_bioseq;

    new_bioseq.Reset(new objects::CBioseq);

    new_bioseq->SetId().swap(ids);

    ProcessForDbxref(*new_bioseq, feat, pp->source);

    new_bioseq->SetInst().SetLength(static_cast<TSeqPos>(seq_data.size()));

    GetProtRefDescr(feat, method, bioseq, new_bioseq->SetDescr());
    GetProtRefAnnot(pbp->ibp, feat, *new_bioseq);

    new_bioseq->SetInst().SetRepr(objects::CSeq_inst::eRepr_raw);
    new_bioseq->SetInst().SetMol(objects::CSeq_inst::eMol_aa);

    /* Seq_code always ncbieaa  08.08.96
    */
    CRef<objects::CSeq_data> data(new objects::CSeq_data(seq_data, objects::CSeq_data::e_Ncbieaa));
    new_bioseq->SetInst().SetSeq_data(*data);

    return new_bioseq;
}

/**********************************************************/
static void AddProtRefSeqEntry(ProtBlkPtr pbp, objects::CBioseq& bioseq)
{
    CRef<objects::CSeq_entry> entry(new objects::CSeq_entry);
    entry->SetSeq(bioseq);
    pbp->entries.push_back(entry);
}

/**********************************************************/
static char* SimpleValuePos(char* qval)
{
    char* bptr;
    char* eptr;

    bptr = StringStr(qval, "(pos:");
    if (bptr == NULL)
        return(NULL);

    bptr += 5;
    while (*bptr == ' ')
        bptr++;
    for (eptr = bptr; *eptr != ',' && *eptr != '\0';)
        eptr++;

    return StringSave(std::string(bptr, eptr).c_str());
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
static void GetCdRegionCB(InfoBioseqPtr ibp, objects::CSeq_feat& feat,
                          TCodeBreakList& code_breaks, unsigned char* dif, bool accver)
{
    Int4 feat_start = -1;
    Int4 feat_stop = -1;

    if (feat.IsSetLocation()) {
        feat_start = feat.GetLocation().GetStart(objects::eExtreme_Positional);
        feat_stop = feat.GetLocation().GetStop(objects::eExtreme_Positional);
    }

    Uint1 res = 2;

    if (feat.IsSetQual()) {
        TQualVector::iterator cur_qual = feat.SetQual().begin(),
            end_qual = feat.SetQual().end();

        char* qval = NULL;
        while ((qval = CpTheQualValueNext(cur_qual, end_qual, "transl_except")) != NULL) {
            CRef<objects::CCode_break> code_break(new objects::CCode_break);

            int ncbieaa_val = GetQualValueAa(qval, false);

            code_break->SetAa().SetNcbieaa(ncbieaa_val);

            char* pos = SimpleValuePos(qval);

            int num_errs = 0;
            bool locmap = false,
                sitesmap = false;

            CRef<objects::CSeq_loc> location = xgbparseint_ver(pos, locmap, sitesmap, num_errs, ibp->ids, accver);
            if (location.NotEmpty())
                code_break->SetLoc(*location);

            Int4 start = code_break->IsSetLoc() ? code_break->GetLoc().GetStart(objects::eExtreme_Positional) : -1;
            Int4 stop = code_break->IsSetLoc() ? code_break->GetLoc().GetStop(objects::eExtreme_Positional) : -1;

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
                pos_range = !itis;

            if (num_errs > 0 || pos_range) {
                ErrPostEx(SEV_WARNING, ERR_FEATURE_LocationParsing,
                          "transl_except range is wrong, %s, drop the transl_except",
                          pos);
                MemFree(pos);
                MemFree(qval);
                break;
            }

            if (code_break->IsSetLoc()) {
                if (feat.GetLocation().IsSetStrand())
                    code_break->SetLoc().SetStrand(feat.GetLocation().GetStrand());

                objects::sequence::ECompare cmp_res = objects::sequence::Compare(feat.GetLocation(), code_break->GetLoc(), nullptr, objects::sequence::fCompareOverlapping);
                if (cmp_res != objects::sequence::eContains) {
                    ErrPostEx(SEV_WARNING, ERR_FEATURE_LocationParsing,
                              "/transl_except not in CDS: %s", qval);
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
static void CkEndStop(const objects::CSeq_feat& feat, Uint1 dif)
{
    Int4        len;
    Int4        r;
    Int4        frm;
    char*     p;

    len = objects::sequence::GetLength(feat.GetLocation(), &GetScope());
    const objects::CCdregion& cdregion = feat.GetData().GetCdregion();

    if (!cdregion.IsSetFrame() || cdregion.GetFrame() == 0)
        frm = 0;
    else
        frm = cdregion.GetFrame() - 1;

    r = (len - frm + (Int4)dif) % 3;
    if (r != 0 && (!feat.IsSetExcept() || feat.GetExcept() == false)) {
        p = location_to_string(feat.GetLocation());
        ErrPostEx(SEV_WARNING, ERR_CDREGION_UnevenLocation,
                  "CDS: %s. Length is not divisable by 3, the remain is: %d",
                  (p == NULL) ? "" : p, r);
        MemFree(p);
    }
}

/**********************************************************/
static void check_end_internal(size_t protlen, const objects::CSeq_feat& feat, Uint1 dif)
{
    Int4        frm;

    const objects::CCdregion& cdregion = feat.GetData().GetCdregion();

    if (!cdregion.IsSetFrame() || cdregion.GetFrame() == 0)
        frm = 0;
    else
        frm = cdregion.GetFrame() - 1;

    size_t len = objects::sequence::GetLength(feat.GetLocation(), &GetScope()) - frm + dif;

    if (protlen * 3 != len && (!feat.IsSetExcept() || feat.GetExcept() == false)) {
        char* p = location_to_string(feat.GetLocation());
        ErrPostEx(SEV_WARNING, ERR_CDREGION_LocationLength,
                  "Length of the CDS: %s (%ld) disagree with the length calculated from the protein: %ld",
                  (p == NULL) ? "" : p, len, protlen * 3);
        MemFree(p);
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
static void ErrByteStorePtr(InfoBioseqPtr ibp, const objects::CSeq_feat& feat,
                            const std::string& prot)
{
    char* str;
    char* qval;

    qval = CpTheQualValue(feat.GetQual(), "translation");
    if (qval == NULL)
        qval = StringSave("no translation qualifier");

    if (!feat.IsSetExcept() || feat.GetExcept() == false) {
        str = location_to_string(feat.GetLocation());
        ErrPostEx(SEV_WARNING, ERR_CDREGION_TranslationDiff,
                  "Location: %s, translation: %s",
                  (str == NULL) ? "" : str, qval);
        MemFree(str);
    }

    MemFree(qval);

    ErrLogPrintStr(prot.c_str());
    ErrLogPrintStr((char *) "\n");
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
static void CkProteinTransl(ParserPtr pp, InfoBioseqPtr ibp,
                            std::string& prot, objects::CSeq_feat& feat,
                            char* qval, bool intercodon,
                            char* gcode, unsigned char* method)
{
    char*      ptr;
    Char         msg2[1100];
    Char         aastr[100];
    char*      msg;
    Int2         residue;
    Int4         num = 0;
    size_t       aa;
    bool         msgout = false;
    bool         first = false;
    Int4         difflen;

    objects::CCdregion& cdregion = feat.SetData().SetCdregion();
    size_t len = StringLen(qval);
    msg2[0] = '\0';

    msg = location_to_string(feat.GetLocation());
    if (msg == NULL)
        msg = StringSave("");

    size_t blen = prot.size();

    if (len != blen && (!feat.IsSetExcept() || feat.GetExcept() == false)) {
        ErrPostEx(SEV_ERROR, ERR_CDREGION_ProteinLenDiff,
                  "Lengths of conceptual translation and translation qualifier differ : %d : %d : %s",
                  blen, len, msg);
    }

    difflen = 0;
    if (!intercodon) {
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
                StringCpy(msg2, "at AA # ");

            if (num < 11 && StringLen(msg2) < 1000) {
                sprintf(aastr, "%d(%c,%c), ",
                        (int)aa, (char)residue, (char)*ptr);
                StringCat(msg2, aastr);
            }
            else if (num == 11 && StringLen(msg2) < 1000)
                StringCat(msg2, ", additional details suppressed, ");
            ptr++;
        }

        if (num > 0) {
            cdregion.SetConflict(true);
            sprintf(aastr, "using genetic code %s, total %d difference%s",
                    gcode, (int)num, (num > 1) ? "s" : "");
            StringCat(msg2, aastr);
            if (!feat.IsSetExcept() || feat.GetExcept() == false) {
                ErrPostEx(SEV_WARNING, ERR_CDREGION_TranslationDiff,
                          "%s:%s", msg2, msg);
            }
        }

        if (!msgout) {
            cdregion.ResetConflict();
            ErrPostEx(SEV_INFO, ERR_CDREGION_TranslationsAgree,
                      "Parser-generated conceptual translation agrees with input translation %s",
                      msg);
        }

        if (difflen == 2) {
            cdregion.SetConflict(true);
            msgout = true;
        }
    } /* intercodon */
    else {
        msgout = true;
        if (!feat.IsSetExcept() || feat.GetExcept() == false) {
            ErrPostEx(SEV_WARNING, ERR_CDREGION_NoTranslationCompare,
                      "Conceptual translation has internal stop codons, no comparison: %s",
                      msg);
        }
    }

    if (msgout) {
        if (pp->debug) {
            ErrByteStorePtr(ibp, feat, prot);
        }
    }

    if (pp->accver == false) {
        if ((num == 1 && first) ||
            (pp->transl && !prot.empty() && cdregion.IsSetConflict() && cdregion.GetConflict())) {
            cdregion.ResetConflict();
            ErrPostEx(SEV_WARNING, ERR_CDREGION_TranslationOverride,
                      "Input translation is replaced with conceptual translation: %s",
                      msg);
            *method = objects::eGIBB_method_concept_trans;
            MemFree(msg);
        }
    }

    if ((cdregion.IsSetConflict() && cdregion.GetConflict() == true) || difflen != 0)
        *method = objects::eGIBB_method_concept_trans_a;

    if (msgout && pp->transl == false && (!feat.IsSetExcept() || feat.GetExcept() == false)) {
        ErrPostEx(SEV_WARNING, ERR_CDREGION_SuppliedProteinUsed,
                  "In spite of previously reported problems, the supplied protein translation will be used : %s",
                  msg);
    }

    prot = qval;
    MemFree(msg);
}

/**********************************************************
*
*   static bool check_translation(bsp, qval):
*
*      If bsp != translation's value return FALSE.
*
**********************************************************/
static bool check_translation(std::string& prot, char* qval)
{
    size_t len = 0;
    size_t blen = prot.size();

    for (len = StringLen(qval); len != 0; len--) {
        if (qval[len - 1] != 'X')          /* remove terminal X */
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
static bool Translate(objects::CSeq_feat& feat, std::string& prot)
{
    std::list<CRef<objects::CSeq_id>> orig_ids;
    const objects::CSeq_id* feat_loc_id = nullptr;
    if (feat.IsSetLocation())
        feat_loc_id = feat.GetLocation().GetId();

    bool change = feat_loc_id && feat.GetData().GetCdregion().IsSetCode_break();
    if (change) {

        NON_CONST_ITERATE(objects::CCdregion::TCode_break, code_break, feat.SetData().SetCdregion().SetCode_break())
        {
            orig_ids.push_back(CRef<objects::CSeq_id>(new objects::CSeq_id));
            orig_ids.back()->Assign(*(*code_break)->GetLoc().GetId());
            (*code_break)->SetLoc().SetId(*feat_loc_id);
        }
    }

    bool ret = true;
    try {
        objects::CSeqTranslator::Translate(feat, GetScope(), prot);
    }
    catch (objects::CSeqMapException& e) {
        ErrPostEx(SEV_REJECT, 0, 0, "%s", e.GetMsg().c_str());
        ret = false;
    }

    if (change) {

        std::list<CRef<objects::CSeq_id>>::iterator it = orig_ids.begin();
        NON_CONST_ITERATE(objects::CCdregion::TCode_break, code_break, feat.SetData().SetCdregion().SetCode_break())
        {
            (*code_break)->SetLoc().SetId(**it);
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
static Int2 EndAdded(objects::CSeq_feat& feat, GeneRefFeats& gene_refs)
{
    Int4         pos;
    Int4         pos1;
    Int4         pos2;
    Int4         len;
    Int4         len2;

    Uint4        remainder;

    Uint4        oldfrom;
    Uint4        oldto;

    size_t       i;

    char*      transl;
    char*      name;

    objects::CCdregion& cdregion = feat.SetData().SetCdregion();
    len = objects::sequence::GetLength(feat.GetLocation(), &GetScope());
    len2 = len;

    int frame = cdregion.IsSetFrame() ? cdregion.GetFrame() : 0;
    if (frame > 1 && frame < 4)
        len -= frame - 1;

    remainder = 3 - len % 3;
    if (remainder == 0)
        return(0);

    if (cdregion.IsSetCode_break()) {
        bool ret_condition = false;
        ITERATE(objects::CCdregion::TCode_break, code_break, cdregion.GetCode_break())
        {
            pos1 = INT4_MAX;
            pos2 = -10;

            for (objects::CSeq_loc_CI loc((*code_break)->GetLoc()); loc; ++loc) {
                pos = objects::sequence::LocationOffset(*loc.GetRangeAsSeq_loc(), feat.GetLocation(), objects::sequence::eOffset_FromStart);
                if (pos < pos1)
                    pos1 = pos;
                pos = objects::sequence::LocationOffset(*loc.GetRangeAsSeq_loc(), feat.GetLocation(), objects::sequence::eOffset_FromEnd);
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
            return(0);
    }

    objects::CSeq_interval* last_interval = nullptr;
    for (CTypeIterator<objects::CSeq_interval> loc(Begin(feat.SetLocation())); loc; ++loc) {
        last_interval = &(*loc);
    }

    if (last_interval == nullptr)
        return(0);

    const objects::CBioseq_Handle& bioseq_h = GetScope().GetBioseqHandle(last_interval->GetId());
    if (bioseq_h.GetState() != objects::CBioseq_Handle::fState_none)
        return(0);

    oldfrom = last_interval->GetFrom();
    oldto = last_interval->GetTo();

    if (last_interval->IsSetStrand() && last_interval->GetStrand() == objects::eNa_strand_minus) {
        if (last_interval->GetFrom() < remainder || last_interval->IsSetFuzz_from())
            return(0);
        last_interval->SetFrom(oldfrom - remainder);
    }
    else {
        if (last_interval->GetTo() >= bioseq_h.GetBioseqLength() - remainder || last_interval->IsSetFuzz_to())
            return(0);
        last_interval->SetTo(oldto + remainder);
    }

    std::string newprot;
    Translate(feat, newprot);

    if (newprot.empty()) {
        last_interval->SetFrom(oldfrom);
        last_interval->SetTo(oldto);
        return(0);
    }

    size_t protlen = newprot.size();
    if (protlen != (size_t) (len + remainder) / 3) {
        last_interval->SetFrom(oldfrom);
        last_interval->SetTo(oldto);
        return(0);
    }

    if (newprot[protlen - 1] != '*') {
        last_interval->SetFrom(oldfrom);
        last_interval->SetTo(oldto);
        return(0);
    }

    transl = CpTheQualValue(feat.GetQual(), "translation");
    if (transl == NULL) {
        last_interval->SetFrom(oldfrom);
        last_interval->SetTo(oldto);
        return(0);
    }

    protlen--;
    newprot = newprot.substr(0, newprot.size() - 1);

    for (i = 0; i < protlen; i++) {
        if (transl[i] != newprot[i])
            break;
    }

    MemFree(transl);
    if (i < protlen) {
        last_interval->SetFrom(oldfrom);
        last_interval->SetTo(oldto);
        return(0);
    }

    if (!gene_refs.valid)
        return(remainder);

    name = CpTheQualValue(feat.GetQual(), "gene");
    if (name == NULL)
        return(remainder);

    for (TSeqFeatList::iterator gene = gene_refs.first; gene != gene_refs.last; ++gene) {
        if (!(*gene)->IsSetData() || !(*gene)->GetData().IsGene())
            continue;

        int cur_strand = (*gene)->GetLocation().IsSetStrand() ? (*gene)->GetLocation().GetStrand() : 0,
            last_strand = last_interval->IsSetStrand() ? last_interval->GetStrand() : 0;
        if (cur_strand != last_strand)
            continue;

        const objects::CGene_ref& cur_gene_ref = (*gene)->GetData().GetGene();
        if (StringICmp(cur_gene_ref.GetLocus().c_str(), name) != 0) {
            if (!cur_gene_ref.IsSetSyn())
                continue;

            bool found = false;
            ITERATE(objects::CGene_ref::TSyn, syn, cur_gene_ref.GetSyn())
            {
                if (StringICmp(name, syn->c_str()) == 0) {
                    found = true;
                    break;
                }
            }

            if (!found)
                continue;
        }

        for (CTypeIterator<objects::CSeq_interval> loc(Begin((*gene)->SetLocation())); loc; ++loc) {
            if (!loc->GetId().Match(last_interval->GetId()))
                continue;

            int cur_strand = loc->IsSetStrand() ? loc->GetStrand() : 0;
            if (cur_strand == objects::eNa_strand_minus && loc->GetFrom() == oldfrom)
                loc->SetFrom(last_interval->GetFrom());
            else if (cur_strand != objects::eNa_strand_minus && loc->GetTo() == oldto)
                loc->SetTo(last_interval->GetTo());
        }
    }

    MemFree(name);
    return(remainder);
}

/**********************************************************/
static void fta_check_codon_quals(objects::CSeq_feat& feat)
{
    char*   p;

    if (!feat.IsSetQual())
        return;

    for (TQualVector::iterator qual = feat.SetQual().begin(); qual != feat.SetQual().end();) {
        if (!(*qual)->IsSetQual() || !(*qual)->IsSetVal() ||
            (*qual)->GetQual() != "codon") {
            ++qual;
            continue;
        }

        p = location_to_string(feat.GetLocation());
        ErrPostEx(SEV_ERROR, ERR_CDREGION_CodonQualifierUsed,
                  "Encountered /codon qualifier for \"CDS\" feature at \"%s\". Code-breaks (/transl_except) should be used instead.",
                  p);
        if (p != NULL)
            MemFree(p);

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
static void InternalStopCodon(ParserPtr pp, InfoBioseqPtr ibp,
                              objects::CSeq_feat& feat, unsigned char* method,
                              Uint1 dif, GeneRefFeats& gene_refs, std::string& seq_data)
{
    char*      qval;
    char*      msg;
    bool         intercodon = false;
    bool         again = true;

    ErrSev       sev;

    Uint1        m = 0;
    Char         gcode_str[10];
    Char         stopmsg[550];
    Char         aastr[100];
    size_t       protlen;
    Int4         aa;
    Int4         num = 0;
    Int2         residue;
    Int2         r;

    objects::CCdregion& cdregion = feat.SetData().SetCdregion();

    if (!cdregion.IsSetCode())
        return;

    objects::CGenetic_code& gen_code = cdregion.SetCode();
    objects::CGenetic_code::C_E* cur_code = nullptr;

    NON_CONST_ITERATE(objects::CGenetic_code::Tdata, gcode, gen_code.Set())
    {
        if ((*gcode)->IsId()) {
            cur_code = (*gcode);
            break;
        }
    }

    if (cur_code == nullptr)
        return;

    if (pp->no_code) {
        int orig_code_id = cur_code->GetId();
        for (int cur_code_id = cur_code->GetId(); again && cur_code_id < 14; ++cur_code_id) {
            cur_code->SetId(cur_code_id);
            intercodon = false;

            std::string prot;
            if (!Translate(feat, prot))
                pp->entrylist[pp->curindx]->drop = 1;

            if (prot.empty())
                continue;

            protlen = prot.size();
            residue = prot[protlen - 1];

            if (residue == '*')
                prot = prot.substr(0, prot.size() - 1);

            intercodon = prot.find('*') != std::string::npos;

            if (!intercodon) {
                qval = CpTheQualValue(feat.GetQual(), "translation");
                if (qval != NULL)        /* compare protein sequence */
                {
                    if (check_translation(prot, qval)) {
                        sev = (pp->taxserver == 0) ? SEV_INFO : SEV_WARNING;
                        ErrPostEx(sev, ERR_CDREGION_GeneticCodeAssumed,
                                  "No genetic code from TaxArch, trying to guess, code %d assumed",
                                  cur_code_id);
                        again = false;
                    }
                    MemFree(qval);
                }
                else
                    break;
            }
        }

        if (again) {
            sev = (pp->taxserver == 0) ? SEV_INFO : SEV_WARNING;
            ErrPostEx(sev, ERR_CDREGION_GeneticCodeAssumed,
                      "Can't guess genetic code, code %d assumed", orig_code_id);
            cur_code->SetId(orig_code_id);
        }
    }

    std::string prot;
    if (!Translate(feat, prot))
        pp->entrylist[pp->curindx]->drop = 1;

    if (cur_code != NULL)
        sprintf(gcode_str, "%d", (int)cur_code->GetId());
    else
        StringCpy(gcode_str, "unknown");

    qval = CpTheQualValue(feat.GetQual(), "translation");
    intercodon = false;

    msg = location_to_string(feat.GetLocation());
    if (msg == NULL)
        msg = StringSave("");

    if (!prot.empty()) {
        protlen = prot.size();
        residue = prot[protlen - 1];
        if ((!feat.IsSetPartial() || feat.GetPartial() == false) && !SeqLocHaveFuzz(feat.GetLocation())) {
            CkEndStop(feat, dif);
        }

        if (residue != '*') {
            r = EndAdded(feat, gene_refs);
            if (r > 0 && (!feat.IsSetExcept() || feat.GetExcept() == false)) {
                ErrPostEx(SEV_WARNING, ERR_CDREGION_TerminalStopCodonMissing,
                          "CDS: %s |found end stop codon after %d bases added",
                          msg, r);
            }

            if ((!feat.IsSetPartial() || feat.GetPartial() == false) && !SeqLocHaveFuzz(feat.GetLocation())) {
                /* if there is no partial qualifier and location
                * doesn't have "fuzz" then output message
                */
                if (!feat.IsSetExcept() || feat.GetExcept() == false) {
                    ErrPostEx(SEV_ERROR, ERR_CDREGION_TerminalStopCodonMissing,
                              "No end stop codon found for CDS: %s", msg);
                }
            }
        }
        else                    /* remove termination codon from protein */
        {
            check_end_internal(protlen, feat, dif);
            prot = prot.substr(0, prot.size() - 1);
        }

        /* check internal stop codon */
        size_t residue_idx = 0;
        protlen = prot.size();
        for (stopmsg[0] = '\0', aa = 1; residue_idx < protlen; ++residue_idx) {
            residue = prot[residue_idx];
            if (aa == 1 && residue == '-') {
                /* if unrecognized start of translation,
                * a ncbigap character is inserted
                */
                if (!feat.IsSetExcept() || feat.GetExcept() == false) {
                    ErrPostEx(SEV_WARNING, ERR_CDREGION_IllegalStart,
                              "unrecognized initiation codon from CDS: %s",
                              msg);
                }
                if (qval == NULL)        /* no /translation */
                {
                    MemFree(msg);
                    return;
                }
            }

            if (residue == '*')  /* only report 10 internal stop codons */
            {
                intercodon = true;
                ++num;

                if (num < 11 && StringLen(stopmsg) < 500) {
                    sprintf(aastr, "%d ", (int)aa);
                    StringCat(stopmsg, aastr);
                }
                else if (num == 11 && StringLen(stopmsg) < 500)
                    StringCat(stopmsg, ", only report 10 positions");
            }

            aa++;
        }

        if (intercodon) {
            if (!feat.IsSetExcept() || feat.GetExcept() == false) {
                ErrPostEx(SEV_ERROR, ERR_CDREGION_InternalStopCodonFound,
                          "Found %d internal stop codon, at AA # %s, on feature key, CDS, frame # %d, genetic code %s:%s",
                          (int)num, stopmsg, cdregion.GetFrame(), gcode_str, msg);
            }

            if (pp->debug) {
                ErrByteStorePtr(ibp, feat, prot);
            }
        }
    }
    else if (!feat.IsSetExcept() || feat.GetExcept() == false) {
        ErrPostEx(SEV_WARNING, ERR_CDREGION_NoProteinSeq,
                  "No protein sequence found:%s", msg);
    }

    if (qval != NULL)                    /* compare protein sequence */
    {
        CkProteinTransl(pp, ibp, prot, feat, qval, intercodon, gcode_str, &m);
        *method = m;
        MemFree(qval);
        MemFree(msg);
        seq_data.swap(prot);
        return;
    }

    if (!prot.empty() && !intercodon) {
        if (prot.size() > 6 || !check_short_CDS(pp, feat, false)) {
            ErrPostEx(SEV_INFO, ERR_CDREGION_TranslationAdded,
                      "input CDS lacks a translation: %s", msg);
        }
        *method = objects::eGIBB_method_concept_trans;
        MemFree(msg);
        seq_data.swap(prot);
        return;
    }

    /* no translation qual and internal stop codon
    */
    if (intercodon) {
        cdregion.SetStops(num);
        if (!feat.IsSetExcept() || feat.GetExcept() == false) {
            ErrPostEx(SEV_WARNING, ERR_CDREGION_NoProteinSeq,
                      "internal stop codons, and no translation qualifier CDS:%s",
                      msg);
        }
    }

    MemFree(msg);
}

/**********************************************************/
static void check_gen_code(char* qval, ProtBlkPtr pbp, Uint1 taxserver)
{
    ErrSev     sev;
    Uint1      gcpvalue;
    Uint1      genome;
    Uint1      value;

    if (pbp == NULL || !pbp->gcode.IsId())
        return;

    gcpvalue = pbp->gcode.GetId();
    value = (Uint1)atoi(qval);
    genome = pbp->genome;

    if (value == gcpvalue)
        return;

    if (value == 7 || value == 8) {
        ErrPostEx(SEV_WARNING, ERR_CDREGION_InvalidGcodeTable,
                  "genetic code table is obsolete /transl_table = %d", value);
        pbp->gcode.SetId(pbp->orig_gcode);
        return;
    }

    if (value != 11 || (genome != 2 && genome != 3 && genome != 6 &&
        genome != 12 && genome != 16 && genome != 17 &&
        genome != 18 && genome != 22)) {
        sev = (taxserver == 0) ? SEV_INFO : SEV_ERROR;
        ErrPostEx(sev, ERR_CDREGION_GeneticCodeDiff,
                  "Genetic code from Taxonomy server: %d, from /transl_table: %d",
                  gcpvalue, value);
    }

    pbp->gcode.SetId(value);
}

/**********************************************************/
static bool CpGeneticCodePtr(objects::CGenetic_code& code, const objects::CGenetic_code::C_E& gcode)
{
    if (!gcode.IsId())
        return false;

    CRef<objects::CGenetic_code::C_E> ce(new objects::CGenetic_code::C_E);
    ce->SetId(gcode.GetId());
    code.Set().push_back(ce);

    return true;
}

/**********************************************************/
static Int4 IfOnlyStopCodon(const objects::CBioseq& bioseq, const objects::CSeq_feat& feat, bool transl)
{
    char* p;
    Uint1   strand;
    Int4    len;
    Int4    i;

    if (!feat.IsSetLocation() || transl)
        return(0);

    const objects::CSeq_loc& loc = feat.GetLocation();
    TSeqPos start = loc.GetStart(objects::eExtreme_Positional),
        stop = loc.GetStop(objects::eExtreme_Positional) + 1;

    if (start == kInvalidSeqPos || stop == kInvalidSeqPos)
        return(0);

    len = stop - start;
    if (len < 1 || len > 5)
        return(0);

    strand = loc.IsSetStrand() ? loc.GetStrand() : 0;

    p = location_to_string(loc);
    if ((strand == 2 && stop == bioseq.GetLength()) || (strand != 2 && start == 0)) {
        ErrPostEx(SEV_INFO, ERR_CDREGION_StopCodonOnly,
                  "Assuming coding region at \"%s\" annotates the stop codon of an upstream or downstream coding region.",
                  (p == NULL) ? "???" : p);
        i = 1;
    }
    else {
        ErrPostEx(SEV_REJECT, ERR_CDREGION_StopCodonBadInterval,
                  "Coding region at \"%s\" appears to annotate a stop codon, but its location does not include a sequence endpoint.",
                  (p == NULL) ? "???" : p);
        i = -1;
    }

    if (p != NULL)
        MemFree(p);

    return(i);
}

/**********************************************************/
static void fta_concat_except_text(objects::CSeq_feat& feat, const Char* text)
{
    if (text == NULL)
        return;

    if (feat.IsSetExcept_text()) {
        feat.SetExcept_text() += ", ";
        feat.SetExcept_text() += text;
    }
    else
        feat.SetExcept_text() = text;
}

/**********************************************************/
static bool fta_check_exception(objects::CSeq_feat& feat, Parser::ESource source)
{
    const char **b;
    ErrSev     sev;
    char*    p;

    if (!feat.IsSetQual())
        return true;

    bool stopped = false;
    for (TQualVector::iterator qual = feat.SetQual().begin(); qual != feat.SetQual().end();) {
        if (!(*qual)->IsSetQual()) {
            ++qual;
            continue;
        }

        if ((*qual)->GetQual() == "ribosomal_slippage")
            p = (char*) "ribosomal slippage";
        else if ((*qual)->GetQual() == "trans_splicing")
            p = (char*) "trans-splicing";
        else
            p = NULL;

        if (p != NULL) {
            feat.SetExcept(true);

            qual = feat.SetQual().erase(qual);
            fta_concat_except_text(feat, p);
            continue;
        }

        if ((*qual)->GetQual() != "exception" || !(*qual)->IsSetVal()) {
            ++qual;
            continue;
        }

        if (source == Parser::ESource::Refseq)
            b = RSExceptionQualVals;
        else
            b = GBExceptionQualVals;

        const Char* cur_val = (*qual)->GetVal().c_str();
        for (; *b != NULL; b++) {
            if (StringICmp(*b, cur_val) == 0)
                break;
        }

        if (*b == NULL) {
            sev = (source == Parser::ESource::Refseq) ? SEV_ERROR : SEV_REJECT;

            p = location_to_string(feat.GetLocation());
            ErrPostEx(sev, ERR_QUALIFIER_InvalidException,
                      "/exception value \"%s\" on feature \"CDS\" at location \"%s\" is invalid.",
                      cur_val, (p == NULL) ? "Unknown" : p);
            if (p != NULL)
                MemFree(p);
            if (source != Parser::ESource::Refseq) {
                stopped = true;
                break;
            }
        }
        else {
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
static Int2 CkCdRegion(ParserPtr pp, CScope& scope, objects::CSeq_feat& cds, objects::CBioseq& bioseq,
                       int* num, GeneRefFeats& gene_refs)
{
    ProtBlkPtr     pbp;
    const char     *r;

    char*        qval = NULL;
    char*        p;
    bool        is_pseudo;
    bool        is_stop;
    bool        is_transl;

    ErrSev         sev;

    Uint1          method = 0;
    Uint1          dif;
    Uint1          codon_start;
    Int2           frame;
    Int2           i;

    pbp = pp->pbp;
    if (pp->buf != NULL)
        MemFree(pp->buf);
    pp->buf = NULL;

    TCodeBreakList code_breaks;
    GetCdRegionCB(pbp->ibp, cds, code_breaks, &dif, pp->accver);

    is_pseudo = cds.IsSetPseudo() ? cds.GetPseudo() : false;
    is_transl = FindTheQual(cds, "translation");

    objects::CCode_break* first_code_break = nullptr;
    if (!code_breaks.empty())
        first_code_break = *code_breaks.begin();

    if (first_code_break != nullptr && first_code_break->GetAa().GetNcbieaa() == 42)
        is_stop = true;
    else if (is_pseudo)
        is_stop = false;
    else {
        i = IfOnlyStopCodon(bioseq, cds, is_transl);
        if (i < 0)
            return(-1);
        is_stop = (i != 0);
    }

    if (!is_transl) {
        bool found = false;
        ITERATE(TQualVector, qual, cds.GetQual())
        {
            if (!(*qual)->IsSetQual() || !(*qual)->IsSetVal())
                continue;

            if ((*qual)->GetQual() == "product" ||
                (*qual)->GetQual() == "function" ||
                (*qual)->GetQual() == "EC_number") {
                found = true;
                break;
            }
        }

        if (found) {
            CRef<objects::CSeqFeatXref> xfer(new objects::CSeqFeatXref);
            objects::CProt_ref& prot_ref = xfer->SetData().SetProt();
            ITERATE(TQualVector, qual, cds.GetQual())
            {
                if (!(*qual)->IsSetQual() || !(*qual)->IsSetVal())
                    continue;

                if ((*qual)->GetQual() == "product")
                    prot_ref.SetName().push_back((*qual)->GetVal());
                else if ((*qual)->GetQual() == "EC_number")
                    prot_ref.SetEc().push_back((*qual)->GetVal());
                else if ((*qual)->GetQual() == "function")
                    prot_ref.SetActivity().push_back((*qual)->GetVal());
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
        p = location_to_string(cds.GetLocation());
        ErrPostEx(SEV_ERROR, ERR_CDREGION_PseudoWithTranslation,
                  "Coding region flagged as /pseudo has a /translation qualifier : \"%s\".",
                  (p == NULL) ? "" : p);
        MemFree(p);
        return(-1);
    }

    if (pp->mode != Parser::EMode::Relaxed &&
        pp->accver && is_transl == false && FindTheQual(cds, "protein_id")) {
        p = location_to_string(cds.GetLocation());
        ErrPostEx(SEV_ERROR, ERR_CDREGION_UnexpectedProteinId,
                  "CDS without /translation should not have a /protein_id qualifier. CDS = \"%s\".",
                  (p == NULL) ? "" : p);
        MemFree(p);
        return(-1);
    }

    if (pp->mode != Parser::EMode::Relaxed && 
        is_transl == false && is_pseudo == false && is_stop == false) {
        p = location_to_string(cds.GetLocation());
        if (pp->accver == false) {
            r = "Feature and protein bioseq";
            i = -2;
        }
        else {
            r = "Record";
            i = -1;
        }
        sev = (i == -1) ? SEV_REJECT : SEV_ERROR;
        ErrPostEx(sev, ERR_CDREGION_MissingTranslation,
                  "Missing /translation qualifier for CDS \"%s\". %s rejected.",
                  (p == NULL) ? "" : p, r);
        MemFree(p);
        return(i);
    }

    /* check exception qualifier
    */
    if (!fta_check_exception(cds, pp->source))
        return(-1);

    CRef<objects::CImp_feat> imp_feat(new objects::CImp_feat);
    if (cds.IsSetData() && cds.GetData().IsImp())
        imp_feat->Assign(cds.GetData().GetImp());

    codon_start = 1;
    qval = GetTheQualValue(cds.SetQual(), "codon_start");

    if (qval == NULL) {
        if (pp->source == Parser::ESource::EMBL)
            frame = 1;
        else {
            frame = 0;
            objects::CCdregion::EFrame loc_frame = objects::CCdregion::eFrame_not_set;
            if (objects::CCleanup::SetFrameFromLoc(loc_frame, cds.GetLocation(), scope))
                frame = loc_frame;

            if (frame == 0 && is_pseudo == false) {
                p = location_to_string(cds.GetLocation());
                sev = (pp->source == Parser::ESource::DDBJ) ? SEV_INFO : SEV_ERROR;
                ErrPostEx(sev, ERR_CDREGION_MissingCodonStart,
                          "CDS feature \"%s\" is lacking /codon_start qualifier; assuming frame = 1.",
                          (p == NULL) ? "" : p);
                MemFree(p);
                frame = 1;
            }
        }
    }
    else {
        frame = (Uint1)atoi(qval);
        MemFree(qval);
    }

    CRef<objects::CCdregion> cdregion(new objects::CCdregion);

    if (frame > 0)
        cdregion->SetFrame(static_cast<objects::CCdregion::EFrame>(frame));

    qval = GetTheQualValue(cds.SetQual(), "transl_table");

    if (qval != NULL) {
        check_gen_code(qval, pbp, pp->taxserver);
        pp->no_code = false;
        MemFree(qval);
    }
    else if (pbp != NULL && pbp->gcode.IsId())
        pbp->gcode.SetId(pbp->orig_gcode);

    if (!code_breaks.empty())
        cdregion->SetCode_break().swap(code_breaks);

    if (!CpGeneticCodePtr(cdregion->SetCode(), pbp->gcode))
        cdregion->ResetCode();

    cds.SetData().SetCdregion(*cdregion);

    if (cds.GetQual().empty())
        cds.ResetQual();

    if (!is_transl) {
        imp_feat.Reset();
        return(0);
    }

    objects::CBioseq::TId ids;
    GetProtRefSeqId(ids, pbp->ibp, num, pp, scope, cds);

    if (!ids.empty())
        fta_check_codon_quals(cds);

    std::string sequence_data;
    InternalStopCodon(pp, pbp->ibp, cds, &method, dif, gene_refs, sequence_data);

    if (cds.GetQual().empty())
        cds.ResetQual();

    if (cdregion->IsSetConflict() && cdregion->GetConflict() && codon_start == 0) {
        p = location_to_string(cds.GetLocation());
        ErrPostEx(SEV_ERROR, ERR_CDREGION_TooBad,
                  "Input translation does not agree with parser generated one, cdregion \"%s\" is lacking /codon_start, frame not set, - so sequence will be rejected.",
                  (p == NULL) ? "" : p);
        MemFree(p);
        return(-1);
    }

    if (!sequence_data.empty()) {
        imp_feat.Reset();
        CRef<objects::CBioseq> new_bioseq = BldProtRefSeqEntry(pbp, cds, sequence_data, method, pp, bioseq, ids);

        if (new_bioseq.Empty()) {
            return(-1);
        }

        scope.AddBioseq(*new_bioseq);

        /* remove qualifiers which were processed before
        */
        DeleteQual(cds.SetQual(), "codon_start");
        DeleteQual(cds.SetQual(), "transl_except");
        DeleteQual(cds.SetQual(), "translation");
        DeleteQual(cds.SetQual(), "protein_id");

        if (cds.GetQual().empty())
            cds.ResetQual();

        if (sequence_data.size() < 6 && pp->accver == false && check_short_CDS(pp, cds, true)) {
            /* make xref from prot-ref for short CDS only
            */
            if (new_bioseq->IsSetAnnot()) {
                NON_CONST_ITERATE(objects::CBioseq::TAnnot, annot, new_bioseq->SetAnnot())
                {
                    if (!(*annot)->IsFtable())
                        continue;

                    ITERATE(TSeqFeatList, cur_feat, (*annot)->GetData().GetFtable())
                    {
                        if (!(*cur_feat)->IsSetData() || !(*cur_feat)->GetData().IsProt())
                            continue;

                        CRef<objects::CSeqFeatXref> new_xref(new objects::CSeqFeatXref);
                        new_xref->SetData().SetProt().Assign((*cur_feat)->GetData().GetProt());

                        cds.SetXref().push_back(new_xref);
                    }
                }
            }
            return(0);
        }

        objects::CSeq_id& first_id = *(*new_bioseq->SetId().begin());
        cds.SetProduct().SetWhole(first_id);

        AddProtRefSeqEntry(pbp, *new_bioseq);

        return(1);
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

    if (!is_pseudo) {
        p = location_to_string(cds.GetLocation());
        ErrPostEx(SEV_ERROR, ERR_CDREGION_ConvertToImpFeat,
                  "non-pseudo CDS with data problems is converted to ImpFeat%s",
                  (p == NULL) ? "" : p);
        MemFree(p);
    }
    return(0);
}

/**********************************************************
*
*   static SeqFeatPtr SrchCdRegion(pp, bsp, sfp, gene):
*
*      Return a link list of SeqFeatPtr of type CDREGION.
*
**********************************************************/
static void SrchCdRegion(ParserPtr pp, CScope& scope, objects::CBioseq& bioseq, objects::CSeq_annot& annot,
                         GeneRefFeats& gene_refs)
{
    char*    p;
    Int4       num = 0;
    Int2       i;

    if (!annot.IsSetData() || !annot.GetData().IsFtable())
        return;

    for (objects::CSeq_annot::C_Data::TFtable::iterator feat = annot.SetData().SetFtable().begin();
         feat != annot.SetData().SetFtable().end();) {
        if (!(*feat)->IsSetData() || !(*feat)->GetData().IsImp()) {
            ++feat;
            continue;
        }

        const objects::CImp_feat& imp_feat = (*feat)->GetData().GetImp();
        if (!imp_feat.IsSetKey() || imp_feat.GetKey() != "CDS") {
            ++feat;
            continue;
        }

        /* remove asn2ff_generated comments
        */
        StripCDSComment(*(*feat));

        const objects::CSeq_loc& loc = (*feat)->GetLocation();
        if (loc.IsEmpty() || loc.IsEquiv() || loc.IsBond()) {
            p = location_to_string(loc);
            ErrPostEx(SEV_REJECT, ERR_CDREGION_BadLocForTranslation,
                      "Coding region feature has a location that cannot be processed: \"%s\".",
                      (p == NULL) ? "" : p);
            if (p != NULL)
                MemFree(p);
            pp->entrylist[pp->curindx]->drop = 1;
            break;
        }

        i = CkCdRegion(pp, scope, *(*feat), bioseq, &num, gene_refs);

        if (i == -2) {
            feat = annot.SetData().SetFtable().erase(feat);
            continue;
        }

        if (i == -1) {
            pp->entrylist[pp->curindx]->drop = 1;
            break;
        }

        if (i != 1) {
            ++feat;
            continue;
        }

        /* prepare cdregion to link list, for nuc-prot level
        */
        pp->pbp->feats.push_back(*feat);

        feat = annot.SetData().SetFtable().erase(feat);
    }
}

/**********************************************************/
static void FindCd(TEntryList& seq_entries, CScope& scope, ParserPtr pp, GeneRefFeats& gene_refs)
{
    ProtBlkPtr    pbp = pp->pbp;

    NON_CONST_ITERATE(TEntryList, entry, seq_entries)
    {
        for (CTypeIterator<objects::CBioseq_set> bio_set(Begin(*(*entry))); bio_set; ++bio_set) {
            pbp->segset = true;
            pbp->biosep = *entry;
            break;
        }

        if (pbp->segset)
            break;
    }

    NON_CONST_ITERATE(TEntryList, entry, seq_entries)
    {
        for (CTypeIterator<objects::CBioseq> bioseq(Begin(*(*entry))); bioseq; ++bioseq) {
            const objects::CSeq_id& first_id = *(*bioseq->GetId().begin());
            if (IsSegBioseq(&first_id))
                continue;

            InfoBioseqFree(pbp->ibp);
            if(pp->source != Parser::ESource::USPTO)
                CpSeqId(pbp->ibp, first_id);

            if (bioseq->IsSetAnnot()) {
                for (objects::CBioseq::TAnnot::iterator annot = bioseq->SetAnnot().begin(); annot != bioseq->SetAnnot().end();) {
                    if (!(*annot)->IsFtable()) {
                        ++annot;
                        continue;
                    }

                    SrchCdRegion(pp, scope, *bioseq, *(*annot), gene_refs);
                    if (!(*annot)->GetData().GetFtable().empty()) {
                        ++annot;
                        continue;
                    }

                    annot = bioseq->SetAnnot().erase(annot);
                }

                if (bioseq->GetAnnot().empty())
                    bioseq->ResetAnnot();
            }

            if (!pbp->segset) {
                pbp->biosep = *entry;
            }
        }
    }
}

/**********************************************************/
static bool check_GIBB(TSeqdescList& descrs)
{
    if (descrs.empty())
        return false;

    const objects::CSeqdesc* descr_modif = nullptr;
    ITERATE(TSeqdescList, descr, descrs)
    {
        if ((*descr)->IsModif()) {
            descr_modif = *descr;
            break;
        }
    }
    if (descr_modif == nullptr)
        return true;

    if (!descr_modif->GetModif().empty()) {
        objects::EGIBB_mod gmod = *descr_modif->GetModif().begin();
        if (gmod == objects::eGIBB_mod_dna || gmod == objects::eGIBB_mod_rna ||
            gmod == objects::eGIBB_mod_est)
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
        }
        else
            ++descr;
    }
}

/**********************************************************/
void ExtractDescrs(TSeqdescList& descrs_from, TSeqdescList& descrs_to, objects::CSeqdesc::E_Choice choice)
{
    for (TSeqdescList::iterator descr = descrs_from.begin(); descr != descrs_from.end();) {
        if ((*descr)->Which() == choice) {
            descrs_to.push_back(*descr);
            descr = descrs_from.erase(descr);
        }
        else
            ++descr;
    }
}

/**********************************************************/
static void GetBioseqSetDescr(ProtBlkPtr pbp, TSeqdescList& descrs)
{
    TSeqdescList* descrs_from = nullptr;
    if (pbp->segset) {
        if (!pbp->biosep->GetSet().GetDescr().Get().empty())
            descrs_from = &pbp->biosep->SetSet().SetDescr().Set();
    }
    else {
        if (!pbp->biosep->GetSeq().GetDescr().Get().empty())
            descrs_from = &pbp->biosep->SetSeq().SetDescr().Set();
    }

    if (descrs_from == nullptr)
        return;

    ExtractDescrs(*descrs_from, descrs, objects::CSeqdesc::e_Org);

    if (check_GIBB(*descrs_from)) {
        ExtractDescrs(*descrs_from, descrs, objects::CSeqdesc::e_Modif);
    }

    ExtractDescrs(*descrs_from, descrs, objects::CSeqdesc::e_Comment);
    ExtractDescrs(*descrs_from, descrs, objects::CSeqdesc::e_Pub);
    ExtractDescrs(*descrs_from, descrs, objects::CSeqdesc::e_Update_date);

    ValNodeExtractUserObject(*descrs_from, descrs, "GenomeProjectsDB");
    ValNodeExtractUserObject(*descrs_from, descrs, "DBLink");
    ValNodeExtractUserObject(*descrs_from, descrs, "FeatureFetchPolicy");
}

/**********************************************************/
static void BuildProtBioseqSet(ProtBlkPtr pbp, TEntryList& entries)
{
    CRef<objects::CSeq_entry> entry(new objects::CSeq_entry);
    objects::CBioseq_set& seq_set = entry->SetSet();
    seq_set.SetClass(objects::CBioseq_set::eClass_nuc_prot);

    /* add descr if nuc-prot
    */
    GetBioseqSetDescr(pbp, seq_set.SetDescr().Set());       /* get from ASN.1 tree */
    if (seq_set.GetDescr().Get().empty())
        seq_set.ResetDescr();

    seq_set.SetSeq_set().splice(seq_set.SetSeq_set().end(), entries);

    CRef<objects::CSeq_annot> annot(new objects::CSeq_annot);

    if (!pbp->feats.empty())
        annot->SetData().SetFtable().swap(pbp->feats);

    seq_set.SetAnnot().push_back(annot);

    entries.push_back(entry);
}

/**********************************************************/
void ProcNucProt(ParserPtr pp, TEntryList& seq_entries, GeneRefFeats& gene_refs)
{
    ProtBlkPtr    pbp;
    ErrSev        sev;
    Int4          gcode = 0;

    pbp = pp->pbp;
    ProtBlkInit(pbp);

    GetGcode(seq_entries, pp);

    if (!pbp->gcode.IsId()) {
        gcode = (pbp->genome == 4 || pbp->genome == 5) ? 2 : 1;
        pp->no_code = true;
        sev = (pp->taxserver == 0) ? SEV_INFO : SEV_WARNING;
        ErrPostEx(sev, ERR_CDREGION_GeneticCodeAssumed,
                  "No %sgenetic code from TaxArch, code %d assumed",
                  (gcode == 2) ? "mitochondrial " : "", gcode);
        pbp->gcode.SetId(gcode);
        pbp->orig_gcode = gcode;
    }

    FindCd(seq_entries, GetScope(), pp, gene_refs);

    if (pp->entrylist[pp->curindx]->drop == 1) {
        ProtBlkFree(pbp);
        seq_entries.clear();
        return;
    }

    if (!pbp->entries.empty()) {
        seq_entries.splice(seq_entries.end(), pbp->entries);

        BuildProtBioseqSet(pbp, seq_entries);
        AssignBioseqSetLevel(seq_entries);
    }

    ProtBlkFree(pbp);
}

/**********************************************************/
static const objects::CDate* GetDateFromDescrs(const TSeqdescList& descrs, objects::CSeqdesc::E_Choice what)
{
    const objects::CDate* set_date = nullptr;
    ITERATE(TSeqdescList, descr, descrs)
    {
        if ((*descr)->Which() == what) {
            if (what == objects::CSeqdesc::e_Create_date)
                set_date = &(*descr)->GetCreate_date();
            else if (what == objects::CSeqdesc::e_Update_date)
                set_date = &(*descr)->GetUpdate_date();

            if (set_date)
                break;
        }
    }

    return set_date;
}

/**********************************************************/
static void FixDupDates(objects::CBioseq_set& bio_set, objects::CSeqdesc::E_Choice what)
{
    if (!bio_set.IsSetSeq_set() || !bio_set.IsSetDescr())
        return;

    NON_CONST_ITERATE(TEntryList, entry, bio_set.SetSeq_set())
    {
        for (CTypeIterator<objects::CBioseq> bioseq(Begin(*(*entry))); bioseq; ++bioseq) {
            if (!bioseq->IsSetInst() || !bioseq->GetInst().IsSetMol() || !bioseq->GetInst().IsNa() || !bioseq->IsSetDescr())
                continue;

            const objects::CDate* set_date = GetDateFromDescrs(bio_set.GetDescr().Get(), what);

            TSeqdescList& cur_descrs = bioseq->SetDescr().Set();
            TSeqdescList::iterator cur_descr = cur_descrs.begin();

            for (; cur_descr != cur_descrs.end(); ++cur_descr) {
                if ((*cur_descr)->Which() == what)
                    break;
            }

            if (cur_descr == cur_descrs.end())
                continue;

            const objects::CDate* seq_date = nullptr;
            if (what == objects::CSeqdesc::e_Create_date)
                seq_date = &(*cur_descr)->GetCreate_date();
            else if (what == objects::CSeqdesc::e_Update_date)
                seq_date = &(*cur_descr)->GetUpdate_date();

            if (seq_date == nullptr)
                continue;

            if (set_date && seq_date->Compare(*set_date) == objects::CDate::eCompare_same)
                cur_descrs.erase(cur_descr);

            if (set_date == nullptr) {
                bio_set.SetDescr().Set().push_back(*cur_descr);
                cur_descrs.erase(cur_descr);
            }
        }
    }
}

/**********************************************************/
static void FixCreateDates(objects::CBioseq_set& bio_set)
{
    FixDupDates(bio_set, objects::CSeqdesc::e_Create_date);
}

/**********************************************************/
static void FixUpdateDates(objects::CBioseq_set& bio_set)
{
    FixDupDates(bio_set, objects::CSeqdesc::e_Update_date);
}

/**********************************************************/
static void FixEmblUpdateDates(objects::CBioseq_set& bio_set)
{
    if (!bio_set.IsSetSeq_set() || !bio_set.IsSetDescr())
        return;

    NON_CONST_ITERATE(TEntryList, entry, bio_set.SetSeq_set())
    {
        for (CTypeIterator<objects::CBioseq> bioseq(Begin(*(*entry))); bioseq; ++bioseq) {
            if (!bioseq->IsSetInst() || !bioseq->GetInst().IsSetMol() || !bioseq->GetInst().IsNa() || !bioseq->IsSetDescr())
                continue;

            const objects::CDate* set_date = GetDateFromDescrs(bio_set.GetDescr().Get(), objects::CSeqdesc::e_Update_date);

            const objects::CEMBL_block* embl_block = nullptr;
            ITERATE(TSeqdescList, descr, bioseq->GetDescr().Get())
            {
                if ((*descr)->IsEmbl()) {
                    embl_block = &(*descr)->GetEmbl();
                    break;
                }
            }

            const objects::CDate* seq_date = nullptr;
            if (embl_block != nullptr && embl_block->IsSetUpdate_date())
                seq_date = &embl_block->GetUpdate_date();

            if (seq_date == nullptr)
                continue;

            if (set_date && seq_date->Compare(*set_date) == objects::CDate::eCompare_same)
                continue;

            if (set_date == nullptr) {
                CRef<objects::CSeqdesc> new_descr(new objects::CSeqdesc);
                new_descr->SetUpdate_date().Assign(*seq_date);
                bio_set.SetDescr().Set().push_back(new_descr);
            }
        }
    }
}

/**********************************************************/
void CheckDupDates(TEntryList& seq_entries)
{
    NON_CONST_ITERATE(TEntryList, entry, seq_entries)
    {
        for (CTypeIterator<objects::CBioseq_set> bio_set(Begin(*(*entry))); bio_set; ++bio_set) {
            if (bio_set->IsSetClass() && bio_set->GetClass() == objects::CBioseq_set::eClass_nuc_prot) {
                FixCreateDates(*bio_set);
                FixUpdateDates(*bio_set);
                FixEmblUpdateDates(*bio_set);
            }
        }
    }
}

END_NCBI_SCOPE
