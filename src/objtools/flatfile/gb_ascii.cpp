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
 * File Name: gb_ascii.cpp
 *
 * Author: Karl Sirotkin, Hsiu-Chuan Chen
 *
 * File Description:
 *      Parse gb from blocks to asn.
 * Build GenBank format entry block.
 *
 */

#include <ncbi_pch.hpp>

#include "ftacpp.hpp"

#include <objects/seq/Seq_inst.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <serial/objostr.hpp>
#include <serial/serial.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objmgr/scope.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqcode/Seq_code_type.hpp>
#include <objects/seq/Pubdesc.hpp>
#include <objects/seq/MolInfo.hpp>

#include "index.h"
#include "genbank.h"

#include <objtools/flatfile/flatfile_parser.hpp>
#include <objtools/flatfile/flatdefn.h>

#include "ftaerr.hpp"
#include "asci_blk.h"
#include "indx_blk.h"
#include "utilref.h"
#include "utilfeat.h"
#include "loadfeat.h"
#include "gb_ascii.h"
#include "add.h"
#include "nucprot.h"
#include "fta_qscore.h"
#include "citation.h"
#include "fcleanup.h"
#include "utilfun.h"
#include "entry.h"
#include "ref.h"
#include "xgbparint.h"
#include "xutils.h"


#ifdef THIS_FILE
#  undef THIS_FILE
#endif
#define THIS_FILE "gb_ascii.cpp"

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

/**********************************************************/
static char* GBDivOffset(const DataBlk& entry, Int4 div_shift)
{
    return (entry.mBuf.ptr + div_shift);
}

/**********************************************************/
static void CheckContigEverywhere(IndexblkPtr ibp, Parser::ESource source)
{
    bool condiv = NStr::EqualNocase(ibp->division, "CON");

    if (! condiv && ibp->is_contig == false && ibp->origin == false &&
        ibp->is_mga == false) {
        FtaErrPost(SEV_ERROR, ERR_FORMAT_MissingSequenceData, "Required sequence data is absent. Entry dropped.");
        ibp->drop = true;
    } else if (! condiv && ibp->is_contig && ibp->origin == false) {
        FtaErrPost(SEV_WARNING, ERR_DIVISION_MappedtoCON, "Division [{}] mapped to CON based on the existence of CONTIG line.", ibp->division);
    } else if (ibp->is_contig && ibp->origin) {
        if (source == Parser::ESource::EMBL || source == Parser::ESource::DDBJ) {
            FtaErrPost(SEV_INFO, ERR_FORMAT_ContigWithSequenceData, "The CONTIG/CO linetype and sequence data are both present. Ignoring sequence data.");
        } else {
            FtaErrPost(SEV_REJECT, ERR_FORMAT_ContigWithSequenceData, "The CONTIG/CO linetype and sequence data may not both be present in a sequence record.");
            ibp->drop = true;
        }
    } else if (condiv && ibp->is_contig == false && ibp->origin == false) {
        FtaErrPost(SEV_ERROR, ERR_FORMAT_MissingContigFeature, "No CONTIG data in GenBank format file, entry dropped.");
        ibp->drop = true;
    } else if (condiv && ibp->is_contig == false && ibp->origin) {
        FtaErrPost(SEV_WARNING, ERR_DIVISION_ConDivLacksContig, "Division is CON, but CONTIG data have not been found.");
    }
}

/**********************************************************/
bool GetGenBankInstContig(const DataBlk& entry, CBioseq& bsp, ParserPtr pp)
{
    char* p;
    char* q;
    char* r;

    bool locmap;

    bool allow_crossdb_featloc;
    Int4 i;
    int  numerr;

    const DataBlk* dbp = TrackNodeType(entry, ParFlat_CONTIG);
    if (! dbp || ! dbp->mBuf.ptr)
        return true;

    i = static_cast<Int4>(dbp->mBuf.len) - ParFlat_COL_DATA;
    if (i <= 1)
        return false;

    p = StringSave(string_view(&dbp->mBuf.ptr[ParFlat_COL_DATA], i - 1)); // exclude trailing EOL
    for (q = p, r = p; *q != '\0'; q++)
        if (*q != '\n' && *q != '\t' && *q != ' ')
            *r++ = *q;
    *r = '\0';

    for (q = p; *q != '\0'; q++)
        if ((q[0] == ',' && q[1] == ',') || (q[0] == '(' && q[1] == ',') ||
            (q[0] == ',' && q[1] == ')'))
            break;
    if (*q != '\0') {
        FtaErrPost(SEV_REJECT, ERR_LOCATION_ContigHasNull, "The join() statement for this record's contig line contains one or more comma-delimited components which are null.");
        MemFree(p);
        return false;
    }

    pp->buf.reset();

    CRef<CSeq_loc> loc = xgbparseint_ver(p, locmap, numerr, bsp.GetId(), pp->accver);
    if (loc.Empty()) {
        MemFree(p);
        return true;
    }

    allow_crossdb_featloc     = pp->allow_crossdb_featloc;
    pp->allow_crossdb_featloc = true;

    TSeqLocList locs;
    locs.push_back(loc);
    i = fta_fix_seq_loc_id(locs, pp, p, {}, true);

    if (i > 999)
        fta_create_far_fetch_policy_user_object(bsp, i);

    pp->allow_crossdb_featloc = allow_crossdb_featloc;

    if (loc->IsMix()) {
        XGappedSeqLocsToDeltaSeqs(loc->GetMix(), bsp.SetInst().SetExt().SetDelta().Set());
        bsp.SetInst().SetRepr(CSeq_inst::eRepr_delta);
    } else
        bsp.SetInst().ResetExt();

    MemFree(p);
    return true;
}

/**********************************************************
 *
 *   bool GetGenBankInst(pp, entry, dnaconv):
 *
 *      Fills in Seq-inst for an entry. Assumes Bioseq
 *   already allocated.
 *
 *                                              3-30-93
 *
 **********************************************************/
static bool GetGenBankInst(ParserPtr pp, const DataBlk& entry, unsigned char* dnaconv)
{
    EntryBlkPtr ebp;
    Int2        topology;
    Int2        strand;
    char*       topstr;

    char*        bptr = entry.mBuf.ptr;
    IndexblkPtr  ibp  = pp->entrylist[pp->curindx];
    LocusContPtr lcp  = &ibp->lc;

    topstr = bptr + lcp->topology;

    ebp             = entry.GetEntryData();
    CBioseq& bioseq = ebp->seq_entry->SetSeq();

    CSeq_inst& inst = bioseq.SetInst();
    inst.SetRepr(ibp->is_mga ? CSeq_inst::eRepr_virtual : CSeq_inst::eRepr_raw);

    /* get linear, circular, tandem topology, blank is linear which = 1
     */
    topology = CheckTPG(topstr);
    if (topology > 1)
        inst.SetTopology(static_cast<CSeq_inst::ETopology>(topology));

    strand = CheckSTRAND((lcp->strand >= 0) ? bptr + lcp->strand : "   ");
    if (strand > 0)
        inst.SetStrand(static_cast<CSeq_inst::EStrand>(strand));

    if (GetSeqData(pp, entry, bioseq, ParFlat_ORIGIN, dnaconv, (ibp->is_prot ? eSeq_code_type_iupacaa : eSeq_code_type_iupacna)) == false)
        return false;

    if (ibp->is_contig && ! GetGenBankInstContig(entry, bioseq, pp))
        return false;

    return true;
}

