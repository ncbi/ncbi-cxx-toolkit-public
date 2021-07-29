/* ftamain.c
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
 * File Name:  ftamain.c
 *
 * Author: Karl Sirotkin, Hsiu-Chuan Chen
 *
 * File Description:
 * -----------------
 *      Main routines for parsing flat files to ASN.1 file format.
 * Available flat file format are GENBANK (LANL), EMBL, SWISS-PROT.
 *
 */
#include <ncbi_pch.hpp>

#include "ftacpp.hpp"

#include <objects/seqset/Bioseq_set.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objects/submit/Seq_submit.hpp>
#include <objects/submit/Submit_block.hpp>
#include <objects/biblio/Cit_sub.hpp>
#include <objects/biblio/Auth_list.hpp>

#include "index.h"
#include "sprot.h"
#include "embl.h"
#include "genbank.h"

#include <objtools/flatfile/ff2asn.h>
#include "ftanet.h"
#include <objtools/flatfile/flatfile_parser.hpp>
#include <objtools/flatfile/flatdefn.h>


#include "flatfile_message_reporter.hpp"
#include "ftaerr.hpp"
#include <objtools/logging/listener.hpp>
#include "indx_blk.h"
#include "asci_blk.h"
#include "add.h"
#include "loadfeat.h"
#include "gb_ascii.h"
#include "sp_ascii.h"
#include "pir_ascii.h"
#include "em_ascii.h"
#include "utilfeat.h"
#include "buf_data_loader.h"
#include "utilfun.h"
#include "entry.h"

#ifdef THIS_FILE
#    undef THIS_FILE
#endif
#define THIS_FILE "ftamain.cpp"

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

extern bool    PrfAscii(ParserPtr pp);
extern bool    XMLAscii(ParserPtr pp);
extern void    fta_init_gbdataloader();


typedef struct ff_entries {
    char*                offset;
    char*                acc;
    Int2                   vernum;
    struct ff_entries* next;
} FFEntries, *FFEntriesPtr;

FFEntriesPtr ffep = NULL;

// LCOV_EXCL_START
// Excluded per Mark's request on 12/14/2016
// Must be restored to test parsing from string buffer
/**********************************************************
 *
 *   static void CkSegmentSet(pp):
 *
 *      After building index block, before parsing ascii
 *   block.
 *      Report "WARN" message to the whole segment set
 *   if any one segment entry was missing, then treat
 *   the whole set to be the individual one, s.t. set each
 *   segment entry's segnum = segtotal = 0.
 *
 *                                              6-24-93
 *
 **********************************************************/
static void CkSegmentSet(ParserPtr pp)
{
    Int4    i;
    Int4    j;
    Int4    bindx;
    Int4    total;
    char* locus;

    bool flag;
    bool drop;
    bool notdrop;

    for(i = 0; i < pp->indx;)
    {
        if(pp->entrylist[i]->segtotal == 0)
        {
            i++;
            continue;
        }

        bindx = i;

        total = pp->entrylist[bindx]->segtotal;
        locus = pp->entrylist[bindx]->blocusname;

        flag = (pp->entrylist[bindx]->segnum != 1);

        for(i++; i < pp->indx &&
            StringCmp(pp->entrylist[i]->blocusname, locus) == 0; i++)
        {
            if(pp->entrylist[i-1]->segnum + 1 != pp->entrylist[i]->segnum)
                flag = true;
        }
        if(i - bindx != total)
            flag = true;

        if(flag)               /* warning the whole segment set */
        {
            ErrPostEx(SEV_ERROR, ERR_SEGMENT_MissSegEntry,
                      "%s|%s: Missing members of segmented set.",
                      pp->entrylist[bindx]->locusname,
                      pp->entrylist[bindx]->acnum);

            for(j = bindx; j < i; j++)
            {
                pp->curindx = j;

                pp->entrylist[j]->segnum = 0;
                pp->entrylist[j]->segtotal = 0;

                if(pp->debug == false)
                    pp->entrylist[j]->drop = 1;
            }
        } /* if, flag */
        else            /* assign all drop = 0 if they have "mix ownership" */
        {
            for(j = bindx, drop = notdrop = false; j < i; j++)
            {
                if(pp->entrylist[j]->drop == 0)
                    notdrop = true;
                else
                    drop = true;
            }

            if(drop && notdrop)       /* mix ownership */
            {
                for(j = bindx; j < i; j++)
                    pp->entrylist[j]->drop = 0;
                if(drop)
                    pp->num_drop--;
            }
        }
    }
}
// LCOV_EXCL_STOP

