/* xm_ascii.c
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
 * File Name:  xm_ascii.c
 *
 * Author: Sergey Bazhin
 *
 * File Description:
 * -----------------
 *      Parse INSDSEQ from blocks to asn.
 * Build XML format entry block.
 *
 */
#include <ncbi_pch.hpp>

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
#    undef THIS_FILE
#endif
#define THIS_FILE "xm_ascii.cpp"

BEGIN_NCBI_SCOPE

/**********************************************************/
static void XMLCheckContigEverywhere(IndexblkPtr ibp, Parser::ESource source)
{
    if(ibp == NULL)
        return;

    bool condiv = (StringICmp(ibp->division, "CON") == 0);

    if(condiv && ibp->segnum != 0)
    {
        ErrPostEx(SEV_ERROR, ERR_DIVISION_ConDivInSegset,
                  "Use of the CON division is not allowed for members of segmented set : %s|%s. Entry dropped.",
                  ibp->locusname, ibp->acnum);
        ibp->drop = 1;
        return;
    }

    if(!condiv && ibp->is_contig == false && ibp->origin == false)
    {
        ErrPostEx(SEV_ERROR, ERR_FORMAT_MissingSequenceData,
                  "Required sequence data is absent. Entry dropped.");
        ibp->drop = 1;
    }
    else if(!condiv && ibp->is_contig && ibp->origin == false)
    {
        ErrPostEx(SEV_WARNING, ERR_DIVISION_MappedtoCON,
                  "Division [%s] mapped to CON based on the existence of <INSDSeq_contig> line.",
                  ibp->division);
    }
    else if(ibp->is_contig && ibp->origin)
    {
        if(source == Parser::ESource::EMBL || source == Parser::ESource::DDBJ)
        {
            ErrPostEx(SEV_INFO, ERR_FORMAT_ContigWithSequenceData,
                      "The <INSDSeq_contig> linetype and sequence data are both present. Ignoring sequence data.");
        }
        else
        {
            ErrPostEx(SEV_REJECT, ERR_FORMAT_ContigWithSequenceData,
                      "The <INSDSeq_contig> linetype and sequence data may not both be present in a sequence record.");
            ibp->drop = 1;
        }
    }
    else if(condiv && ibp->is_contig == false && ibp->origin == false)
    {
        ErrPostEx(SEV_ERROR, ERR_FORMAT_MissingContigFeature,
                  "No <INSDSeq_contig> data in XML format file. Entry dropped.");
        ibp->drop = 1;
    }
    else if(condiv && ibp->is_contig == false && ibp->origin)
    {
        ErrPostEx(SEV_WARNING, ERR_DIVISION_ConDivLacksContig,
                  "Division is CON, but <INSDSeq_contig> data have not been found.");
    }
}

/**********************************************************/
static bool XMLGetInstContig(XmlIndexPtr xip, DataBlkPtr dbp,
                             objects::CBioseq& bioseq, ParserPtr pp)
{
    char*    p;
    char*    q;
    char*    r;
    bool    locmap;
    bool    sitemap;
    bool    allow_crossdb_featloc;
    Int4       i;
    int        numerr;

    p = XMLFindTagValue(dbp->offset, xip, INSDSEQ_CONTIG);
    if(p == NULL)
        return false;

    for(q = p, r = p; *q != '\0'; q++)
        if(*q != '\n' && *q != '\t' && *q != ' ')
            *r++ = *q;
    *r = '\0';

    for(q = p; *q != '\0'; q++)
        if((q[0] == ',' && q[1] == ',') || (q[0] == '(' && q[1] == ',') ||
           (q[0] == ',' && q[1] == ')'))
            break;
    if(*q != '\0')
    {
        ErrPostEx(SEV_REJECT, ERR_LOCATION_ContigHasNull,
                  "The join() statement for this record's contig line contains one or more comma-delimited components which are null.");
        MemFree(p);
        return false;
    }

    if(pp->buf != NULL)
        MemFree(pp->buf);
    pp->buf = NULL;

    CRef<objects::CSeq_loc> loc = xgbparseint_ver(p, locmap, sitemap, numerr, bioseq.GetId(), pp->accver);

    if (loc.Empty())
    {
        MemFree(p);
        return true;
    }

    allow_crossdb_featloc = pp->allow_crossdb_featloc;
    pp->allow_crossdb_featloc = true;

    TSeqLocList locs;
    locs.push_back(loc);
    i = fta_fix_seq_loc_id(locs, pp, p, NULL, true);
    if(i > 999)
        fta_create_far_fetch_policy_user_object(bioseq, i);

    pp->allow_crossdb_featloc = allow_crossdb_featloc;

    if (loc->IsMix())
    {
        XGappedSeqLocsToDeltaSeqs(loc->GetMix(), bioseq.SetInst().SetExt().SetDelta().Set());
        bioseq.SetInst().SetRepr(objects::CSeq_inst::eRepr_delta);
    }
    else
        bioseq.SetInst().ResetExt();

    MemFree(p);

    return true;
}

/**********************************************************/
bool XMLGetInst(ParserPtr pp, DataBlkPtr dbp, unsigned char* dnaconv,
                objects::CBioseq& bioseq)
{
    IndexblkPtr ibp;
    XmlIndexPtr xip;
    Int2        topology;
    Int2        strand;
    char*     topstr;
    char*     strandstr;

    ibp = pp->entrylist[pp->curindx];
    topstr = NULL;
    strandstr = NULL;
    for(xip = ibp->xip; xip != NULL; xip = xip->next)
    {
        if(xip->tag == INSDSEQ_TOPOLOGY && topstr == NULL)
            topstr = XMLGetTagValue(dbp->offset, xip);
        else if(xip->tag == INSDSEQ_STRANDEDNESS && strandstr == NULL)
            strandstr = XMLGetTagValue(dbp->offset, xip);
    }
    if(topstr == NULL)
        topstr = StringSave("   ");
    if(strandstr == NULL)
        strandstr = StringSave("   ");

    objects::CSeq_inst& inst = bioseq.SetInst();
    inst.SetRepr(objects::CSeq_inst::eRepr_raw);

    /* get linear, circular, tandem topology, blank is linear which = 1
     */
    topology = XMLCheckTPG(topstr);
    if(topology > 1)
        inst.SetTopology(static_cast<objects::CSeq_inst::ETopology>(topology));

    strand = XMLCheckSTRAND(strandstr);
    if (strand > 0)
        inst.SetStrand(static_cast<objects::CSeq_inst::EStrand>(strand));

    if(topstr != NULL)
        MemFree(topstr);
    if(strandstr != NULL)
        MemFree(strandstr);

    if (!GetSeqData(pp, dbp, bioseq, 0, dnaconv, objects::eSeq_code_type_iupacna))
        return false;

    if(ibp->is_contig && !XMLGetInstContig(ibp->xip, dbp, bioseq, pp))
        return false;

    return true;
}

