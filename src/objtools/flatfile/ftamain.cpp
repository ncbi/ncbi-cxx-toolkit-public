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
 * File Name: ftamain.cpp
 *
 * Author: Karl Sirotkin, Hsiu-Chuan Chen
 *
 * File Description:
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
#include <serial/objostr.hpp>

#include "index.h"
#include "sprot.h"
#include "embl.h"
#include "genbank.h"

#include <objtools/flatfile/ff2asn.h>
#include "ftanet.h"
#include <objtools/flatfile/flatfile_parser.hpp>
#include <objtools/flatfile/flatdefn.h>
#include "keyword_parse.hpp"

#include "flatfile_message_reporter.hpp"
#include "ftaerr.hpp"
#include <objtools/logging/listener.hpp>
#include "indx_blk.h"
#include "asci_blk.h"
#include "add.h"
#include "loadfeat.h"
#include "gb_ascii.h"
#include "sp_ascii.h"
#include "em_ascii.h"
#include "utilfeat.h"
#include "buf_data_loader.h"
#include "utilfun.h"
#include "entry.h"
#include "xm_ascii.h"

#include "writer.hpp"


#ifdef THIS_FILE
#  undef THIS_FILE
#endif
#define THIS_FILE "ftamain.cpp"

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

extern void fta_init_gbdataloader();


/**********************************************************/
static bool CompareAccs(const Indexblk* p1, const Indexblk* p2)
{
    return StringCmp(p1->acnum, p2->acnum) < 0;
}

/**********************************************************/
static bool CompareAccsV(const Indexblk* p1, const Indexblk* p2)
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
 *      Sorted by blocusname, acnum, offset.
 *
 *
 **********************************************************/
static bool CompareData(const Indexblk* p1, const Indexblk* p2)
{
    int retval = StringCmp(p1->blocusname, p2->blocusname);
    if (retval == 0) {
        retval = StringCmp(p1->acnum, p2->acnum);
        if (retval == 0) {
            retval = p1->offset >= p2->offset ? static_cast<int>(p1->offset - p2->offset) : -1;
        }
    }
    return retval < 0;
}

/**********************************************************/
static bool CompareDataV(const Indexblk* p1, const Indexblk* p2)
{
    int retval = StringCmp(p1->blocusname, p2->blocusname);

    if (retval == 0) {
        retval = StringCmp(p1->acnum, p2->acnum);
        if (retval == 0) {
            retval = p1->vernum - p2->vernum;
            if (retval == 0) {
                retval = p1->offset >= p2->offset ? static_cast<int>(p1->offset - p2->offset) : -1;
            }
        }
    }
    return retval < 0;
}

/**********************************************************/
static void CheckDupEntries(ParserPtr pp)
{
    Int4        i;
    Int4        j;
    IndexblkPtr first;
    IndexblkPtr second;

    vector<IndexblkPtr> tibp = pp->entrylist;
    tibp.resize(pp->indx);

    std::sort(tibp.begin(), tibp.end(), (pp->accver ? CompareAccsV : CompareAccs));

    for (i = 0; i < pp->indx; i++) {
        first = tibp[i];
        if (first->drop)
            continue;
        for (j = i + 1; j < pp->indx; j++) {
            second = tibp[j];
            if (second->drop)
                continue;
            if (StringCmp(first->acnum, second->acnum) < 0)
                break;

            if (pp->accver && first->vernum != second->vernum)
                break;

            if (! first->date || ! second->date) {
                continue;
            }

            CDate::ECompare dtm = first->date->Compare(*second->date);
            if (dtm == CDate::eCompare_before) {
                /* 2 after 1 take 2 remove 1
                 */
                first->drop = true;
                FtaErrPost(SEV_WARNING, ERR_ENTRY_Repeated, "{} ({}) skipped in favor of another entry with a later update date", first->acnum, first->locusname);
            } else if (dtm == CDate::eCompare_same) {
                if (first->offset > second->offset) {
                    /* 1 larger than 2 take 1 remove 2
                     */
                    second->drop = true;
                    FtaErrPost(SEV_WARNING, ERR_ENTRY_Repeated, "{} ({}) skipped in favor of another entry located at a larger byte offset", second->acnum, second->locusname);
                } else /* take 2 remove 1 */
                {
                    first->drop = true;
                    FtaErrPost(SEV_WARNING, ERR_ENTRY_Repeated, "{} ({}) skipped in favor of another entry located at a larger byte offset", first->acnum, first->locusname);
                }
            } else /* take 1 remove 2 */
            {
                second->drop = true;
                FtaErrPost(SEV_WARNING, ERR_ENTRY_Repeated, "{} ({}) skipped in favor of another entry with a later update date", second->acnum, second->locusname);
            }
        }
    }
}