/**********************************************************/
static bool CompareAccs(const IndexblkPtr& p1, const IndexblkPtr& p2)
{
    return StringCmp(p1->acnum, p2->acnum) < 0;
}

/**********************************************************/
static bool CompareAccsV(const IndexblkPtr& p1, const IndexblkPtr& p2)
{
    int i = StringCmp(p1->acnum, p2->acnum);
    if (i != 0)
        return i < 0;

    return p1->vernum < p2->vernum;
}

/**********************************************************
 *
 *   static int CompareData(pp1, pp2):
 *
 *      Group all the segments data together.
 *      To solve duplicated entries which have the same
 *   accession number but locusname changed.
 *      Sorted by blocusname, segtotal, segnum, acnum,
 *   offset.
 *
 *                                              3-9-93
 *
 **********************************************************/
static bool CompareData(const IndexblkPtr& p1, const IndexblkPtr& p2)
{
    int retval = StringCmp(p1->blocusname, p2->blocusname);
    if (retval == 0)
    {
        if(p1->segtotal != 0 || p2->segtotal != 0)
        {
            if(p1->segtotal == p2->segtotal)
            {
                retval = p1->segnum - p2->segnum;

                if(retval == 0)
                {
                    retval = StringCmp(p1->acnum, p2->acnum);

                    if(retval == 0)
                        retval = p1->offset >= p2->offset ? static_cast<int>(p1->offset - p2->offset) : -1;
                }
            } /* segtotal */
            else
                retval = p1->segtotal - p2->segtotal;
        }
        else
        {
            retval = StringCmp(p1->acnum, p2->acnum);

            if(retval == 0)
                retval = p1->offset >= p2->offset ? static_cast<int>(p1->offset - p2->offset) : -1;
        }
    }

    return retval < 0;
}

/**********************************************************/
static bool CompareDataV(const IndexblkPtr& p1, const IndexblkPtr& p2)
{
    int retval = StringCmp(p1->blocusname, p2->blocusname);

    if (retval == 0)
    {
        if(p1->segtotal != 0 || p2->segtotal != 0)
        {
            if(p1->segtotal == p2->segtotal)
            {
                retval = p1->segnum - p2->segnum;

                if(retval == 0)
                {
                    retval = StringCmp(p1->acnum, p2->acnum);

                    if(retval == 0)
                    {
                        retval = p1->vernum - p2->vernum;
                        if(retval == 0)
                            retval = p1->offset >= p2->offset ? static_cast<int>(p1->offset - p2->offset) : -1;
                    }
                }
            } /* segtotal */
            else
                retval = p1->segtotal - p2->segtotal;
        }
        else
        {
            retval = StringCmp(p1->acnum, p2->acnum);

            if(retval == 0)
            {
                retval = p1->vernum - p2->vernum;
                if(retval == 0)
                    retval = p1->offset >= p2->offset ? static_cast<int>(p1->offset - p2->offset) : -1;
            }
        }
    }

    return retval < 0;
}

/**********************************************************/
static void CheckDupEntries(ParserPtr pp)
{
    Int4             i;
    Int4             j;
    IndexblkPtr      first;
    IndexblkPtr      second;
    IndexblkPtr* tibp;

    i = pp->indx * sizeof(IndexblkPtr);
    tibp = (IndexblkPtr*) MemNew(i);
    MemCpy(tibp, pp->entrylist, i);

    std::sort(tibp, tibp + pp->indx, (pp->accver ? CompareAccsV : CompareAccs));

    for(i = 0; i < pp->indx; i++)
    {
        first = tibp[i];
        if(first->drop != 0)
            continue;
        for(j = i + 1; j < pp->indx; j++)
        {
            second = tibp[j];
            if(second->drop != 0)
                continue;
            if(StringCmp(first->acnum, second->acnum) < 0)
                break;

            if(pp->accver && first->vernum != second->vernum)
                break;

            if (!first->date || !second->date) {
                continue;
            }

            objects::CDate::ECompare dtm = first->date->Compare(*second->date);
            if (dtm == objects::CDate::eCompare_before)
            {
                /* 2 after 1 take 2 remove 1
                 */
                first->drop = 1;
                ErrPostEx(SEV_WARNING, ERR_ENTRY_Repeated,
                          "%s (%s) skipped in favor of another entry with a later update date",
                          first->acnum, first->locusname);
            }
            else if (dtm == objects::CDate::eCompare_same)
            {
                if(first->offset > second->offset)
                {
                    /* 1 larger than 2 take 1 remove 2
                     */
                    second->drop = 1;
                    ErrPostEx(SEV_WARNING, ERR_ENTRY_Repeated,
                              "%s (%s) skipped in favor of another entry located at a larger byte offset",
                              second->acnum, second->locusname);
                }
                else                    /* take 2 remove 1 */
                {
                    first->drop = 1;
                    ErrPostEx(SEV_WARNING, ERR_ENTRY_Repeated,
                              "%s (%s) skipped in favor of another entry located at a larger byte offset",
                              first->acnum, first->locusname);
                }
            }
            else                        /* take 1 remove 2 */
            {
                second->drop = 1;
                ErrPostEx(SEV_WARNING, ERR_ENTRY_Repeated,
                          "%s (%s) skipped in favor of another entry with a later update date",
                          second->acnum, second->locusname);
            }
        }
    }
    MemFree(tibp);
}