/**********************************************************/
static CRef<objects::CGB_block> XMLGetGBBlock(ParserPtr pp, char* entry, objects::CMolInfo& mol_info,
                                                          objects::CBioSource* bio_src)
{
    CRef<objects::CGB_block> gbb(new objects::CGB_block),
                                         ret;

    IndexblkPtr  ibp;
    char*      bptr;
    char*      str;
    char         msg[4];
    char*      kw;
    char*      kwp;
    Int2         div;
    bool         if_cds;

    bool         pat_ref = false;
    bool         est_kwd = false;
    bool         sts_kwd = false;
    bool         gss_kwd = false;
    bool         htc_kwd = false;
    bool         fli_kwd = false;
    bool         wgs_kwd = false;
    bool         tpa_kwd = false;
    bool         tsa_kwd = false;
    bool         tls_kwd = false;
    bool         env_kwd = false;
    bool         mga_kwd = false;

    bool         cancelled;
    bool         drop;
    char*      tempdiv;
    Int2         thtg;
    char*      p;
    Int4         i;

    ibp = pp->entrylist[pp->curindx];

    ibp->wgssec[0] = '\0';

    str = XMLFindTagValue(entry, ibp->xip, INSDSEQ_SOURCE);
    if(str != NULL)
    {
        p = StringRChr(str, '.');
        if(p != NULL && p > str && p[1] == '\0' && *(p - 1) == '.')
            *p = '\0';

        gbb->SetSource(str);
        MemFree(str);
    }

    if (!ibp->keywords.empty())
    {
        gbb->SetKeywords().swap(ibp->keywords);
        ibp->keywords.clear();
    }
    else
        XMLGetKeywords(entry, ibp->xip, gbb->SetKeywords());

    if(ibp->is_mga && !fta_check_mga_keywords(mol_info, gbb->GetKeywords()))
    {
        return ret;
    }

    if (ibp->is_tpa && !fta_tpa_keywords_check(gbb->SetKeywords()))
    {
        return ret;
    }

    if(ibp->is_tsa && !fta_tsa_keywords_check(gbb->SetKeywords(), pp->source))
    {
        return ret;
    }

    if(ibp->is_tls && !fta_tls_keywords_check(gbb->SetKeywords(), pp->source))
    {
        return ret;
    }

    ITERATE(TKeywordList, key, gbb->GetKeywords())
    {
        fta_keywords_check(key->c_str(), &est_kwd, &sts_kwd,
            &gss_kwd, &htc_kwd, &fli_kwd, &wgs_kwd, &tpa_kwd,
            &env_kwd, &mga_kwd, &tsa_kwd, &tls_kwd);
    }

    if(ibp->env_sample_qual == false && env_kwd)
    {
        ErrPostEx(SEV_REJECT, ERR_KEYWORD_ENV_NoMatchingQualifier,
                  "This record utilizes the ENV keyword, but there are no /environmental_sample qualifiers among its source features.");
        return ret;
    }

    bptr = XMLFindTagValue(entry, ibp->xip, INSDSEQ_DIVISION);
    if(bptr != NULL)
    {
        if_cds = XMLCheckCDS(entry, ibp->xip);
        div = CheckDIV(bptr);
        if(div != -1)
        {
            std::string div_str(bptr, bptr + 3);
            gbb->SetDiv(div_str);

            if(div == 16)               /* "ORG" replaced by "UNA" */
                gbb->SetDiv("UNA");

            /* preserve the division code for later use
             */
            const char* p_div = gbb->GetDiv().c_str();
            StringCpy(ibp->division, p_div);

            if(ibp->psip.NotEmpty())
                pat_ref = true;

            if(ibp->is_tpa &&
               (StringCmp(p_div, "EST") == 0 || StringCmp(p_div, "GSS") == 0 ||
                StringCmp(p_div, "PAT") == 0 || StringCmp(p_div, "HTG") == 0))
            {
                ErrPostEx(SEV_REJECT, ERR_DIVISION_BadTPADivcode,
                          "Division code \"%s\" is not legal for TPA records. Entry dropped.",
                          p_div);
                return ret;
            }

            if(ibp->is_tsa && StringCmp(p_div, "TSA") != 0)
            {
                ErrPostEx(SEV_REJECT, ERR_DIVISION_BadTSADivcode,
                          "Division code \"%s\" is not legal for TSA records. Entry dropped.",
                          p_div);
                return ret;
            }

            cancelled = IsCancelled(gbb->GetKeywords());

            if (StringCmp(p_div, "HTG") == 0)
            {
                if (!HasHtg(gbb->GetKeywords()))
                {
                    ErrPostEx(SEV_ERROR, ERR_DIVISION_MissingHTGKeywords,
                              "Division is HTG, but entry lacks HTG-related keywords. Entry dropped.");
                    return ret;
                }
            }

            tempdiv = StringSave(gbb->GetDiv().c_str());

            if (fta_check_htg_kwds(gbb->SetKeywords(), pp->entrylist[pp->curindx], mol_info))
                gbb->SetDiv("");

            XMLDefVsHTGKeywords(mol_info.GetTech(), entry, ibp->xip, cancelled);

            CheckHTGDivision(tempdiv, mol_info.GetTech());
            if(tempdiv != NULL)
                MemFree(tempdiv);

            i = 0;
            if(est_kwd)
                i++;
            if(sts_kwd)
                i++;
            if(gss_kwd)
                i++;
            if(ibp->htg > 0)
                i++;
            if(htc_kwd)
                i++;
            if(fli_kwd)
                i++;
            if(wgs_kwd)
                i++;
            if(env_kwd)
                i++;
            if(mga_kwd)
            {
                if(ibp->is_mga == false)
                {
                    ErrPostEx(SEV_REJECT, ERR_KEYWORD_ShouldNotBeCAGE,
                              "This is apparently _not_ a CAGE record, but the special keywords are present. Entry dropped.");
                    return ret;
                }
                i++;
            }
            else if(ibp->is_mga)
            {
                ErrPostEx(SEV_REJECT, ERR_KEYWORD_NoGeneExpressionKeywords,
                          "This is apparently a CAGE or 5'-SAGE record, but it lacks the required keywords. Entry dropped.");
                return ret;
            }
            if(tpa_kwd)
            {
                if(ibp->is_tpa == false && pp->source != Parser::ESource::EMBL)
                {
                    ErrPostEx(SEV_REJECT, ERR_KEYWORD_ShouldNotBeTPA,
                              "This is apparently _not_ a TPA record, but the special \"TPA\" and/or \"Third Party Annotation\" keywords are present. Entry dropped.");
                    return ret;
                }
                i++;
            }
            else if(ibp->is_tpa)
            {
                ErrPostEx(SEV_REJECT, ERR_KEYWORD_MissingTPA,
                          "This is apparently a TPA record, but it lacks the required \"TPA\" and/or \"Third Party Annotation\" keywords. Entry dropped.");
                return ret;
            }
            if(tsa_kwd)
            {
                if(ibp->is_tsa == false)
                {
                    ErrPostEx(SEV_REJECT, ERR_KEYWORD_ShouldNotBeTSA,
                              "This is apparently _not_ a TSA record, but the special \"TSA\" and/or \"Transcriptome Shotgun Assembly\" keywords are present. Entry dropped.");
                    return ret;
                }
                i++;
            }
            else if(ibp->is_tsa)
            {
                ErrPostEx(SEV_REJECT, ERR_KEYWORD_MissingTSA,
                          "This is apparently a TSA record, but it lacks the required \"TSA\" and/or \"Transcriptome Shotgun Assembly\" keywords. Entry dropped.");
                return ret;
            }
            if(tls_kwd)
            {
                if(ibp->is_tls == false)
                {
                    ErrPostEx(SEV_REJECT, ERR_KEYWORD_ShouldNotBeTLS,
                              "This is apparently _not_ a TLS record, but the special \"TLS\" and/or \"Targeted Locus Study\" keywords are present. Entry dropped.");
                    return ret;
                }
                i++;
            }
            else if(ibp->is_tls)
            {
                ErrPostEx(SEV_REJECT, ERR_KEYWORD_MissingTLS,
                          "This is apparently a TLS record, but it lacks the required \"TLS\" and/or \"Targeted Locus Study\" keywords. Entry dropped.");
                return ret;
            }
            if(i > 1)
            {
                if(i == 2 && ibp->htg > 0 && env_kwd)
                    ErrPostEx(SEV_WARNING, ERR_KEYWORD_HTGPlusENV,
                              "This HTG record also has the ENV keyword, which is an unusual combination. Confirmation that isolation and cloning steps actually occured might be appropriate.");
                else if(i != 2 || env_kwd == false ||
                   (est_kwd == false && gss_kwd == false && wgs_kwd == false))
                {
                    ErrPostEx(SEV_REJECT, ERR_KEYWORD_ConflictingKeywords,
                              "This record contains more than one of the special keywords used to indicate that a sequence is an HTG, EST, GSS, STS, HTC, WGS, ENV, FLI_CDNA, TPA, CAGE, TSA or TLS sequence.");
                    return ret;
                }
            }

            if(wgs_kwd)
                i--;
            if(ibp->is_contig && i > 0 &&
               wgs_kwd == false && tpa_kwd == false && env_kwd == false)
            {
                ErrPostEx(SEV_REJECT, ERR_KEYWORD_IllegalForCON,
                          "This CON record should not have HTG, EST, GSS, STS, HTC, FLI_CDNA, CAGE, TSA or TLS special keywords. Entry dropped.");
                return ret;
            }

            thtg = mol_info.GetTech();
            if (thtg == objects::CMolInfo::eTech_htgs_0 || thtg == objects::CMolInfo::eTech_htgs_1 ||
                thtg == objects::CMolInfo::eTech_htgs_2 || thtg == objects::CMolInfo::eTech_htgs_3)
            {
                RemoveHtgPhase(gbb->SetKeywords());
            }

            kw = XMLConcatSubTags(entry, ibp->xip, INSDSEQ_KEYWORDS, ';');
            if(kw != NULL)
            {
                kwp = StringStr(kw, "EST");
                if(kwp != NULL && est_kwd == false)
                {
                    ErrPostEx(SEV_WARNING, ERR_KEYWORD_ESTSubstring,
                              "Keyword %s has substring EST, but no official EST keywords found",
                              kw);
                }
                kwp = StringStr(kw, "STS");
                if(kwp != NULL && sts_kwd == false)
                {
                    ErrPostEx(SEV_WARNING, ERR_KEYWORD_STSSubstring,
                              "Keyword %s has substring STS, but no official STS keywords found",
                              kw);
                }
                MemFree(kw);
            }

            if(!ibp->is_contig)
            {
                drop = false;
                Uint1 tech = mol_info.GetTech();
                char* div_to_check = StringSave(gbb->GetDiv().c_str());
                char* p_div = check_div(ibp->is_pat, pat_ref, est_kwd, sts_kwd,
                                          gss_kwd, if_cds, div_to_check, &tech,
                                          ibp->bases, pp->source, drop);
                if (tech != 0)
                    mol_info.SetTech(tech);
                else
                    mol_info.ResetTech();

                if (p_div != NULL)
                {
                    gbb->SetDiv(p_div);
                    MemFree(p_div);
                }
                else
                    gbb->SetDiv("");

                if(drop)
                {
                    MemFree(bptr);
                    return ret;
                }
            }
            else if(gbb->GetDiv() == "CON")
            {
                gbb->SetDiv("");
            }
        }
        else
        {
            MemCpy(msg, bptr, 3);
            msg[3] = '\0';
            ErrPostEx(SEV_REJECT, ERR_DIVISION_UnknownDivCode,
                      "Unknown division code \"%s\" found in GenBank flatfile. Record rejected.",
                      msg);
            MemFree(bptr);
            return ret;
        }

        if(IsNewAccessFormat(ibp->acnum) == 0 && *ibp->acnum == 'T' &&
           gbb->GetDiv() != "EST")
        {
            ErrPostStr(SEV_INFO, ERR_DIVISION_MappedtoEST,
                       "Leading T in accession number.");
            mol_info.SetTech(objects::CMolInfo::eTech_est);

            gbb->SetDiv("");
        }

        MemFree(bptr);
    }

    bool is_htc_div = gbb->GetDiv() == "HTC",
         has_htc = HasHtc(gbb->GetKeywords());

    if (is_htc_div && !has_htc)
    {
        ErrPostEx(SEV_ERROR, ERR_DIVISION_MissingHTCKeyword,
                  "This record is in the HTC division, but lacks the required HTC keyword.");
        return ret;
    }

    if (!is_htc_div && has_htc)
    {
        ErrPostEx(SEV_ERROR, ERR_DIVISION_InvalidHTCKeyword,
                  "This record has the special HTC keyword, but is not in HTC division. If this record has graduated out of HTC, then the keyword should be removed.");
        return ret;
    }

    if (is_htc_div)
    {
        str = XMLFindTagValue(entry, ibp->xip, INSDSEQ_MOLTYPE);
        if(str != NULL)
        {
            p = str;
            if(*str == 'm' || *str == 'r')
                p = str + 1;
            else if(StringNCmp(str, "pre-", 4) == 0)
                p = str + 4;
            else if(StringNCmp(str, "transcribed ", 12) == 0)
                p = str + 12;

            if(StringNCmp(p, "RNA", 3) != 0)
            {
                ErrPostEx(SEV_ERROR, ERR_DIVISION_HTCWrongMolType,
                          "All HTC division records should have a moltype of pre-RNA, mRNA or RNA.");
                MemFree(str);
                return ret;
            }
            MemFree(str);
        }
    }

    if (fli_kwd)
        mol_info.SetTech(objects::CMolInfo::eTech_fli_cdna);

    /* will be used in flat file database
     */
    if(!gbb->GetDiv().empty())
    {
        if (gbb->GetDiv() == "EST")
        {
            ibp->EST = true;
            mol_info.SetTech(objects::CMolInfo::eTech_est);
            gbb->SetDiv("");
        }
        else if (gbb->GetDiv() == "STS")
        {
            ibp->STS = true;
            mol_info.SetTech(objects::CMolInfo::eTech_sts);
            gbb->SetDiv("");
        }
        else if (gbb->GetDiv() == "GSS")
        {
            ibp->GSS = true;
            mol_info.SetTech(objects::CMolInfo::eTech_survey);
            gbb->SetDiv("");
        }
        else if (gbb->GetDiv() == "HTC")
        {
            ibp->HTC = true;
            mol_info.SetTech(objects::CMolInfo::eTech_htc);
            gbb->SetDiv("");
        }
        else if (gbb->GetDiv() == "SYN" && bio_src != NULL && bio_src->IsSetOrigin() &&
                 bio_src->GetOrigin() == 5)     /* synthetic */
        {
            gbb->SetDiv("");
        }
    }
    else if (mol_info.IsSetTech())
    {
        if (mol_info.GetTech() == objects::CMolInfo::eTech_est)
            ibp->EST = true;
        if (mol_info.GetTech() == objects::CMolInfo::eTech_sts)
            ibp->STS = true;
        if (mol_info.GetTech() == objects::CMolInfo::eTech_survey)
            ibp->GSS = true;
        if (mol_info.GetTech() == objects::CMolInfo::eTech_htc)
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

    if (bio_src != NULL && bio_src->IsSetSubtype())
    {
        ITERATE(objects::CBioSource::TSubtype, subtype, bio_src->GetSubtype())
        {
            if ((*subtype)->GetSubtype() == 27)
            {
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
static CRef<objects::CMolInfo> XMLGetMolInfo(ParserPtr pp, DataBlkPtr entry,
                                                         objects::COrg_ref* org_ref)
{
    IndexblkPtr ibp;

    char*     div;
    char*     molstr;

    ibp = pp->entrylist[pp->curindx];

    CRef<objects::CMolInfo> mol_info(new objects::CMolInfo);

    molstr = XMLFindTagValue(entry->offset, ibp->xip, INSDSEQ_MOLTYPE);
    div = XMLFindTagValue(entry->offset, ibp->xip, INSDSEQ_DIVISION);

    if(StringNCmp(div, "EST", 3) == 0)
        mol_info->SetTech(objects::CMolInfo::eTech_est);
    else if(StringNCmp(div, "STS", 3) == 0)
        mol_info->SetTech(objects::CMolInfo::eTech_sts);
    else if(StringNCmp(div, "GSS", 3) == 0)
        mol_info->SetTech(objects::CMolInfo::eTech_survey);
    else if(StringNCmp(div, "HTG", 3) == 0)
        mol_info->SetTech(objects::CMolInfo::eTech_htgs_1);
    else if(ibp->is_wgs)
    {
        if(ibp->is_tsa)
            mol_info->SetTech(objects::CMolInfo::eTech_tsa);
        else if(ibp->is_tls)
            mol_info->SetTech(objects::CMolInfo::eTech_targeted);
        else
            mol_info->SetTech(objects::CMolInfo::eTech_wgs);
    }
    else if(ibp->is_tsa)
        mol_info->SetTech(objects::CMolInfo::eTech_tsa);
    else if(ibp->is_tls)
        mol_info->SetTech(objects::CMolInfo::eTech_targeted);

    MemFree(div);
    GetFlatBiomol(mol_info->SetBiomol(), mol_info->GetTech(), molstr, pp, entry, org_ref);
    if (mol_info->GetBiomol() == 0) // not set
        mol_info->ResetBiomol();

    if(molstr != NULL)
        MemFree(molstr);

    return mol_info;
}

/**********************************************************/
static void XMLFakeBioSources(XmlIndexPtr xip, char* entry, objects::CBioseq& bioseq,
                              Parser::ESource source)
{
    char*      organism = NULL;
    char*      taxonomy = NULL;

    char*      p;
    char*      q;

    for(; xip != NULL; xip = xip->next)
    {
        if(xip->tag == INSDSEQ_ORGANISM && organism == NULL)
            organism = XMLGetTagValue(entry, xip);
        else if(xip->tag == INSDSEQ_TAXONOMY && taxonomy == NULL)
            taxonomy = XMLGetTagValue(entry, xip);
    }
        
    if(organism == NULL)
    {
        ErrPostStr(SEV_WARNING, ERR_ORGANISM_NoOrganism,
                   "No <INSDSeq_organism> data in XML format file.");
        if(taxonomy != NULL)
            MemFree(taxonomy);
        return;
    }

    CRef<objects::CBioSource> bio_src(new objects::CBioSource);

    p = organism;
    if (GetGenomeInfo(*bio_src, p) && bio_src->GetGenome() != 9)      /* ! Plasmid */
    {
        while(*p != ' ' && *p != '\0')
            p++;
        while(*p == ' ')
            p++;
    }

    objects::COrg_ref& org_ref = bio_src->SetOrg();

    if(source == Parser::ESource::EMBL)
    {
        q = StringChr(p, '(');
        if(q != NULL && q > p)
        {
            for(q--; *q == ' ' || *q == '\t'; q--)
                if(q == p)
                    break;
            if(*q != ' ' && *q != '\t')
                q++;
            if(q > p)
            {
                *q = '\0';
                org_ref.SetCommon(p);
            }
        }
    }

    org_ref.SetTaxname(p);
    MemFree(organism);

    if (org_ref.GetTaxname() == "Unknown.")
    {
        std::string& taxname = org_ref.SetTaxname();
        taxname = taxname.substr(0, taxname.size() - 1);
    }

    if(taxonomy != NULL)
    {
        org_ref.SetOrgname().SetLineage(taxonomy);
    }

    CRef<objects::CSeqdesc> descr(new objects::CSeqdesc);
    descr->SetSource(*bio_src);
    bioseq.SetDescr().Set().push_back(descr);
}

/**********************************************************/
static void XMLGetDescrComment(char* offset)
{
    char* p;
    char* q;

    for(p = offset; *p == '\n' || *p == ' ';)
        p++;
    if(p > offset)
        fta_StringCpy(offset, p);

    for(p = offset, q = offset; *p != '\0';)
    {
        if(*p != '\n')
        {
            *q++ = *p++;
            continue;
        }

        *q++ = '~';
        for(p++; *p == ' ';)
            p++;
    }
    *q = '\0';

    for(p = offset;;)
    {
        p = StringStr(p, "; ");
        if(p == NULL)
            break;
        for(p += 2, q = p; *q == ' ';)
            q++;
        if(q > p)
            fta_StringCpy(p, q);
    }

    for(p = offset; *p == ' ';)
        p++;
    if(p > offset)
        fta_StringCpy(offset, p);
    for(p = offset; *p != '\0';)
        p++;

    if(p > offset)
    {
        for(p--;; p--)
        {
            if(*p == ' ' || *p == '\t' || *p == ';' || *p == ',' ||
               *p == '.' || *p == '~')
            {
                if(p > offset)
                    continue;
                *p = '\0';
            }
            break;
        }
        if(*p != '\0')
        {
            p++;
            if(StringNCmp(p, "...", 3) == 0)
                p[3] = '\0';
            else if(StringChr(p, '.') != NULL)
            {
                *p = '.';
                p[1] = '\0';
            }
            else
                *p = '\0';
        }
    }
}

/**********************************************************/
static void XMLGetDescr(ParserPtr pp, DataBlkPtr entry, objects::CBioseq& bioseq)
{
    IndexblkPtr   ibp;

    DataBlkPtr    dbp;
    DataBlkPtr    dbpnext;

    char*       crdate;
    char*       update;
    char*       offset;
    char*       str;
    char*       p;
    char*       q;
    char*       gbdiv;

    ibp = pp->entrylist[pp->curindx];

    objects::CBioSource* bio_src = nullptr;
    objects::COrg_ref* org_ref = nullptr;

    /* ORGANISM
     */
    NON_CONST_ITERATE(objects::CSeq_descr::Tdata, descr, bioseq.SetDescr().Set())
    {
        if ((*descr)->IsSource())
        {
            bio_src = &((*descr)->SetSource());
            if (bio_src->IsSetOrg())
                org_ref = &bio_src->SetOrg();
            break;
        }
    }

    /* MolInfo from LOCUS line
     */
    CRef<objects::CMolInfo> mol_info = XMLGetMolInfo(pp, entry, org_ref);

    /* DEFINITION data ==> descr_title
     */
    str = XMLFindTagValue(entry->offset, ibp->xip, INSDSEQ_DEFINITION);
    std::string title;

    if (str != NULL)
    {
        for(p = str; *p == ' ';)
            p++;
        if(p > str)
            fta_StringCpy(str, p);
        if(pp->xml_comp && pp->source != Parser::ESource::EMBL)
        {
            p = StringRChr(str, '.');
            if(p == NULL || p[1] != '\0')
            {
                p = (char*) MemNew(StringLen(str) + 2);
                StringCpy(p, str);
                StringCat(p, ".");
                MemFree(str);
                str = p;
                p = NULL;
            }
        }

        title = str;
        MemFree(str);
        str = NULL;

        CRef<objects::CSeqdesc> descr(new objects::CSeqdesc);
        descr->SetTitle(title);
        bioseq.SetDescr().Set().push_back(descr);

        if(ibp->is_tpa == false && pp->source != Parser::ESource::EMBL &&
           StringNCmp(title.c_str(), "TPA:", 4) == 0)
        {
            ErrPostEx(SEV_REJECT, ERR_DEFINITION_ShouldNotBeTPA,
                      "This is apparently _not_ a TPA record, but the special \"TPA:\" prefix is present on its definition line. Entry dropped.");
            ibp->drop = 1;
            return;
        }

        if (ibp->is_tsa == false && StringNCmp(title.c_str(), "TSA:", 4) == 0)
        {
            ErrPostEx(SEV_REJECT, ERR_DEFINITION_ShouldNotBeTSA,
                      "This is apparently _not_ a TSA record, but the special \"TSA:\" prefix is present on its definition line. Entry dropped.");
            ibp->drop = 1;
            return;
        }

        if (ibp->is_tls == false && StringNCmp(title.c_str(), "TLS:", 4) == 0)
        {
            ErrPostEx(SEV_REJECT, ERR_DEFINITION_ShouldNotBeTLS,
                      "This is apparently _not_ a TLS record, but the special \"TLS:\" prefix is present on its definition line. Entry dropped.");
            ibp->drop = 1;
            return;
        }
    }

    if(ibp->is_tpa &&
       (title.empty() || StringNCmp(title.c_str(), "TPA:", 4) != 0))
    {
        ErrPostEx(SEV_REJECT, ERR_DEFINITION_MissingTPA,
                  "This is apparently a TPA record, but it lacks the required \"TPA:\" prefix on its definition line. Entry dropped.");
        ibp->drop = 1;
        return;
    }

    if(ibp->is_tsa &&
       (title.empty() || StringNCmp(title.c_str(), "TSA:", 4) != 0))
    {
        ErrPostEx(SEV_REJECT, ERR_DEFINITION_MissingTSA,
                  "This is apparently a TSA record, but it lacks the required \"TSA:\" prefix on its definition line. Entry dropped.");
        ibp->drop = 1;
        return;
    }

    if(ibp->is_tls &&
       (title.empty() || StringNCmp(title.c_str(), "TLS:", 4) != 0))
    {
        ErrPostEx(SEV_REJECT, ERR_DEFINITION_MissingTLS,
                  "This is apparently a TLS record, but it lacks the required \"TLS:\" prefix on its definition line. Entry dropped.");
        ibp->drop = 1;
        return;
    }

    /* REFERENCE
     */
    /* pub should be before GBblock because we need patent ref
     */
    dbp = XMLBuildRefDataBlk(entry->offset, ibp->xip, ParFlat_REF_END);
    for(; dbp != NULL; dbp = dbpnext)
    {
        dbpnext = dbp->next;

        CRef<objects::CPubdesc> pubdesc = DescrRefs(pp, dbp, 0);
        if (pubdesc.NotEmpty())
        {
            CRef<objects::CSeqdesc> descr(new objects::CSeqdesc);
            descr->SetPub(*pubdesc);
            bioseq.SetDescr().Set().push_back(descr);
        }

        MemFree(dbp);
    }

    dbp = XMLBuildRefDataBlk(entry->offset, ibp->xip, ParFlat_REF_NO_TARGET);
    for(; dbp != NULL; dbp = dbpnext)
    {
        dbpnext = dbp->next;

        CRef<objects::CPubdesc> pubdesc = DescrRefs(pp, dbp, 0);
        if (pubdesc.NotEmpty())
        {
            CRef<objects::CSeqdesc> descr(new objects::CSeqdesc);
            descr->SetPub(*pubdesc);
            bioseq.SetDescr().Set().push_back(descr);
        }

        MemFree(dbp);
    }

    gbdiv = NULL;

    TStringList dr_ena,
                dr_biosample;

    CRef<objects::CEMBL_block> embl;
    CRef<objects::CGB_block> gbb;

    if (pp->source == Parser::ESource::EMBL)
        embl = XMLGetEMBLBlock(pp, entry->offset, *mol_info, &gbdiv, bio_src, dr_ena, dr_biosample);
    else
        gbb = XMLGetGBBlock(pp, entry->offset, *mol_info, bio_src);

    CRef<objects::CUser_object> dbuop;
    if (!dr_ena.empty() || !dr_biosample.empty())
        fta_build_ena_user_object(bioseq.SetDescr().Set(), dr_ena, dr_biosample, dbuop);

    if (mol_info->IsSetBiomol() || mol_info->IsSetTech())
    {
        CRef<objects::CSeqdesc> descr(new objects::CSeqdesc);
        descr->SetMolinfo(*mol_info);
        bioseq.SetDescr().Set().push_back(descr);
    }

    if(pp->source == Parser::ESource::EMBL)
    {
        if (embl.Empty())
        {
            ibp->drop = 1;
            return;
        }
    }
    else if(gbb.Empty())
    {
        ibp->drop = 1;
        return;
    }

    if(pp->source == Parser::ESource::EMBL)
    {
        if(StringNICmp(ibp->division, "CON", 3) == 0)
            fta_add_hist(pp, bioseq, embl->SetExtra_acc(), Parser::ESource::EMBL,
                         objects::CSeq_id::e_Embl, true, ibp->acnum);
        else
            fta_add_hist(pp, bioseq, embl->SetExtra_acc(), Parser::ESource::EMBL,
                         objects::CSeq_id::e_Embl, false, ibp->acnum);

        if (embl->GetExtra_acc().empty())
            embl->ResetExtra_acc();
    }
    else
    {
        if(StringNICmp(ibp->division, "CON", 3) == 0)
            fta_add_hist(pp, bioseq, gbb->SetExtra_accessions(), Parser::ESource::DDBJ,
                         objects::CSeq_id::e_Ddbj, true, ibp->acnum);
        else
            fta_add_hist(pp, bioseq, gbb->SetExtra_accessions(), Parser::ESource::DDBJ,
                         objects::CSeq_id::e_Ddbj, false, ibp->acnum);
    }

    if(pp->source == Parser::ESource::EMBL)
    {
        if (gbdiv != NULL)
        {
            gbb.Reset(new objects::CGB_block);
            gbb->SetDiv(gbdiv);

            MemFree(gbdiv);
            gbdiv = NULL;
        }

        CRef<objects::CSeqdesc> descr(new objects::CSeqdesc);
        descr->SetEmbl(*embl);
        bioseq.SetDescr().Set().push_back(descr);
    }

    offset = XMLFindTagValue(entry->offset, ibp->xip, INSDSEQ_PRIMARY);
    if(offset == NULL && ibp->is_tpa && ibp->is_wgs == false)
    {
        if(ibp->inferential || ibp->experimental)
        {
            if(!fta_dblink_has_sra(dbuop))
            {
                ErrPostEx(SEV_REJECT, ERR_TPA_TpaSpansMissing,
                          "TPA:%s record lacks both AH/PRIMARY linetype and Sequence Read Archive links. Entry dropped.",
                          (ibp->inferential == false) ? "experimental" : "inferential");
                ibp->drop = 1;
                return;
            }
        }
        else if(ibp->specialist_db == false)
        {
            ErrPostEx(SEV_REJECT, ERR_TPA_TpaSpansMissing,
                      "TPA record lacks required AH/PRIMARY linetype. Entry dropped.");
            ibp->drop = 1;
            return;
        }
    }

    if(offset != NULL)
    {
        if(!fta_parse_tpa_tsa_block(bioseq, offset, ibp->acnum, ibp->vernum, 10, 0, ibp->is_tpa))
        {
            ibp->drop = 1;
            MemFree(offset);
            return;
        }
        MemFree(offset);
    }

    if (gbb.NotEmpty())
    {
        if(pp->taxserver == 1 && gbb->IsSetDiv())
            fta_fix_orgref_div(bioseq.SetAnnot(), *org_ref, *gbb);

        CRef<objects::CSeqdesc> descr(new objects::CSeqdesc);
        descr->SetGenbank(*gbb);
        bioseq.SetDescr().Set().push_back(descr);
    }

    /* COMMENT data
     */
    offset = XMLFindTagValue(entry->offset, ibp->xip, INSDSEQ_COMMENT);
    if(offset != NULL)
    {
        bool bad = false;
        TUserObjVector user_objs;

        fta_parse_structured_comment(offset, bad, user_objs);

        if(bad)
        {
            ibp->drop = 1;
            MemFree(offset);
            return;
        }

        NON_CONST_ITERATE(TUserObjVector, user_obj, user_objs)
        {
            CRef<objects::CSeqdesc> descr(new objects::CSeqdesc);
            descr->SetUser(*(*user_obj));
            bioseq.SetDescr().Set().push_back(descr);
        }

        XMLGetDescrComment(offset);
        if(pp->xml_comp)
        {
            for(q = offset, p = q; *p != '\0';)
            {
                if(*p == ';' && (p[1] == ' ' || p[1] == '~'))
                    *p = ' ';
                if(*p == '~' || *p == ' ')
                {
                    *q++ = ' ';
                    for(p++; *p == ' ' || *p == '~';)
                        p++;
                }
                else
                    *q++ = *p++;
            }
            *q = '\0';
        }

        if (offset[0] != 0)
        {
            CRef<objects::CSeqdesc> descr(new objects::CSeqdesc);
            descr->SetComment(offset);
            bioseq.SetDescr().Set().push_back(descr);
        }
        MemFree(offset);
    }

    /* DATE
     */
    if(pp->no_date)            /* -N in command line means no date */
        return;

    CRef<objects::CDate_std> std_upd_date,
                                         std_cre_date;

    if(pp->date)               /* -L in command line means replace
                                           date */
    {
        CTime cur_time(CTime::eCurrent);

        std_upd_date.Reset(new objects::CDate_std);
        std_upd_date->SetToTime(cur_time);

        std_cre_date.Reset(new objects::CDate_std);
        std_cre_date->SetToTime(cur_time);

        update = NULL;
        crdate = NULL;
    }
    else
    {
        update = XMLFindTagValue(entry->offset, ibp->xip, INSDSEQ_UPDATE_DATE);
        if (update != NULL)
            std_upd_date = GetUpdateDate(update, pp->source);

        crdate = XMLFindTagValue(entry->offset, ibp->xip, INSDSEQ_CREATE_DATE);
        if (crdate != NULL)
            std_cre_date = GetUpdateDate(crdate, pp->source);
    }

    if (std_upd_date.NotEmpty())
    {
        CRef<objects::CSeqdesc> descr(new objects::CSeqdesc);
        descr->SetUpdate_date().SetStd(*std_upd_date);
        bioseq.SetDescr().Set().push_back(descr);

        if (std_cre_date.NotEmpty() && std_cre_date->Compare(*std_upd_date) == objects::CDate::eCompare_after)
        {
            ErrPostEx(SEV_ERROR, ERR_DATE_IllegalDate,
                      "Update-date \"%s\" precedes create-date \"%s\".",
                      update, crdate);
        }
    }

    if (std_cre_date.NotEmpty())
    {
        if(pp->xml_comp == false || pp->source == Parser::ESource::EMBL)
        {
            CRef<objects::CSeqdesc> descr(new objects::CSeqdesc);
            descr->SetCreate_date().SetStd(*std_cre_date);
            bioseq.SetDescr().Set().push_back(descr);
        }
    }

    if(update != NULL)
        MemFree(update);
    if(crdate != NULL)
        MemFree(crdate);
}

/**********************************************************/
static void XMLGetDivision(char* entry, IndexblkPtr ibp)
{
    char* div;

    if(ibp == NULL || entry == NULL)
        return;

    div = XMLFindTagValue(entry, ibp->xip, INSDSEQ_DIVISION);
    if(div == NULL)
        return;
    div[3] = '\0';
    StringCpy(ibp->division, div);
    MemFree(div);
}
    
/**********************************************************/
bool XMLAscii(ParserPtr pp)
{
    Int4        i;
    Int4        imax;
    Int4        j;
    Int4        segindx;
    Int4        total = 0;
    Int4        total_long = 0;
    Int4        total_dropped = 0;
    char*     div;
    char*     entry;
    EntryBlkPtr ebp;


    TEntryList seq_entries;

    objects::CSeq_loc locs;

    bool        seq_long = false;
    IndexblkPtr ibp;
    IndexblkPtr tibp;
    DataBlkPtr  dbp;

    auto dnaconv = GetDNAConv();             /* set up sequence alphabets */

    segindx = -1;

    for(imax = pp->indx, i = 0; i < imax; i++)
    {
        pp->curindx = i;
        ibp = pp->entrylist[i];

        err_install(ibp, pp->accver);

        if(ibp->segnum == 1)
            segindx = i;

        if(ibp->drop == 1 && ibp->segnum == 0)
        {
            ErrPostEx(SEV_ERROR, ERR_ENTRY_Skipped,
                      "Entry skipped: \"%s|%s\".", ibp->locusname, ibp->acnum);
            total_dropped++;
            continue;
        }

        entry = XMLLoadEntry(pp, false);
        if(entry == NULL)
        {
            FtaDeletePrefix(PREFIX_LOCUS | PREFIX_ACCESSION);
            return false;
        }

        XMLGetDivision(entry, ibp);

        if(StringCmp(ibp->division, "TSA") == 0)
        {
            if(ibp->tsa_allowed == false)
                ErrPostEx(SEV_WARNING, ERR_TSA_UnexpectedPrimaryAccession,
                          "The record with accession \"%s\" is not expected to have a TSA division code.",
                          ibp->acnum);
            ibp->is_tsa = true;
        }

        XMLCheckContigEverywhere(ibp, pp->source);
        if(ibp->drop == 1 && ibp->segnum == 0)
        {
            ErrPostEx(SEV_ERROR, ERR_ENTRY_Skipped,
                      "Entry skipped: \"%s|%s\".", ibp->locusname, ibp->acnum);
            MemFree(entry);
            total_dropped++;
            continue;
        }

        ebp = CreateEntryBlk();

        CRef<objects::CBioseq> bioseq = CreateEntryBioseq(pp, true);
        ebp->seq_entry.Reset(new objects::CSeq_entry);
        ebp->seq_entry->SetSeq(*bioseq);
        GetScope().AddBioseq(*bioseq);

        dbp = (DataBlkPtr) MemNew(sizeof(DataBlk));
        dbp->data = ebp;
        dbp->offset = entry;
        dbp->len = StringLen(entry);

        if(!XMLGetInst(pp, dbp, dnaconv.get(), *bioseq))
        {
            ibp->drop = 1;
            ErrPostStr(SEV_REJECT, ERR_SEQUENCE_BadData,
                       "Bad sequence data. Entry dropped.");
            if(ibp->segnum == 0)
            {
                ErrPostEx(SEV_ERROR, ERR_ENTRY_Skipped,
                          "Entry skipped: \"%s|%s\".",
                          ibp->locusname, ibp->acnum);
                FreeEntry(dbp);
                MemFree(entry);
                total_dropped++;
                continue;
            }
        }

        XMLFakeBioSources(ibp->xip, dbp->offset, *bioseq, pp->source);
        LoadFeat(pp, dbp, *bioseq);

        if (!bioseq->IsSetAnnot() && ibp->drop != 0 && ibp->segnum == 0)
        {
            ErrPostEx(SEV_ERROR, ERR_ENTRY_Skipped,
                      "Entry skipped: \"%s|%s\".", ibp->locusname, ibp->acnum);
            FreeEntry(dbp);
            MemFree(entry);
            total_dropped++;
            continue;
        }

        XMLGetDescr(pp, dbp, *bioseq);

        if (ibp->drop != 0 && ibp->segnum == 0)
        {
            ErrPostEx(SEV_ERROR, ERR_ENTRY_Skipped,
                      "Entry skipped: \"%s|%s\".", ibp->locusname, ibp->acnum);
            FreeEntry(dbp);
            MemFree(entry);
            total_dropped++;
            continue;
        }

        fta_set_molinfo_completeness(*bioseq, ibp);

        if (ibp->is_tsa)
            fta_tsa_tls_comment_dblink_check(*bioseq, true);

        if (ibp->is_tls)
            fta_tsa_tls_comment_dblink_check(*bioseq, false);

        if (bioseq->GetInst().IsNa())
        {
            if (bioseq->GetInst().GetRepr() == objects::CSeq_inst::eRepr_raw)
            {
                if(ibp->gaps != NULL)
                    GapsToDelta(*bioseq, ibp->gaps, &ibp->drop);
                else if(ibp->htg == 4 || ibp->htg == 1 || ibp->htg == 2 ||
                        (ibp->is_pat && pp->source == Parser::ESource::DDBJ))
                        SeqToDelta(*bioseq, ibp->htg);
            }
            else if(ibp->gaps != NULL)
                AssemblyGapsToDelta(*bioseq, ibp->gaps, &ibp->drop);
        }

        if(no_date(pp->format, bioseq->GetDescr().Get()) &&
           pp->debug == false && pp->no_date == false &&
           pp->xml_comp == false && pp->source != Parser::ESource::USPTO)
        {
            ibp->drop = 1;
            ErrPostStr(SEV_ERROR, ERR_DATE_IllegalDate,
                       "Illegal create date. Entry dropped.");
            if(ibp->segnum == 0)
            {
                ErrPostEx(SEV_ERROR, ERR_ENTRY_Skipped,
                          "Entry skipped: \"%s|%s\".",
                          ibp->locusname, ibp->acnum);
                FreeEntry(dbp);
                MemFree(entry);
                total_dropped++;
                continue;
            }
        }

        if (dbp->qscore == NULL && pp->accver)
        {
            if (pp->ff_get_qscore != NULL)
                dbp->qscore = (*pp->ff_get_qscore)(ibp->acnum, ibp->vernum);
            else if (pp->ff_get_qscore_pp != NULL)
                dbp->qscore = (*pp->ff_get_qscore_pp)(ibp->acnum, ibp->vernum, pp);
            else if (pp->qsfd != NULL && ibp->qslength > 0)
                dbp->qscore = GetQSFromFile(pp->qsfd, ibp);
        }

        if(!QscoreToSeqAnnot(dbp->qscore, *bioseq, ibp->acnum, ibp->vernum, false, true))
        {
            if(pp->ign_bad_qs == false)
            {
                ibp->drop = 1;
                ErrPostEx(SEV_ERROR, ERR_QSCORE_FailedToParse,
                          "Error while parsing QScore. Entry dropped.");
                if(ibp->segnum == 0)
                {
                    ErrPostEx(SEV_ERROR, ERR_ENTRY_Skipped,
                              "Entry skipped: \"%s|%s\".",
                              ibp->locusname, ibp->acnum);
                    FreeEntry(dbp);
                    MemFree(entry);
                    total_dropped++;
                    continue;
                }
            }
            else
            {
                ErrPostEx(SEV_ERROR, ERR_QSCORE_FailedToParse,
                          "Error while parsing QScore.");
            }
        }

        if(dbp->qscore != NULL)
        {
            MemFree(dbp->qscore);
            dbp->qscore = NULL;
        }

        if (ibp->psip.NotEmpty())
        {
            CRef<objects::CSeq_id> id(new objects::CSeq_id);
            id->SetPatent(*ibp->psip);
            bioseq->SetId().push_back(id);
            ibp->psip.Reset();
        }

        /* add PatentSeqId if patent is found in reference
         */
        if(no_reference(*bioseq) && !pp->debug)
        {
            if(pp->source == Parser::ESource::Flybase)
            {
                ErrPostStr(SEV_ERROR, ERR_REFERENCE_No_references,
                           "No references for entry from FlyBase. Continue anyway.");
            }
            else if(pp->source == Parser::ESource::Refseq &&
                    StringNCmp(ibp->acnum, "NW_", 3) == 0)
            {
                ErrPostStr(SEV_ERROR, ERR_REFERENCE_No_references,
                           "No references for RefSeq's NW_ entry. Continue anyway.");
            }
            else if(ibp->is_wgs)
            {
                ErrPostStr(SEV_ERROR, ERR_REFERENCE_No_references,
                           "No references for WGS entry. Continue anyway.");
            }
            else
            {
                ibp->drop = 1;
                ErrPostStr(SEV_ERROR, ERR_REFERENCE_No_references,
                           "No references. Entry dropped.");
                if(ibp->segnum == 0)
                {
                    ErrPostEx(SEV_ERROR, ERR_ENTRY_Skipped,
                              "Entry skipped: \"%s|%s\".",
                              ibp->locusname, ibp->acnum);
                    FreeEntry(dbp);
                    MemFree(entry);
                    total_dropped++;
                    continue;
                }
            }
        }

        if(ibp->segnum == ibp->segtotal)
        {
            seq_entries.push_back(ebp->seq_entry);
            ebp->seq_entry.Reset();

            if (ibp->segnum < 2)
            {
                if(ibp->segnum != 0)
                {
                    ErrPostEx(SEV_WARNING, ERR_SEGMENT_OnlyOneMember,
                              "Segmented set contains only one member.");
                }
                segindx = i;
            }
            else
            {
                GetSeqExt(pp, locs);
// LCOV_EXCL_START
// Excluded per Mark's request on 12/14/2016
                BuildBioSegHeader(pp, seq_entries, locs);
// LCOV_EXCL_STOP
            }

            /* reject the whole set if any one entry was rejected
             */
            if(ibp->segnum != 0)
            {
                div = pp->entrylist[segindx]->division;
                for(j = segindx; j <= i; j++)
                {
                    tibp = pp->entrylist[j];
                    err_install(tibp, pp->accver);
                    if(StringCmp(div, tibp->division) != 0)
                    {
                        ErrPostEx(SEV_WARNING, ERR_DIVISION_Mismatch,
                                  "Division different in segmented set: %s: %s",
                                  div, tibp->division);
                    }
                    if(tibp->drop != 0)
                    {
                        ErrPostEx(SEV_WARNING, ERR_SEGMENT_Rejected,
                                  "Reject the whole segmented set");
                        break;
                    }
                }
                if(j <= i)
                {
                    for(j = segindx; j <= i; j++)
                    {
                        tibp = pp->entrylist[j];
                        err_install(tibp, pp->accver);
                        ErrPostEx(SEV_ERROR, ERR_ENTRY_Skipped,
                                  "Entry skipped: \"%s|%s\".",
                                  tibp->locusname, tibp->acnum);
                        total_dropped++;
                    }

                    seq_entries.clear();

                    FreeEntry(dbp);
                    MemFree(entry);
                    GetScope().ResetHistory();
                    continue;
                }
            }

            if(pp->source == Parser::ESource::USPTO)
            {
                GeneRefFeats gene_refs;
                gene_refs.valid = false;
                ProcNucProt(pp, seq_entries, gene_refs);
            }
            else
                DealWithGenes(seq_entries, pp);

            if (seq_entries.empty())
            {
                if(ibp->segnum != 0)
                {
                    ErrPostEx(SEV_WARNING, ERR_SEGMENT_Rejected,
                              "Reject the whole segmented set.");
                    for(j = segindx; j <= i; j++)
                    {
                        tibp = pp->entrylist[j];
                        err_install(tibp, pp->accver);
                        ErrPostEx(SEV_ERROR, ERR_ENTRY_Skipped,
                                  "Entry skipped: \"%s|%s\".",
                                  tibp->locusname, tibp->acnum);
                        total_dropped++;
                    }
                }
                else
                {
                    ErrPostEx(SEV_ERROR, ERR_ENTRY_Skipped,
                              "Entry skipped: \"%s|%s\".",
                              ibp->locusname, ibp->acnum);
                    total_dropped++;
                }
                FreeEntry(dbp);
                MemFree(entry);
                GetScope().ResetHistory();
                continue;
            }

            /* remove out all the features if their seqloc has
             * "join" or "order" among other segments, to the annot
             * which in class = parts
             */
            if(ibp->segnum != 0)
// LCOV_EXCL_START
// Excluded per Mark's request on 12/14/2016
                CheckFeatSeqLoc(seq_entries);
// LCOV_EXCL_STOP

            fta_find_pub_explore(pp, seq_entries);

            /* change qual "citation' on features to SeqFeat.cit
             * find citation in the list by serial_number.
             * If serial number not found remove /citation
             */
            ProcessCitations(seq_entries);

            /* check for long sequences in each segment
             */
            if(pp->limit != 0)
            {
                if(ibp->segnum != 0)
                {
                    for(j = segindx; j <= i; j++)
                    {
                        tibp = pp->entrylist[j];
                        err_install(tibp, pp->accver);
                        if(tibp->bases <= (size_t) pp->limit)
                            continue;

                        if(tibp->htg == 1 || tibp->htg == 2 || tibp->htg == 4)
                        {
                            ErrPostEx(SEV_WARNING, ERR_ENTRY_LongHTGSSequence,
                                      "HTGS Phase 0/1/2 sequence %s|%s exceeds length limit %ld: entry has been processed regardless of this problem",
                                      tibp->locusname, tibp->acnum, pp->limit);
                        }
                        else
                        {
                            seq_long = true;
                            ErrPostEx(SEV_REJECT, ERR_ENTRY_LongSequence,
                                      "Sequence %s|%s is longer than limit %ld",
                                      tibp->locusname, tibp->acnum, pp->limit);
                        }
                    }
                }
                else if(ibp->bases > (size_t) pp->limit)
                {
                    if(ibp->htg == 1 || ibp->htg == 2 || ibp->htg == 4)
                    {
                        ErrPostEx(SEV_WARNING, ERR_ENTRY_LongHTGSSequence,
                                  "HTGS Phase 0/1/2 sequence %s|%s exceeds length limit %ld: entry has been processed regardless of this problem",
                                  ibp->locusname, ibp->acnum, pp->limit);
                    }
                    else
                    {
                        seq_long = true;
                        ErrPostEx(SEV_REJECT, ERR_ENTRY_LongSequence,
                                  "Sequence %s|%s is longer than limit %ld",
                                  ibp->locusname, ibp->acnum, pp->limit);
                    }
                }
            }

            if (pp->convert)
            {
                if(pp->cleanup <= 1)
                {
                    FinalCleanup(seq_entries);

                    if (pp->qamode && !seq_entries.empty())
                        fta_remove_cleanup_user_object(*(*seq_entries.begin()));
                }

                MaybeCutGbblockSource(seq_entries);
            }

            EntryCheckDivCode(seq_entries, pp);

            if(pp->xml_comp)
                fta_set_strandedness(seq_entries);

            if (fta_EntryCheckGBBlock(seq_entries))
            {
                ErrPostStr(SEV_WARNING, ERR_ENTRY_GBBlock_not_Empty,
                           "Attention: GBBlock is not empty");
            }

            /* check for identical features
             */
            if(pp->qamode)
            {
                fta_sort_descr(seq_entries);
                fta_sort_seqfeat_cit(seq_entries);
            }

            if (pp->citat)
            {
                StripSerialNumbers(seq_entries);
            }

            PackEntries(seq_entries);
            CheckDupDates(seq_entries);

            if(ibp->segnum != 0)
                for(j = segindx; j <= i; j++)
                    err_install(pp->entrylist[j], pp->accver);

            if(seq_long)
            {
                seq_long = false;
                if(ibp->segnum != 0)
                    total_long += (i - segindx + 1);
                else
                    total_long++;
            }
            else
            {
                pp->entries.splice(pp->entries.end(), seq_entries);

                if(ibp->segnum != 0)
                    total += (i - segindx + 1);
                else
                    total++;
            }

            if(ibp->segnum != 0)
            {
                for(j = segindx; j <= i; j++)
                {
                    tibp = pp->entrylist[j];
                    err_install(tibp, pp->accver);
                    ErrPostEx(SEV_INFO, ERR_ENTRY_Parsed,
                              "OK - entry parsed successfully: \"%s|%s\".",
                              tibp->locusname, tibp->acnum);
                }
            }
            else
            {
                ErrPostEx(SEV_INFO, ERR_ENTRY_Parsed,
                          "OK - entry parsed successfully: \"%s|%s\".",
                          ibp->locusname, ibp->acnum);
            }

            seq_entries.clear();
        }
        else
        {
            GetSeqExt(pp, locs);

            seq_entries.push_back(ebp->seq_entry);
            ebp->seq_entry.Reset();
        }

        FreeEntry(dbp);
        MemFree(entry);
        GetScope().ResetHistory();

    } /* for, ascii block entries */

    FtaDeletePrefix(PREFIX_LOCUS | PREFIX_ACCESSION);

    ErrPostEx(SEV_INFO, ERR_ENTRY_ParsingComplete,
              "COMPLETED : SUCCEEDED = %d (including: LONG ones = %d); SKIPPED = %d.",
              total, total_long, total_dropped);

    return true;
}

END_NCBI_SCOPE