/**********************************************************/
static string GetGenBankLineage(string_view sv)
{
    if (sv.empty())
        return {};

    string str = GetBlkDataReplaceNewLine(sv, ParFlat_COL_DATA);

    while (! str.empty()) {
        char c = str.back();
        if (c == ' ' || c == '\t' || c == '\n' || c == '.' || c == ';')
            str.pop_back();
        else
            break;
    }

    return str;
}

/**********************************************************
 *
 *   static GBBlockPtr GetGBBlock(pp, entry, mfp, biosp):
 *
 *                                              4-7-93
 *
 **********************************************************/
static CRef<CGB_block> GetGBBlock(ParserPtr pp, const DataBlk& entry, CMolInfo& mol_info, CBioSource* bio_src)
{
    LocusContPtr lcp;

    CRef<CGB_block> gbb(new CGB_block),
        ret;

    IndexblkPtr ibp;
    char*       bptr;
    char*       eptr;
    char*       ptr;
    Char        msg[4];
    size_t      len;
    Int2        div;

    bool if_cds;
    bool pat_ref = false;
    bool est_kwd = false;
    bool sts_kwd = false;
    bool gss_kwd = false;
    bool htc_kwd = false;
    bool fli_kwd = false;
    bool wgs_kwd = false;
    bool tpa_kwd = false;
    bool tsa_kwd = false;
    bool tls_kwd = false;
    bool env_kwd = false;
    bool mga_kwd = false;

    bool cancelled;
    bool drop;

    char* tempdiv;
    char* p;
    Int4  i;

    ibp            = pp->entrylist[pp->curindx];
    ibp->wgssec[0] = '\0';

    SrchNodeType(entry, ParFlat_SOURCE, &len, &bptr);
    string str = GetBlkDataReplaceNewLine(string_view(bptr, len), ParFlat_COL_DATA);
    if (! str.empty()) {
        if (str.back() == '.') {
            if (str.size() >= 2 && *(str.end() - 2) == '.')
                str.pop_back();
        }

        gbb->SetSource(std::move(str));
    }

    if (! ibp->keywords.empty()) {
        gbb->SetKeywords().swap(ibp->keywords);
        ibp->keywords.clear();
    } else
        GetSequenceOfKeywords(entry, ParFlat_KEYWORDS, ParFlat_COL_DATA, gbb->SetKeywords());

    if (ibp->is_mga && ! fta_check_mga_keywords(mol_info, gbb->GetKeywords())) {
        return ret;
    }

    if (ibp->is_tpa && ! fta_tpa_keywords_check(gbb->GetKeywords())) {
        return ret;
    }

    if (ibp->is_tsa && ! fta_tsa_keywords_check(gbb->GetKeywords(), pp->source)) {
        return ret;
    }

    if (ibp->is_tls && ! fta_tls_keywords_check(gbb->GetKeywords(), pp->source)) {
        return ret;
    }

    for (const string& key : gbb->GetKeywords()) {
        fta_keywords_check(key, &est_kwd, &sts_kwd, &gss_kwd, &htc_kwd, &fli_kwd, &wgs_kwd, &tpa_kwd, &env_kwd, &mga_kwd, &tsa_kwd, &tls_kwd);
    }

    if (ibp->env_sample_qual == false && env_kwd) {
        FtaErrPost(SEV_REJECT, ERR_KEYWORD_ENV_NoMatchingQualifier, "This record utilizes the ENV keyword, but there are no /environmental_sample qualifiers among its source features.");
        return ret;
    }

    SrchNodeType(entry, ParFlat_ORIGIN, &len, &bptr);
    eptr = bptr + len;
    ptr  = SrchTheChar(string_view(bptr, eptr), '\n');
    if (ptr) {
        eptr = ptr;
        bptr += 6;

        if (eptr != bptr) {
            while (isspace(*bptr) != 0)
                bptr++;
            len = eptr - bptr;
            if (len > 0) {
                gbb->SetOrigin(string(bptr, eptr));
            }
        }
    }

    lcp = &ibp->lc;

    bptr = GBDivOffset(entry, lcp->div);

    if (*bptr != ' ') {
        if_cds = check_cds(entry, pp->format);
        div    = CheckDIV(bptr);
        if (div != -1) {
            string div_str(bptr, bptr + 3);
            gbb->SetDiv(div_str);

            if (div == 16) /* "ORG" replaced by "UNA" */
                gbb->SetDiv("UNA");

            /* preserve the division code for later use
             */
            const char* p_div = gbb->GetDiv().c_str();
            StringCpy(ibp->division, p_div);

            if (ibp->psip.NotEmpty())
                pat_ref = true;

            if (ibp->is_tpa &&
                (StringEqu(p_div, "EST") || StringEqu(p_div, "GSS") ||
                 StringEqu(p_div, "PAT") || StringEqu(p_div, "HTG"))) {
                FtaErrPost(SEV_REJECT, ERR_DIVISION_BadTPADivcode, "Division code \"{}\" is not legal for TPA records. Entry dropped.", p_div);
                return ret;
            }

            if (ibp->is_tsa && ! StringEqu(p_div, "TSA")) {
                FtaErrPost(SEV_REJECT, ERR_DIVISION_BadTSADivcode, "Division code \"{}\" is not legal for TSA records. Entry dropped.", p_div);
                return ret;
            }

            cancelled = IsCancelled(gbb->GetKeywords());

            if (StringEqu(p_div, "HTG")) {
                if (! HasHtg(gbb->GetKeywords())) {
                    FtaErrPost(SEV_ERROR, ERR_DIVISION_MissingHTGKeywords, "Division is HTG, but entry lacks HTG-related keywords. Entry dropped.");
                    return ret;
                }
            }

            tempdiv = StringSave(gbb->GetDiv());

            if (fta_check_htg_kwds(gbb->SetKeywords(), pp->entrylist[pp->curindx], mol_info))
                gbb->ResetDiv();

            DefVsHTGKeywords(mol_info.GetTech(), entry, ParFlat_DEFINITION, ParFlat_ORIGIN, cancelled);

            CheckHTGDivision(tempdiv, mol_info.GetTech());
            if (tempdiv)
                MemFree(tempdiv);

            i = 0;
            if (est_kwd)
                i++;
            if (sts_kwd)
                i++;
            if (gss_kwd)
                i++;
            if (ibp->htg > 0)
                i++;
            if (htc_kwd)
                i++;
            if (fli_kwd)
                i++;
            if (wgs_kwd)
                i++;
            if (env_kwd)
                i++;
            if (mga_kwd) {
                if (ibp->is_mga == false) {
                    FtaErrPost(SEV_REJECT, ERR_KEYWORD_ShouldNotBeCAGE, "This is apparently _not_ a CAGE record, but the special keywords are present. Entry dropped.");
                    return ret;
                }
                i++;
            } else if (ibp->is_mga) {
                FtaErrPost(SEV_ERROR, ERR_KEYWORD_NoGeneExpressionKeywords, "This is apparently a CAGE or 5'-SAGE record, but it lacks the required keywords. Entry dropped.");
            }
            if (tpa_kwd) {
                if (ibp->is_tpa == false && pp->source != Parser::ESource::EMBL) {
                    FtaErrPost(SEV_REJECT, ERR_KEYWORD_ShouldNotBeTPA, "This is apparently _not_ a TPA record, but the special \"TPA\" and/or \"Third Party Annotation\" keywords are present. Entry dropped.");
                    return ret;
                }
                i++;
            } else if (ibp->is_tpa) {
                FtaErrPost(SEV_REJECT, ERR_KEYWORD_MissingTPA, "This is apparently a TPA record, but it lacks the required \"TPA\" and/or \"Third Party Annotation\" keywords. Entry dropped.");
                return ret;
            }
            if (tsa_kwd) {
                if (ibp->is_tsa == false) {
                    FtaErrPost(SEV_REJECT, ERR_KEYWORD_ShouldNotBeTSA, "This is apparently _not_ a TSA record, but the special \"TSA\" and/or \"Transcriptome Shotgun Assembly\" keywords are present. Entry dropped.");
                    return ret;
                }
                i++;
            } else if (ibp->is_tsa) {
                FtaErrPost(SEV_REJECT, ERR_KEYWORD_MissingTSA, "This is apparently a TSA record, but it lacks the required \"TSA\" and/or \"Transcriptome Shotgun Assembly\" keywords. Entry dropped.");
                return ret;
            }
            if (tls_kwd) {
                if (ibp->is_tls == false) {
                    FtaErrPost(SEV_REJECT, ERR_KEYWORD_ShouldNotBeTLS, "This is apparently _not_ a TLS record, but the special \"TLS\" and/or \"Targeted Locus Study\" keywords are present. Entry dropped.");
                    return ret;
                }
                i++;
            } else if (ibp->is_tls) {
                FtaErrPost(SEV_REJECT, ERR_KEYWORD_MissingTLS, "This is apparently a TLS record, but it lacks the required \"TLS\" and/or \"Targeted Locus Study\" keywords. Entry dropped.");
                return ret;
            }
            if (i > 1) {
                if (i == 2 && ibp->htg > 0 && env_kwd)
                    FtaErrPost(SEV_WARNING, ERR_KEYWORD_HTGPlusENV, "This HTG record also has the ENV keyword, which is an unusual combination. Confirmation that isolation and cloning steps actually occured might be appropriate.");
                else if ((i == 2 && wgs_kwd && tpa_kwd) ||
                         (i == 2 && tsa_kwd && tpa_kwd) ||
                         (i == 2 && pp->source == Parser::ESource::DDBJ &&
                          env_kwd && tpa_kwd)) {
                } else if (i != 2 || env_kwd == false ||
                           (est_kwd == false && gss_kwd == false && wgs_kwd == false)) {
                    if (i != 2 || pp->source != Parser::ESource::DDBJ ||
                        ibp->is_tsa == false || env_kwd == false) {
                        if (pp->source != Parser::ESource::DDBJ || ibp->is_wgs == false ||
                            (env_kwd == false && tpa_kwd == false)) {
                            FtaErrPost(SEV_REJECT, ERR_KEYWORD_ConflictingKeywords, "This record contains more than one of the special keywords used to indicate that a sequence is an HTG, EST, GSS, STS, HTC, WGS, ENV, FLI_CDNA, TPA, CAGE, TSA or TLS sequence.");
                            return ret;
                        }
                    }
                }
            }

            if (wgs_kwd)
                i--;

            if (ibp->is_contig && i > 0 &&
                wgs_kwd == false && tpa_kwd == false && env_kwd == false) {
                FtaErrPost(SEV_REJECT, ERR_KEYWORD_IllegalForCON, "This CON record should not have HTG, EST, GSS, STS, HTC, FLI_CDNA, CAGE, TSA or TLS special keywords. Entry dropped.");
                return ret;
            }

            CMolInfo::TTech thtg = mol_info.GetTech();
            if (thtg == CMolInfo::eTech_htgs_0 || thtg == CMolInfo::eTech_htgs_1 ||
                thtg == CMolInfo::eTech_htgs_2 || thtg == CMolInfo::eTech_htgs_3) {
                RemoveHtgPhase(gbb->SetKeywords());
            }

            if (SrchNodeType(entry, ParFlat_KEYWORDS, &len, &bptr)) {
                string kw = GetBlkDataReplaceNewLine(string_view(bptr, len), ParFlat_COL_DATA);

                if (! est_kwd && kw.find("EST") != string::npos) {
                    FtaErrPost(SEV_WARNING, ERR_KEYWORD_ESTSubstring, "Keyword {} has substring EST, but no official EST keywords found", kw);
                }
                if (! sts_kwd && kw.find("STS") != string::npos) {
                    FtaErrPost(SEV_WARNING, ERR_KEYWORD_STSSubstring, "Keyword {} has substring STS, but no official STS keywords found", kw);
                }
            }

            if (! ibp->is_contig) {
                drop                 = false;
                CMolInfo::TTech tech = mol_info.GetTech();
                string          p_div;
                if (gbb->IsSetDiv())
                    p_div = gbb->GetDiv();

                check_div(ibp->is_pat, pat_ref, est_kwd, sts_kwd, gss_kwd, if_cds, p_div, &tech, ibp->bases, pp->source, drop);

                if (tech != CMolInfo::eTech_unknown)
                    mol_info.SetTech(tech);
                else
                    mol_info.ResetTech();

                if (! p_div.empty())
                    gbb->SetDiv(p_div);
                else
                    gbb->ResetDiv();

                if (drop) {
                    return ret;
                }
            } else if (gbb->GetDiv() == "CON") {
                gbb->ResetDiv();
            }
        } else if (pp->mode != Parser::EMode::Relaxed) {
            MemCpy(msg, bptr, 3);
            msg[3] = '\0';
            FtaErrPost(SEV_REJECT, ERR_DIVISION_UnknownDivCode, "Unknown division code \"{}\" found in GenBank flatfile. Record rejected.", msg);
            return ret;
        }

        if (IsNewAccessFormat(ibp->acnum) == 0 && *ibp->acnum == 'T' &&
            gbb->IsSetDiv() && gbb->GetDiv() != "EST") {
            FtaErrPost(SEV_INFO, ERR_DIVISION_MappedtoEST, "Leading T in accession number.");

            mol_info.SetTech(CMolInfo::eTech_est);
            gbb->ResetDiv();
        }
    }

    bool is_htc_div = gbb->IsSetDiv() && gbb->GetDiv() == "HTC",
         has_htc    = HasHtc(gbb->GetKeywords());

    if (is_htc_div && ! has_htc) {
        FtaErrPost(SEV_ERROR, ERR_DIVISION_MissingHTCKeyword, "This record is in the HTC division, but lacks the required HTC keyword.");
        return ret;
    }

    if (! is_htc_div && has_htc) {
        FtaErrPost(SEV_ERROR, ERR_DIVISION_InvalidHTCKeyword, "This record has the special HTC keyword, but is not in HTC division. If this record has graduated out of HTC, then the keyword should be removed.");
        return ret;
    }

    if (is_htc_div) {
        bptr = entry.mBuf.ptr;
        p    = bptr + lcp->molecule;
        if (*p == 'm' || *p == 'r')
            p++;
        else if (StringEquN(p, "pre-", 4))
            p += 4;
        else if (StringEquN(p, "transcribed ", 12))
            p += 12;

        if (! fta_StartsWith(p, "RNA"sv)) {
            FtaErrPost(SEV_ERROR, ERR_DIVISION_HTCWrongMolType, "All HTC division records should have a moltype of pre-RNA, mRNA or RNA.");
            return ret;
        }
    }

    if (fli_kwd)
        mol_info.SetTech(CMolInfo::eTech_fli_cdna);

    /* will be used in flat file database
     */
    if (gbb->IsSetDiv()) {
        if (gbb->GetDiv() == "EST") {
            ibp->EST = true;
            mol_info.SetTech(CMolInfo::eTech_est);

            gbb->ResetDiv();
        } else if (gbb->GetDiv() == "STS") {
            ibp->STS = true;
            mol_info.SetTech(CMolInfo::eTech_sts);

            gbb->ResetDiv();
        } else if (gbb->GetDiv() == "GSS") {
            ibp->GSS = true;
            mol_info.SetTech(CMolInfo::eTech_survey);

            gbb->ResetDiv();
        } else if (gbb->GetDiv() == "HTC") {
            ibp->HTC = true;
            mol_info.SetTech(CMolInfo::eTech_htc);

            gbb->ResetDiv();
        } else if (gbb->GetDiv() == "SYN" && bio_src && bio_src->IsSetOrigin() &&
                   bio_src->GetOrigin() == CBioSource::eOrigin_synthetic) {
            gbb->ResetDiv();
        }
    } else if (mol_info.IsSetTech()) {
        if (mol_info.GetTech() == CMolInfo::eTech_est)
            ibp->EST = true;
        if (mol_info.GetTech() == CMolInfo::eTech_sts)
            ibp->STS = true;
        if (mol_info.GetTech() == CMolInfo::eTech_survey)
            ibp->GSS = true;
        if (mol_info.GetTech() == CMolInfo::eTech_htc)
            ibp->HTC = true;
    }

    if (mol_info.IsSetTech())
        fta_remove_keywords(mol_info.GetTech(), gbb->SetKeywords());

    if (ibp->is_tpa)
        fta_remove_tpa_keywords(gbb->SetKeywords());

    if (ibp->is_tsa)
        fta_remove_tsa_keywords(gbb->SetKeywords(), pp->source);

    if (ibp->is_tls)
        fta_remove_tls_keywords(gbb->SetKeywords(), pp->source);

    if (bio_src) {
        if (bio_src->IsSetSubtype()) {
            for (const auto& subtype : bio_src->GetSubtype()) {
                if (subtype->GetSubtype() == CSubSource::eSubtype_environmental_sample) {
                    fta_remove_env_keywords(gbb->SetKeywords());
                    break;
                }
            }
        }
        if (bio_src->IsSetOrg()) {
            const COrg_ref& org_ref = bio_src->GetOrg();
            if (org_ref.IsSetOrgname() && org_ref.GetOrgname().IsSetMod()) {
                for (const auto& mod : org_ref.GetOrgname().GetMod()) {
                    if (! mod->IsSetSubtype())
                        continue;

                    COrgMod::TSubtype stype = mod->GetSubtype();
                    if (stype == COrgMod::eSubtype_metagenome_source) {
                        fta_remove_mag_keywords(gbb->SetKeywords());
                        break;
                    }
                }
            }
        }
    }

    if (pp->source == Parser::ESource::DDBJ && gbb->IsSetDiv() && bio_src &&
        bio_src->IsSetOrg() && bio_src->GetOrg().IsSetOrgname() &&
        bio_src->GetOrg().GetOrgname().IsSetDiv()) {
        gbb->ResetDiv();
    } else if (gbb->IsSetDiv() &&
               bio_src &&
               bio_src->IsSetOrg() &&
               bio_src->GetOrg().IsSetOrgname() &&
               bio_src->GetOrg().GetOrgname().IsSetDiv() &&
               bio_src->GetOrg().GetOrgname().GetDiv() == gbb->GetDiv()) {
        gbb->ResetDiv();
    }

    GetExtraAccession(ibp, pp->allow_uwsec, pp->source, gbb->SetExtra_accessions());
    ret.Reset(gbb.Release());

    return ret;
}