static CRef<CSerialObject> MakeBioseqSet(ParserPtr pp)
{
    CRef<objects::CBioseq_set> bio_set(new objects::CBioseq_set);

    if (pp->source == Parser::ESource::PIR)
        bio_set->SetClass(objects::CBioseq_set::eClass_pir);
    else
        bio_set->SetClass(objects::CBioseq_set::eClass_genbank);

    if(pp->release_str != NULL)
        bio_set->SetRelease(pp->release_str);

    bio_set->SetSeq_set().splice(bio_set->SetSeq_set().end(), pp->entries);

    if (!pp->qamode)
    {
        bio_set->SetDate().SetToTime(CTime(CTime::eCurrent), objects::CDate::ePrecision_day);
    }

    return bio_set;
}

static CRef<CSerialObject> MakeSeqSubmit(ParserPtr pp)
{
    CRef<objects::CSeq_submit> seq_submit(new objects::CSeq_submit);
    objects::CSubmit_block& submit_blk = seq_submit->SetSub();

    submit_blk.SetCit().SetAuthors().SetNames().SetStr().push_back(pp->authors_str);

    TEntryList& entries = seq_submit->SetData().SetEntrys();
    entries.splice(entries.end(), pp->entries);

    return seq_submit;
}

/**********************************************************/
static void SetReleaseStr(ParserPtr pp)
{
    if (!pp->xml_comp)
    {
        if(pp->source == Parser::ESource::NCBI)
        {
            if(pp->format == Parser::EFormat::GenBank)
                pp->release_str = "source:ncbi, format:genbank";
            else if(pp->format == Parser::EFormat::EMBL)
                pp->release_str = "source:ncbi, format:embl";
            else if(pp->format == Parser::EFormat::XML)
                pp->release_str = "source:ncbi, format:xml";
        }
        else if(pp->source == Parser::ESource::DDBJ)
        {
            if(pp->format == Parser::EFormat::GenBank)
                pp->release_str = "source:ddbj, format:genbank";
            else if(pp->format == Parser::EFormat::EMBL)
                pp->release_str = "source:ddbj, format:embl";
            else if(pp->format == Parser::EFormat::XML)
                pp->release_str = "source:ddbj, format:xml";
        }
        else if(pp->source == Parser::ESource::LANL)
        {
            if(pp->format == Parser::EFormat::XML)
                pp->release_str = "source:lanl, format:xml";
            else
                pp->release_str = "source:lanl, format:genbank";
        }
        else if(pp->source == Parser::ESource::Flybase)
        {
            if(pp->format == Parser::EFormat::XML)
                pp->release_str = "source:flybase, format:xml";
            else
                pp->release_str = "source:flybase, format:genbank";
        }
        else if(pp->source == Parser::ESource::Refseq)
        {
            if(pp->format == Parser::EFormat::XML)
                pp->release_str = "source:refseq, format:xml";
            else
                pp->release_str = "source:refseq, format:genbank";
        }
        else if(pp->source == Parser::ESource::EMBL)
        {
            if(pp->format == Parser::EFormat::XML)
                pp->release_str = "source:embl, format:xml";
            else
                pp->release_str = "source:embl, format:embl";
        }
        else if(pp->source == Parser::ESource::SPROT)
            pp->release_str = "source:swissprot, format:swissprot";
        else if(pp->source == Parser::ESource::PIR)
            pp->release_str = "source:pir, format:pir";
        else if(pp->source == Parser::ESource::PRF)
            pp->release_str = "source:prf, format:prf";
        else if(pp->source == Parser::ESource::USPTO)
            pp->release_str = "source:uspto, format:xml";
        else
            pp->release_str = "source:unknown, format:unknown";
    }
}

