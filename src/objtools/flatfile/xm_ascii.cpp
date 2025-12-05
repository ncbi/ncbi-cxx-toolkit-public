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
 * File Name: xm_ascii.cpp
 *
 * Author: Sergey Bazhin
 *
 * File Description:
 *      Parse INSDSEQ from blocks to asn.
 * Build XML format entry block.
 *
 */

#include <ncbi_pch.hpp>

#include "xm_ascii.h"

#include "ftacpp.hpp"

#include <objects/seq/Seq_inst.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objmgr/scope.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/general/User_object.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqcode/Seq_code_type.hpp>
#include <objects/seqblock/EMBL_block.hpp>
#include <objects/seq/Pubdesc.hpp>
#include <serial/objostr.hpp>


#include "index.h"

#include "ftanet.h"
#include <objtools/flatfile/flatfile_parser.hpp>
#include <objtools/flatfile/flatdefn.h>

#include "ftaerr.hpp"
#include "indx_blk.h"
#include "asci_blk.h"
#include "utilref.h"
#include "utilfeat.h"
#include "loadfeat.h"
#include "add.h"
#include "gb_ascii.h"
#include "nucprot.h"
#include "fta_qscore.h"
#include "em_ascii.h"
#include "citation.h"
#include "fcleanup.h"
#include "utilfun.h"
#include "ref.h"
#include "xgbparint.h"
#include "xutils.h"
#include "fta_xml.h"

#ifdef THIS_FILE
#  undef THIS_FILE
#endif
#define THIS_FILE "xm_ascii.cpp"

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

/**********************************************************/
static void XMLCheckContigEverywhere(IndexblkPtr ibp, Parser::ESource source)
{
    if (! ibp)
        return;

    bool condiv = NStr::EqualNocase(ibp->division, "CON");

    if (! condiv && ibp->is_contig == false && ibp->origin == false) {
        FtaErrPost(SEV_ERROR, ERR_FORMAT_MissingSequenceData, "Required sequence data is absent. Entry dropped.");
        ibp->drop = true;
    } else if (! condiv && ibp->is_contig && ibp->origin == false) {
        FtaErrPost(SEV_WARNING, ERR_DIVISION_MappedtoCON, "Division [{}] mapped to CON based on the existence of <INSDSeq_contig> line.", ibp->division);
    } else if (ibp->is_contig && ibp->origin) {
        if (source == Parser::ESource::EMBL || source == Parser::ESource::DDBJ) {
            FtaErrPost(SEV_INFO, ERR_FORMAT_ContigWithSequenceData, "The <INSDSeq_contig> linetype and sequence data are both present. Ignoring sequence data.");
        } else {
            FtaErrPost(SEV_REJECT, ERR_FORMAT_ContigWithSequenceData, "The <INSDSeq_contig> linetype and sequence data may not both be present in a sequence record.");
            ibp->drop = true;
        }
    } else if (condiv && ibp->is_contig == false && ibp->origin == false) {
        FtaErrPost(SEV_ERROR, ERR_FORMAT_MissingContigFeature, "No <INSDSeq_contig> data in XML format file. Entry dropped.");
        ibp->drop = true;
    } else if (condiv && ibp->is_contig == false && ibp->origin) {
        FtaErrPost(SEV_WARNING, ERR_DIVISION_ConDivLacksContig, "Division is CON, but <INSDSeq_contig> data have not been found.");
    }
}

/**********************************************************/
static bool XMLGetInstContig(const TXmlIndexList& xil, const DataBlk& dbp, CBioseq& bioseq, ParserPtr pp)
{
    char* p;
    char* q;
    char* r;
    bool  locmap;
    bool  allow_crossdb_featloc;
    Int4  i;
    int   numerr;

    p = StringSave(XMLFindTagValue(dbp.mBuf.ptr, xil, INSDSEQ_CONTIG));
    if (! p)
        return false;

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

    CRef<CSeq_loc> loc = xgbparseint_ver(p, locmap, numerr, bioseq.GetId(), pp->accver);

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
        fta_create_far_fetch_policy_user_object(bioseq, i);

    pp->allow_crossdb_featloc = allow_crossdb_featloc;

    if (loc->IsMix()) {
        XGappedSeqLocsToDeltaSeqs(loc->GetMix(), bioseq.SetInst().SetExt().SetDelta().Set());
        bioseq.SetInst().SetRepr(CSeq_inst::eRepr_delta);
    } else
        bioseq.SetInst().ResetExt();

    MemFree(p);

    return true;
}