static CRef<CSerialObject> MakeBioseqSet(ParserPtr pp)
{
    CRef<CBioseq_set> bio_set(new CBioseq_set);

    bio_set->SetClass(CBioseq_set::eClass_genbank);

    if (! pp->release_str.empty())
        bio_set->SetRelease(pp->release_str);

    bio_set->SetSeq_set().splice(bio_set->SetSeq_set().end(), pp->entries);

    if (! pp->qamode) {
        bio_set->SetDate().SetToTime(CTime(CTime::eCurrent), CDate::ePrecision_day);
    }

    return bio_set;
}


static unique_ptr<CMappedInput2Asn> s_GetInput2Asn(Parser& parser)
{
    switch (parser.format) {
    case Parser::EFormat::GenBank:
        return make_unique<CGenbank2Asn>(parser);

    case Parser::EFormat::EMBL:
        return make_unique<CEmbl2Asn>(parser);
    
    case Parser::EFormat::SPROT:
        return make_unique<CSwissProt2Asn>(parser);

    case Parser::EFormat::XML:
        return make_unique<CXml2Asn>(parser);

    default:
        break;
    }

    return unique_ptr<CMappedInput2Asn>();
}


static void s_WriteSeqSubmit(Parser& parser, CObjectOStream& objOstr)
{
    auto pInput2Asn = s_GetInput2Asn(parser);

    auto pFirstEntry = (*pInput2Asn)();

    if (pFirstEntry) {
        auto pSubmit = Ref(new CSeq_submit());
    
        auto& submitBlock = pSubmit->SetSub();
    
        submitBlock.SetCit().SetAuthors().SetNames().SetStr().push_back(parser.authors_str);

        pSubmit->SetData().SetEntrys();

        CFlat2AsnWriter writer(objOstr);
        writer.Write(pSubmit, 
            [&pFirstEntry, &pInput2Asn](){ 
                if (pFirstEntry) {
                    auto pResult = pFirstEntry;
                    pFirstEntry.Reset();
                    return pResult;
                }
                return (*pInput2Asn)(); 
            }
            ); 
    } // if (pFirstEntry)

    pInput2Asn->PostTotals();
}


static void s_WriteBioseqSet(Parser& parser, CObjectOStream& objOstr)
{
    auto pInput2Asn = s_GetInput2Asn(parser);

    auto pFirstEntry = (*pInput2Asn)();

    if (pFirstEntry) {

        auto pBioseqSet = Ref(new CBioseq_set());
        pBioseqSet->SetClass(CBioseq_set::eClass_genbank);

        if (! parser.release_str.empty()) {
            pBioseqSet->SetRelease(parser.release_str);
        }

        if (! parser.qamode) {
            pBioseqSet->SetDate().SetToTime(CTime(CTime::eCurrent), CDate::ePrecision_day);
        }

        pBioseqSet->SetSeq_set();

        CFlat2AsnWriter writer(objOstr);
        writer.Write(pBioseqSet, 
            [&pFirstEntry, &pInput2Asn](){ 
                if (pFirstEntry) {
                    auto pResult = pFirstEntry;
                    pFirstEntry.Reset();
                    return pResult;
                }
                return (*pInput2Asn)(); 
            }
            ); 
    } // if (pFirstEntry)

    pInput2Asn->PostTotals();
}


static bool s_WriteOutput(Parser& parser, CObjectOStream& objOstr)
{
    if (parser.output_format == Parser::EOutput::BioseqSet) {
        s_WriteBioseqSet(parser, objOstr);
    } 
    else {
        _ASSERT(parser.output_format == Parser::EOutput::Seqsubmit);
        s_WriteSeqSubmit(parser, objOstr);
    }

    return true;
}