/**********************************************************
 *
 *   static MolInfoPtr GetGenBankMolInfo(pp, entry, orp):
 *
 *      Data from :
 *   LOCUS ... column 37, or column 53 if "EST"
 *
 **********************************************************/
static CRef<CMolInfo> GetGenBankMolInfo(ParserPtr pp, const DataBlk& entry, const COrg_ref* org_ref)
{
    IndexblkPtr ibp;
    char*       bptr;
    char*       molstr = nullptr;

    CRef<CMolInfo> mol_info(new CMolInfo);

    bptr = entry.mBuf.ptr;
    ibp  = pp->entrylist[pp->curindx];

    molstr = bptr + ibp->lc.molecule;

    bptr = GBDivOffset(entry, ibp->lc.div);

    if (fta_StartsWith(bptr, "EST"sv))
        mol_info->SetTech(CMolInfo::eTech_est);
    else if (fta_StartsWith(bptr, "STS"sv))
        mol_info->SetTech(CMolInfo::eTech_sts);
    else if (fta_StartsWith(bptr, "GSS"sv))
        mol_info->SetTech(CMolInfo::eTech_survey);
    else if (fta_StartsWith(bptr, "HTG"sv))
        mol_info->SetTech(CMolInfo::eTech_htgs_1);
    else if (ibp->is_wgs) {
        if (ibp->is_tsa)
            mol_info->SetTech(CMolInfo::eTech_tsa);
        else if (ibp->is_tls)
            mol_info->SetTech(CMolInfo::eTech_targeted);
        else
            mol_info->SetTech(CMolInfo::eTech_wgs);
    } else if (ibp->is_tsa)
        mol_info->SetTech(CMolInfo::eTech_tsa);
    else if (ibp->is_tls)
        mol_info->SetTech(CMolInfo::eTech_targeted);
    else if (ibp->is_mga) {
        mol_info->SetTech(CMolInfo::eTech_other);
        mol_info->SetTechexp("cage");
    }

    GetFlatBiomol(mol_info->SetBiomol(), mol_info->GetTech(), molstr, pp, entry, org_ref);
    if (mol_info->GetBiomol() == CMolInfo::eBiomol_unknown) // not set
        mol_info->ResetBiomol();

    return mol_info;
}