/**********************************************************/
static void GetAuthorsStr(ParserPtr pp)
{
    if(pp->source == Parser::ESource::EMBL)
        pp->authors_str = "European Nucleotide Archive";
    else if(pp->source == Parser::ESource::DDBJ)
        pp->authors_str = "DNA Databank of Japan";
    else if(pp->source == Parser::ESource::NCBI || pp->source == Parser::ESource::LANL ||
            pp->source == Parser::ESource::Refseq)
        pp->authors_str = "National Center for Biotechnology Information";
    else if(pp->source == Parser::ESource::SPROT)
        pp->authors_str = "UniProt KnowledgeBase";
    else if(pp->source == Parser::ESource::PIR)
        pp->authors_str = "PIR";
    else if(pp->source == Parser::ESource::PRF)
        pp->authors_str = "PRF";
    else
        pp->authors_str = "FlyBase";
}

/**********************************************************/
static CRef<CSerialObject> CloseAll(ParserPtr pp)
{
    CloseFiles(pp);

    CRef<CSerialObject> ret;

    if (!pp->entries.empty())
    {
        if(pp->output_format == Parser::EOutput::BioseqSet)
        {
            ret = MakeBioseqSet(pp);
        }
        else if(pp->output_format == Parser::EOutput::Seqsubmit)
        {
            ret = MakeSeqSubmit(pp);
        }
    }
    return ret;
}


static bool sParseFlatfile(CRef<CSerialObject>& ret, ParserPtr pp, bool already=false)
{
    if(pp->output_format == Parser::EOutput::BioseqSet)
        SetReleaseStr(pp);
    else if(pp->output_format == Parser::EOutput::Seqsubmit)
        GetAuthorsStr(pp);

    if (!already)
        fta_init_servers(pp);

    FtaInstallPrefix(PREFIX_LOCUS, (char *) "INDEXING", NULL);

    bool good = FlatFileIndex(pp, NULL);

    FtaDeletePrefix(PREFIX_LOCUS | PREFIX_ACCESSION);

    if(!good)
    {
        if(!already)
            fta_fini_servers(pp);
        ret = CloseAll(pp);
        return good;
    }

    fta_init_gbdataloader();
    GetScope().AddDefaults();

    if(pp->format == Parser::EFormat::SPROT || pp->format == Parser::EFormat::PIR ||
       pp->format == Parser::EFormat::PRF)
    {
        FtaInstallPrefix(PREFIX_LOCUS, (char *) "PARSING", NULL);

        if(pp->format == Parser::EFormat::SPROT)
            good = SprotAscii(pp);
        else if(pp->format == Parser::EFormat::PIR)
            good = PirAscii(pp);
        else
            good = PrfAscii(pp);
        if(!already)
            fta_fini_servers(pp);

        ret = CloseAll(pp);

        FtaDeletePrefix(PREFIX_LOCUS | PREFIX_ACCESSION);

        return good;
    }

    FtaInstallPrefix(PREFIX_LOCUS, (char *) "SET-UP", NULL);

    //fta_entrez_fetch_enable(pp);

    /* CompareData: group all the segments data together
     */
    if(pp->sort)
    {
        std::sort(pp->entrylist, pp->entrylist + pp->indx, (pp->accver ? CompareDataV : CompareData));
    }

    CkSegmentSet(pp);           /* check for missing entries in segment set */

    CheckDupEntries(pp);

    ErrPostEx(SEV_INFO, ERR_ENTRY_ParsingSetup,
              "Parsing %ld entries", (size_t) pp->indx);

    pp->pbp = new ProtBlk;
    pp->pbp->ibp = new InfoBioseq;

    if(pp->num_drop > 0)
    {
        ErrPostEx(SEV_WARNING, ERR_ACCESSION_InvalidAccessNum,
                  "%ld invalid accession%s skipped", (size_t) pp->num_drop,
                  (pp->num_drop == 1) ? "" : "s");
    }

    FtaDeletePrefix(PREFIX_LOCUS | PREFIX_ACCESSION);
    FtaInstallPrefix(PREFIX_LOCUS, (char *) "PARSING", NULL);

    if(pp->format == Parser::EFormat::GenBank)
    {
        good = GenBankAscii(pp);
    }
    else if(pp->format == Parser::EFormat::EMBL)
    {
        good = EmblAscii(pp);
    }
    else if(pp->format == Parser::EFormat::XML)
    {
        good = XMLAscii(pp);
    }

    FtaDeletePrefix(PREFIX_LOCUS | PREFIX_ACCESSION);

    if(!already)
        fta_fini_servers(pp);

    GetScope().ResetHistory();

    //fta_entrez_fetch_disable(pp);

    ret = CloseAll(pp);

    // TransTableFreeAll(); // TODO probably needs to be replaced with C++ functionality
    return good;
}