static CRef<CSerialObject> MakeSeqSubmit(ParserPtr pp)
{
    CRef<CSeq_submit> seq_submit(new CSeq_submit);
    CSubmit_block&    submit_blk = seq_submit->SetSub();

    submit_blk.SetCit().SetAuthors().SetNames().SetStr().push_back(pp->authors_str);

    TEntryList& entries = seq_submit->SetData().SetEntrys();
    entries.splice(entries.end(), pp->entries);

    return seq_submit;
}

/**********************************************************/
static void SetReleaseStr(ParserPtr pp)
{
    {
        if (pp->source == Parser::ESource::NCBI) {
            if (pp->format == Parser::EFormat::GenBank)
                pp->release_str = "source:ncbi, format:genbank";
            else if (pp->format == Parser::EFormat::EMBL)
                pp->release_str = "source:ncbi, format:embl";
            else if (pp->format == Parser::EFormat::XML)
                pp->release_str = "source:ncbi, format:xml";
        } else if (pp->source == Parser::ESource::DDBJ) {
            if (pp->format == Parser::EFormat::GenBank)
                pp->release_str = "source:ddbj, format:genbank";
            else if (pp->format == Parser::EFormat::EMBL)
                pp->release_str = "source:ddbj, format:embl";
            else if (pp->format == Parser::EFormat::XML)
                pp->release_str = "source:ddbj, format:xml";
        } else if (pp->source == Parser::ESource::LANL) {
            if (pp->format == Parser::EFormat::XML)
                pp->release_str = "source:lanl, format:xml";
            else
                pp->release_str = "source:lanl, format:genbank";
        } else if (pp->source == Parser::ESource::Refseq) {
            if (pp->format == Parser::EFormat::XML)
                pp->release_str = "source:refseq, format:xml";
            else
                pp->release_str = "source:refseq, format:genbank";
        } else if (pp->source == Parser::ESource::EMBL) {
            if (pp->format == Parser::EFormat::XML)
                pp->release_str = "source:embl, format:xml";
            else
                pp->release_str = "source:embl, format:embl";
        } else if (pp->source == Parser::ESource::SPROT)
            pp->release_str = "source:swissprot, format:swissprot";
        else if (pp->source == Parser::ESource::USPTO)
            pp->release_str = "source:uspto, format:xml";
        else
            pp->release_str = "source:unknown, format:unknown";
    }
}

/**********************************************************/
static void GetAuthorsStr(ParserPtr pp)
{
    if (pp->source == Parser::ESource::EMBL)
        pp->authors_str = "European Nucleotide Archive";
    else if (pp->source == Parser::ESource::DDBJ)
        pp->authors_str = "DNA Databank of Japan";
    else if (pp->source == Parser::ESource::NCBI || pp->source == Parser::ESource::LANL ||
             pp->source == Parser::ESource::Refseq)
        pp->authors_str = "National Center for Biotechnology Information";
    else if (pp->source == Parser::ESource::SPROT)
        pp->authors_str = "UniProt KnowledgeBase";
    else
        pp->authors_str = "unknown";
}

/**********************************************************/
static CRef<CSerialObject> CloseAll(ParserPtr pp)
{
    CloseFiles(pp);

    CRef<CSerialObject> ret;

    if (! pp->entries.empty()) {
        if (pp->output_format == Parser::EOutput::BioseqSet) {
            ret = MakeBioseqSet(pp);
        } else if (pp->output_format == Parser::EOutput::Seqsubmit) {
            ret = MakeSeqSubmit(pp);
        }
    }
    return ret;
}