/**********************************************************/
bool XMLGetInst(ParserPtr pp, const DataBlk& dbp, unsigned char* dnaconv, CBioseq& bioseq)
{
    IndexblkPtr ibp;

    unique_ptr<string> topstr;
    unique_ptr<string> strandstr;

    ibp = pp->entrylist[pp->curindx];
    for (const auto& xip : ibp->xip) {
        if (xip.tag == INSDSEQ_TOPOLOGY && ! topstr)
            topstr = XMLGetTagValue(dbp.mBuf.ptr, xip);
        else if (xip.tag == INSDSEQ_STRANDEDNESS && ! strandstr)
            strandstr = XMLGetTagValue(dbp.mBuf.ptr, xip);
    }

    CSeq_inst& inst = bioseq.SetInst();
    inst.SetRepr(CSeq_inst::eRepr_raw);

    /* get linear, circular, tandem topology, blank is linear which = 1
     */
    if (topstr) {
        auto topology = XMLCheckTPG(*topstr);
        if (topology > 1)
            inst.SetTopology(static_cast<CSeq_inst::ETopology>(topology));
        topstr.reset();
    }

    if (strandstr) {
        auto strand = XMLCheckSTRAND(*strandstr);
        if (strand > 0)
            inst.SetStrand(static_cast<CSeq_inst::EStrand>(strand));
        strandstr.reset();
    }

    if (! GetSeqData(pp, dbp, bioseq, 0, dnaconv, ibp->is_prot ? eSeq_code_type_iupacaa : eSeq_code_type_iupacna))
        return false;

    if (ibp->is_contig && ! XMLGetInstContig(ibp->xip, dbp, bioseq, pp))
        return false;

    return true;
}