/**********************************************************/
Int2 fta_main(ParserPtr pp, bool already)
{
    CRef<CSerialObject> ret;

    auto good = sParseFlatfile(ret, pp, already);    

    return((good == false) ? 1 : 0);
}

/**********************************************************/
static bool FillAccsBySource(Parser& pp, const std::string& source, bool all)
{
    if (NStr::EqualNocase(source, "PIR"))
    {
        pp.acprefix = ParFlat_PIR_AC;
        pp.seqtype = objects::CSeq_id::e_Pir;
        pp.source = Parser::ESource::PIR;
    }
    else if (NStr::EqualNocase(source, "PRF"))
    {
        pp.acprefix = ParFlat_PRF_AC;
        pp.seqtype = objects::CSeq_id::e_Prf;
        pp.source = Parser::ESource::PRF;
    }
    else if (NStr::EqualNocase(source, "SPROT"))
    {
        pp.acprefix = ParFlat_SPROT_AC;
        pp.seqtype = objects::CSeq_id::e_Swissprot;
        pp.source = Parser::ESource::SPROT;
    }
    else if (NStr::EqualNocase(source, "LANL"))
    {
        pp.acprefix = ParFlat_LANL_AC;         /* lanl or genbank */
        pp.seqtype = objects::CSeq_id::e_Genbank;
        pp.source = Parser::ESource::LANL;
    }
    else if (NStr::EqualNocase(source, "EMBL"))
    {
        pp.acprefix = ParFlat_EMBL_AC;
        pp.seqtype = objects::CSeq_id::e_Embl;
        pp.source = Parser::ESource::EMBL;
    }
    else if (NStr::EqualNocase(source, "DDBJ"))
    {
        pp.acprefix = ParFlat_DDBJ_AC;
        pp.seqtype = objects::CSeq_id::e_Ddbj;
        pp.source = Parser::ESource::DDBJ;
    }
    else if (NStr::EqualNocase(source, "FLYBASE"))
    {
        pp.source = Parser::ESource::Flybase;
        pp.seqtype = objects::CSeq_id::e_Genbank;
        pp.acprefix = NULL;
        if(pp.format != Parser::EFormat::GenBank)
        {
            ErrPostEx(SEV_FATAL, 0, 0,
                      "Source \"FLYBASE\" requires format \"GENBANK\" only. Cannot parse.");
            return false;
        }
    }
    else if (NStr::EqualNocase(source, "REFSEQ"))
    {
        pp.source = Parser::ESource::Refseq;
        pp.seqtype = objects::CSeq_id::e_Other;
        pp.acprefix = NULL;
        if(pp.format != Parser::EFormat::GenBank)
        {
            ErrPostEx(SEV_FATAL, 0, 0,
                      "Source \"REFSEQ\" requires format \"GENBANK\" only. Cannot parse.");
            return false;
        }
    }
    else if (NStr::EqualNocase(source, "NCBI"))
    {
        if (pp.mode == Parser::EMode::Relaxed) {
            pp.acprefix = NULL;
            pp.accpref = NULL;
            pp.source = Parser::ESource::NCBI;
        }
        else {
            /* for NCBI, the legal formats are embl and genbank, and
             * filenames, etc. need to be set accordingly. For example,
             * in -i (subtool) mode, both embl and genbank format might
             * be expected.
             */
            if(pp.format != Parser::EFormat::EMBL && pp.format != Parser::EFormat::GenBank &&
              pp.format != Parser::EFormat::XML)
            {
                ErrPostEx(SEV_FATAL, 0, 0,
                      "Source \"NCBI\" requires format \"GENBANK\" or \"EMBL\".");
                return false;
            }

            pp.acprefix = ParFlat_NCBI_AC;
            pp.seqtype = objects::CSeq_id::e_Genbank;    /* even though EMBL format, make
                                                               GenBank SEQIDS - Karl */
            pp.source = Parser::ESource::NCBI;
        }
    }
    else if(NStr::EqualNocase(source, "USPTO"))
    {
        if(pp.format != Parser::EFormat::XML)
        {
            ErrPostEx(SEV_FATAL, 0, 0,
                      "Source \"USPTO\" requires format \"XML\" only.");
            return(false);
        }

        pp.acprefix = ParFlat_SPROT_AC;
        pp.seqtype = objects::CSeq_id::e_Other;
        pp.source = Parser::ESource::USPTO;
        pp.accver = false;
    }
    else
    {
        ErrPostEx(SEV_FATAL, 0, 0,
                  "Sorry, %s is not a valid source. Valid source ==> PIR, SPROT, LANL, NCBI, EMBL, DDBJ, FLYBASE, REFSEQ, USPTO", source.c_str());
        return false;
    }

    /* parse regardless of source, overwrite prefix
     */
    if (all)
    {
        pp.acprefix = NULL;
        pp.all = true;
        pp.accpref = NULL;
    }
    else
        pp.accpref = (char**) GetAccArray(pp.source);

    pp.citat = (pp.source != Parser::ESource::SPROT);

    return true;
}