static bool sParseFlatfile(ParserPtr pp, CObjectOStream& objOstr, bool already = false)
{

    // For now.
    // In a perfect (future?) world, this would be a method of CFlatFileParser,
    //  and the keywordparser would be a subsystem of CFlatFileParser itself, not of
    //  its configuration.
    pp->InitializeKeywordParser(pp->format);

    if (pp->output_format == Parser::EOutput::BioseqSet)
        SetReleaseStr(pp);
    else if (pp->output_format == Parser::EOutput::Seqsubmit)
        GetAuthorsStr(pp);

    if (! already)
        fta_init_servers(pp);

    FtaInstallPrefix(PREFIX_LOCUS, "INDEXING");

    bool good = FlatFileIndex(pp, nullptr);

    FtaDeletePrefix(PREFIX_LOCUS | PREFIX_ACCESSION);


    if (! good) {
        if (! already)
            fta_fini_servers(pp);
        CloseFiles(pp);
        return good;
    }

    fta_init_gbdataloader();
    GetScope().AddDefaults();

    if (pp->format == Parser::EFormat::SPROT) {
        FtaInstallPrefix(PREFIX_LOCUS, "PARSING");

        good = s_WriteOutput(*pp, objOstr);

        if (! already)
            fta_fini_servers(pp);

        CloseFiles(pp);

        FtaDeletePrefix(PREFIX_LOCUS | PREFIX_ACCESSION);

        return good;
    }

    FtaInstallPrefix(PREFIX_LOCUS, "SET-UP");

    // fta_entrez_fetch_enable(pp);

    /* CompareData: group all the segments data together
     */
    if (pp->sort) {
        std::sort(pp->entrylist.begin(), pp->entrylist.end(), (pp->accver ? CompareDataV : CompareData));
    }

    CheckDupEntries(pp);

    FtaErrPost(SEV_INFO, ERR_ENTRY_ParsingSetup, "Parsing {} entries", (size_t)pp->indx);

    pp->pbp      = new ProtBlk;
    pp->pbp->ibp = new InfoBioseq;

    if (pp->num_drop > 0) {
        FtaErrPost(SEV_WARNING, ERR_ACCESSION_InvalidAccessNum, "{} invalid accession{} skipped", (size_t)pp->num_drop, (pp->num_drop == 1) ? "" : "s");
    }

    FtaDeletePrefix(PREFIX_LOCUS | PREFIX_ACCESSION);
    FtaInstallPrefix(PREFIX_LOCUS, "PARSING");

    const bool written = s_WriteOutput(*pp, objOstr);

    FtaDeletePrefix(PREFIX_LOCUS | PREFIX_ACCESSION);

    if (! already)
        fta_fini_servers(pp);

    GetScope().ResetDataAndHistory();


    CloseFiles(pp);

    return written;
}

static bool s_GetEntries(CMappedInput2Asn& input2Asn, TEntryList& entries)
{
    try {
        CRef<CSeq_entry> entry;
        while ((entry = input2Asn())) {
            entries.push_back(entry);
        }
        input2Asn.PostTotals();
    }
    catch (CException& e) {
        FtaErrPost(SEV_FATAL, 0, 0, e.GetMsg());
        return false;
    }

    return true;
}