/**********************************************************/
static void FakeGenBankBioSources(const DataBlk& entry, CBioseq& bioseq)
{
    char* bptr;
    char* end;
    char* ptr;

    Char ch;

    size_t len = 0;
    bptr       = SrchNodeSubType(entry, ParFlat_SOURCE, ParFlat_ORGANISM, &len);

    if (! bptr) {
        FtaErrPost(SEV_WARNING, ERR_ORGANISM_NoOrganism, "No Organism data in genbank format file");
        return;
    }

    end  = bptr + len;
    ch   = *end;
    *end = '\0';

    CRef<CBioSource> bio_src(new CBioSource);
    bptr += ParFlat_COL_DATA;

    if (GetGenomeInfo(*bio_src, bptr) && bio_src->GetGenome() != CBioSource::eGenome_plasmid) {
        while (*bptr != ' ' && *bptr != '\0')
            bptr++;
        while (*bptr == ' ')
            bptr++;
    }

    ptr = StringChr(bptr, '\n');
    if (! ptr) {
        *end = ch;
        return;
    }

    COrg_ref& org_ref = bio_src->SetOrg();

    org_ref.SetTaxname(string(bptr, ptr));

    for (;;) {
        bptr = ptr + 1;
        if (! StringEquN(bptr, "               ", ParFlat_COL_DATA))
            break;
        bptr += ParFlat_COL_DATA;

        ptr = StringChr(bptr, '\n');
        if (! ptr)
            break;

        if (SrchTheChar(string_view(bptr, ptr), ';') || ! StringChr(ptr + 1, '\n')) {
            break;
        }

        string& taxname = org_ref.SetTaxname();
        taxname += ' ';
        taxname.append(bptr, ptr);
    }

    *end = ch;

    if (org_ref.GetTaxname() == "Unknown.") {
        string& taxname = org_ref.SetTaxname();
        taxname.pop_back();
    }

    string s = GetGenBankLineage(string_view(bptr, end - bptr));
    if (! s.empty()) {
        org_ref.SetOrgname().SetLineage(std::move(s));
    }

    CRef<CSeqdesc> descr(new CSeqdesc);
    descr->SetSource(*bio_src);
    bioseq.SetDescr().Set().push_back(descr);
}

