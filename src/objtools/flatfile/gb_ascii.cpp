/* gb_ascii.c
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
 * File Name:  gb_ascii.c
 *
 * Author: Karl Sirotkin, Hsiu-Chuan Chen
 *
 * File Description:
 * -----------------
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
#include "ftanet.h"

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
#    undef THIS_FILE
#endif
#define THIS_FILE "gb_ascii.cpp"

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

/**********************************************************/
static char* GBDivOffset(DataBlkPtr entry, Int4 div_shift)
{
    return(entry->offset + div_shift);
}

/**********************************************************/
static void CheckContigEverywhere(IndexblkPtr ibp, Parser::ESource source)
{
    bool condiv = (StringICmp(ibp->division, "CON") == 0);

    if(condiv && ibp->segnum != 0)
    {
        ErrPostEx(SEV_ERROR, ERR_DIVISION_ConDivInSegset,
                  "Use of the CON division is not allowed for members of segmented set : %s|%s. Entry skipped.",
                  ibp->locusname, ibp->acnum);
        ibp->drop = 1;
        return;
    }

    if(!condiv && ibp->is_contig == false && ibp->origin == false &&
       ibp->is_mga == false)
    {
        ErrPostEx(SEV_ERROR, ERR_FORMAT_MissingSequenceData,
                  "Required sequence data is absent. Entry dropped.");
        ibp->drop = 1;
    }
    else if(!condiv && ibp->is_contig && ibp->origin == false)
    {
        ErrPostEx(SEV_WARNING, ERR_DIVISION_MappedtoCON,
                  "Division [%s] mapped to CON based on the existence of CONTIG line.",
                  ibp->division);
    }
    else if(ibp->is_contig && ibp->origin)
    {
        if(source == Parser::ESource::EMBL || source == Parser::ESource::DDBJ)
        {
            ErrPostEx(SEV_INFO, ERR_FORMAT_ContigWithSequenceData,
                      "The CONTIG/CO linetype and sequence data are both present. Ignoring sequence data.");
        }
        else
        {
            ErrPostEx(SEV_REJECT, ERR_FORMAT_ContigWithSequenceData,
                      "The CONTIG/CO linetype and sequence data may not both be present in a sequence record.");
            ibp->drop = 1;
        }
    }
    else if(condiv && ibp->is_contig == false && ibp->origin == false)
    {
        ErrPostEx(SEV_ERROR, ERR_FORMAT_MissingContigFeature,
                  "No CONTIG data in GenBank format file, entry dropped.");
        ibp->drop = 1;
    }
    else if(condiv && ibp->is_contig == false && ibp->origin)
    {
        ErrPostEx(SEV_WARNING, ERR_DIVISION_ConDivLacksContig,
                  "Division is CON, but CONTIG data have not been found.");
    }
}