static bool sParseFlatfile(CRef<CSerialObject>& ret, ParserPtr pp, bool already = false)
{
    // For now.
    // In a perfect (future?) world, this would be a method of CFlatFileParser,
    //  and the keywordparser would be a subsystem of CFlatFileParser itself, not of
    //  its configuration.
    pp->InitializeKeywordParser(pp->format);

    if (pp->output_format == Parser::EOutput::BioseqSet)
        SetReleaseStr(pp);
    else if (pp->output_format == Parser::EOutput::Seqsubmit)
        GetAuthorsStr(pp);

    if (! already)
        fta_init_servers(pp);

    FtaInstallPrefix(PREFIX_LOCUS, "INDEXING");

    bool good = FlatFileIndex(pp, nullptr);

    FtaDeletePrefix(PREFIX_LOCUS | PREFIX_ACCESSION);

    if (! good) {
        if (! already)
            fta_fini_servers(pp);
        ret = CloseAll(pp);
        return good;
    }

    fta_init_gbdataloader();
    GetScope().AddDefaults();

    auto pInput2Asn = s_GetInput2Asn(*pp);

    if (pp->format == Parser::EFormat::SPROT) {
        FtaInstallPrefix(PREFIX_LOCUS, "PARSING");

        good = s_GetEntries(*pInput2Asn, pp->entries);

        if (! already)
            fta_fini_servers(pp);

        ret = CloseAll(pp);

        FtaDeletePrefix(PREFIX_LOCUS | PREFIX_ACCESSION);

        return good;
    }

    FtaInstallPrefix(PREFIX_LOCUS, "SET-UP");

    if (pp->sort) {
        std::sort(pp->entrylist.begin(), pp->entrylist.end(), (pp->accver ? CompareDataV : CompareData));
    }

    CheckDupEntries(pp);

    FtaErrPost(SEV_INFO, ERR_ENTRY_ParsingSetup, "Parsing {} entries", (size_t)pp->indx);

    pp->pbp      = new ProtBlk;
    pp->pbp->ibp = new InfoBioseq;

    if (pp->num_drop > 0) {
        FtaErrPost(SEV_WARNING, ERR_ACCESSION_InvalidAccessNum, "{} invalid accession{} skipped", (size_t)pp->num_drop, (pp->num_drop == 1) ? "" : "s");
    }

    FtaDeletePrefix(PREFIX_LOCUS | PREFIX_ACCESSION);
    FtaInstallPrefix(PREFIX_LOCUS, "PARSING");
        
    good = s_GetEntries(*pInput2Asn, pp->entries);

    FtaDeletePrefix(PREFIX_LOCUS | PREFIX_ACCESSION);

    if (! already)
        fta_fini_servers(pp);

    GetScope().ResetHistory();

    ret = CloseAll(pp);

    return good;
}


/**********************************************************/
Int2 fta_main(ParserPtr pp, bool already)
{
    CRef<CSerialObject> ret;

    auto good = sParseFlatfile(ret, pp, already);

    return ((good == false) ? 1 : 0);
}

/**********************************************************/
static bool FillAccsBySource(Parser& pp, const string& source, bool all)
{
    if (NStr::EqualNocase(source, "SPROT")) {
        pp.acprefix = ParFlat_SPROT_AC;
        pp.seqtype  = CSeq_id::e_Swissprot;
        pp.source   = Parser::ESource::SPROT;
    } else if (NStr::EqualNocase(source, "LANL")) {
        pp.acprefix = ParFlat_LANL_AC; /* lanl or genbank */
        pp.seqtype  = CSeq_id::e_Genbank;
        pp.source   = Parser::ESource::LANL;
    } else if (NStr::EqualNocase(source, "EMBL")) {
        pp.acprefix = ParFlat_EMBL_AC;
        pp.seqtype  = CSeq_id::e_Embl;
        pp.source   = Parser::ESource::EMBL;
    } else if (NStr::EqualNocase(source, "DDBJ")) {
        pp.acprefix = ParFlat_DDBJ_AC;
        pp.seqtype  = CSeq_id::e_Ddbj;
        pp.source   = Parser::ESource::DDBJ;
    } else if (NStr::EqualNocase(source, "REFSEQ")) {
        pp.source   = Parser::ESource::Refseq;
        pp.seqtype  = CSeq_id::e_Other;
        pp.acprefix = nullptr;
        if (pp.format != Parser::EFormat::GenBank) {
            FtaErrPost(SEV_FATAL, 0, 0, "Source \"REFSEQ\" requires format \"GENBANK\" only. Cannot parse.");
            return false;
        }
    } else if (NStr::EqualNocase(source, "NCBI")) {
        if (pp.mode == Parser::EMode::Relaxed) {
            pp.acprefix = nullptr;
            pp.accpref  = nullptr;
            pp.source   = Parser::ESource::NCBI;
        } else {
            /* for NCBI, the legal formats are embl and genbank, and
             * filenames, etc. need to be set accordingly. For example,
             * in -i (subtool) mode, both embl and genbank format might
             * be expected.
             */
            if (pp.format != Parser::EFormat::EMBL && pp.format != Parser::EFormat::GenBank &&
                pp.format != Parser::EFormat::XML) {
                FtaErrPost(SEV_FATAL, 0, 0, "Source \"NCBI\" requires format \"GENBANK\" or \"EMBL\".");
                return false;
            }

            pp.acprefix = ParFlat_NCBI_AC;
            pp.seqtype  = CSeq_id::e_Genbank; /* even though EMBL format, make
                                                               GenBank SEQIDS - Karl */
            pp.source   = Parser::ESource::NCBI;
        }
    } else if (NStr::EqualNocase(source, "USPTO")) {
        if (pp.format != Parser::EFormat::XML) {
            FtaErrPost(SEV_FATAL, 0, 0, "Source \"USPTO\" requires format \"XML\" only.");
            return (false);
        }

        pp.acprefix = ParFlat_SPROT_AC;
        pp.seqtype  = CSeq_id::e_Other;
        pp.source   = Parser::ESource::USPTO;
        pp.accver   = false;
    } else {
        FtaErrPost(SEV_FATAL, 0, 0, "Sorry, {} is not a valid source. Valid source ==> PIR, SPROT, LANL, NCBI, EMBL, DDBJ, REFSEQ, USPTO", source);
        return false;
    }

    /* parse regardless of source, overwrite prefix
     */
    if (all) {
        pp.acprefix = nullptr;
        pp.all      = true;
        pp.accpref  = nullptr;
    } else
        pp.accpref = GetAccArray(pp.source);

    pp.citat = (pp.source != Parser::ESource::SPROT);

    return true;
}