// LCOV_EXCL_START
// Excluded per Mark's request on 12/14/2016
// Must be restored to test parsing from string buffer
/**********************************************************/
// TODO function is not used 
void Flat2AsnCheck(char* ffentry, char* source, char* format,
                   bool accver, Parser::EMode mode, Int4 limit)
{
    ParserPtr pp;
    Parser::EFormat form;

    if (NStr::EqualNocase(format, "embl"))
        form = Parser::EFormat::EMBL;
    else if (NStr::EqualNocase(format, "genbank"))
        form = Parser::EFormat::GenBank;
    else if (NStr::EqualNocase(format, "sprot"))
        form = Parser::EFormat::SPROT;
    else if (NStr::EqualNocase(format, "xml"))
        form = Parser::EFormat::XML;
    else
    {
        ErrPostEx(SEV_ERROR, 0, 0, "Unknown format of flat entry");
        return;
    }

    pp = new Parser;
    pp->format = form;

    if (!FillAccsBySource(*pp, source, false))
    {
        delete pp;
        return;
    }

    /* As of June, 2004 the sequence length limitation removed
     */
    pp->limit = 0;
    pp->sort = true;
    pp->accver = accver;
    pp->mode = mode;
    pp->convert = true;
    pp->taxserver = 1;
    pp->medserver = 1;
    pp->sp_dt_seq_ver = true;
    pp->cleanup = 1;
    pp->allow_crossdb_featloc = false;
    pp->genenull = true;
    pp->qsfile = NULL;
    pp->qsfd = NULL;
    pp->qamode = false;

    pp->ffbuf = new FileBuf();
    pp->ffbuf->start = ffentry;
    pp->ffbuf->current = pp->ffbuf->start;

    fta_fill_find_pub_option(pp, false, false);
    fta_main(pp, true);
}
// LCOV_EXCL_STOP

/*
objects::CScope& GetScope()
{
    static objects::CScope scope(*objects::CObjectManager::GetInstance());

    return scope;
}

// CErrorMgr class implements RAII paradigm
class CErrorMgr
{
public:
    CErrorMgr() {
        FtaErrInit();
    }

    ~CErrorMgr() {
        FtaErrFini();
    }
};
*/

CFlatFileParser::CFlatFileParser(IObjtoolsListener* pMessageListener)
    : m_pMessageListener(pMessageListener)
{
    FtaErrInit();
    CFlatFileMessageReporter::GetInstance().SetListener(pMessageListener);

    // Do we really need this?
    CGBDataLoader::RegisterInObjectManager(*CObjectManager::GetInstance());
    GetScope().AddDefaults();
}


CFlatFileParser::~CFlatFileParser()
{
    FtaErrFini();
}


CRef<CSerialObject> CFlatFileParser::Parse(Parser& parseInfo)
{
    CRef<CSerialObject> pResult;
    if (sParseFlatfile(pResult, &parseInfo)) {
        return pResult;
    }

   return CRef<CSerialObject>();
}