/**********************************************************/
static void fta_get_mga_user_object(TSeqdescList& descrs, string_view str, size_t len)
{
    if (str.size() < ParFlat_COL_DATA)
        return;
    str.remove_prefix(ParFlat_COL_DATA);

    if (auto p = str.find('\n'); p != string_view::npos)
        str = str.substr(0, p);

    string first(str), last;
    if (auto p = first.find('-'); p != string::npos) {
        last = first.substr(p + 1);
        first.resize(p);
    }

    CRef<CUser_object> user_obj(new CUser_object);

    CObject_id& id = user_obj->SetType();
    id.SetStr("CAGE-Tag-List");

    CRef<CUser_field> field(new CUser_field);

    field->SetLabel().SetStr("CAGE_tag_total");
    field->SetData().SetInt(static_cast<CUser_field::C_Data::TInt>(len));
    user_obj->SetData().push_back(field);

    field.Reset(new CUser_field);

    field->SetLabel().SetStr("CAGE_accession_first");
    field->SetData().SetStr(first);
    user_obj->SetData().push_back(field);

    field.Reset(new CUser_field);

    field->SetLabel().SetStr("CAGE_accession_last");
    field->SetData().SetStr(last);
    user_obj->SetData().push_back(field);

    CRef<CSeqdesc> descr(new CSeqdesc);
    descr->SetUser(*user_obj);

    descrs.push_back(descr);
}