// LCOV_EXCL_START
// Excluded per Mark's request on 12/14/2016
// Must be restored to test parsing from string buffer
/**********************************************************/
// TODO function is not used
void Flat2AsnCheck(char* ffentry, char* source, char* format, bool accver, Parser::EMode mode, Int4 limit)
{
    ParserPtr       pp;
    Parser::EFormat form;

    if (NStr::EqualNocase(format, "embl"))
        form = Parser::EFormat::EMBL;
    else if (NStr::EqualNocase(format, "genbank"))
        form = Parser::EFormat::GenBank;
    else if (NStr::EqualNocase(format, "sprot"))
        form = Parser::EFormat::SPROT;
    else if (NStr::EqualNocase(format, "xml"))
        form = Parser::EFormat::XML;
    else {
        FtaErrPost(SEV_ERROR, 0, 0, "Unknown format of flat entry");
        return;
    }

    pp         = new Parser;
    pp->format = form;

    if (! FillAccsBySource(*pp, source, false)) {
        delete pp;
        return;
    }

    /* As of June, 2004 the sequence length limitation removed
     */
    pp->limit                 = 0;
    pp->sort                  = true;
    pp->accver                = accver;
    pp->mode                  = mode;
    pp->convert               = true;
    pp->taxserver             = 1;
    pp->medserver             = 1;
    pp->sp_dt_seq_ver         = true;
    pp->cleanup               = 1;
    pp->allow_crossdb_featloc = false;
    pp->genenull              = true;
    pp->qsfile                = nullptr;
    pp->qsfd                  = nullptr;
    pp->qamode                = false;

    pp->ffbuf.set(ffentry);

    fta_fill_find_pub_option(pp, false, false);
    fta_main(pp, true);
}
// LCOV_EXCL_STOP