/**********************************************************/
static CRef<CGB_block> XMLGetGBBlock(ParserPtr pp, const char* entry, CMolInfo& mol_info, CBioSource* bio_src)
{
    CRef<CGB_block> gbb(new CGB_block),
        ret;

    IndexblkPtr ibp;
    char*       bptr;
    char        msg[4];
    Int2        div;
    bool        if_cds;

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

    bool  cancelled;
    bool  drop;
    char* tempdiv;
    Int2  thtg;
    char* p;
    Int4  i;

    ibp = pp->entrylist[pp->curindx];

    ibp->wgssec[0] = '\0';

    if((ibp->biodrop == false || pp->source != Parser::ESource::USPTO ||
        pp->format != Parser::EFormat::XML || pp->taxserver == 0) &&
       ibp->no_gbblock_source == false)
    {
        if (char* str = StringSave(XMLFindTagValue(entry, ibp->xip, INSDSEQ_SOURCE))) {
            p = StringRChr(str, '.');
            if (p && p > str && p[1] == '\0' && *(p - 1) == '.')
                *p = '\0';

            gbb->SetSource(str);
            MemFree(str);
        }
    }

    if (! ibp->keywords.empty()) {
        gbb->SetKeywords().swap(ibp->keywords);
        ibp->keywords.clear();
    } else
        XMLGetKeywords(entry, ibp->xip, gbb->SetKeywords());

    if (ibp->is_mga && ! fta_check_mga_keywords(mol_info, gbb->GetKeywords())) {
        return ret;
    }

    if (ibp->is_tpa && ! fta_tpa_keywords_check(gbb->SetKeywords())) {
        return ret;
    }

    if (ibp->is_tsa && ! fta_tsa_keywords_check(gbb->SetKeywords(), pp->source)) {
        return ret;
    }

    if (ibp->is_tls && ! fta_tls_keywords_check(gbb->SetKeywords(), pp->source)) {
        return ret;
    }

    for (const string& key : gbb->GetKeywords()) {
        fta_keywords_check(key, &est_kwd, &sts_kwd, &gss_kwd, &htc_kwd, &fli_kwd, &wgs_kwd, &tpa_kwd, &env_kwd, &mga_kwd, &tsa_kwd, &tls_kwd);
    }

    if (ibp->env_sample_qual == false && env_kwd) {
        FtaErrPost(SEV_REJECT, ERR_KEYWORD_ENV_NoMatchingQualifier, "This record utilizes the ENV keyword, but there are no /environmental_sample qualifiers among its source features.");
        return ret;
    }

    bptr = StringSave(XMLFindTagValue(entry, ibp->xip, INSDSEQ_DIVISION));
    if (bptr) {
        if_cds = XMLCheckCDS(entry, ibp->xip);
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
                gbb->SetDiv("");

            XMLDefVsHTGKeywords(mol_info.GetTech(), entry, ibp->xip, cancelled);

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
                FtaErrPost(SEV_REJECT, ERR_KEYWORD_NoGeneExpressionKeywords, "This is apparently a CAGE or 5'-SAGE record, but it lacks the required keywords. Entry dropped.");
                return ret;
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
                else if (i != 2 || env_kwd == false ||
                         (est_kwd == false && gss_kwd == false && wgs_kwd == false)) {
                    FtaErrPost(SEV_REJECT, ERR_KEYWORD_ConflictingKeywords, "This record contains more than one of the special keywords used to indicate that a sequence is an HTG, EST, GSS, STS, HTC, WGS, ENV, FLI_CDNA, TPA, CAGE, TSA or TLS sequence.");
                    return ret;
                }
            }

            if (wgs_kwd)
                i--;
            if (ibp->is_contig && i > 0 &&
                wgs_kwd == false && tpa_kwd == false && env_kwd == false) {
                FtaErrPost(SEV_REJECT, ERR_KEYWORD_IllegalForCON, "This CON record should not have HTG, EST, GSS, STS, HTC, FLI_CDNA, CAGE, TSA or TLS special keywords. Entry dropped.");
                return ret;
            }

            thtg = mol_info.GetTech();
            if (thtg == CMolInfo::eTech_htgs_0 || thtg == CMolInfo::eTech_htgs_1 ||
                thtg == CMolInfo::eTech_htgs_2 || thtg == CMolInfo::eTech_htgs_3) {
                RemoveHtgPhase(gbb->SetKeywords());
            }

            if (auto kw = XMLConcatSubTags(entry, ibp->xip, INSDSEQ_KEYWORDS, ';')) {
                if (! est_kwd && kw->find("EST") != string::npos) {
                    FtaErrPost(SEV_WARNING, ERR_KEYWORD_ESTSubstring, "Keyword {} has substring EST, but no official EST keywords found", *kw);
                }
                if (! sts_kwd && kw->find("STS") != string::npos) {
                    FtaErrPost(SEV_WARNING, ERR_KEYWORD_STSSubstring, "Keyword {} has substring STS, but no official STS keywords found", *kw);
                }
            }

            if (! ibp->is_contig) {
                drop                  = false;
                CMolInfo::TTech tech  = mol_info.GetTech();
                string          p_div = gbb->GetDiv();

                check_div(ibp->is_pat, pat_ref, est_kwd, sts_kwd, gss_kwd, if_cds, p_div, &tech, ibp->bases, pp->source, drop);

                if (tech != CMolInfo::eTech_unknown)
                    mol_info.SetTech(tech);
                else
                    mol_info.ResetTech();

                if (! p_div.empty())
                    gbb->SetDiv(p_div);
                else
                    gbb->SetDiv("");

                if (drop) {
                    MemFree(bptr);
                    return ret;
                }
            } else if (gbb->GetDiv() == "CON") {
                gbb->SetDiv("");
            }
        } else {
            MemCpy(msg, bptr, 3);
            msg[3] = '\0';
            FtaErrPost(SEV_REJECT, ERR_DIVISION_UnknownDivCode, "Unknown division code \"{}\" found in GenBank flatfile. Record rejected.", msg);
            MemFree(bptr);
            return ret;
        }

        if (IsNewAccessFormat(ibp->acnum) == 0 && *ibp->acnum == 'T' &&
            gbb->GetDiv() != "EST") {
            FtaErrPost(SEV_INFO, ERR_DIVISION_MappedtoEST, "Leading T in accession number.");
            mol_info.SetTech(CMolInfo::eTech_est);

            gbb->SetDiv("");
        }

        MemFree(bptr);
    }

    bool is_htc_div = gbb->GetDiv() == "HTC",
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
        char* str = StringSave(XMLFindTagValue(entry, ibp->xip, INSDSEQ_MOLTYPE));
        if (str) {
            p = str;
            if (*str == 'm' || *str == 'r')
                p = str + 1;
            else if (StringEquN(str, "pre-", 4))
                p = str + 4;
            else if (StringEquN(str, "transcribed ", 12))
                p = str + 12;

            if (! fta_StartsWith(p, "RNA"sv)) {
                FtaErrPost(SEV_ERROR, ERR_DIVISION_HTCWrongMolType, "All HTC division records should have a moltype of pre-RNA, mRNA or RNA.");
                MemFree(str);
                return ret;
            }
            MemFree(str);
        }
    }

    if (fli_kwd)
        mol_info.SetTech(CMolInfo::eTech_fli_cdna);

    /* will be used in flat file database
     */
    if (! gbb->GetDiv().empty()) {
        if (gbb->GetDiv() == "EST") {
            ibp->EST = true;
            mol_info.SetTech(CMolInfo::eTech_est);
            gbb->SetDiv("");
        } else if (gbb->GetDiv() == "STS") {
            ibp->STS = true;
            mol_info.SetTech(CMolInfo::eTech_sts);
            gbb->SetDiv("");
        } else if (gbb->GetDiv() == "GSS") {
            ibp->GSS = true;
            mol_info.SetTech(CMolInfo::eTech_survey);
            gbb->SetDiv("");
        } else if (gbb->GetDiv() == "HTC") {
            ibp->HTC = true;
            mol_info.SetTech(CMolInfo::eTech_htc);
            gbb->SetDiv("");
        } else if (gbb->GetDiv() == "SYN" && bio_src && bio_src->IsSetOrigin() &&
                   bio_src->GetOrigin() == 5) /* synthetic */
        {
            gbb->SetDiv("");
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

    if (bio_src && bio_src->IsSetSubtype()) {
        for (const auto& subtype : bio_src->GetSubtype()) {
            if (subtype->GetSubtype() == CSubSource::eSubtype_environmental_sample) {
                fta_remove_env_keywords(gbb->SetKeywords());
                break;
            }
        }
    }

    GetExtraAccession(ibp, pp->allow_uwsec, pp->source, gbb->SetExtra_accessions());

    if (gbb->IsSetDiv() &&
        bio_src &&
        bio_src->IsSetOrg() &&
        bio_src->GetOrg().IsSetOrgname() &&
        bio_src->GetOrg().GetOrgname().IsSetDiv() &&
        bio_src->GetOrg().GetOrgname().GetDiv() == gbb->GetDiv()) {
        gbb->ResetDiv();
    }

    return gbb;
}

/**********************************************************/
static CRef<CMolInfo> XMLGetMolInfo(ParserPtr pp, const DataBlk& entry, COrg_ref* org_ref)
{
    IndexblkPtr ibp;

    char* div;
    char* molstr;

    ibp = pp->entrylist[pp->curindx];

    CRef<CMolInfo> mol_info(new CMolInfo);

    molstr = StringSave(XMLFindTagValue(entry.mBuf.ptr, ibp->xip, INSDSEQ_MOLTYPE));
    div    = StringSave(XMLFindTagValue(entry.mBuf.ptr, ibp->xip, INSDSEQ_DIVISION));

    if (fta_StartsWith(div, "EST"sv))
        mol_info->SetTech(CMolInfo::eTech_est);
    else if (fta_StartsWith(div, "STS"sv))
        mol_info->SetTech(CMolInfo::eTech_sts);
    else if (fta_StartsWith(div, "GSS"sv))
        mol_info->SetTech(CMolInfo::eTech_survey);
    else if (fta_StartsWith(div, "HTG"sv))
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

    MemFree(div);
    GetFlatBiomol(mol_info->SetBiomol(), mol_info->GetTech(), molstr, pp, entry, org_ref);
    if (mol_info->GetBiomol() == CMolInfo::eBiomol_unknown) // not set
        mol_info->ResetBiomol();

    if (molstr)
        MemFree(molstr);

    return mol_info;
}

/**********************************************************/
static void XMLFakeBioSources(const TXmlIndexList& xil, const char* entry, CBioseq& bioseq, Parser::ESource source)
{
    unique_ptr<string> organism;
    unique_ptr<string> taxonomy;

    const char* p;

    for (const auto& xip : xil) {
        if (xip.tag == INSDSEQ_ORGANISM && ! organism)
            organism = XMLGetTagValue(entry, xip);
        else if (xip.tag == INSDSEQ_TAXONOMY && ! taxonomy)
            taxonomy = XMLGetTagValue(entry, xip);
    }

    if (! organism) {
        FtaErrPost(SEV_WARNING, ERR_ORGANISM_NoOrganism, "No <INSDSeq_organism> data in XML format file.");
        return;
    }

    CRef<CBioSource> bio_src(new CBioSource);

    p = organism->c_str();
    if (GetGenomeInfo(*bio_src, p) && bio_src->GetGenome() != CBioSource::eGenome_plasmid) {
        while (*p != ' ' && *p != '\0')
            p++;
        while (*p == ' ')
            p++;
    }

    COrg_ref& org_ref = bio_src->SetOrg();

    if (source == Parser::ESource::EMBL) {
        const char* q = StringChr(p, '(');
        if (q && q > p) {
            for (q--; *q == ' ' || *q == '\t'; q--)
                if (q == p)
                    break;
            if (*q != ' ' && *q != '\t')
                q++;
            if (q > p) {
                org_ref.SetCommon(string(p, q));
            }
        }
    }

    org_ref.SetTaxname(p);
    organism.reset();

    if (org_ref.GetTaxname() == "Unknown.") {
        string& taxname = org_ref.SetTaxname();
        taxname.pop_back();
    }

    if (taxonomy) {
        org_ref.SetOrgname().SetLineage(*taxonomy);
    }

    CRef<CSeqdesc> descr(new CSeqdesc);
    descr->SetSource(*bio_src);
    bioseq.SetDescr().Set().push_back(descr);
}

/**********************************************************/
static void XMLGetDescrComment(string& str)
{
    {
        size_t j = 0;
        for (char c : str)
            if (c == '\n' || c == ' ')
                j++;
            else
                break;
        if (j > 0)
            str.erase(0, j);
    }

    string com;
    com.reserve(str.size());
    for (auto q = str.begin(); q != str.end();) {
        if (*q != '\n') {
            com += *q++;
            continue;
        }

        com += '~';
        q++;
        while (q != str.end() && *q == ' ')
            q++;
    }

    for (size_t i = 0;;) {
        i = com.find("; ", i);
        if (i == string::npos)
            break;
        i += 2;
        size_t j = i;
        while (j < com.size() && com[j] == ' ')
            j++;
        if (j > i)
            com.erase(i, j - i);
    }

    {
        size_t j = 0;
        for (char c : com)
            if (c == ' ')
                j++;
            else
                break;
        if (j > 0)
            com.erase(0, j);
    }

    if (! com.empty()) {
        size_t i = com.size();
        for (auto rit = com.rbegin(); rit != com.crend(); ++rit) {
            char c = *rit;
            if (c == ' ' || c == '\t' || c == ';' || c == ',' ||
                c == '.' || c == '~') {
                --i;
                continue;
            }
            break;
        }
        if (i > 0) {
            string_view tail(com.begin() + i, com.end());
            if (tail.starts_with("..."))
                com.resize(i + 3);
            else if (fta_contains(tail, ".")) {
                com[i] = '.';
                com.resize(i + 1);
            } else
                com.resize(i);
        } else
            com.clear();
    }

    str.swap(com);
}

/**********************************************************/
static void XMLGetDescr(ParserPtr pp, const DataBlk& entry, CBioseq& bioseq)
{
    IndexblkPtr ibp;

    char*  crdate;
    char*  update;
    char*  offset;
    char*  str;
    char*  p;
    string gbdiv;

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
    CRef<CMolInfo> mol_info = XMLGetMolInfo(pp, entry, org_ref);

    /* DEFINITION data ==> descr_title
     */
    str = StringSave(XMLFindTagValue(entry.mBuf.ptr, ibp->xip, INSDSEQ_DEFINITION));
    string title;

    if (str) {
        for (p = str; *p == ' ';)
            p++;
        if (p > str)
            fta_StringCpy(str, p);
        if (pp->xml_comp && pp->source != Parser::ESource::EMBL) {
            p = StringRChr(str, '.');
            if (! p || p[1] != '\0') {
                string s = str;
                s += '.';
                MemFree(str);
                str = StringSave(s);
                p   = nullptr;
            }
        }

        title = str;
        MemFree(str);
        str = nullptr;

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

    if (ibp->is_tpa &&
        (title.empty() || ! title.starts_with("TPA:"sv))) {
        FtaErrPost(SEV_REJECT, ERR_DEFINITION_MissingTPA, "This is apparently a TPA record, but it lacks the required \"TPA:\" prefix on its definition line. Entry dropped.");
        ibp->drop = true;
        return;
    }

    if (ibp->is_tsa &&
        (title.empty() || ! title.starts_with("TSA:"))) {
        FtaErrPost(SEV_REJECT, ERR_DEFINITION_MissingTSA, "This is apparently a TSA record, but it lacks the required \"TSA:\" prefix on its definition line. Entry dropped.");
        ibp->drop = true;
        return;
    }

    if (ibp->is_tls &&
        (title.empty() || ! title.starts_with("TLS:"sv))) {
        FtaErrPost(SEV_REJECT, ERR_DEFINITION_MissingTLS, "This is apparently a TLS record, but it lacks the required \"TLS:\" prefix on its definition line. Entry dropped.");
        ibp->drop = true;
        return;
    }

    /* REFERENCE
     */
    /* pub should be before GBblock because we need patent ref
     */
    TDataBlkList dbl = XMLBuildRefDataBlk(entry.mBuf.ptr, ibp->xip, ParFlat_REF_END);
    for (auto& dbp : dbl) {
        CRef<CPubdesc> pubdesc = DescrRefs(pp, dbp, 0);
        if (pubdesc.NotEmpty()) {
            CRef<CSeqdesc> descr(new CSeqdesc);
            descr->SetPub(*pubdesc);
            bioseq.SetDescr().Set().push_back(descr);
        }
    }
    dbl.clear();

    dbl = XMLBuildRefDataBlk(entry.mBuf.ptr, ibp->xip, ParFlat_REF_NO_TARGET);
    for (auto& dbp : dbl) {
        CRef<CPubdesc> pubdesc = DescrRefs(pp, dbp, 0);
        if (pubdesc.NotEmpty()) {
            CRef<CSeqdesc> descr(new CSeqdesc);
            descr->SetPub(*pubdesc);
            bioseq.SetDescr().Set().push_back(descr);
        }
    }
    dbl.clear();

    TStringList dr_ena,
        dr_biosample;

    CRef<CEMBL_block> embl;
    CRef<CGB_block>   gbb;

    if (pp->source == Parser::ESource::EMBL)
        embl = XMLGetEMBLBlock(pp, entry.mBuf.ptr, *mol_info, gbdiv, bio_src, dr_ena, dr_biosample);
    else
        gbb = XMLGetGBBlock(pp, entry.mBuf.ptr, *mol_info, bio_src);

    CRef<CUser_object> dbuop;
    if (! dr_ena.empty() || ! dr_biosample.empty())
        fta_build_ena_user_object(bioseq.SetDescr().Set(), dr_ena, dr_biosample, dbuop);

    if (mol_info->IsSetBiomol() || mol_info->IsSetTech()) {
        CRef<CSeqdesc> descr(new CSeqdesc);
        descr->SetMolinfo(*mol_info);
        bioseq.SetDescr().Set().push_back(descr);
    }

    if (pp->source == Parser::ESource::EMBL) {
        if (embl.Empty()) {
            ibp->drop = true;
            return;
        }
    } else if (gbb.Empty()) {
        ibp->drop = true;
        return;
    }

    if (pp->source == Parser::ESource::EMBL) {
        if (NStr::StartsWith(ibp->division, "CON"sv, NStr::eNocase))
            fta_add_hist(pp, bioseq, embl->SetExtra_acc(), Parser::ESource::EMBL, CSeq_id::e_Embl, true, ibp->acnum);
        else
            fta_add_hist(pp, bioseq, embl->SetExtra_acc(), Parser::ESource::EMBL, CSeq_id::e_Embl, false, ibp->acnum);

        if (embl->GetExtra_acc().empty())
            embl->ResetExtra_acc();
    } else {
        if (NStr::StartsWith(ibp->division, "CON"sv, NStr::eNocase))
            fta_add_hist(pp, bioseq, gbb->SetExtra_accessions(), Parser::ESource::DDBJ, CSeq_id::e_Ddbj, true, ibp->acnum);
        else
            fta_add_hist(pp, bioseq, gbb->SetExtra_accessions(), Parser::ESource::DDBJ, CSeq_id::e_Ddbj, false, ibp->acnum);
    }

    if (pp->source == Parser::ESource::EMBL) {
        if (! gbdiv.empty()) {
            gbb.Reset(new CGB_block);
            gbb->SetDiv(gbdiv);
            gbdiv.clear();
        }

        CRef<CSeqdesc> descr(new CSeqdesc);
        descr->SetEmbl(*embl);
        bioseq.SetDescr().Set().push_back(descr);
    }

    offset = StringSave(XMLFindTagValue(entry.mBuf.ptr, ibp->xip, INSDSEQ_PRIMARY));
    if (! offset && ibp->is_tpa && ibp->is_wgs == false) {
        if (ibp->inferential || ibp->experimental) {
            if (! fta_dblink_has_sra(dbuop) &&
                ! XMLFindTagValue(entry.mBuf.ptr, ibp->xip, INSDSEQ_COMMENT)) {
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

    if (offset) {
        if (! fta_parse_tpa_tsa_block(bioseq, offset, ibp->acnum, ibp->vernum, 10, 0, ibp->is_tpa)) {
            ibp->drop = true;
            MemFree(offset);
            return;
        }
        MemFree(offset);
    }

    if (gbb.NotEmpty()) {
        if (pp->taxserver == 1 && gbb->IsSetDiv())
            fta_fix_orgref_div(bioseq.SetAnnot(), org_ref, *gbb);

        CRef<CSeqdesc> descr(new CSeqdesc);
        descr->SetGenbank(*gbb);
        bioseq.SetDescr().Set().push_back(descr);
    }

    /* COMMENT data
     */
    if (auto str = XMLFindTagValue(entry.mBuf.ptr, ibp->xip, INSDSEQ_COMMENT)) {
        string comment(*str);
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

        XMLGetDescrComment(comment);
        if (pp->xml_comp) {
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

    /* DATE
     */
    if (pp->no_date) /* -N in command line means no date */
        return;

    CRef<CDate_std> std_upd_date,
        std_cre_date;

    if (pp->date) /* -L in command line means replace
                                           date */
    {
        CTime cur_time(CTime::eCurrent);

        std_upd_date.Reset(new CDate_std);
        std_upd_date->SetToTime(cur_time);

        std_cre_date.Reset(new CDate_std);
        std_cre_date->SetToTime(cur_time);

        update = nullptr;
        crdate = nullptr;
    } else {
        update = StringSave(XMLFindTagValue(entry.mBuf.ptr, ibp->xip, INSDSEQ_UPDATE_DATE));
        if (update)
            std_upd_date = GetUpdateDate(update, pp->source);

        crdate = StringSave(XMLFindTagValue(entry.mBuf.ptr, ibp->xip, INSDSEQ_CREATE_DATE));
        if (crdate)
            std_cre_date = GetUpdateDate(crdate, pp->source);
    }

    if (std_upd_date.NotEmpty()) {
        CRef<CSeqdesc> descr(new CSeqdesc);
        descr->SetUpdate_date().SetStd(*std_upd_date);
        bioseq.SetDescr().Set().push_back(descr);

        if (std_cre_date.NotEmpty() && std_cre_date->Compare(*std_upd_date) == CDate::eCompare_after) {
            FtaErrPost(SEV_ERROR, ERR_DATE_IllegalDate, "Update-date \"{}\" precedes create-date \"{}\".", update, crdate);
        }
    }

    if (std_cre_date.NotEmpty()) {
        if (pp->xml_comp == false || pp->source == Parser::ESource::EMBL) {
            CRef<CSeqdesc> descr(new CSeqdesc);
            descr->SetCreate_date().SetStd(*std_cre_date);
            bioseq.SetDescr().Set().push_back(descr);
        }
    }

    if (update)
        MemFree(update);
    if (crdate)
        MemFree(crdate);
}

/**********************************************************/
static void XMLGetDivision(const char* entry, IndexblkPtr ibp)
{
    char* div;

    if (! ibp || ! entry)
        return;

    div = StringSave(XMLFindTagValue(entry, ibp->xip, INSDSEQ_DIVISION));
    if (! div)
        return;
    div[3] = '\0';
    StringCpy(ibp->division, div);
    MemFree(div);
}


CRef<CSeq_entry> CXml2Asn::xGetEntry()
{
    CRef<CSeq_entry> result;
    if (mParser.curindx >= mParser.indx) {
        return result;
    }

    const auto i = mParser.curindx;

    auto ibp = mParser.entrylist[i];

    err_install(ibp, mParser.accver);

    if (ibp->drop) {
        FtaErrPost(SEV_ERROR, ERR_ENTRY_Skipped, "Entry skipped: \"{}|{}\".", ibp->locusname, ibp->acnum);
        mTotals.Dropped++;
        mParser.curindx++;
        return result;
    }

    auto entry = XMLLoadEntry(&mParser, false);
    if (! entry) {
        FtaDeletePrefix(PREFIX_LOCUS | PREFIX_ACCESSION);
        NCBI_THROW(CException, eUnknown, "Unable to load entry");
    }

    XMLGetDivision(entry, ibp);

    if (StringEqu(ibp->division, "TSA")) {
        if (ibp->tsa_allowed == false)
            FtaErrPost(SEV_WARNING, ERR_TSA_UnexpectedPrimaryAccession, "The record with accession \"{}\" is not expected to have a TSA division code.", ibp->acnum);
        ibp->is_tsa = true;
    }

    XMLCheckContigEverywhere(ibp, mParser.source);
    if (ibp->drop) {
        FtaErrPost(SEV_ERROR, ERR_ENTRY_Skipped, "Entry skipped: \"{}|{}\".", ibp->locusname, ibp->acnum);
        MemFree(entry);
        mTotals.Dropped++;
        // continue;
        mParser.curindx++;
        return result;
    }

    auto ebp = new EntryBlk();

    CRef<CBioseq> bioseq = CreateEntryBioseq(&mParser);
    ebp->seq_entry.Reset(new CSeq_entry);
    ebp->seq_entry->SetSeq(*bioseq);

    unique_ptr<DataBlk> dbp(new DataBlk());
    dbp->SetEntryData(ebp);
    dbp->mBuf.ptr = entry;
    dbp->mBuf.len = StringLen(entry);

    if (! XMLGetInst(&mParser, *dbp, ibp->is_prot ? GetProtConvTable() : GetDNAConvTable(), *bioseq)) {
        ibp->drop = true;
        FtaErrPost(SEV_REJECT, ERR_SEQUENCE_BadData, "Bad sequence data. Entry dropped.");
        FtaErrPost(SEV_ERROR, ERR_ENTRY_Skipped, "Entry skipped: \"{}|{}\".", ibp->locusname, ibp->acnum);
        MemFree(entry);
        mTotals.Dropped++;
        mParser.curindx++;
        return result;
    }

    XMLFakeBioSources(ibp->xip, dbp->mBuf.ptr, *bioseq, mParser.source);
 
    GetScope().AddBioseq(*bioseq);

    LoadFeat(&mParser, *dbp, *bioseq); // uses scope to validate locations

    if (! bioseq->IsSetAnnot() && ibp->drop) {
        FtaErrPost(SEV_ERROR, ERR_ENTRY_Skipped, "Entry skipped: \"{}|{}\".", ibp->locusname, ibp->acnum);
        MemFree(entry);
        mTotals.Dropped++;
        mParser.curindx++;
        return result;
    }

    XMLGetDescr(&mParser, *dbp, *bioseq);

    if (ibp->drop) {
        FtaErrPost(SEV_ERROR, ERR_ENTRY_Skipped, "Entry skipped: \"{}|{}\".", ibp->locusname, ibp->acnum);
        MemFree(entry);
        mTotals.Dropped++;
        mParser.curindx++;
        return result;
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

    if (no_date(mParser.format, bioseq->GetDescr().Get()) &&
        mParser.debug == false && mParser.no_date == false &&
        mParser.xml_comp == false && mParser.source != Parser::ESource::USPTO) {
        ibp->drop = true;
        FtaErrPost(SEV_ERROR, ERR_DATE_IllegalDate, "Illegal create date. Entry dropped.");
        FtaErrPost(SEV_ERROR, ERR_ENTRY_Skipped, "Entry skipped: \"{}|{}\".", ibp->locusname, ibp->acnum);
        MemFree(entry);
        mTotals.Dropped++;
        mParser.curindx++;
        return result;
    }

    if (dbp->mpQscore.empty() && mParser.accver) {
        if (mParser.ff_get_qscore)
            dbp->mpQscore = (*mParser.ff_get_qscore)(ibp->acnum, ibp->vernum);
        else if (mParser.ff_get_qscore_pp)
            dbp->mpQscore = (*mParser.ff_get_qscore_pp)(ibp->acnum, ibp->vernum, &mParser);
        else if (mParser.qsfd && ibp->qslength > 0)
            dbp->mpQscore = GetQSFromFile(mParser.qsfd, ibp);
    }

    if (! QscoreToSeqAnnot(dbp->mpQscore, *bioseq, ibp->acnum, ibp->vernum, false, true)) {
        if (mParser.ign_bad_qs == false) {
            ibp->drop = true;
            FtaErrPost(SEV_ERROR, ERR_QSCORE_FailedToParse, "Error while parsing QScore. Entry dropped.");
            FtaErrPost(SEV_ERROR, ERR_ENTRY_Skipped, "Entry skipped: \"{}|{}\".", ibp->locusname, ibp->acnum);
            MemFree(entry);
            mTotals.Dropped++;
            mParser.curindx++;
            return result;
            // continue;
        } else {
            FtaErrPost(SEV_ERROR, ERR_QSCORE_FailedToParse, "Error while parsing QScore.");
        }
    }

    dbp->mpQscore.clear();

    // add PatentSeqId if patent is found in reference
    if (no_reference(*bioseq) && ! mParser.debug) {
        if (mParser.source == Parser::ESource::Refseq &&
                   fta_StartsWith(ibp->acnum, "NW_"sv)) {
            FtaErrPost(SEV_ERROR, ERR_REFERENCE_No_references, "No references for RefSeq's NW_ entry. Continue anyway.");
        } else if (ibp->is_wgs) {
            FtaErrPost(SEV_ERROR, ERR_REFERENCE_No_references, "No references for WGS entry. Continue anyway.");
        } else {
            ibp->drop = true;
            FtaErrPost(SEV_ERROR, ERR_REFERENCE_No_references, "No references. Entry dropped.");
            FtaErrPost(SEV_ERROR, ERR_ENTRY_Skipped, "Entry skipped: \"{}|{}\".", ibp->locusname, ibp->acnum);
            MemFree(entry);
            mTotals.Dropped++;
            mParser.curindx++;
            return result;
        }
    }

    if (mParser.source == Parser::ESource::USPTO) {
        GeneRefFeats gene_refs;
        gene_refs.valid = false;
        ProcNucProt(&mParser, ebp->seq_entry, gene_refs, GetScope());
    } else {
        DealWithGenes(ebp->seq_entry, &mParser);
    }

    if (ebp->seq_entry.Empty()) {
        FtaErrPost(SEV_ERROR, ERR_ENTRY_Skipped, "Entry skipped: \"{}|{}\".", ibp->locusname, ibp->acnum);
        mTotals.Dropped++;
        MemFree(entry);
        GetScope().ResetDataAndHistory();
        mParser.curindx++;
        return result;
    }

    TEntryList seq_entries;
    seq_entries.push_back(ebp->seq_entry);
    ebp->seq_entry.Reset();

    fta_find_pub_explore(&mParser, seq_entries);

    // change qual "citation' on features to SeqFeat.cit
    // find citation in the list by serial_number.
    // If serial number not found remove /citation

    ProcessCitations(seq_entries);

    // check for long sequences in each segment

    bool seq_long{ false };
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

    if (mParser.convert) {
        if (mParser.cleanup <= 1) {
            FinalCleanup(seq_entries);

            if (mParser.qamode && ! seq_entries.empty())
                fta_remove_cleanup_user_object(*(*seq_entries.begin()));
        }
        MaybeCutGbblockSource(seq_entries);
    }

    EntryCheckDivCode(seq_entries, &mParser);

    // if () fta_set_strandedness(seq_entries);

    if (fta_EntryCheckGBBlock(seq_entries)) {
        FtaErrPost(SEV_WARNING, ERR_ENTRY_GBBlock_not_Empty, "Attention: GBBlock is not empty");
    }

    // check for identical features
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
        seq_long = false;
        mTotals.Long++;
    } else {
        result = seq_entries.front();
        mTotals.Succeeded++;
    }

    FtaErrPost(SEV_INFO, ERR_ENTRY_Parsed, "OK - entry parsed successfully: \"{}|{}\".", ibp->locusname, ibp->acnum);

    MemFree(entry);
    GetScope().ResetDataAndHistory();

    mParser.curindx++;
    return result;
}


void CXml2Asn::PostTotals()
{
    FtaErrPost(SEV_INFO, ERR_ENTRY_ParsingComplete, 
            "COMPLETED : SUCCEEDED = {} (including: LONG ones = {}); SKIPPED = {}.", 
            mTotals.Succeeded, mTotals.Long, mTotals.Dropped);
}

END_NCBI_SCOPE