/**********************************************************/
bool GetGenBankInstContig(DataBlkPtr entry, objects::CBioseq& bsp, ParserPtr pp)
{
    DataBlkPtr dbp;

    char*    p;
    char*    q;
    char*    r;

    bool    locmap;
    bool    sitemap;

    bool    allow_crossdb_featloc;
    Int4       i;
    int        numerr;

    dbp = TrackNodeType(entry, ParFlat_CONTIG);
    if(dbp == NULL || dbp->offset == NULL)
        return true;

    i = static_cast<Int4>(dbp->len) - ParFlat_COL_DATA;
    if(i <= 0)
        return false;

    p = (char*) MemNew(i + 1);
    StringNCpy(p, &dbp->offset[ParFlat_COL_DATA], i);
    p[i-1] = '\0';
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

    CRef<objects::CSeq_loc> loc = xgbparseint_ver(p, locmap, sitemap, numerr, bsp.GetId(), pp->accver);
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
        fta_create_far_fetch_policy_user_object(bsp, i);

    pp->allow_crossdb_featloc = allow_crossdb_featloc;

    if (loc->IsMix())
    {
        XGappedSeqLocsToDeltaSeqs(loc->GetMix(), bsp.SetInst().SetExt().SetDelta().Set());
        bsp.SetInst().SetRepr(objects::CSeq_inst::eRepr_delta);
    }
    else
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
static bool GetGenBankInst(ParserPtr pp, DataBlkPtr entry, unsigned char* dnaconv)
{
    EntryBlkPtr  ebp;
    Int2         topology;
    Int2         strand;
    char*      bptr;
    char*      topstr;
    //char*      strandstr;
    LocusContPtr lcp;
    IndexblkPtr  ibp;

    bptr = entry->offset;
    ibp = pp->entrylist[pp->curindx];
    lcp = &ibp->lc;

    topstr = bptr + lcp->topology;

    ebp = reinterpret_cast<EntryBlkPtr>(entry->data);
    objects::CBioseq& bioseq = ebp->seq_entry->SetSeq();

    objects::CSeq_inst& inst = bioseq.SetInst();
    inst.SetRepr(ibp->is_mga ? objects::CSeq_inst::eRepr_virtual : objects::CSeq_inst::eRepr_raw);

    /* get linear, circular, tandem topology, blank is linear which = 1
     */
    topology = CheckTPG(topstr);
    if (topology > 1)
        inst.SetTopology(static_cast<objects::CSeq_inst::ETopology>(topology));

    strand = CheckSTRAND((lcp->strand >= 0) ? bptr+lcp->strand : "   ");
    if (strand > 0)
        inst.SetStrand(static_cast<objects::CSeq_inst::EStrand>(strand));

    if (GetSeqData(pp, entry, bioseq, ParFlat_ORIGIN, dnaconv,
        (ibp->is_prot ? objects::eSeq_code_type_iupacaa : objects::eSeq_code_type_iupacna)) == false)
        return false;

    if(ibp->is_contig && !GetGenBankInstContig(entry, bioseq, pp))
        return false;

    return true;
}

/**********************************************************/
static char* GetGenBankLineage(char* start, char* end)
{
    char* p;
    char* str;

    if(start >= end)
        return(NULL);

    str = GetBlkDataReplaceNewLine(start, end, ParFlat_COL_DATA);
    if(str == NULL)
        return(NULL);

    for(p = str; *p != '\0';)
        p++;
    if(p == str)
    {
        MemFree(str);
        return(NULL);
    }
    for(p--;; p--)
    {
        if(*p != ' ' && *p != '\t' && *p != '\n' && *p != '.' && *p != ';')
        {
            p++;
            break;
        }
        if(p == str)
            break;
    }
    if(p == str)
    {
        MemFree(str);
        return(NULL);
    }
    *p = '\0';
    return(str);
}

/**********************************************************
 *
 *   static GBBlockPtr GetGBBlock(pp, entry, mfp, biosp):
 *
 *                                              4-7-93
 *
 **********************************************************/
static CRef<objects::CGB_block> GetGBBlock(ParserPtr pp, DataBlkPtr entry, objects::CMolInfo& mol_info,
                                                       objects::CBioSource* bio_src)
{
    LocusContPtr lcp;

    CRef<objects::CGB_block> gbb(new objects::CGB_block),
                                         ret;

    IndexblkPtr  ibp;
    char*      bptr;
    char*      eptr;
    char*      ptr;
    char*      str;
    Char         msg[4];
    char*      kw;
    char*      kwp;
    size_t       len;
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
    char*      p;
    Int4         i;

    ibp = pp->entrylist[pp->curindx];
    ibp->wgssec[0] = '\0';

    bptr = SrchNodeType(entry, ParFlat_SOURCE, &len);
    str = GetBlkDataReplaceNewLine(bptr, bptr + len, ParFlat_COL_DATA);
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
        GetSequenceOfKeywords(entry, ParFlat_KEYWORDS, ParFlat_COL_DATA, gbb->SetKeywords());

    if (ibp->is_mga && !fta_check_mga_keywords(mol_info, gbb->GetKeywords()))
    {
        return ret;
    }

    if (ibp->is_tpa && !fta_tpa_keywords_check(gbb->GetKeywords()))
    {
        return ret;
    }

    if(ibp->is_tsa && !fta_tsa_keywords_check(gbb->GetKeywords(), pp->source))
    {
        return ret;
    }

    if(ibp->is_tls && !fta_tls_keywords_check(gbb->GetKeywords(), pp->source))
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

    bptr = SrchNodeType(entry, ParFlat_ORIGIN, &len);
    eptr = bptr + len;
    ptr = SrchTheChar(bptr, eptr, '\n');
    if(ptr != NULL)
    {
        eptr = ptr;
        bptr += 6;

        if(eptr != bptr)
        {
            while(IS_WHITESP(*bptr) != 0)
                bptr++;
            len = eptr - bptr;
            if(len > 0)
            {
                gbb->SetOrigin(std::string(bptr, eptr));
            }
        }
    }

    lcp = &ibp->lc;

    bptr = GBDivOffset(entry, lcp->div);

    if(*bptr != ' ')
    {
        if_cds = check_cds(entry, pp->format);
        div = CheckDIV(bptr);
        if(div != -1)
        {
            std::string div_str(bptr, bptr + 3);
            gbb->SetDiv(div_str);

            if (div == 16)               /* "ORG" replaced by "UNA" */
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

            if(StringCmp(p_div, "HTG") == 0)
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
                gbb->ResetDiv();

            DefVsHTGKeywords(mol_info.GetTech(), entry, ParFlat_DEFINITION,
                             ParFlat_ORIGIN, cancelled);

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
                ErrPostEx(SEV_ERROR, ERR_KEYWORD_NoGeneExpressionKeywords,
                          "This is apparently a CAGE or 5'-SAGE record, but it lacks the required keywords. Entry dropped.");
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
                else if((i == 2 && wgs_kwd && tpa_kwd) ||
                        (i == 2 && tsa_kwd && tpa_kwd))
                {
                }
                else if(i != 2 || env_kwd == false ||
                   (est_kwd == false && gss_kwd == false && wgs_kwd == false))
                {
                    if(i != 2 || pp->source != Parser::ESource::DDBJ ||
                       ibp->is_tsa == false || env_kwd == false)
                    {
                        ErrPostEx(SEV_REJECT, ERR_KEYWORD_ConflictingKeywords,
                                  "This record contains more than one of the special keywords used to indicate that a sequence is an HTG, EST, GSS, STS, HTC, WGS, ENV, FLI_CDNA, TPA, CAGE, TSA or TLS sequence.");
                        return ret;
                    }
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

            objects::CMolInfo::TTech thtg = mol_info.GetTech();
            if (thtg == objects::CMolInfo::eTech_htgs_0 || thtg == objects::CMolInfo::eTech_htgs_1 ||
                thtg == objects::CMolInfo::eTech_htgs_2 || thtg == objects::CMolInfo::eTech_htgs_3)
            {
                RemoveHtgPhase(gbb->SetKeywords());
            }

            bptr = SrchNodeType(entry, ParFlat_KEYWORDS, &len);
            if(bptr != NULL)
            {
                kw = GetBlkDataReplaceNewLine(bptr, bptr + len,
                                              ParFlat_COL_DATA);

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
                char* div_to_check = gbb->IsSetDiv() ? StringSave(gbb->GetDiv().c_str()) : StringSave("");
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
                    gbb->ResetDiv();

                if (drop)
                {
                    return ret;
                }
            }
            else if (gbb->GetDiv() == "CON")
            {
                gbb->ResetDiv();
            }
        }
        else if (pp->mode != Parser::EMode::Relaxed)
        {
            MemCpy(msg, bptr, 3);
            msg[3] = '\0';
            ErrPostEx(SEV_REJECT, ERR_DIVISION_UnknownDivCode,
                      "Unknown division code \"%s\" found in GenBank flatfile. Record rejected.",
                      msg);
            return ret;
        }

        if(IsNewAccessFormat(ibp->acnum) == 0 && *ibp->acnum == 'T' &&
           gbb->IsSetDiv() && gbb->GetDiv() != "EST")
        {
            ErrPostStr(SEV_INFO, ERR_DIVISION_MappedtoEST,
                       "Leading T in accession number.");

            mol_info.SetTech(objects::CMolInfo::eTech_est);
            gbb->ResetDiv();
        }
    }

    bool is_htc_div = gbb->IsSetDiv() && gbb->GetDiv() == "HTC",
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
        bptr = entry->offset;
        p = bptr + lcp->molecule;
        if(*p == 'm' || *p == 'r')
            p++;
        else if(StringNCmp(p, "pre-", 4) == 0)
            p += 4;
        else if(StringNCmp(p, "transcribed ", 12) == 0)
            p += 12;

        if(StringNCmp(p, "RNA", 3) != 0)
        {
            ErrPostEx(SEV_ERROR, ERR_DIVISION_HTCWrongMolType,
                      "All HTC division records should have a moltype of pre-RNA, mRNA or RNA.");
            return ret;
        }
    }

    if (fli_kwd)
        mol_info.SetTech(objects::CMolInfo::eTech_fli_cdna);

    /* will be used in flat file database
     */
    if (gbb->IsSetDiv())
    {
        if (gbb->GetDiv() == "EST")
        {
            ibp->EST = true;
            mol_info.SetTech(objects::CMolInfo::eTech_est);

            gbb->ResetDiv();
        }
        else if (gbb->GetDiv() == "STS")
        {
            ibp->STS = true;
            mol_info.SetTech(objects::CMolInfo::eTech_sts);

            gbb->ResetDiv();
        }
        else if (gbb->GetDiv() == "GSS")
        {
            ibp->GSS = true;
            mol_info.SetTech(objects::CMolInfo::eTech_survey);

            gbb->ResetDiv();
        }
        else if (gbb->GetDiv() == "HTC")
        {
            ibp->HTC = true;
            mol_info.SetTech(objects::CMolInfo::eTech_htc);

            gbb->ResetDiv();
        }
        else if (gbb->GetDiv() == "SYN" && bio_src != NULL && bio_src->IsSetOrigin() &&
                bio_src->GetOrigin() == 5)     /* synthetic */
        {
            gbb->ResetDiv();
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

    if(ibp->is_tpa)
        fta_remove_tpa_keywords(gbb->SetKeywords());

    if(ibp->is_tsa)
        fta_remove_tsa_keywords(gbb->SetKeywords(), pp->source);

    if(ibp->is_tls)
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

    if (pp->source == Parser::ESource::DDBJ && gbb->IsSetDiv() && bio_src != NULL &&
        bio_src->IsSetOrg() && bio_src->GetOrg().IsSetOrgname() &&
        bio_src->GetOrg().GetOrgname().IsSetDiv())
    {
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
static CRef<objects::CMolInfo> GetGenBankMolInfo(ParserPtr pp, DataBlkPtr entry,
                                                             const objects::COrg_ref* org_ref)
{
    IndexblkPtr ibp;
    char*     bptr;
    char*     molstr = NULL;

    CRef<objects::CMolInfo> mol_info(new objects::CMolInfo);

    bptr = entry->offset;
    ibp = pp->entrylist[pp->curindx];

    molstr = bptr + ibp->lc.molecule;

    bptr = GBDivOffset(entry, ibp->lc.div);

    if(StringNCmp(bptr, "EST", 3) == 0)
        mol_info->SetTech(objects::CMolInfo::eTech_est);

    else if(StringNCmp(bptr, "STS", 3) == 0)
        mol_info->SetTech(objects::CMolInfo::eTech_sts);

    else if(StringNCmp(bptr, "GSS", 3) == 0)
        mol_info->SetTech(objects::CMolInfo::eTech_survey);

    else if(StringNCmp(bptr, "HTG", 3) == 0)
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

    else if(ibp->is_mga)
    {
        mol_info->SetTech(objects::CMolInfo::eTech_other);
        mol_info->SetTechexp("cage");
    }

    GetFlatBiomol(mol_info->SetBiomol(), mol_info->GetTech(), molstr, pp, entry, org_ref);
    if (mol_info->GetBiomol() == objects::CMolInfo::eBiomol_unknown) // not set
        mol_info->ResetBiomol();

    return mol_info;
}

/**********************************************************/
static void FakeGenBankBioSources(DataBlkPtr entry, objects::CBioseq& bioseq)
{
    char*      bptr;
    char*      end;
    char*      ptr;

    Char         ch;

    size_t len = 0;
    bptr = SrchNodeSubType(entry, ParFlat_SOURCE, ParFlat_ORGANISM, &len);

    if(bptr == NULL)
    {
        ErrPostStr(SEV_WARNING, ERR_ORGANISM_NoOrganism,
                   "No Organism data in genbank format file");
        return;
    }

    end = bptr + len;
    ch = *end;
    *end = '\0';

    CRef<objects::CBioSource> bio_src(new objects::CBioSource);
    bptr += ParFlat_COL_DATA;

    if (GetGenomeInfo(*bio_src, bptr) && bio_src->GetGenome() != 9)   /* ! Plasmid */
    {
        while(*bptr != ' ' && *bptr != '\0')
            bptr++;
        while(*bptr == ' ')
            bptr++;
    }

    ptr = StringChr(bptr, '\n');
    if(ptr == NULL)
    {
        *end = ch;
        return;
    }

    objects::COrg_ref& org_ref = bio_src->SetOrg();

    *ptr = '\0';
    org_ref.SetTaxname(bptr);
    *ptr = '\n';

    for(;;)
    {
        bptr = ptr + 1;
        if(StringNCmp(bptr, "               ", ParFlat_COL_DATA) != 0)
            break;

        ptr = StringChr(bptr, '\n');
        if(ptr == NULL)
            break;

        *ptr = '\0';
        if(StringChr(bptr, ';') != NULL || StringChr(ptr + 1, '\n') == NULL)
        {
            *ptr = '\n';
            break;
        }

        bptr += ParFlat_COL_DATA;
        std::string& taxname = org_ref.SetTaxname();
        taxname += ' ';
        taxname += bptr;

        *ptr = '\n';
    }

    *end = ch;

    if (org_ref.GetTaxname() == "Unknown.")
    {
        std::string& taxname = org_ref.SetTaxname();
        taxname = taxname.substr(0, taxname.size() - 1);
    }

    ptr = GetGenBankLineage(bptr, end);
    if(ptr != NULL)
    {
        org_ref.SetOrgname().SetLineage(ptr);
    }

    CRef<objects::CSeqdesc> descr(new objects::CSeqdesc);
    descr->SetSource(*bio_src);
    bioseq.SetDescr().Set().push_back(descr);
}

/**********************************************************/
static void fta_get_user_field(char* line, const Char *tag, objects::CUser_object& user_obj)
{
    char*      p;
    char*      q;
    char*      res;
    Char         ch;

    p = StringStr(line, "USER        ");
    if(p == NULL)
        ch = '\0';
    else
    {
        ch = 'U';
        *p = '\0';
    }

    res = StringSave(line);
    if(ch == 'U')
        *p = 'U';

    for(q = res, p = res; *p != '\0'; p++)
        if(*p != ' ')
            *q++ = *p;
    *q = '\0';

    CRef<objects::CUser_field> root_field(new objects::CUser_field);
    root_field->SetLabel().SetStr(tag);

    for(q = res;;)
    {
        q = StringStr(q, "\nACCESSION=");
        if(q == NULL)
            break;

        q += 11;
        for(p = q; *p != '\0' && *p != '\n' && *p != ';';)
            p++;
        ch = *p;
        *p = '\0';

        CRef<objects::CUser_field> cur_field(new objects::CUser_field);
        cur_field->SetLabel().SetStr("accession");
        cur_field->SetString(q);

        *p = ch;

        CRef<objects::CUser_field> field_set(new objects::CUser_field);
        field_set->SetData().SetFields().push_back(cur_field);

        if(StringNCmp(p, ";gi=", 4) == 0)
        {
            p += 4;
            for(q = p; *p >= '0' && *p <= '9';)
                p++;
            ch = *p;
            *p = '\0';

            cur_field.Reset(new objects::CUser_field);
            cur_field->SetLabel().SetStr("gi");
            cur_field->SetNum(atoi(q));
            field_set->SetData().SetFields().push_back(cur_field);

            *p = ch;
        }

        root_field->SetData().SetFields().push_back(cur_field);
    }

    MemFree(res);

    if (!root_field->IsSetData())
        return;

    user_obj.SetData().push_back(root_field);
}

/**********************************************************/
static void fta_get_str_user_field(char* line, const Char *tag, objects::CUser_object& user_obj)
{
    char*      p;
    char*      q;
    char*      r;
    char*      res;
    Char         ch;

    p = StringStr(line, "USER        ");
    if(p == NULL)
        ch = '\0';
    else
    {
        ch = 'U';
        *p = '\0';
    }

    res = (char*) MemNew(StringLen(line) + 1);
    for(q = line; *q == ' ' || *q == '\n';)
        q++;
    for(r = res; *q != '\0';)
    {
        if(*q != '\n')
        {
            *r++ = *q++;
            continue;
        }
        while(*q == ' ' || *q == '\n')
            q++;
        if(*q != '\0')
            *r++ = ' ';
    }
    *r = '\0';
    if(ch == 'U')
        *p = 'U';

    if(*res == '\0')
    {
        MemFree(res);
        return;
    }

    CRef<objects::CUser_field> field(new objects::CUser_field);
    field->SetLabel().SetStr(tag);
    field->SetString(res);

    MemFree(res);

    user_obj.SetData().push_back(field);
}

/**********************************************************/
static void fta_get_user_object(objects::CSeq_entry& seq_entry, DataBlkPtr entry)
{
    char*       p;
    char*       q;
    char*       r;
    Char          ch;
    size_t        l;

    p = SrchNodeType(entry, ParFlat_USER, &l);
    if(l < ParFlat_COL_DATA)
        return;

    ch = p[l-1];
    p[l-1] = '\0';
    q = StringSave(p);
    p[l-1] = ch;

    CRef<objects::CUser_object> user_obj(new objects::CUser_object);
    user_obj->SetType().SetStr("RefGeneTracking");

    for (p = q;;)
    {
        p = StringStr(p, "USER        ");
        if(p == NULL)
            break;
        for(p += 12; *p == ' ';)
            p++;
        for(r = p; *p != '\0' && *p != '\n' && *p != ' ';)
            p++;
        if(*p == '\0' || p == r)
            break;
        if(StringNCmp(r, "Related", 7) == 0)
            fta_get_user_field(p, "Related", *user_obj);
        else if(StringNCmp(r, "Assembly", 8) == 0)
            fta_get_user_field(p, "Assembly", *user_obj);
        else if(StringNCmp(r, "Comment", 7) == 0)
            fta_get_str_user_field(p, "Comment", *user_obj);
        else
            continue;
    }

    if (!user_obj->IsSetData())
        return;

    CRef<objects::CSeqdesc> descr(new objects::CSeqdesc);
    descr->SetUser(*user_obj);

    if (seq_entry.IsSeq())
        seq_entry.SetSeq().SetDescr().Set().push_back(descr);
    else
        seq_entry.SetSet().SetDescr().Set().push_back(descr);
}

/**********************************************************/
static void fta_get_mga_user_object(TSeqdescList& descrs, char* offset,
                                    size_t len)
{
    char*       str;
    char*       p;

    if(offset == NULL)
        return;

    str = StringSave(offset + ParFlat_COL_DATA);
    p = StringChr(str, '\n');
    if(p != NULL)
        *p = '\0';
    p = StringChr(str, '-');
    if(p != NULL)
        *p++ = '\0';

    CRef<objects::CUser_object> user_obj(new objects::CUser_object);

    objects::CObject_id& id = user_obj->SetType();
    id.SetStr("CAGE-Tag-List");

    CRef<objects::CUser_field> field(new objects::CUser_field);

    field->SetLabel().SetStr("CAGE_tag_total");
    field->SetData().SetInt(static_cast<objects::CUser_field::C_Data::TInt>(len));
    user_obj->SetData().push_back(field);

    field.Reset(new objects::CUser_field);

    field->SetLabel().SetStr("CAGE_accession_first");
    field->SetData().SetStr(str);
    user_obj->SetData().push_back(field);

    field.Reset(new objects::CUser_field);

    field->SetLabel().SetStr("CAGE_accession_last");
    field->SetData().SetStr(p);
    user_obj->SetData().push_back(field);

    MemFree(str);

    CRef<objects::CSeqdesc> descr(new objects::CSeqdesc);
    descr->SetUser(*user_obj);

    descrs.push_back(descr);
}

/**********************************************************/
static void GetGenBankDescr(ParserPtr pp, DataBlkPtr entry, objects::CBioseq& bioseq)
{
    IndexblkPtr   ibp;

    DataBlkPtr    dbp;

    char*       offset;
    char*       str;
    char*       p;
    char*       q;

    bool          is_htg;

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
    CRef<objects::CMolInfo> mol_info = GetGenBankMolInfo(pp, entry, org_ref);

    /* DEFINITION data ==> descr_title
     */
    str = NULL;
    size_t len = 0;
    offset = SrchNodeType(entry, ParFlat_DEFINITION, &len);

    std::string title;
    if(offset != NULL)
    {
        str = GetBlkDataReplaceNewLine(offset, offset + len, ParFlat_COL_DATA);

        for(p = str; *p == ' ';)
            p++;
        if(p > str)
            fta_StringCpy(str, p);

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

    CRef<objects::CUser_object> dbuop;
    offset = SrchNodeType(entry, ParFlat_DBLINK, &len);
    if (offset != NULL)
        fta_get_dblink_user_object(bioseq.SetDescr().Set(), offset, len,
                                   pp->source, &ibp->drop, dbuop);
    else
    {
        offset = SrchNodeType(entry, ParFlat_PROJECT, &len);
        if(offset != NULL)
            fta_get_project_user_object(bioseq.SetDescr().Set(), offset, Parser::EFormat::GenBank,
                                        &ibp->drop, pp->source);
    }

    if(ibp->is_mga)
    {
        offset = SrchNodeType(entry, ParFlat_MGA, &len);
        fta_get_mga_user_object(bioseq.SetDescr().Set(), offset, ibp->bases);
    }
    if(ibp->is_tpa &&
       (title.empty() || (StringNCmp(title.c_str(), "TPA:", 4) != 0 &&
                          StringNCmp(title.c_str(), "TPA_exp:", 8) != 0 &&
                          StringNCmp(title.c_str(), "TPA_inf:", 8) != 0 &&
                          StringNCmp(title.c_str(), "TPA_asm:", 8) != 0 &&
                          StringNCmp(title.c_str(), "TPA_reasm:", 10) != 0)))
    {
        ErrPostEx(SEV_REJECT, ERR_DEFINITION_MissingTPA,
                  "This is apparently a TPA record, but it lacks the required \"TPA:\" prefix on its definition line. Entry dropped.");
        ibp->drop = 1;
        return;
    }
    if(ibp->is_tsa && !ibp->is_tpa &&
       (title.empty() || StringNCmp(title.c_str(), "TSA:", 4) != 0))
    {
        ErrPostEx(SEV_REJECT, ERR_DEFINITION_MissingTSA,
                  "This is apparently a TSA record, but it lacks the required \"TSA:\" prefix on its definition line. Entry dropped.");
        ibp->drop = 1;
        return;
    }
    if(ibp->is_tls && (title.empty() || StringNCmp(title.c_str(), "TLS:", 4) != 0))
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
    dbp = TrackNodeType(entry, ParFlat_REF_END);
    for(; dbp != NULL; dbp = dbp->next)
    {
        if(dbp->type != ParFlat_REF_END)
            continue;

        CRef<objects::CPubdesc> pubdesc = DescrRefs(pp, dbp, ParFlat_COL_DATA);
        if (pubdesc.NotEmpty())
        {
            CRef<objects::CSeqdesc> descr(new objects::CSeqdesc);
            descr->SetPub(*pubdesc);
            bioseq.SetDescr().Set().push_back(descr);
        }
    }

    dbp = TrackNodeType(entry, ParFlat_REF_NO_TARGET);
    for(; dbp != NULL; dbp = dbp->next)
    {
        if(dbp->type != ParFlat_REF_NO_TARGET)
            continue;

        CRef<objects::CPubdesc> pubdesc = DescrRefs(pp, dbp, ParFlat_COL_DATA);
        if (pubdesc.NotEmpty())
        {
            CRef<objects::CSeqdesc> descr(new objects::CSeqdesc);
            descr->SetPub(*pubdesc);
            bioseq.SetDescr().Set().push_back(descr);
        }
    }

    /* GB-block
     */
    CRef<objects::CGB_block> gbbp = GetGBBlock(pp, entry, *mol_info, bio_src);

    if ((pp->source == Parser::ESource::DDBJ || pp->source == Parser::ESource::EMBL) &&
        ibp->is_contig && (!mol_info->IsSetTech() || mol_info->GetTech() == 0))
    {
        Uint1 tech = fta_check_con_for_wgs(bioseq);
        if (tech == 0)
            mol_info->ResetTech();
        else
            mol_info->SetTech(tech);
    }

    if (mol_info->IsSetBiomol() || mol_info->IsSetTech())
    {
        CRef<objects::CSeqdesc> descr(new objects::CSeqdesc);
        descr->SetMolinfo(*mol_info);
        bioseq.SetDescr().Set().push_back(descr);
    }

    if (gbbp.Empty())
    {
        ibp->drop = 1;
        return;
    }

    if(pp->taxserver == 1 && gbbp->IsSetDiv())
        fta_fix_orgref_div(bioseq.GetAnnot(), *org_ref, *gbbp);

    if(StringNICmp(ibp->division, "CON", 3) == 0)
        fta_add_hist(pp, bioseq, gbbp->SetExtra_accessions(), Parser::ESource::DDBJ,
                     objects::CSeq_id::e_Ddbj, true, ibp->acnum);
    else
        fta_add_hist(pp, bioseq, gbbp->SetExtra_accessions(), Parser::ESource::DDBJ,
                     objects::CSeq_id::e_Ddbj, false, ibp->acnum);

    {
        CRef<objects::CSeqdesc> descr(new objects::CSeqdesc);
        descr->SetGenbank(*gbbp);
        bioseq.SetDescr().Set().push_back(descr);
    }

    offset = SrchNodeType(entry, ParFlat_PRIMARY, &len);
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

    if(offset != NULL && len > 0 &&
       fta_parse_tpa_tsa_block(bioseq, offset, ibp->acnum, ibp->vernum,
                               len, ParFlat_COL_DATA, ibp->is_tpa) == false)
    {
        ibp->drop = 1;
        return;
    }

    if(mol_info.NotEmpty() && mol_info->IsSetTech() &&
       (mol_info->GetTech() == objects::CMolInfo::eTech_htgs_0 ||
       mol_info->GetTech() == objects::CMolInfo::eTech_htgs_1 ||
       mol_info->GetTech() == objects::CMolInfo::eTech_htgs_2))
        is_htg = true;
    else
        is_htg = false;

    /* COMMENT data
     */
    offset = SrchNodeType(entry, ParFlat_COMMENT, &len);
    if(offset != NULL && len > 0)
    {
        str = GetDescrComment(offset, len, ParFlat_COL_DATA,
                              (pp->xml_comp ? false : is_htg),
                              ibp->is_pat);
        if(str != NULL)
        {
            bool bad = false;
            TUserObjVector user_objs;

            fta_parse_structured_comment(str, bad, user_objs);
            if(bad)
            {
                ibp->drop = 1;
                MemFree(str);
                return;
            }

            NON_CONST_ITERATE(TUserObjVector, user_obj, user_objs)
            {
                CRef<objects::CSeqdesc> descr(new objects::CSeqdesc);
                descr->SetUser(*(*user_obj));
                bioseq.SetDescr().Set().push_back(descr);
            }

            if(pp->xml_comp)
            {
                for(q = str, p = q; *p != '\0';)
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

            if (str[0] != 0)
            {
                CRef<objects::CSeqdesc> descr(new objects::CSeqdesc);
                descr->SetComment(str);
                bioseq.SetDescr().Set().push_back(descr);
            }
            MemFree(str);
        }
    }

    /* DATE
     */
    if(pp->no_date)            /* -N in command line means no date */
        return;

    CRef<objects::CDate> date;
    if (pp->date)               /* -L in command line means replace date */
    {
        CTime time(CTime::eCurrent);
        date.Reset(new objects::CDate);
        date->SetToTime(time);
    }
    else if(ibp->lc.date > 0)
    {
        CRef<objects::CDate_std> std_date = GetUpdateDate(entry->offset+ibp->lc.date, pp->source);
        if (std_date.NotEmpty())
        {
            date.Reset(new objects::CDate);
            date->SetStd(*std_date);
        }
    }

    if (date.NotEmpty())
    {
        CRef<objects::CSeqdesc> descr(new objects::CSeqdesc);
        descr->SetUpdate_date(*date);
        bioseq.SetDescr().Set().push_back(descr);
    }
}

/**********************************************************/
static void GenBankGetDivision(char* division, Int4 div, DataBlkPtr entry)
{
    StringNCpy(division, GBDivOffset(entry, div), 3);
    division[3] = '\0';
}

/**********************************************************
 *
 *   bool GenBankAscii(pp):
 *
 *      Return FALSE if allocate entry block failed.
 *
 *                                              3-17-93
 *
 **********************************************************/
bool GenBankAscii(ParserPtr pp)
{
    Int2        curkw;
    Int4        i;
    Int4        imax;
    Int4        j;
    Int4        segindx;
    Int4        total = 0;
    Int4        total_long = 0;
    Int4        total_dropped = 0;
    char*     ptr;
    char*     eptr;
    char*     div;
    DataBlkPtr  entry;
    EntryBlkPtr ebp;

//    unsigned char*    dnaconv;
//    unsigned char*    protconv;
    unsigned char*    conv;

    TEntryList seq_entries;

    objects::CSeq_loc locs;

    bool     seq_long = false;

    IndexblkPtr ibp;
    IndexblkPtr tibp;

    auto dnaconv = GetDNAConv();             /* set up sequence alphabets */
    auto protconv = GetProteinConv();        /* set up sequence alphabets */

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

        entry = LoadEntry(pp, ibp->offset, ibp->len);
        if(entry == NULL)
        {
            FtaDeletePrefix(PREFIX_LOCUS | PREFIX_ACCESSION);
            //MemFree(dnaconv);
            //MemFree(protconv);
            return false;
        }

        ebp = (EntryBlkPtr) entry->data;
        ptr = entry->offset;
        eptr = ptr + entry->len;
        curkw = ParFlat_LOCUS;
        while(curkw != ParFlat_END && ptr < eptr)
        {
            ptr = GetGenBankBlock(&ebp->chain, ptr, &curkw, eptr);
        }

        if (pp->entrylist[pp->curindx]->lc.div > -1) {
            GenBankGetDivision(pp->entrylist[pp->curindx]->division, pp->entrylist[pp->curindx]->lc.div, entry);
            if(StringCmp(ibp->division, "TSA") == 0)
            {
                if(ibp->tsa_allowed == false)
                    ErrPostEx(SEV_WARNING, ERR_TSA_UnexpectedPrimaryAccession,
                            "The record with accession \"%s\" is not expected to have a TSA division code.",
                            ibp->acnum);
                ibp->is_tsa = true;
            }
        }

        CheckContigEverywhere(ibp, pp->source);
        if(ibp->drop == 1 && ibp->segnum == 0)
        {
            ErrPostEx(SEV_ERROR, ERR_ENTRY_Skipped,
                      "Entry skipped: \"%s|%s\".", ibp->locusname, ibp->acnum);
            FreeEntry(entry);
            total_dropped++;
            continue;
        }

        if(ptr >= eptr)
        {
            ibp->drop = 1;
            ErrPostStr(SEV_ERROR, ERR_FORMAT_MissingEnd,
                       "Missing end of the entry. Entry dropped.");
            if(ibp->segnum == 0)
            {
                ErrPostEx(SEV_ERROR, ERR_ENTRY_Skipped,
                          "Entry skipped: \"%s|%s\".",
                          ibp->locusname, ibp->acnum);
                FreeEntry(entry);
                total_dropped++;
                continue;
            }
        }
        GetGenBankSubBlock(entry, ibp->bases);

        CRef<objects::CBioseq> bioseq = CreateEntryBioseq(pp, true);
        AddNIDSeqId(*bioseq, entry, ParFlat_NCBI_GI, ParFlat_COL_DATA, pp->source);

        if(StringNCmp(entry->offset + ibp->lc.bp, "aa", 2) == 0)
        {
            ibp->is_prot = true;
            conv = protconv.get();
        }
        else
        {
            ibp->is_prot = false;
            conv = dnaconv.get();
        }

        ebp->seq_entry.Reset(new objects::CSeq_entry);
        ebp->seq_entry->SetSeq(*bioseq);
        GetScope().AddBioseq(*bioseq);

        if (!GetGenBankInst(pp, entry, conv))
        {
            ibp->drop = 1;
            ErrPostStr(SEV_REJECT, ERR_SEQUENCE_BadData,
                       "Bad sequence data. Entry dropped.");
            if(ibp->segnum == 0)
            {
                ErrPostEx(SEV_ERROR, ERR_ENTRY_Skipped,
                          "Entry skipped: \"%s|%s\".",
                          ibp->locusname, ibp->acnum);
                FreeEntry(entry);
                total_dropped++;
                continue;
            }
        }

        FakeGenBankBioSources(entry, *bioseq);
        LoadFeat(pp, entry, *bioseq);

        if (!bioseq->IsSetAnnot() && ibp->drop != 0 && ibp->segnum == 0)
        {
            ErrPostEx(SEV_ERROR, ERR_ENTRY_Skipped,
                      "Entry skipped: \"%s|%s\".", ibp->locusname, ibp->acnum);
            FreeEntry(entry);
            total_dropped++;
            continue;
        }

        GetGenBankDescr(pp, entry, *bioseq);
        if(ibp->drop != 0 && ibp->segnum == 0)
        {
            ErrPostEx(SEV_ERROR, ERR_ENTRY_Skipped,
                      "Entry skipped: \"%s|%s\".", ibp->locusname, ibp->acnum);
            FreeEntry(entry);
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

        if (no_date(pp->format, bioseq->GetDescr().Get()) && pp->debug == false &&
           pp->no_date == false &&
           pp->mode != Parser::EMode::Relaxed)
        {
            ibp->drop = 1;
            ErrPostStr(SEV_ERROR, ERR_DATE_IllegalDate,
                       "Illegal create date. Entry dropped.");
            if(ibp->segnum == 0)
            {
                ErrPostEx(SEV_ERROR, ERR_ENTRY_Skipped,
                          "Entry skipped: \"%s|%s\".",
                          ibp->locusname, ibp->acnum);
                FreeEntry(entry);
                total_dropped++;
                continue;
            }
        }

        if (entry->qscore == NULL && pp->accver)
        {
            if (pp->ff_get_qscore != NULL)
                entry->qscore = (*pp->ff_get_qscore)(ibp->acnum, ibp->vernum);
            else if (pp->ff_get_qscore_pp != NULL)
                entry->qscore = (*pp->ff_get_qscore_pp)(ibp->acnum, ibp->vernum, pp);
            if (pp->qsfd != NULL && ibp->qslength > 0)
                entry->qscore = GetQSFromFile(pp->qsfd, ibp);
        }

        if (!QscoreToSeqAnnot(entry->qscore, *bioseq, ibp->acnum, ibp->vernum, false, true))
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
                    FreeEntry(entry);
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

        if(entry->qscore != NULL)
        {
            MemFree(entry->qscore);
            entry->qscore = NULL;
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
        if(pp->mode != Parser::EMode::Relaxed &&
           pp->debug == false &&
           ibp->wgs_and_gi != 3 &&
           no_reference(*bioseq))
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
                    FreeEntry(entry);
                    total_dropped++;
                    continue;
                }
            }
        }

        if (ibp->segnum == ibp->segtotal)
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

                    FreeEntry(entry);
                    continue;
                }
            }

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
                FreeEntry(entry);
                continue;
            }

            if (pp->source == Parser::ESource::Flybase && !seq_entries.empty())
                fta_get_user_object(*(*seq_entries.begin()), entry);

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

            /* change qual "citation" on features to SeqFeat.cit
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
            if (pp->mode == Parser::EMode::Relaxed) {
                for(auto pEntry : seq_entries) {
                    auto pScope = Ref(new CScope(*CObjectManager::GetInstance()));
                    g_InstantiateMissingProteins(pScope->AddTopLevelSeqEntry(*pEntry));
                }
            }
            if (pp->convert)
            {
                if(pp->cleanup <= 1)
                {
                    FinalCleanup(seq_entries);

                    if (pp->qamode && !seq_entries.empty())
                        fta_remove_cleanup_user_object(*seq_entries.front());
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

            if (seq_long)
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

        FreeEntry(entry);

    } /* for, ascii block entries */

    FtaDeletePrefix(PREFIX_LOCUS | PREFIX_ACCESSION);

    ErrPostEx(SEV_INFO, ERR_ENTRY_ParsingComplete,
              "COMPLETED : SUCCEEDED = %d (including: LONG ones = %d); SKIPPED = %d.",
              total, total_long, total_dropped);
   // MemFree(dnaconv);
   // MemFree(protconv);

    return true;
}

// LCOV_EXCL_START
// Excluded per Mark's request on 12/14/2016
/**********************************************************
 *
 *   static void SrchFeatSeqLoc(sslbp, sfp):
 *
 *                                              9-14-93
 *
 **********************************************************/
static void SrchFeatSeqLoc(TSeqFeatList& feats, objects::CSeq_annot::C_Data::TFtable& feat_table)
{
    for (objects::CSeq_annot::C_Data::TFtable::iterator feat = feat_table.begin(); feat != feat_table.end(); )
    {
        if ((*feat)->IsSetLocation() && (*feat)->GetLocation().GetId() != nullptr)
        {
            ++feat;
            continue;
        }

        /* SeqLocId will return NULL if any one of seqid in the SeqLocPtr
         * is diffenent, so move out cursfp to sslbp
         */

        feats.push_back(*feat);
        feat = feat_table.erase(feat);
    }
}

/**********************************************************
 *
 *   static void FindFeatSeqLoc(sep, data, index, indent):
 *
 *                                              9-14-93
 *
 **********************************************************/
static void FindFeatSeqLoc(TEntryList& seq_entries, TSeqFeatList& feats)
{
    NON_CONST_ITERATE(TEntryList, entry, seq_entries)
    {
        for (CTypeIterator<objects::CBioseq> bioseq(Begin(*(*entry))); bioseq; ++bioseq)
        {
            const objects::CSeq_id& first_id = *(*bioseq->GetId().begin());
            if (IsSegBioseq(&first_id) || !bioseq->IsSetAnnot())
                continue;

            /* process this bioseq entry
            */
            objects::CBioseq::TAnnot annots = bioseq->SetAnnot();
            for (objects::CBioseq::TAnnot::iterator annot = annots.begin(); annot != annots.end();)
            {
                if (!(*annot)->IsSetData() || !(*annot)->GetData().IsFtable())
                {
                    ++annot;
                    continue;
                }

                objects::CSeq_annot::C_Data::TFtable& feat_table = (*annot)->SetData().SetFtable();
                SrchFeatSeqLoc(feats, feat_table);

                if (!feat_table.empty())
                {
                    ++annot;
                    continue;
                }

                annot = annots.erase(annot);
            }
        }
    }
}

/**********************************************************/
static objects::CBioseq_set* GetParts(TEntryList& seq_entries)
{
    NON_CONST_ITERATE(TEntryList, entry, seq_entries)
    {
        for (CTypeIterator<objects::CBioseq_set> bio_set(Begin(*(*entry))); bio_set; ++bio_set)
        {
            if (bio_set->IsSetClass() && bio_set->GetClass() == objects::CBioseq_set::eClass_parts)
                return bio_set.operator->();
        }
    }

    return nullptr;
}

/**********************************************************
 *
 *   void CheckFeatSeqLoc(sep):
 *
 *      Remove out all the features which its seqloc has
 *   "join" or "order" among other segments, then insert
 *   into the annot which in the level of the class = parts.
 *
 *                                              9-14-93
 *
 **********************************************************/
void CheckFeatSeqLoc(TEntryList& seq_entries)
{
    TSeqFeatList feats_no_id;
    FindFeatSeqLoc(seq_entries, feats_no_id);

    objects::CBioseq_set* parts = GetParts(seq_entries);

    if (!feats_no_id.empty() && parts != nullptr)       /* may need to delete duplicate
                                                           one   9-14-93 */
    {
        NON_CONST_ITERATE(objects::CBioseq::TAnnot, annot, parts->SetAnnot())
        {
            if (!(*annot)->IsFtable())
                continue;

            (*annot)->SetData().SetFtable().splice((*annot)->SetData().SetFtable().end(), feats_no_id);
            break;
        }

        if (parts->GetAnnot().empty())
        {
            CRef<objects::CSeq_annot> new_annot(new objects::CSeq_annot);
            new_annot->SetData().SetFtable().swap(feats_no_id);
            parts->SetAnnot().push_back(new_annot);
        }
    }
}

END_NCBI_SCOPE
// LCOV_EXCL_STOP