/*
CScope& GetScope()
{
    static CScope scope(*CObjectManager::GetInstance());

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

CFlatFileParser::CFlatFileParser(IObjtoolsListener* pMessageListener) :
    m_pMessageListener(pMessageListener)
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



bool CFlatFileParser::Parse(Parser& parseInfo, CObjectOStream& objOstr)
{
    return sParseFlatfile(&parseInfo, objOstr);
}


CRef<CSerialObject> CFlatFileParser::Parse(Parser& parseInfo)
{
    CRef<CSerialObject> pResult;
    if (sParseFlatfile(pResult, &parseInfo)) {
        return pResult;
    }

    return CRef<CSerialObject>();
}


static void s_ReportFatalError(const string& msg, IObjtoolsListener* pListener)
{
    if (pListener) {
        pListener->PutMessage(CObjtoolsMessage(msg, eDiag_Fatal));
        return;
    }
    NCBI_THROW(CException, eUnknown, msg);
}

CRef<CSerialObject> CFlatFileParser::Parse(Parser& parseInfo, const string& filename)
{
    CDirEntry dirEntry(filename);
    if (! dirEntry.Exists()) {
        string msg = filename + " does not exist";
        s_ReportFatalError(msg, m_pMessageListener);
    }

    if (! dirEntry.IsFile()) {
        string msg = filename + " is not a valid file";
        s_ReportFatalError(msg, m_pMessageListener);
    }


    if (parseInfo.ffbuf.start) {
        string msg = "Attempting to reinitialize input buffer";
        s_ReportFatalError(msg, m_pMessageListener);
    }

    auto       pFileMap = make_unique<CMemoryFileMap>(filename);
    const auto fileSize = pFileMap->GetFileSize();
    parseInfo.ffbuf.set((const char*)pFileMap->Map(0, fileSize));

    if (! parseInfo.ffbuf.start) {
        string msg = "Failed to open input file " + filename;
        s_ReportFatalError(msg, m_pMessageListener);
    }

    CRef<CSerialObject> pResult;
    if (sParseFlatfile(pResult, &parseInfo)) {
        return pResult;
    }
    return CRef<CSerialObject>();
}

CRef<CSerialObject> CFlatFileParser::Parse(Parser& parseInfo, CNcbiIstream& istr)
{
    if (parseInfo.ffbuf.start) {
        string msg = "Attempting to reinitialize input buffer";
        s_ReportFatalError(msg, m_pMessageListener);
    }

    ostringstream os;
    os << istr.rdbuf();
    string buffer = os.str();

    parseInfo.ffbuf.set(buffer.c_str());

    CRef<CSerialObject> pResult;
    if (sParseFlatfile(pResult, &parseInfo)) {
        return pResult;
    }
    return CRef<CSerialObject>();
}


TEntryList& fta_parse_buf(Parser& pp, const char* buf)
{
    if (! buf || *buf == '\0') {
        return pp.entries;
    }

// It shouldn't be here. These should be set prior calling
// fta_parse_buf() function
//    pp.entrez_fetch = pp.taxserver = pp.medserver = 1;


    FtaInstallPrefix(PREFIX_LOCUS, "SET-UP");
    pp.ffbuf.set(buf);
    FtaDeletePrefix(PREFIX_LOCUS);

    FtaInstallPrefix(PREFIX_LOCUS, "INDEXING");
    bool good = FlatFileIndex(&pp, nullptr);
    FtaDeletePrefix(PREFIX_LOCUS | PREFIX_ACCESSION);

    if (! good) {
        ResetParserStruct(&pp);
        return pp.entries;
    }

    fta_init_servers(&pp);

    CRef<CObjectManager> obj_mgr = CObjectManager::GetInstance();
    CBuffer_DataLoader::RegisterInObjectManager(*obj_mgr, &pp, CObjectManager::eDefault, CObjectManager::kPriority_Default);

    GetScope().AddDefaults();

    auto pInput2Asn = s_GetInput2Asn(pp);
    
    if (pp.format == Parser::EFormat::SPROT) {
        FtaInstallPrefix(PREFIX_LOCUS, "PARSING");

        good = s_GetEntries(*pInput2Asn, pp.entries);

        FtaDeletePrefix(PREFIX_LOCUS | PREFIX_ACCESSION);

        fta_fini_servers(&pp);

        if (! good) {
            ResetParserStruct(&pp);
        }
        return pp.entries;
    }

    FtaInstallPrefix(PREFIX_LOCUS, "SET-UP");

    // fta_entrez_fetch_enable(&pp);

    /*
    if (pp->ffdb == 0)
        fetchname = "index";
    else
        fetchname = "FFDBFetch";

    if (pp->procid != 0) {
        omp = ObjMgrWriteLock();
        ompp = ObjMgrProcFind(omp, pp->procid, fetchname, OMPROC_FETCH);
        ObjMgrUnlock();
    }
    else
        ompp = nullptr;

    if (ompp)
        ompp->procdata = (Pointer)pp;
    else {
        pp->procid = ObjMgrProcLoad(OMPROC_FETCH, fetchname, "fetch",
                                    OBJ_SEQID, 0, OBJ_BIOSEQ, 0, pp,
                                    (pp->ffdb == 0) ?
                                    (ObjMgrGenFunc)IndexFetch :
                                    (ObjMgrGenFunc)FFDBFetch,
                                    PROC_PRIORITY_DEFAULT);
    }
    */

    if (pp.sort) {
        std::sort(pp.entrylist.begin(), pp.entrylist.end(), (pp.accver ? CompareDataV : CompareData));
    }

    CheckDupEntries(&pp);

    FtaErrPost(SEV_INFO, ERR_ENTRY_ParsingSetup, "Parsing {} entries", (size_t)pp.indx);

    if (pp.num_drop > 0) {
        FtaErrPost(SEV_WARNING, ERR_ACCESSION_InvalidAccessNum, "{} invalid accession{} skipped", (size_t)pp.num_drop, (pp.num_drop == 1) ? "" : "s");
    }

    FtaDeletePrefix(PREFIX_LOCUS | PREFIX_ACCESSION);
    FtaInstallPrefix(PREFIX_LOCUS, "PARSING");

    pp.pbp      = new ProtBlk;
    pp.pbp->ibp = new InfoBioseq;

    good = s_GetEntries(*pInput2Asn, pp.entries);

    FtaDeletePrefix(PREFIX_LOCUS | PREFIX_ACCESSION);

    if (! good) {
        ResetParserStruct(&pp);
    }

    //? SeqMgrFreeCache();
    //? FreeFFEntries();

    // fta_entrez_fetch_disable(&pp);
    fta_fini_servers(&pp);

    return pp.entries;

    /*if (pp->procid != 0) {
        omp = ObjMgrWriteLock();
        ompp = ObjMgrProcFind(omp, pp->procid, fetchname, OMPROC_FETCH);
        ObjMgrUnlock();
        if (ompp)
            ompp->procdata = nullptr;
    }

    TransTableFreeAll();*/
}