/**********************************************************/
static void GetGenBankDescr(ParserPtr pp, const DataBlk& entry, CBioseq& bioseq)
{
    IndexblkPtr ibp;

    bool is_htg;

    ibp = pp->entrylist[pp->curindx];

    CBioSource* bio_src = nullptr;
    COrg_ref*   org_ref = nullptr;

    /* ORGANISM
     */

    for (auto& descr : bioseq.SetDescr().Set()) {
        if (descr->IsSource()) {
            bio_src = &(descr->SetSource());
            if (bio_src->IsSetOrg())
                org_ref = &bio_src->SetOrg();
            break;
        }
    }

    /* MolInfo from LOCUS line
     */
    CRef<CMolInfo> mol_info = GetGenBankMolInfo(pp, entry, org_ref);

    /* DEFINITION data ==> descr_title
     */
    char*  offset = nullptr;
    size_t len = 0;
    string title;
    if (SrchNodeType(entry, ParFlat_DEFINITION, &len, &offset)) {
        string str = GetBlkDataReplaceNewLine(string_view(offset, len), ParFlat_COL_DATA);

        if (! str.empty() && str.front() == ' ') {
            size_t i = 0;
            for (char c : str) {
                if (c == ' ')
                    ++i;
                else
                    break;
            }
            str.erase(0, i);
        }

        title.swap(str);

        CRef<CSeqdesc> descr(new CSeqdesc);
        descr->SetTitle(title);
        bioseq.SetDescr().Set().push_back(descr);

        if (! ibp->is_tpa && pp->source != Parser::ESource::EMBL &&
            title.starts_with("TPA:"sv)) {
            FtaErrPost(SEV_REJECT, ERR_DEFINITION_ShouldNotBeTPA, "This is apparently _not_ a TPA record, but the special \"TPA:\" prefix is present on its definition line. Entry dropped.");
            ibp->drop = true;
            return;
        }
        if (! ibp->is_tsa && title.starts_with("TSA:"sv)) {
            FtaErrPost(SEV_REJECT, ERR_DEFINITION_ShouldNotBeTSA, "This is apparently _not_ a TSA record, but the special \"TSA:\" prefix is present on its definition line. Entry dropped.");
            ibp->drop = true;
            return;
        }
        if (! ibp->is_tls && title.starts_with("TLS:"sv)) {
            FtaErrPost(SEV_REJECT, ERR_DEFINITION_ShouldNotBeTLS, "This is apparently _not_ a TLS record, but the special \"TLS:\" prefix is present on its definition line. Entry dropped.");
            ibp->drop = true;
            return;
        }
    }

    CRef<CUser_object> dbuop;
    if (SrchNodeType(entry, ParFlat_DBLINK, &len, &offset))
        fta_get_dblink_user_object(bioseq.SetDescr().Set(), offset, len, pp->source, &ibp->drop, dbuop);
    else {
        SrchNodeType(entry, ParFlat_PROJECT, &len, &offset);
        if (offset)
            fta_get_project_user_object(bioseq.SetDescr().Set(), offset, Parser::EFormat::GenBank, &ibp->drop, pp->source);
    }

    if (ibp->is_mga) {
        SrchNodeType(entry, ParFlat_MGA, &len, &offset);
        fta_get_mga_user_object(bioseq.SetDescr().Set(), offset, ibp->bases);
    }
    if (ibp->is_tpa &&
        (title.empty() || (! title.starts_with("TPA:"sv) &&
                           ! title.starts_with("TPA_exp:"sv) &&
                           ! title.starts_with("TPA_inf:"sv) &&
                           ! title.starts_with("TPA_asm:"sv) &&
                           ! title.starts_with("TPA_reasm:"sv)))) {
        FtaErrPost(SEV_REJECT, ERR_DEFINITION_MissingTPA, "This is apparently a TPA record, but it lacks the required \"TPA:\" prefix on its definition line. Entry dropped.");
        ibp->drop = true;
        return;
    }
    if (ibp->is_tsa && ! ibp->is_tpa &&
        (title.empty() || ! title.starts_with("TSA:"sv))) {
        FtaErrPost(SEV_REJECT, ERR_DEFINITION_MissingTSA, "This is apparently a TSA record, but it lacks the required \"TSA:\" prefix on its definition line. Entry dropped.");
        ibp->drop = true;
        return;
    }
    if (ibp->is_tls && (title.empty() || ! title.starts_with("TLS:"sv))) {
        FtaErrPost(SEV_REJECT, ERR_DEFINITION_MissingTLS, "This is apparently a TLS record, but it lacks the required \"TLS:\" prefix on its definition line. Entry dropped.");
        ibp->drop = true;
        return;
    }

    /* REFERENCE
     */
    /* pub should be before GBblock because we need patent ref
     */
    auto& chain = TrackNodes(entry);
    for (auto& ref_blk : chain) {
        if (ref_blk.mType != ParFlat_REF_END)
            continue;

        CRef<CPubdesc> pubdesc = DescrRefs(pp, ref_blk, ParFlat_COL_DATA);
        if (pubdesc.NotEmpty()) {
            CRef<CSeqdesc> descr(new CSeqdesc);
            descr->SetPub(*pubdesc);
            bioseq.SetDescr().Set().push_back(descr);
        }
    }

    for (auto& ref_blk : chain) {
        if (ref_blk.mType != ParFlat_REF_NO_TARGET)
            continue;

        CRef<CPubdesc> pubdesc = DescrRefs(pp, ref_blk, ParFlat_COL_DATA);
        if (pubdesc.NotEmpty()) {
            CRef<CSeqdesc> descr(new CSeqdesc);
            descr->SetPub(*pubdesc);
            bioseq.SetDescr().Set().push_back(descr);
        }
    }

    /* GB-block
     */
    CRef<CGB_block> gbbp = GetGBBlock(pp, entry, *mol_info, bio_src);

    if ((pp->source == Parser::ESource::DDBJ || pp->source == Parser::ESource::EMBL) &&
        ibp->is_contig && (! mol_info->IsSetTech() || mol_info->GetTech() == CMolInfo::eTech_unknown)) {
        CMolInfo::TTech tech = fta_check_con_for_wgs(bioseq);
        if (tech == CMolInfo::eTech_unknown)
            mol_info->ResetTech();
        else
            mol_info->SetTech(tech);
    }

    if (mol_info->IsSetBiomol() || mol_info->IsSetTech()) {
        CRef<CSeqdesc> descr(new CSeqdesc);
        descr->SetMolinfo(*mol_info);
        bioseq.SetDescr().Set().push_back(descr);
    }

    if (gbbp.Empty()) {
        ibp->drop = true;
        return;
    }

    if (pp->taxserver == 1 && gbbp->IsSetDiv())
        fta_fix_orgref_div(bioseq.GetAnnot(), org_ref, *gbbp);

    if (NStr::StartsWith(ibp->division, "CON"sv, NStr::eNocase))
        fta_add_hist(pp, bioseq, gbbp->SetExtra_accessions(), Parser::ESource::DDBJ, CSeq_id::e_Ddbj, true, ibp->acnum);
    else
        fta_add_hist(pp, bioseq, gbbp->SetExtra_accessions(), Parser::ESource::DDBJ, CSeq_id::e_Ddbj, false, ibp->acnum);

    {
        CRef<CSeqdesc> descr(new CSeqdesc);
        descr->SetGenbank(*gbbp);
        bioseq.SetDescr().Set().push_back(descr);
    }

    if (! SrchNodeType(entry, ParFlat_PRIMARY, &len, &offset) && ibp->is_tpa && ibp->is_wgs == false) {
        if (ibp->inferential || ibp->experimental) {
            if (! fta_dblink_has_sra(dbuop) &&
                ! TrackNodeType(entry, ParFlat_COMMENT)) {
                FtaErrPost(SEV_REJECT, ERR_TPA_TpaCommentMissing,
                           "TPA:{} record lacks the mandatory comment line. Entry dropped.",
                           (ibp->inferential == false) ? "experimental" : "inferential");
                ibp->drop = true;
                return;
            }
        } else if (ibp->specialist_db == false) {
            FtaErrPost(SEV_REJECT, ERR_TPA_TpaSpansMissing, "TPA record lacks required AH/PRIMARY linetype. Entry dropped.");
            ibp->drop = true;
            return;
        }
    }

    if (offset && len > 0 &&
        fta_parse_tpa_tsa_block(bioseq, offset, ibp->acnum, ibp->vernum, len, ParFlat_COL_DATA, ibp->is_tpa) == false) {
        ibp->drop = true;
        return;
    }

    if (mol_info.NotEmpty() && mol_info->IsSetTech() &&
        (mol_info->GetTech() == CMolInfo::eTech_htgs_0 ||
         mol_info->GetTech() == CMolInfo::eTech_htgs_1 ||
         mol_info->GetTech() == CMolInfo::eTech_htgs_2))
        is_htg = true;
    else
        is_htg = false;

    /* COMMENT data
     */
    if (SrchNodeType(entry, ParFlat_COMMENT, &len, &offset)) {
        string comment = GetDescrComment(offset, len, ParFlat_COL_DATA, is_htg, ibp->is_pat);
        if (! comment.empty()) {
            bool           bad = false;
            TUserObjVector user_objs;

            fta_parse_structured_comment(comment, bad, user_objs);
            if (bad) {
                ibp->drop = true;
                return;
            }

            for (auto& user_obj : user_objs) {
                CRef<CSeqdesc> descr(new CSeqdesc);
                descr->SetUser(*user_obj);
                bioseq.SetDescr().Set().push_back(descr);
            }

            if (false) {
                string q;
                q.reserve(comment.size());
                for (auto p = comment.begin(); p != comment.end();) {
                    if (*p == ';') {
                        auto p1 = p + 1;
                        if (p1 != comment.end() && (*p1 == ' ' || *p1 == '~'))
                            *p = ' ';
                    }
                    if (*p == ' ' || *p == '~') {
                        q += ' ';
                        p++;
                        while (p != comment.end() && (*p == ' ' || *p == '~'))
                            p++;
                    } else
                        q += *p++;
                }
                comment.swap(q);
            }

            if (! comment.empty()) {
                CRef<CSeqdesc> descr(new CSeqdesc);
                descr->SetComment(comment);
                bioseq.SetDescr().Set().push_back(descr);
            }
        }
    }

    /* DATE
     */
    if (pp->no_date) /* -N in command line means no date */
        return;

    CRef<CDate> date;
    if (pp->date) /* -L in command line means replace date */
    {
        CTime time(CTime::eCurrent);
        date.Reset(new CDate);
        date->SetToTime(time);
    } else if (ibp->lc.date > 0) {
        CRef<CDate_std> std_date = GetUpdateDate(entry.mBuf.ptr + ibp->lc.date, pp->source);
        if (std_date.NotEmpty()) {
            date.Reset(new CDate);
            date->SetStd(*std_date);
        }
    }

    if (date.NotEmpty()) {
        CRef<CSeqdesc> descr(new CSeqdesc);
        descr->SetUpdate_date(*date);
        bioseq.SetDescr().Set().push_back(descr);
    }
}