CRef<CSerialObject> CFlatFileParser::Parse(Parser& parseInfo, CNcbiIstream& istr)
{
    CRef<CSerialObject> pResult;
    if (parseInfo.ifp) {
        string msg = "Ambiguous input. File pointer and input stream both specified";
        if (!m_pMessageListener) {
            NCBI_THROW(CException, eUnknown, msg);
        }   
        m_pMessageListener->PutMessage(CObjtoolsMessage(msg, eDiag_Fatal));  
    }

    if (parseInfo.ffbuf) {
        string msg = "Attempting to reinitialize input buffer";
        if (!m_pMessageListener) { // Throw an exception it no listener
            NCBI_THROW(CException, eUnknown, msg);
        }
        m_pMessageListener->PutMessage(CObjtoolsMessage(msg, eDiag_Fatal));
    }

    ostringstream os;
    os << istr.rdbuf();
    string buffer = os.str();

    parseInfo.ffbuf = new FileBuf();
    parseInfo.ffbuf->start = buffer.c_str();
    parseInfo.ffbuf->current = parseInfo.ffbuf->start;

    if (sParseFlatfile(pResult, &parseInfo)) {
        return pResult;
    }
    return CRef<CSerialObject>();
}


TEntryList& fta_parse_buf(Parser& pp, const char* buf)
{
    if (buf == NULL || *buf == '\0') {
        return pp.entries;
    }
    pp.entrez_fetch = pp.taxserver = pp.medserver = 1;

//    CErrorMgr err_mgr;

    FtaInstallPrefix(PREFIX_LOCUS, (char *) "SET-UP", NULL);

    pp.ffbuf = new FileBuf();
    pp.ffbuf->start = buf;
    pp.ffbuf->current = buf;

    FtaDeletePrefix(PREFIX_LOCUS);

    FtaInstallPrefix(PREFIX_LOCUS, (char *) "INDEXING", NULL);

    bool good = FlatFileIndex(&pp, NULL);

    FtaDeletePrefix(PREFIX_LOCUS | PREFIX_ACCESSION);

    if (!good) {
        ResetParserStruct(&pp);
        return pp.entries;
    }

    fta_init_servers(&pp);

    CRef<objects::CObjectManager> obj_mgr = objects::CObjectManager::GetInstance();
    objects::CBuffer_DataLoader::RegisterInObjectManager(*obj_mgr, &pp, objects::CObjectManager::eDefault, objects::CObjectManager::kPriority_Default);

    GetScope().AddDefaults();

    if (pp.format == Parser::EFormat::SPROT || pp.format == Parser::EFormat::PIR ||
        pp.format == Parser::EFormat::PRF) {
        FtaInstallPrefix(PREFIX_LOCUS, (char *) "PARSING", NULL);

        if (pp.format == Parser::EFormat::SPROT)
            good = SprotAscii(&pp);
        else if (pp.format == Parser::EFormat::PIR)
            good = PirAscii(&pp);
        else
            good = PrfAscii(&pp);

        FtaDeletePrefix(PREFIX_LOCUS | PREFIX_ACCESSION);

        fta_fini_servers(&pp);

        if (!good) {
            ResetParserStruct(&pp);
        }
        return pp.entries;
    }

    FtaInstallPrefix(PREFIX_LOCUS, (char *) "SET-UP", NULL);

    //fta_entrez_fetch_enable(&pp);

    /*if (pp->ffdb == 0)
        fetchname = "index";
    else
        fetchname = "FFDBFetch";

    if (pp->procid != 0) {
        omp = ObjMgrWriteLock();
        ompp = ObjMgrProcFind(omp, pp->procid, fetchname, OMPROC_FETCH);
        ObjMgrUnlock();
    }
    else
        ompp = NULL;

    if (ompp != NULL)
        ompp->procdata = (Pointer)pp;
    else {
        pp->procid = ObjMgrProcLoad(OMPROC_FETCH, fetchname, "fetch",
                                    OBJ_SEQID, 0, OBJ_BIOSEQ, 0, pp,
                                    (pp->ffdb == 0) ?
                                    (ObjMgrGenFunc)IndexFetch :
                                    (ObjMgrGenFunc)FFDBFetch,
                                    PROC_PRIORITY_DEFAULT);
    }*/

    if (pp.sort) {
        std::sort(pp.entrylist, pp.entrylist + pp.indx, (pp.accver ? CompareDataV : CompareData));
    }

    CkSegmentSet(&pp);           /* check for missing entries in segment set */
    CheckDupEntries(&pp);

    ErrPostEx(SEV_INFO, ERR_ENTRY_ParsingSetup,
              "Parsing %ld entries", (size_t)pp.indx);

    if (pp.num_drop > 0) {
        ErrPostEx(SEV_WARNING, ERR_ACCESSION_InvalidAccessNum,
                  "%ld invalid accession%s skipped", (size_t)pp.num_drop,
                  (pp.num_drop == 1) ? "" : "s");
    }

    FtaDeletePrefix(PREFIX_LOCUS | PREFIX_ACCESSION);
    FtaInstallPrefix(PREFIX_LOCUS, (char *) "PARSING", NULL);

    good = false;

    pp.pbp = new ProtBlk;
    pp.pbp->ibp = new InfoBioseq;

    if (pp.format == Parser::EFormat::GenBank) {
        good = GenBankAscii(&pp);
    }
    else if (pp.format == Parser::EFormat::EMBL) {
        good = EmblAscii(&pp);
    }
    else if (pp.format == Parser::EFormat::XML) {
        good = XMLAscii(&pp);
    }

    FtaDeletePrefix(PREFIX_LOCUS | PREFIX_ACCESSION);

    if (!good) {
        ResetParserStruct(&pp);
    }

    //? SeqMgrFreeCache();
    //? FreeFFEntries();

    //fta_entrez_fetch_disable(&pp);
    fta_fini_servers(&pp);

    return pp.entries;

    /*if (pp->procid != 0) {
        omp = ObjMgrWriteLock();
        ompp = ObjMgrProcFind(omp, pp->procid, fetchname, OMPROC_FETCH);
        ObjMgrUnlock();
        if (ompp != NULL)
            ompp->procdata = NULL;
    }

    TransTableFreeAll();*/
}