bool fta_set_format_source(Parser& pp, const string& format, const string& source)
{
    if (format == "embl")
        pp.format = Parser::EFormat::EMBL;
    else if (format == "genbank")
        pp.format = Parser::EFormat::GenBank;
    else if (format == "sprot")
        pp.format = Parser::EFormat::SPROT;
    else if (format == "xml")
        pp.format = Parser::EFormat::XML;
    else {
        FtaErrPost(SEV_FATAL, 0, 0, "Sorry, the format is not available yet ==> available format embl, genbank, prf, sprot, xml.");
        return false;
    }

    return FillAccsBySource(pp, source, pp.all != 0);
}

void fta_init_pp(Parser& pp)
{
    pp.ign_toks     = false;
    pp.date         = false;
    pp.convert      = true;
    pp.no_date      = false;
    pp.sort         = true;
    pp.debug        = false;
    pp.accver       = true;
    pp.histacc      = true;
    pp.transl       = false;
    pp.entrez_fetch = 1;
    pp.taxserver    = 0;
    pp.medserver    = 0;
    pp.ffdb         = false;

    /* as of june, 2004 the sequence length limitation removed
     */
    pp.limit = 0;
    pp.all   = 0;
    // pp.fpo = nullptr;
    // fta_fill_find_pub_option(&pp, false, false);

    pp.indx = 0;
    pp.entrylist.clear();
    pp.curindx               = 0;
    pp.seqtype               = CSeq_id::e_not_set;
    pp.num_drop              = 0;
    pp.acprefix              = nullptr;
    pp.pbp                   = nullptr;
    pp.citat                 = false;
    pp.no_code               = false;
    pp.accpref               = nullptr;
    pp.user_data             = nullptr;
    pp.ff_get_entry          = nullptr;
    pp.ff_get_entry_v        = nullptr;
    pp.ff_get_qscore         = nullptr;
    pp.ff_get_qscore_pp      = nullptr;
    pp.ff_get_entry_pp       = nullptr;
    pp.ff_get_entry_v_pp     = nullptr;
    pp.ign_bad_qs            = false;
    pp.mode                  = Parser::EMode::Release;
    pp.sp_dt_seq_ver         = true;
    pp.simple_genes          = false;
    pp.cleanup               = 1;
    pp.allow_crossdb_featloc = false;
    pp.genenull              = true;
    pp.qsfile                = nullptr;
    pp.qsfd                  = nullptr;
    pp.qamode                = false;
}

END_NCBI_SCOPE