/**********************************************************/
static void GenBankGetDivision(char* division, Int4 div, const DataBlk& entry)
{
    StringNCpy(division, GBDivOffset(entry, div), 3);
    division[3] = '\0';
}

static void xGenBankGetDivision(char* division, Int4 div, const string& locusText)
{
    StringCpy(division, locusText.substr(64, 3).c_str());
}

CRef<CSeq_entry> CGenbank2Asn::xGetEntry()
{
    CRef<CSeq_entry> pResult;
    if (mParser.curindx >= mParser.indx) {
        return pResult;
    }

    int         i = mParser.curindx;
    bool        seq_long = false;
    IndexblkPtr ibp      = mParser.entrylist[i];

    err_install(ibp, mParser.accver);

    if (ibp->drop) {
        FtaErrPost(SEV_ERROR, ERR_ENTRY_Skipped, "Entry skipped: \"{}|{}\".", ibp->locusname, ibp->acnum);
        mTotals.Dropped++;
        // continue;
        mParser.curindx++;
        return pResult;
    }

    auto pEntry(LoadEntry(&mParser, ibp));
    if (! pEntry) {
        FtaDeletePrefix(PREFIX_LOCUS | PREFIX_ACCESSION);
        NCBI_THROW(CException, eUnknown, "Unable to load entry");
    }

    EntryBlkPtr ebp   = pEntry->GetEntryData();
    char*       ptr   = pEntry->mBuf.ptr;
    char*       eptr  = ptr + pEntry->mBuf.len;
    Int2        curkw = ParFlat_LOCUS;
    while (curkw != ParFlat_END && ptr < eptr) {
        ptr = GetGenBankBlock(ebp->chain, ptr, &curkw, eptr);
    }

    auto ppCurrentEntry = mParser.entrylist[mParser.curindx];
    if (ppCurrentEntry->lc.div > -1) {
        GenBankGetDivision(ppCurrentEntry->division, ppCurrentEntry->lc.div, *pEntry);
        if (StringEqu(ibp->division, "TSA")) {
            if (ibp->tsa_allowed == false)
                FtaErrPost(SEV_WARNING, ERR_TSA_UnexpectedPrimaryAccession, "The record with accession \"{}\" is not expected to have a TSA division code.", ibp->acnum);
            ibp->is_tsa = true;
        }
    }

    CheckContigEverywhere(ibp, mParser.source);
    if (ibp->drop) {
        FtaErrPost(SEV_ERROR, ERR_ENTRY_Skipped, "Entry skipped: \"{}|{}\".", ibp->locusname, ibp->acnum);
        mTotals.Dropped++;
        mParser.curindx++;
        return pResult;
    }

    if (ptr >= eptr) {
        ibp->drop = true;
        FtaErrPost(SEV_ERROR, ERR_FORMAT_MissingEnd, "Missing end of the entry. Entry dropped.");
        FtaErrPost(SEV_ERROR, ERR_ENTRY_Skipped, "Entry skipped: \"{}|{}\".", ibp->locusname, ibp->acnum);
        mTotals.Dropped++;
        mParser.curindx++;
        return pResult;
    }
    GetGenBankSubBlock(*pEntry, ibp->bases);

    CRef<CBioseq> bioseq = CreateEntryBioseq(&mParser);
    ebp->seq_entry.Reset(new CSeq_entry);
    ebp->seq_entry->SetSeq(*bioseq);

    AddNIDSeqId(*bioseq, *pEntry, ParFlat_NCBI_GI, ParFlat_COL_DATA, mParser.source);

    ibp->is_prot = fta_StartsWith(pEntry->mBuf.ptr + ibp->lc.bp, "aa"sv);

    unsigned char* const conv = ibp->is_prot ? GetProtConvTable() : GetDNAConvTable();


    if (! GetGenBankInst(&mParser, *pEntry, conv)) {
        ibp->drop = true;
        FtaErrPost(SEV_REJECT, ERR_SEQUENCE_BadData, "Bad sequence data. Entry dropped.");
        FtaErrPost(SEV_ERROR, ERR_ENTRY_Skipped, "Entry skipped: \"{}|{}\".", ibp->locusname, ibp->acnum);
        mTotals.Dropped++;
        mParser.curindx++;
        return pResult;
    }

    FakeGenBankBioSources(*pEntry, *bioseq);
    
    GetScope().AddBioseq(*bioseq);
    LoadFeat(&mParser, *pEntry, *bioseq);

    if (! bioseq->IsSetAnnot() && ibp->drop) {
        FtaErrPost(SEV_ERROR, ERR_ENTRY_Skipped, "Entry skipped: \"{}|{}\".", ibp->locusname, ibp->acnum);
        mTotals.Dropped++;
        mParser.curindx++;
        return pResult;
    }

    GetGenBankDescr(&mParser, *pEntry, *bioseq);
    if (ibp->drop) {
        FtaErrPost(SEV_ERROR, ERR_ENTRY_Skipped, "Entry skipped: \"{}|{}\".", ibp->locusname, ibp->acnum);
        mTotals.Dropped++;
        mParser.curindx++;
        return pResult;
    }

    fta_set_molinfo_completeness(*bioseq, ibp);

    if (ibp->is_tsa)
        fta_tsa_tls_comment_dblink_check(*bioseq, true);

    if (ibp->is_tls)
        fta_tsa_tls_comment_dblink_check(*bioseq, false);

    if (bioseq->GetInst().IsNa()) {
        if (bioseq->GetInst().GetRepr() == CSeq_inst::eRepr_raw) {
            if (! ibp->gaps.empty())
                GapsToDelta(*bioseq, ibp->gaps, &ibp->drop);
            else if (ibp->htg == 4 || ibp->htg == 1 || ibp->htg == 2 ||
                     (ibp->is_pat && mParser.source == Parser::ESource::DDBJ))
                SeqToDelta(*bioseq, ibp->htg);
        } else if (! ibp->gaps.empty())
            AssemblyGapsToDelta(*bioseq, ibp->gaps, &ibp->drop);
    }

    if (no_date(mParser.format, bioseq->GetDescr().Get()) && mParser.debug == false &&
        mParser.no_date == false &&
        mParser.mode != Parser::EMode::Relaxed) {
        ibp->drop = true;
        FtaErrPost(SEV_ERROR, ERR_DATE_IllegalDate, "Illegal create date. Entry dropped.");
        FtaErrPost(SEV_ERROR, ERR_ENTRY_Skipped, "Entry skipped: \"{}|{}\".", ibp->locusname, ibp->acnum);
        mTotals.Dropped++;
        mParser.curindx++;
        return pResult;
    }

    if (pEntry->mpQscore.empty() && mParser.accver) {
        if (mParser.ff_get_qscore)
            pEntry->mpQscore = (*mParser.ff_get_qscore)(ibp->acnum, ibp->vernum);
        else if (mParser.ff_get_qscore_pp)
            pEntry->mpQscore = (*mParser.ff_get_qscore_pp)(ibp->acnum, ibp->vernum, &mParser);
        if (mParser.qsfd && ibp->qslength > 0)
            pEntry->mpQscore = GetQSFromFile(mParser.qsfd, ibp);
    }

    if (! QscoreToSeqAnnot(pEntry->mpQscore, *bioseq, ibp->acnum, ibp->vernum, false, true)) {
        if (mParser.ign_bad_qs == false) {
            ibp->drop = true;
            FtaErrPost(SEV_ERROR, ERR_QSCORE_FailedToParse, "Error while parsing QScore. Entry dropped.");
            FtaErrPost(SEV_ERROR, ERR_ENTRY_Skipped, "Entry skipped: \"{}|{}\".", ibp->locusname, ibp->acnum);
            mTotals.Dropped++;
            mParser.curindx++;
            return pResult;
        } else {
            FtaErrPost(SEV_ERROR, ERR_QSCORE_FailedToParse, "Error while parsing QScore.");
        }
    }

    pEntry->mpQscore.clear();

    if (ibp->psip.NotEmpty()) {
        CRef<CSeq_id> id(new CSeq_id);
        id->SetPatent(*ibp->psip);
        bioseq->SetId().push_back(id);
        ibp->psip.Reset();
    }

    /* add PatentSeqId if patent is found in reference
     */
    if (mParser.mode != Parser::EMode::Relaxed &&
        mParser.debug == false &&
        ibp->wgs_and_gi != 3 &&
        no_reference(*bioseq)) {
        if (mParser.source == Parser::ESource::Refseq &&
                   fta_StartsWith(ibp->acnum, "NW_"sv)) {
            FtaErrPost(SEV_ERROR, ERR_REFERENCE_No_references, "No references for RefSeq's NW_ entry. Continue anyway.");
        } else if (ibp->is_wgs) {
            FtaErrPost(SEV_ERROR, ERR_REFERENCE_No_references, "No references for WGS entry. Continue anyway.");
        } else {
            ibp->drop = true;
            FtaErrPost(SEV_ERROR, ERR_REFERENCE_No_references, "No references. Entry dropped.");
            FtaErrPost(SEV_ERROR, ERR_ENTRY_Skipped, "Entry skipped: \"{}|{}\".", ibp->locusname, ibp->acnum);
            mTotals.Dropped++;
            mParser.curindx++;
            return pResult;
        }
    }


    DealWithGenes(ebp->seq_entry, &mParser);

    if (ebp->seq_entry.Empty()) {
        FtaErrPost(SEV_ERROR, ERR_ENTRY_Skipped, "Entry skipped: \"{}|{}\".", ibp->locusname, ibp->acnum);
        mTotals.Dropped++;
        mParser.curindx++;
        return pResult;
    }

    TEntryList  seq_entries;
    seq_entries.push_back(ebp->seq_entry);
    ebp->seq_entry.Reset();

    /* change qual "citation" on features to SeqFeat.cit
     * find citation in the list by serial_number.
     * If serial number not found remove /citation
     */
    ProcessCitations(seq_entries);

    /* check for long sequences in each segment */
    if (mParser.limit != 0) {
        if (ibp->bases > (size_t)mParser.limit) {
            if (ibp->htg == 1 || ibp->htg == 2 || ibp->htg == 4) {
                FtaErrPost(SEV_WARNING, ERR_ENTRY_LongHTGSSequence, "HTGS Phase 0/1/2 sequence {}|{} exceeds length limit {}: entry has been processed regardless of this problem", ibp->locusname, ibp->acnum, mParser.limit);
            } else {
                seq_long = true;
                FtaErrPost(SEV_REJECT, ERR_ENTRY_LongSequence, "Sequence {}|{} is longer than limit {}", ibp->locusname, ibp->acnum, mParser.limit);
            }
        }
    }

    if (mParser.mode == Parser::EMode::Relaxed) {
        for (auto pSeqEntry : seq_entries) {
            auto pScope = Ref(new CScope(*CObjectManager::GetInstance()));
            g_InstantiateMissingProteins(pScope->AddTopLevelSeqEntry(*pSeqEntry));
        }
    }
    if (mParser.convert) {
        if (mParser.cleanup <= 1) {
            FinalCleanup(seq_entries);

            if (mParser.qamode && ! seq_entries.empty())
                fta_remove_cleanup_user_object(*seq_entries.front());
        }

        MaybeCutGbblockSource(seq_entries);
    }

    EntryCheckDivCode(seq_entries, &mParser);

    // if () fta_set_strandedness(seq_entries);

    if (fta_EntryCheckGBBlock(seq_entries)) {
        FtaErrPost(SEV_WARNING, ERR_ENTRY_GBBlock_not_Empty, "Attention: GBBlock is not empty");
    }

    /* check for identical features
     */
    if (mParser.qamode) {
        fta_sort_descr(seq_entries);
        fta_sort_seqfeat_cit(seq_entries);
    }

    if (mParser.citat) {
        StripSerialNumbers(seq_entries);
    }

    PackEntries(seq_entries);
    CheckDupDates(seq_entries);

    if (seq_long) {
        mTotals.Long++;
    } else {
        mTotals.Succeeded++;
        pResult = seq_entries.front();
    }
    GetScope().ResetDataAndHistory();

    FtaErrPost(SEV_INFO, ERR_ENTRY_Parsed, "OK - entry parsed successfully: \"{}|{}\".", ibp->locusname, ibp->acnum);
    seq_entries.clear();

    mParser.curindx++;
    return pResult;
}


void CGenbank2Asn::PostTotals() 
{
    FtaErrPost(SEV_INFO, ERR_ENTRY_ParsingComplete, 
        "COMPLETED : SUCCEEDED = {} (including: LONG ones = {}); SKIPPED = {}.", 
        mTotals.Succeeded, mTotals.Long, mTotals.Dropped);
}

END_NCBI_SCOPE