bool fta_set_format_source(Parser& pp, const std::string& format, const std::string& source)
{
    if (format == "embl")
        pp.format = Parser::EFormat::EMBL;
    else if (format == "genbank")
        pp.format = Parser::EFormat::GenBank;
    else if (format == "sprot")
        pp.format = Parser::EFormat::SPROT;
    else if (format == "pir")
        pp.format = Parser::EFormat::PIR;
    else if (format == "prf")
        pp.format = Parser::EFormat::PRF;
    else if (format == "xml")
        pp.format = Parser::EFormat::XML;
    else {
        ErrPostEx(SEV_FATAL, 0, 0,
                  "Sorry, the format is not available yet ==> available format embl, genbank, pir, prf, sprot, xml.");
        return false;
    }

    return FillAccsBySource(pp, source, pp.all != 0);
}

void fta_init_pp(Parser& pp)
{
	pp.ign_toks = false;
	pp.date = false;
	pp.convert = true;
	pp.seg_acc = false;
	pp.no_date = false;
	pp.sort = true;
	pp.debug = false;
	pp.segment = false;
	pp.accver = true;
	pp.histacc = true;
	pp.transl = false;
	pp.entrez_fetch = 1;
	pp.taxserver = 0;
	pp.medserver = 0;
	pp.ffdb = false;

	/* as of june, 2004 the sequence length limitation removed
	*/
	pp.limit = 0;
	pp.all = 0;
	//pp.fpo = nullptr;
	//fta_fill_find_pub_option(&pp, false, false);

	pp.indx = 0;
	pp.entrylist = nullptr;
	pp.curindx = 0;
	pp.ifp = nullptr;
	pp.ffbuf = nullptr;
	pp.seqtype = 0;
	pp.num_drop = 0;
	pp.acprefix = nullptr;
	pp.pbp = nullptr;
	pp.citat = false;
	pp.no_code = false;
	pp.accpref = nullptr;
	pp.user_data = nullptr;
	pp.ff_get_entry = nullptr;
	pp.ff_get_entry_v = nullptr;
	pp.ff_get_qscore = nullptr;
	pp.ff_get_qscore_pp = nullptr;
	pp.ff_get_entry_pp = nullptr;
	pp.ff_get_entry_v_pp = nullptr;
	pp.ign_bad_qs = false;
	pp.mode = Parser::EMode::Release;
	pp.sp_dt_seq_ver = true;
	pp.simple_genes = false;
	pp.cleanup = 1;
	pp.allow_crossdb_featloc = false;
	pp.genenull = true;
	pp.qsfile = nullptr;
	pp.qsfd = nullptr;
	pp.qamode = false;
}

END_NCBI_SCOPE
