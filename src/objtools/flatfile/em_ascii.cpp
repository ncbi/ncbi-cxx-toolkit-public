/* ===========================================================================
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
 * File Name: em_ascii.cpp
 *
 * Author: Karl Sirotkin, Hsiu-Chuan Chen
 *
 * File Description:
 *      Preprocessing embl from blocks in memory to asn.
 * Build EMBL format entry block.
 *
 */

#include <ncbi_pch.hpp>

#include "ftacpp.hpp"

#include <objects/seq/Seq_descr.hpp>
#include <objects/seqblock/EMBL_block.hpp>
#include <objects/seqblock/GB_block.hpp>
#include <objects/general/Date.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objmgr/scope.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqblock/EMBL_xref.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqblock/EMBL_dbname.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/User_object.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqcode/Seq_code_type.hpp>
#include <objects/seq/Pubdesc.hpp>
#include <objects/seqfeat/SubSource.hpp>

#include "index.h"
#include "embl.h"

#include <objtools/flatfile/flatdefn.h>
#include <objtools/flatfile/flatfile_parser.hpp>

#include "ftaerr.hpp"
#include "indx_blk.h"
#include "asci_blk.h"
#include "utilfun.h"
#include "utilref.h"
#include "em_ascii.h"
#include "add.h"
#include "utilfeat.h"
#include "loadfeat.h"
#include "nucprot.h"
#include "fta_qscore.h"
#include "citation.h"
#include "fcleanup.h"
#include "entry.h"
#include "ref.h"
#include "xgbparint.h"
#include "xutils.h"
#include "fta_xml.h"
#include "keyword_parse.hpp"

#ifdef THIS_FILE
#  undef THIS_FILE
#endif
#define THIS_FILE "em_ascii.cpp"


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

// clang-format off

/* For new stile of ID line in EMBL data check the "data class"
 * field first to figure out division code
 */
static string_view ParFlat_Embl_dataclass_array[] = {
    "ANN", "CON", "PAT", "EST", "GSS", "HTC", "HTG", "STS", "TSA",
};

/* order by EMBL-block in asn.all
 */
static string_view ParFlat_Embl_DIV_array[] = {
    "FUN", "INV", "MAM", "ORG", "PHG", "PLN", "PRI", "PRO", "ROD",
    "SYN", "UNA", "VRL", "VRT", "PAT", "EST", "STS", "UNC", "GSS",
    "HUM", "HTG", "HTC", "CON", "ENV", "MUS", "TGN", "TSA",
};

/* correspond "DIV" genbank string. Must have the same number
 * of elements !
 */
static const char* ParFlat_GBDIV_array[] = {
    "PLN", "INV", "MAM", "UNA", "PHG", "PLN", "PRI", "BCT", "ROD",
    "SYN", "UNA", "VRL", "VRT", "PAT", "EST", "STS", "UNA", "GSS",
    "PRI", "HTG", "HTC", "CON", "ENV", "ROD", "SYN", "TSA",
    nullptr
};

// clang-format on

static string_view ParFlat_DBname_array[] = {
    "EMBL",
    "GENBANK",
    "DDBJ",
    "GENINFO",
    "MEDLINE",
    "SWISS-PROT",
    "PIR",
    "PDB",
    "EPD",
    "ECD",
    "TFD",
    "FLYBASE",
    "PROSITE",
    "ENZYME",
    "MIM",
    "ECOSEQ",
    "HIV",
};

static const char* ParFlat_DRname_array[] = {
    "ARAPORT",
    "ARRAYEXPRESS",
    "ASTD",
    "BEEBASE",
    "BGD",
    "BIOGRID",
    "BIOMUTA",
    "BIOSAMPLE",
    "CABRI",
    "CCDS",
    "CHEMBL",
    "CHITARS",
    "COLLECTF",
    "DEPOD",
    "DMDM",
    "DNASU",
    "ENA",
    "ENA-CON",
    "ENSEMBL",
    "ENSEMBL-GN",
    "ENSEMBL-SCAFFOLDS",
    "ENSEMBL-TR",
    "ENSEMBLGENOMES",
    "ENSEMBLGENOMES-GN",
    "ENSEMBLGENOMES-TR",
    "ESTHER",
    "EUROPEPMC",
    "EVOLUTIONARYTRACE",
    "EXPRESSIONATLAS",
    "GENE3D",
    "GENEDB",
    "GENEREVIEWS",
    "GENEVISIBLE",
    "GENEWIKI",
    "GENOMERNAI",
    "GDB",
    "GOA",
    "GR",
    "GRAINGENES",
    "GUIDETOPHARMACOLOGY",
    "H-INVDB",
    "HGNC",
    "HOMD",
    "HSSP",
    "IMAGENES",
    "IMGT/GENE-DB",
    "IMGT/HLA",
    "IMGT/LIGM",
    "IMGT_GENE-DB",
    "INTERPRO",
    "IPD-KIR",
    "IPTMNET",
    "KEGG",
    "KO",
    "MALACARDS",
    "MAXQB",
    "MGI",
    "MIRBASE",
    "MOONPROT",
    "MYCOBANK",
    "MYCOCLAP",
    "PATRIC",
    "PAXDB",
    "POMBASE",
    "PR2",
    "PRO",
    "PROTEOMES",
    "PUBMED",
    "RFAM",
    "RZPD",
    "SABIO-RK",
    "SFLD",
    "SGN",
    "SIGNALINK",
    "SIGNALLINK",
    "SIGNOR",
    "SILVA-LSU",
    "SILVA-SSU",
    "STRAININFO",
    "SWISSLIPIDS",
    "SWISSPALM",
    "TMRNA-WEBSITE",
    "TOPDOWNPROTEOMICS",
    "TRANSFAC",
    "TREEFAM",
    "UNICARBKB",
    "UNILIB",
    "UNIPATHWAY",
    "UNIPROT/SWISS-PROT",
    "UNIPROT/TREMBL",
    "UNIPROTKB/SWISS-PROT",
    "UNIPROTKB/TREMBL",
    "UNITE",
    "VBASE2",
    "VEGA-TR",
    "VEGA-GN",
    "VGNC",
    "WBPARASITE",
    "WORMBASE",
    "ZFIN",
    nullptr
};


/**********************************************************
 *
 *   static void GetEmblDate(source, entry, crdate, update):
 *
 *      Contain two lines, first created date, second
 *   updated date.
 *      In the direct submission, it may only have one
 *   DT line, if it is, then created date = update date.
 *
 *                                              9-24-93
 *
 *      Skip XX line between DT line.
 *
 *                                              12-22-93
 *
 **********************************************************/
static void GetEmblDate(Parser::ESource source, const DataBlk& entry, CRef<CDate_std>& crdate, CRef<CDate_std>& update)
{
    char*  offset;
    char*  eptr;
    size_t len;

    crdate.Reset();
    update.Reset();
    if (! SrchNodeType(entry, ParFlat_DT, &len, &offset))
        return;

    eptr   = offset + len;
    crdate = GetUpdateDate(offset + ParFlat_COL_DATA_EMBL, source);
    if (! crdate) {
        return;
    }

    while (offset < eptr) {
        offset = SrchTheChar(string_view(offset, eptr), '\n');
        if (! offset)
            break;

        offset++; /* newline */
        if (fta_StartsWith(offset, "DT"sv)) {
            update = GetUpdateDate(offset + ParFlat_COL_DATA_EMBL,
                                   source);
            break;
        }
    }
    if (update.Empty()) {
        update.Reset(new CDate_std);
        update->SetDay(crdate->GetDay());
        update->SetMonth(crdate->GetMonth());
        update->SetYear(crdate->GetYear());
    }
}


/**********************************************************/
static CRef<CSeq_entry> OutputEmblAsn(bool seq_long, ParserPtr pp, TEntryList& seq_entries)
{
    CRef<CSeq_entry> result;

    DealWithGenes(seq_entries.front(), pp);

    if (seq_entries.front().Empty()) {
        GetScope().ResetDataAndHistory();
        return result;
    }

    /* change qual "citation" on features to SeqFeat.cit find citation
     * in the list by serial_number. If serial number not found remove
     * /citation
     */
    ProcessCitations(seq_entries);

    if (pp->convert) {
        if (pp->cleanup <= 1) {
            FinalCleanup(seq_entries);

            if (pp->qamode && ! seq_entries.empty())
                fta_remove_cleanup_user_object(*(*seq_entries.begin()));
        }

        MaybeCutGbblockSource(seq_entries);
    }

    EntryCheckDivCode(seq_entries, pp);

    // if () fta_set_strandedness(seq_entries);

    if (fta_EntryCheckGBBlock(seq_entries)) {
        FtaErrPost(SEV_WARNING, ERR_ENTRY_GBBlock_not_Empty, "Attention: GBBlock is not empty");
    }

    if (pp->qamode) {
        fta_sort_descr(seq_entries);
        fta_sort_seqfeat_cit(seq_entries);
    }

    if (pp->citat) {
        StripSerialNumbers(seq_entries);
    }

    PackEntries(seq_entries);
    CheckDupDates(seq_entries);

    if (seq_long) {
        FtaErrPost(SEV_REJECT, ERR_ENTRY_LongSequence, "Sequence {}|{} is longer than limit {}", pp->entrylist[pp->curindx]->locusname, pp->entrylist[pp->curindx]->acnum, pp->limit);
    } else {
        result = seq_entries.front();
    }

    seq_entries.clear();
    GetScope().ResetDataAndHistory();

    return result;
}

static void SetXrefObjId(CEMBL_xref& xref, const string& str)
{
    if (str.empty())
        return;

    CEMBL_xref::TId& ids = xref.SetId();

    bool found = false;
    for (const auto& id : ids) {
        if (id->IsStr() && id->GetStr() == str) {
            found = true;
            break;
        }
    }

    if (found)
        return;

    CRef<CObject_id> obj_id(new CObject_id);
    obj_id->SetStr(str);

    ids.push_back(obj_id);
}

/**********************************************************
 *
 *   static void GetEmblBlockXref(entry, xip,
 *                                chentry, dr_ena,
 *                                dr_biosample,
 *                                drop, dr_pubmed,
 *                                ignore_pubmed_dr):
 *
 *      Return a list of EMBLXrefPtr, one EMBLXrefPtr per
 *   type (DR) line.
 *
 **********************************************************/
static void GetEmblBlockXref(const DataBlk& entry, const TXmlIndexList* xil, const char* chentry, TStringList& dr_ena, TStringList& dr_biosample, bool* drop, CEMBL_block& embl, TStringList& dr_pubmed, bool ignore_pubmed_dr)
{
    const char** b;

    const char* drline;

    char* bptr;
    char* eptr;
    char* ptr;
    char* xref;
    char* p;
    char* q;

    bool   valid_biosample;
    bool   many_biosample;
    size_t len;

    Int2 col_data;
    Int2 code;

    CEMBL_block::TXref new_xrefs;

    bool xip = xil && ! xil->empty();
    if (! xip) {
        if (! SrchNodeType(entry, ParFlat_DR, &len, &bptr))
            return;
        col_data = ParFlat_COL_DATA_EMBL;
        xref     = nullptr;
    } else {
        auto tmp = XMLFindTagValue(chentry, *xil, INSDSEQ_DATABASE_REFERENCE);
        if (! tmp)
            return;
        col_data = 0;
        bptr     = StringSave(*tmp);
        len      = tmp->length();
        xref     = bptr;
    }

    for (eptr = bptr + len; bptr < eptr; bptr = ptr) {
        drline = bptr;
        bptr += col_data; /* bptr points to database_identifier */
        code = fta_StringMatch(ParFlat_DBname_array, bptr);

        string name;
        if (code < 0) {
            ptr = SrchTheChar(string_view(bptr, eptr), ';');
            name.assign(bptr, ptr);

            if (NStr::EqualNocase(name, "MD5")) {
                while (ptr < eptr) {
                    if (NStr::Equal(ptr, 0, 2, "DR"))
                        break;

                    ptr = SrchTheChar(string_view(ptr, eptr), '\n');
                    if (*ptr == '\n')
                        ptr++;
                }
                continue;
            }

            for (b = ParFlat_DRname_array; *b; b++) {
                if (NStr::EqualNocase(name, *b))
                    break;
            }

            if (! *b)
                FtaErrPost(SEV_WARNING, ERR_DRXREF_UnknownDBname, "Encountered a new/unknown database name in DR line: \"{}\".", name);
            else if (NStr::EqualNocase(*b, "UNIPROT/SWISS-PROT")) {
                name = "UniProtKB/Swiss-Prot";
            } else if (NStr::EqualNocase(*b, "UNIPROT/TREMBL")) {
                name = "UniProtKB/TrEMBL";
            }
        }

        PointToNextToken(bptr); /* bptr points to primary_identifier */
        p    = SrchTheChar(string_view(bptr, eptr), '\n');
        ptr  = SrchTheChar(string_view(bptr, eptr), ';');

        string id, id1;

        if (ptr && ptr < p) {
            id.assign(bptr, ptr);
            CleanTailNonAlphaChar(id);

            bptr = ptr;
            PointToNextToken(bptr); /* points to secondary_identifier */
        }
        if (p) {
            id1.assign(bptr, p);
            CleanTailNonAlphaChar(id1);
        }

        if (id.empty()) {
            id = id1;
            id1.clear();
        }

        if (name == "BioSample" && ! id.empty()) {
            many_biosample  = (! id.empty() && ! id1.empty());
            valid_biosample = fta_if_valid_biosample(id, false);
            if (! id1.empty() && ! fta_if_valid_biosample(id1, false))
                valid_biosample = false;
            if (many_biosample || ! valid_biosample) {
                q = nullptr;
                if (! drline)
                    drline = "[Empty]";
                else {
                    q = StringChr(const_cast<char*>(drline), '\n');
                    if (q)
                        *q = '\0';
                }
                if (many_biosample)
                    FtaErrPost(SEV_REJECT, ERR_DRXREF_InvalidBioSample, "Multiple BioSample ids provided in the same DR line: \"{}\".", drline);
                if (! valid_biosample)
                    FtaErrPost(SEV_REJECT, ERR_DRXREF_InvalidBioSample, "Invalid BioSample id(s) provided in DR line: \"{}\".", drline);
                *drop = true;
                if (q)
                    *q = '\n';
            } else {
                bool found = false;
                for (const string& val : dr_biosample) {
                    if (val == id) {
                        found = true;
                        break;
                    }
                }

                if (found) {
                    FtaErrPost(SEV_WARNING, ERR_DRXREF_DuplicatedBioSamples, "Duplicated BioSample ids found within DR lines contents: \"{}\".", id);
                } else {
                    dr_biosample.push_back(id);
                }
            }
        } else if (name == "ENA" && ! id.empty() && fta_if_valid_sra(id, false)) {
            if (! id.empty() && ! id1.empty()) {
                q = nullptr;
                if (! drline)
                    drline = "[Empty]";
                else {
                    q = StringChr(const_cast<char*>(drline), '\n');
                    if (q)
                        *q = '\0';
                }
                FtaErrPost(SEV_REJECT, ERR_DRXREF_InvalidSRA, "Multiple possible SRA ids provided in the same DR line: \"{}\".", drline);
                *drop = true;
                if (q)
                    *q = '\n';
            } else {
                bool found = false;
                for (const string& val : dr_ena) {
                    if (val == id) {
                        found = true;
                        break;
                    }
                }

                if (found) {
                    FtaErrPost(SEV_WARNING, ERR_DRXREF_DuplicatedSRA, "Duplicated Sequence Read Archive ids found within DR lines contents: \"{}\".", id);
                } else {
                    dr_ena.push_back(id);
                }
            }
        } else if (name == "PUBMED" && ! id.empty()) {
            if (! ignore_pubmed_dr) {
                bool found = false;
                for (const string& val : dr_pubmed) {
                    if (val == id) {
                        found = true;
                        break;
                    }
                }

                if (! found) {
                    dr_pubmed.push_back(id);
                }
            }
        } else {
            CRef<CEMBL_xref> new_xref(new CEMBL_xref);

            if (code != -1)
                new_xref->SetDbname().SetCode(static_cast<CEMBL_dbname::ECode>(code));
            else
                new_xref->SetDbname().SetName(name);

            if (! id.empty())
                SetXrefObjId(*new_xref, id);

            if (! id1.empty())
                SetXrefObjId(*new_xref, id1);

            new_xrefs.push_back(new_xref);
        }

        ptr = p + 1;

        if (xip)
            continue;

        /* skip "XX" line
         */
        while (ptr < eptr) {
            if (fta_StartsWith(ptr, "DR"sv))
                break;

            ptr = SrchTheChar(string_view(ptr, eptr), '\n');
            if (*ptr == '\n')
                ptr++;
        }
    }

    if (xref)
        MemFree(xref);

    if (! new_xrefs.empty())
        embl.SetXref().swap(new_xrefs);
}

static CTextseq_id& SetTextIdRef(CSeq_id& id)
{
    static CTextseq_id noTextId;

    switch (id.Which()) {
    case CSeq_id::e_Genbank:
        return id.SetGenbank();
    case CSeq_id::e_Embl:
        return id.SetEmbl();
    case CSeq_id::e_Pir:
        return id.SetPir();
    case CSeq_id::e_Swissprot:
        return id.SetSwissprot();
    case CSeq_id::e_Other:
        return id.SetOther();
    case CSeq_id::e_Ddbj:
        return id.SetDdbj();
    case CSeq_id::e_Prf:
        return id.SetPrf();
    case CSeq_id::e_Tpg:
        return id.SetTpg();
    case CSeq_id::e_Tpe:
        return id.SetTpe();
    case CSeq_id::e_Tpd:
        return id.SetTpd();
    case CSeq_id::e_Gpipe:
        return id.SetGpipe();
    case CSeq_id::e_Named_annot_track:
        return id.SetNamed_annot_track();
    default:; // do nothing
    }

    return noTextId;
}

/**********************************************************/
static void GetReleaseInfo(const DataBlk& entry)
{
    char* offset;
    size_t len;

    EntryBlk*    ebp    = entry.GetEntryData();
    CBioseq&     bioseq = ebp->seq_entry->SetSeq();
    CTextseq_id& id     = SetTextIdRef(*(bioseq.SetId().front()));

    if (! SrchNodeType(entry, ParFlat_DT, &len, &offset))
        return;

    char* bptr = offset;
    char* eptr = offset + len;
    if (auto i = string_view(bptr, eptr).find('\n'); i != string_view::npos)
        bptr += i;
    else
        return;

    if (auto i = string_view(bptr, eptr).find("Version"); i != string_view::npos)
        bptr += i;
    else
        return;

    PointToNextToken(bptr); /* bptr points to next token */

    id.SetVersion(NStr::StringToInt(bptr, NStr::fAllowTrailingSymbols));
}

/**********************************************************
 *
 *   static OrgRefPtr GetEmblOrgRef(dbp):
 *
 *      >= 1 OS per entry.
 *
 **********************************************************/
static CRef<COrg_ref> GetEmblOrgRef(const DataBlk& dbp)
{
    string         sTaxname;
    vector<string> taxLines;
    NStr::Split(string_view(dbp.mBuf.ptr, dbp.mBuf.len), "\n", taxLines);
    for (auto& line : taxLines) {
        NStr::TruncateSpacesInPlace(line);
        if (line.empty() || line.starts_with("XX"sv)) {
            continue;
        }
        if (! sTaxname.empty()) {
            sTaxname += ' ';
        }
        sTaxname += line.substr(ParFlat_COL_DATA_EMBL);
    }

    CRef<COrg_ref> org_ref;
    if (sTaxname.empty()) {
        return org_ref;
    }

    org_ref.Reset(new COrg_ref);
    org_ref->SetTaxname(sTaxname);

    auto openP = sTaxname.find('(');
    if (openP != string::npos) {
        auto sCommonName = sTaxname.substr(0, openP);
        auto commonTerm  = sCommonName.find_last_not_of(" \t(");
        if (commonTerm != string::npos) {
            sCommonName = sCommonName.substr(0, commonTerm + 1);
            org_ref->SetCommon(sCommonName);
        }
    }
    return org_ref;
}

/**********************************************************/
static bool CheckEmblContigEverywhere(const IndexblkPtr ibp, Parser::ESource source)
{
    bool condiv = NStr::EqualNocase(ibp->division, "CON");

    bool result = true;

    if (! condiv && ibp->is_contig == false && ibp->origin == false) {
        FtaErrPost(SEV_ERROR, ERR_FORMAT_MissingSequenceData, "Required sequence data is absent. Entry dropped.");
        // ibp->drop = true;
        result = false;
    } else if (! condiv && ibp->is_contig && ibp->origin == false) {
        FtaErrPost(SEV_WARNING, ERR_DIVISION_MappedtoCON, "Division [{}] mapped to CON based on the existence of CONTIG line.", ibp->division);
    } else if (ibp->is_contig && ibp->origin) {
        if (source == Parser::ESource::EMBL || source == Parser::ESource::DDBJ) {
            FtaErrPost(SEV_INFO, ERR_FORMAT_ContigWithSequenceData, "The CONTIG/CO linetype and sequence data are both present. Ignoring sequence data.");
        } else {
            FtaErrPost(SEV_REJECT, ERR_FORMAT_ContigWithSequenceData, "The CONTIG/CO linetype and sequence data may not both be present in a sequence record.");
            // ibp->drop = true;
            result = false;
        }
    } else if (condiv && ! ibp->is_contig && ! ibp->origin) {
        FtaErrPost(SEV_ERROR, ERR_FORMAT_MissingContigFeature, "No CONTIG data in GenBank format file, entry dropped.");
        // ibp->drop = true;
        result = false;
    } else if (condiv && ! ibp->is_contig && ibp->origin) {
        FtaErrPost(SEV_WARNING, ERR_DIVISION_ConDivLacksContig, "Division is CON, but CONTIG data have not been found.");
    }
    return result;
}

/**********************************************************/
bool GetEmblInstContig(const DataBlk& entry, CBioseq& bioseq, ParserPtr pp)
{
    char* p;
    char* q;
    char* r;
    bool  locmap;

    bool allow_crossdb_featloc;
    int  numerr;

    const DataBlk* dbp = TrackNodeType(entry, ParFlat_CO);
    if (! dbp || ! dbp->mBuf.ptr)
        return true;

    Int4 i = static_cast<Int4>(dbp->mBuf.len) - ParFlat_COL_DATA_EMBL;
    if (i <= 1)
        return false;

    p = StringSave(string_view(&dbp->mBuf.ptr[ParFlat_COL_DATA_EMBL], i - 1)); // exclude trailing EOL
    for (q = p; *q != '\0'; q++) {
        if (*q == '\t')
            *q = ' ';
        else if (*q == '\n') {
            *q = ' ';
            if (q[1] == 'C' && q[2] == 'O' && q[3] == ' ') {
                q[1] = ' ';
                q[2] = ' ';
            }
        }
    }
    for (q = p, r = p; *q != '\0'; q++)
        if (*q != ' ')
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

    if (loc.NotEmpty() && loc->IsMix()) {
        allow_crossdb_featloc     = pp->allow_crossdb_featloc;
        pp->allow_crossdb_featloc = true;

        TSeqLocList locs;
        locs.push_back(loc);

        i = fta_fix_seq_loc_id(locs, pp, p, {}, true);
        if (i > 999)
            fta_create_far_fetch_policy_user_object(bioseq, i);
        pp->allow_crossdb_featloc = allow_crossdb_featloc;

        XGappedSeqLocsToDeltaSeqs(loc->GetMix(), bioseq.SetInst().SetExt().SetDelta().Set());
        bioseq.SetInst().SetRepr(CSeq_inst::eRepr_delta);
    } else
        bioseq.SetInst().ResetExt();

    MemFree(p);
    return true;
}

/**********************************************************
 *
 *   bool s_GetEmblInst(pp, entry, dnaconv):
 *
 *      Fills in Seq-inst for an entry. Assumes Bioseq
 *   already allocated.
 *
 **********************************************************/
static bool s_GetEmblInst(ParserPtr pp, const DataBlk& entry, unsigned char* const dnaconv)
{
    EntryBlkPtr ebp;
    IndexblkPtr ibp;

    char* p;
    char* q;
    char* r;

    Int4 i;
    Int2 strand;

    ebp = entry.GetEntryData();

    CBioseq& bioseq = ebp->seq_entry->SetSeq();

    CSeq_inst& inst = bioseq.SetInst();
    inst.SetRepr(CSeq_inst::eRepr_raw);

    ibp = pp->entrylist[pp->curindx];

    p = entry.mBuf.ptr + ParFlat_COL_DATA_EMBL;
    PointToNextToken(p); /* p points to 2nd token */
    PointToNextToken(p); /* p points to 3rd token */

    if (ibp->embl_new_ID)
        PointToNextToken(p);

    /* some entries have "circular" before molecule type in embl
     */
    if (NStr::StartsWith(p, "circular"sv, NStr::eNocase)) {
        inst.SetTopology(CSeq_inst::eTopology_circular);
        PointToNextToken(p);
    } else if (ibp->embl_new_ID)
        PointToNextToken(p);

    r = StringChr(p, ';');
    if (r)
        *r = '\0';

    for (i = 0, q = p; *q != '\0'; q++) {
        if (*q != ' ')
            continue;

        while (*q == ' ')
            q++;
        if (*q != '\0')
            i++;
        q--;
    }

    if (ibp->embl_new_ID == false && inst.GetTopology() != CSeq_inst::eTopology_circular &&
        ! StringStr(p, "DNA") && ! StringStr(p, "RNA") &&
        (pp->source != Parser::ESource::EMBL || (! StringStr(p, "xxx") &&
                                                 ! StringStr(p, "XXX")))) {
        FtaErrPost(SEV_WARNING, ERR_LOCUS_WrongTopology, "Other than circular topology found in EMBL, \"{}\", assign default topology", p);
    }

    /* the "p" must be the mol-type
     */
    if (i == 0 && pp->source == Parser::ESource::NCBI) {
        /* source = NCBI can be full variety of strands/mol-type
         */
        strand = CheckSTRAND(p);
        if (strand > 0)
            inst.SetStrand(static_cast<CSeq_inst::EStrand>(strand));
    }

    if (r)
        *r = ';';

    if (! GetSeqData(pp, entry, bioseq, ParFlat_SQ, dnaconv, eSeq_code_type_iupacna))
        return false;

    if (ibp->is_contig && ! GetEmblInstContig(entry, bioseq, pp))
        return false;

    return true;
}

/**********************************************************
 *
 *   static CRef<CEMBL_block> GetDescrEmblBlock(pp, entry, mfp,
 *                                         gbdiv, biosp,
 *                                         dr_ena, dr_biosample,
 *                                         dr_pubmed):
 *
 *      class is 2nd token of ID line.
 *      div :
 *         - 4th or 5th (if circular) token of ID line;
 *         - but actually Genbank DIV string has to get by
 *           mapping GBDIV_array;
 *         - EST DIV string by searching KW line to map
 *           ParFlat_EST_kw_array;
 *         - PAT DIV string by accession number starting
 *           with "A".
 *      DR line for xref.
 *
 **********************************************************/
static CRef<CEMBL_block> GetDescrEmblBlock(
    ParserPtr pp, const DataBlk& entry, CMolInfo& mol_info, string& gbdiv, const CBioSource* bio_src, TStringList& dr_ena, TStringList& dr_biosample, TStringList& dr_pubmed)
{
    CRef<CEMBL_block> ret, embl(new CEMBL_block);

    IndexblkPtr ibp;
    char*       bptr;
    Char        dataclass[4];
    Char        ch;

    CEMBL_block::TDiv div;
    TKeywordList      keywords;

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

    bool  cancelled;
    bool  drop;
    char* tempdiv;
    Int4  i;

    ibp = pp->entrylist[pp->curindx];

    bptr = entry.mBuf.ptr + ParFlat_COL_DATA_EMBL;
    PointToNextToken(bptr); /* bptr points to 2nd token */

    if (ibp->embl_new_ID == false) {
        if (NStr::StartsWith(bptr, "standard"sv, NStr::eNocase)) {
            // embl->SetClass(CEMBL_block::eClass_standard);
        } else if (NStr::StartsWith(bptr, "unannotated"sv, NStr::eNocase)) {
            embl->SetClass(CEMBL_block::eClass_unannotated);
        } else if (NStr::StartsWith(bptr, "unreviewed"sv, NStr::eNocase) ||
                   NStr::StartsWith(bptr, "preliminary"sv, NStr::eNocase)) {
            embl->SetClass(CEMBL_block::eClass_other);
        } else {
            embl->SetClass(CEMBL_block::eClass_not_set);
        }

        bptr = StringChr(bptr, ';');
        if (bptr)
            bptr = StringChr(bptr + 1, ';');
    } else {
        bptr = StringChr(bptr, ';');
        if (bptr)
            bptr = StringChr(bptr + 1, ';');
        if (bptr)
            bptr = StringChr(bptr + 1, ';');
        if (bptr) {
            while (*bptr == ' ' || *bptr == ';')
                bptr++;
            i = fta_StringMatch(ParFlat_Embl_dataclass_array, bptr);
            if (i < 0)
                bptr = StringChr(bptr, ';');
            else if (i == 0)
                bptr = (char*)"CON";
        }
    }

    if (bptr) {
        while (*bptr == ' ' || *bptr == ';')
            bptr++;
        StringNCpy(dataclass, bptr, 3);
        dataclass[3] = '\0';
        if (StringEqu(dataclass, "TSA"))
            ibp->is_tsa = true;
    } else {
        bptr         = (char*)"   ";
        dataclass[0] = '\0';
    }

    if_cds = check_cds(entry, pp->format);

    if (ibp->psip.NotEmpty())
        pat_ref = true;

    pp->KeywordParser().Cleanup();
    keywords = pp->KeywordParser().KeywordList();

    embl->SetKeywords() = keywords;
    if (ibp->is_tpa && ! fta_tpa_keywords_check(keywords)) {
        return ret;
    }

    if (ibp->is_tsa && ! fta_tsa_keywords_check(keywords, pp->source)) {
        return ret;
    }

    if (ibp->is_tls && ! fta_tls_keywords_check(keywords, pp->source)) {
        return ret;
    }

    for (const string& key : keywords) {
        fta_keywords_check(key, &est_kwd, &sts_kwd, &gss_kwd, &htc_kwd, &fli_kwd, &wgs_kwd, &tpa_kwd, &env_kwd, &mga_kwd, &tsa_kwd, &tls_kwd);
    }

    if (ibp->env_sample_qual == false && env_kwd) {
        FtaErrPost(SEV_REJECT, ERR_KEYWORD_ENV_NoMatchingQualifier, "This record utilizes the ENV keyword, but there are no /environmental_sample qualifiers among its source features.");
        return ret;
    }

    div = static_cast<CEMBL_block::TDiv>(fta_StringMatch(ParFlat_Embl_DIV_array, bptr));
    if (div < 0) {
        ch      = bptr[3];
        bptr[3] = '\0';
        FtaErrPost(SEV_REJECT, ERR_DIVISION_UnknownDivCode, "Unknown division code \"{}\" found in Embl flatfile. Record rejected.", bptr);
        bptr[3] = ch;
        return ret;
    }

    /* Embl has recently (7-19-93, email) decided to change the name of
     * its "UNA"==10 division to "UNC"==16 (for "unclassified")
     */
    if (div == 16)
        div = CEMBL_block::eDiv_una;

    StringCpy(ibp->division, ParFlat_GBDIV_array[div]);

    /* 06-10-96 new HUM division replaces the PRI
     * it's temporarily mapped to 'other' in asn.1 embl-block.
     * Divisions GSS, HUM, HTG, CON, ENV and MUS are mapped to other.
     */
    int thtg = (div == 18) ? CEMBL_block::eDiv_pri : div;
    gbdiv    = ParFlat_GBDIV_array[thtg];

    if (div <= CEMBL_block::eDiv_sts)
        embl->SetDiv(div);

    const char* p = gbdiv.c_str();
    if (ibp->is_tpa &&
        (StringEqu(p, "EST") || StringEqu(p, "GSS") ||
         StringEqu(p, "PAT") || StringEqu(p, "HTG"))) {
        FtaErrPost(SEV_REJECT, ERR_DIVISION_BadTPADivcode, "Division code \"{}\" is not legal for TPA records. Entry dropped.", p);
        return ret;
    }

    if (ibp->is_tsa && ! StringEqu(p, "TSA")) {
        FtaErrPost(SEV_REJECT, ERR_DIVISION_BadTSADivcode, "Division code \"{}\" is not legal for TSA records. Entry dropped.", p);
        return ret;
    }

    cancelled = IsCancelled(embl->GetKeywords());

    if (div == 19) /* HTG */
    {
        if (! HasHtg(embl->GetKeywords())) {
            FtaErrPost(SEV_ERROR, ERR_DIVISION_MissingHTGKeywords, "Division is HTG, but entry lacks HTG-related keywords. Entry dropped.");
            return ret;
        }
        tempdiv = StringSave("HTG");
    } else
        tempdiv = nullptr;

    fta_check_htg_kwds(embl->SetKeywords(), pp->entrylist[pp->curindx], mol_info);

    DefVsHTGKeywords(mol_info.GetTech(), entry, ParFlat_DE, ParFlat_SQ, cancelled);
    if ((mol_info.GetTech() == CMolInfo::eTech_htgs_0 || mol_info.GetTech() == CMolInfo::eTech_htgs_1 ||
         mol_info.GetTech() == CMolInfo::eTech_htgs_2) &&
        ! gbdiv.empty()) {
        gbdiv.clear();
    }

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
        else if ((i == 2 && wgs_kwd && tpa_kwd) ||
                 (i == 2 && tsa_kwd && tpa_kwd)) {
        } else if (i != 2 || env_kwd == false ||
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

    CMolInfo::TTech tech = mol_info.GetTech();
    if (tech == CMolInfo::eTech_htgs_0 || tech == CMolInfo::eTech_htgs_1 ||
        tech == CMolInfo::eTech_htgs_2 || tech == CMolInfo::eTech_htgs_3) {
        RemoveHtgPhase(embl->SetKeywords());
    }

    size_t len = 0;
    if (SrchNodeType(entry, ParFlat_KW, &len, &bptr)) {
        string kw = GetBlkDataReplaceNewLine(string_view(bptr, len), ParFlat_COL_DATA_EMBL);

        if (! est_kwd && kw.find("EST") != string::npos) {
            FtaErrPost(SEV_WARNING, ERR_KEYWORD_ESTSubstring, "Keyword {} has substring EST, but no official EST keywords found", kw);
        }
        if (! sts_kwd && kw.find("STS") != string::npos) {
            FtaErrPost(SEV_WARNING, ERR_KEYWORD_STSSubstring, "Keyword {} has substring STS, but no official STS keywords found", kw);
        }
        if (! gss_kwd && kw.find("GSS") != string::npos) {
            FtaErrPost(SEV_WARNING, ERR_KEYWORD_GSSSubstring, "Keyword {} has substring GSS, but no official GSS keywords found", kw);
        }
    }

    if (! ibp->is_contig) {
        drop                 = false;
        CMolInfo::TTech tech = mol_info.GetTech();

        check_div(ibp->is_pat, pat_ref, est_kwd, sts_kwd, gss_kwd, if_cds, gbdiv, &tech, ibp->bases, pp->source, drop);
        if (tech != CMolInfo::eTech_unknown)
            mol_info.SetTech(tech);
        else
            mol_info.ResetTech();

        if (drop) {
            return ret;
        }
    } else if (gbdiv == "CON") {
        gbdiv.clear();
    }

    bool is_htc_div = (gbdiv == "HTC");
    bool has_htc    = HasHtc(embl->GetKeywords());

    if (is_htc_div && ! has_htc) {
        FtaErrPost(SEV_ERROR, ERR_DIVISION_MissingHTCKeyword, "This record is in the HTC division, but lacks the required HTC keyword.");
        return ret;
    }
    if (! is_htc_div && has_htc) {
        FtaErrPost(SEV_ERROR, ERR_DIVISION_InvalidHTCKeyword, "This record has the special HTC keyword, but is not in HTC division. If this record has graduated out of HTC, then the keyword should be removed.");
        return ret;
    }

    if (is_htc_div) {
        char* p;
        p = entry.mBuf.ptr + ParFlat_COL_DATA_EMBL; /* p points to 1st token */
        PointToNextToken(p);                       /* p points to 2nd token */
        PointToNextToken(p);                       /* p points to 3rd token */

        if (ibp->embl_new_ID) {
            PointToNextToken(p);
            PointToNextToken(p);
        } else if (NStr::StartsWith(p, "circular"sv, NStr::eNocase))
            PointToNextToken(p); /* p points to 4th token */

        if (StringEquN(p + 1, "s-", 2))
            p += 3;
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
    if (! gbdiv.empty()) {
        if (gbdiv == "EST") {
            ibp->EST = true;
            mol_info.SetTech(CMolInfo::eTech_est);
        } else if (gbdiv == "STS") {
            ibp->STS = true;
            mol_info.SetTech(CMolInfo::eTech_sts);
        } else if (gbdiv == "GSS") {
            ibp->GSS = true;
            mol_info.SetTech(CMolInfo::eTech_survey);
        } else if (gbdiv == "HTC") {
            ibp->HTC = true;
            mol_info.SetTech(CMolInfo::eTech_htc);
            gbdiv.clear();
        } else if ((gbdiv == "SYN") && bio_src &&
                   bio_src->IsSetOrigin() && bio_src->GetOrigin() == CBioSource::eOrigin_synthetic) {
            gbdiv.clear();
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
        fta_remove_keywords(mol_info.GetTech(), embl->SetKeywords());

    if (ibp->is_tpa)
        fta_remove_tpa_keywords(embl->SetKeywords());

    if (ibp->is_tsa)
        fta_remove_tsa_keywords(embl->SetKeywords(), pp->source);

    if (ibp->is_tls)
        fta_remove_tls_keywords(embl->SetKeywords(), pp->source);

    if (bio_src && bio_src->IsSetSubtype()) {
        for (const auto& subtype : bio_src->GetSubtype()) {
            if (subtype->GetSubtype() == CSubSource::eSubtype_environmental_sample) {
                fta_remove_env_keywords(embl->SetKeywords());
                break;
            }
        }
    }


    CRef<CDate_std> std_creation_date,
        std_update_date;

    GetEmblDate(pp->source, entry, std_creation_date, std_update_date);
    
    if (! std_creation_date || ! std_update_date) { // The ASN.1 spec for EMBL-block requires 
        return {};                                  // both a creation date and an update date.
    }

    embl->SetCreation_date().SetStd(*std_creation_date);
    embl->SetUpdate_date().SetStd(*std_update_date);

    ibp->wgssec[0] = '\0';
    GetExtraAccession(ibp, pp->allow_uwsec, pp->source, embl->SetExtra_acc());

    GetEmblBlockXref(entry, nullptr, nullptr, dr_ena, dr_biosample, &ibp->drop, *embl, dr_pubmed, pp->ignore_pubmed_dr);

    if (StringEqu(dataclass, "ANN") || StringEqu(dataclass, "CON")) {
        if (StringLen(ibp->acnum) == 8 &&
            (fta_StartsWith(ibp->acnum, "CT"sv) ||
             fta_StartsWith(ibp->acnum, "CU"sv))) {
            bool found = false;
            for (const string& acc : embl->SetExtra_acc()) {
                if (fta_if_wgs_acc(acc) == 0 &&
                    (acc[0] == 'C' || acc[0] == 'U')) {
                    found = true;
                    break;
                }
            }
            if (found)
                mol_info.SetTech(CMolInfo::eTech_wgs);
        }
    }

    return embl;
}


static bool s_DuplicatesBiosource(const CBioSource& biosource, const string& gbdiv)
{
    return (biosource.IsSetOrg() &&
            biosource.GetOrg().IsSetOrgname() &&
            biosource.GetOrg().GetOrgname().IsSetDiv() &&
            NStr::Equal(biosource.GetOrg().GetOrgname().GetDiv(), gbdiv));
}

/**********************************************************/
static CRef<CGB_block> GetEmblGBBlock(ParserPtr pp, const DataBlk& entry, const string& gbdiv, CBioSource* bio_src)
{
    IndexblkPtr ibp;

    CRef<CGB_block> gbb(new CGB_block);

    ibp = pp->entrylist[pp->curindx];

    if (pp->source == Parser::ESource::NCBI) {
        ibp->wgssec[0] = '\0';
        GetExtraAccession(ibp, pp->allow_uwsec, pp->source, gbb->SetExtra_accessions());
        pp->KeywordParser().Cleanup();
        gbb->SetKeywords() = pp->KeywordParser().KeywordList();
    }

    if (! gbdiv.empty()) {
        if (NStr::EqualNocase(gbdiv.c_str(), "ENV") &&
            bio_src && bio_src->IsSetSubtype()) {
            const auto& subtype = bio_src->GetSubtype();
            const auto  it =
                find_if(begin(subtype), end(subtype), [](auto pSubSource) {
                    return pSubSource->GetSubtype() == CSubSource::eSubtype_environmental_sample;
                });
            if ((it == subtype.end()) && ! s_DuplicatesBiosource(*bio_src, gbdiv)) { // Not found
                gbb->SetDiv(gbdiv);
            }
        } else if (! bio_src ||
                   ! s_DuplicatesBiosource(*bio_src, gbdiv)) {
            gbb->SetDiv(gbdiv);
        }
    }

    if (! gbb->IsSetExtra_accessions() && ! gbb->IsSetKeywords() && ! gbb->IsSetDiv())
        gbb.Reset();

    return gbb;
}

/**********************************************************
 *
 *   static MolInfoPtr GetEmblMolInfo(entry, pp, orp):
 *
 *      3rd or 4th token in the ID line.
 *   OG line.
 *
 **********************************************************/
static CRef<CMolInfo> GetEmblMolInfo(ParserPtr pp, const DataBlk& entry, const COrg_ref* org_ref)
{
    IndexblkPtr ibp;

    char* bptr;
    char* p;
    char* q;
    char* r;
    Int4  i;

    ibp  = pp->entrylist[pp->curindx];
    bptr = entry.mBuf.ptr + ParFlat_COL_DATA_EMBL; /* bptr points to 1st
                                                           token */
    PointToNextToken(bptr);                /* bptr points to 2nd token */
    PointToNextToken(bptr);                /* bptr points to 3rd token */

    if (NStr::StartsWith(bptr, "circular"sv, NStr::eNocase) || ibp->embl_new_ID)
        PointToNextToken(bptr); /* bptr points to 4th token */
    if (ibp->embl_new_ID)
        PointToNextToken(bptr); /* bptr points to 5th token */

    r = StringChr(bptr, ';');
    if (r)
        *r = '\0';

    for (i = 0, q = bptr; *q != '\0'; q++) {
        if (*q != ' ')
            continue;

        while (*q == ' ')
            q++;
        if (*q != '\0')
            i++;
        q--;
    }

    if (r)
        for (p = r + 1; *p == ' ' || *p == ';';)
            p++;
    else
        p = bptr;

    CRef<CMolInfo> mol_info(new CMolInfo);

    if (fta_StartsWith(p, "EST"sv))
        mol_info->SetTech(CMolInfo::eTech_est);
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

    if (i == 0 && CheckSTRAND(bptr) >= 0)
        bptr = bptr + 3;

    GetFlatBiomol(mol_info->SetBiomol(), mol_info->GetTech(), bptr, pp, entry, org_ref);
    if (mol_info->GetBiomol() == CMolInfo::eBiomol_unknown) // not set
        mol_info->ResetBiomol();

    if (r)
        *r = ';';

    return mol_info;
}

/**********************************************************/
static CRef<CUser_field> fta_create_user_field(const char* tag, TStringList& lst)
{
    CRef<CUser_field> field;
    if (! tag || lst.empty())
        return field;

    field.Reset(new CUser_field);
    field->SetLabel().SetStr(tag);
    field->SetNum(static_cast<CUser_field::TNum>(lst.size()));

    for (const string& item : lst) {
        field->SetData().SetStrs().push_back(item);
    }

    return field;
}

/**********************************************************/
void fta_build_ena_user_object(list<CRef<CSeqdesc>>& descrs, TStringList& dr_ena, TStringList& dr_biosample, CRef<CUser_object>& dbuop)
{
    bool got = false;

    if (dr_ena.empty() && dr_biosample.empty())
        return;

    CUser_object* user_obj_ptr = nullptr;

    for (auto& descr : descrs) {
        if (! descr->IsUser() || ! descr->GetUser().IsSetType())
            continue;

        const CObject_id& obj_id = descr->GetUser().GetType();

        if (obj_id.IsStr() && obj_id.GetStr() == "DBLink") {
            user_obj_ptr = &descr->SetUser();
            got          = true;
            break;
        }
    }

    CRef<CUser_field> field_bs;
    if (! dr_biosample.empty())
        field_bs = fta_create_user_field("BioSample", dr_biosample);

    CRef<CUser_field> field_ena;
    if (! dr_ena.empty()) {
        field_ena = fta_create_user_field("Sequence Read Archive", dr_ena);
    }

    if (field_bs.Empty() && field_ena.Empty())
        return;

    CRef<CUser_object> user_obj;

    if (! got) {
        user_obj.Reset(new CUser_object);
        user_obj->SetType().SetStr("DBLink");

        user_obj_ptr = user_obj.GetPointer();
    }

    if (field_bs.NotEmpty())
        user_obj_ptr->SetData().push_back(field_bs);
    if (field_ena.NotEmpty())
        user_obj_ptr->SetData().push_back(field_ena);

    if (! got) {
        CRef<CSeqdesc> descr(new CSeqdesc);
        descr->SetUser(*user_obj);
        descrs.push_back(descr);
    }

    if (! got)
        dbuop = user_obj;
    else {
        dbuop.Reset(new CUser_object);
        dbuop->Assign(*user_obj_ptr);
    }
}

/**********************************************************/
static void fta_create_imgt_misc_feat(CBioseq& bioseq, CEMBL_block& embl_block, IndexblkPtr ibp)
{
    if (! embl_block.IsSetXref())
        return;

    CSeq_feat::TDbxref xrefs;
    for (const auto& xref : embl_block.GetXref()) {
        if (! xref->IsSetDbname() || ! xref->GetDbname().IsName() ||
            ! xref->GetDbname().GetName().starts_with("IMGT/"sv))
            continue;

        bool empty = true;
        for (const auto& id : xref->GetId()) {
            if (id->IsStr() && ! id->GetStr().empty()) {
                empty = false;
                break;
            }
        }

        if (empty)
            continue;

        CRef<CDbtag> tag(new CDbtag);
        tag->SetDb(xref->GetDbname().GetName());

        string& id_str = tag->SetTag().SetStr();

        bool need_delimiter = false;
        for (const auto& id : xref->GetId()) {
            if (id->IsStr() && ! id->GetStr().empty()) {
                if (need_delimiter)
                    id_str += "; ";
                else
                    need_delimiter = true;

                id_str += id->GetStr();
            }
        }

        xrefs.push_back(tag);
    }

    if (xrefs.empty())
        return;

    CRef<CSeq_feat> feat(new CSeq_feat);
    CImp_feat&      imp = feat->SetData().SetImp();
    imp.SetKey("misc_feature");
    feat->SetDbxref().swap(xrefs);
    feat->SetLocation(*fta_get_seqloc_int_whole(*(*bioseq.SetId().begin()), ibp->bases));

    CBioseq::TAnnot& annot = bioseq.SetAnnot();
    if (annot.empty() || ! (*annot.begin())->IsFtable()) {
        CRef<CSeq_annot> new_annot(new CSeq_annot);
        new_annot->SetData().SetFtable().push_back(feat);

        annot.push_back(new_annot);
    } else {
        CSeq_annot& old_annot = *(*annot.begin());
        old_annot.SetData().SetFtable().push_front(feat);
    }
}

static bool s_HasTPAPrefix(string_view line)
{
    return line.starts_with("TPA:"sv) ||
           line.starts_with("TPA_exp:"sv) ||
           line.starts_with("TPA_inf:"sv) ||
           line.starts_with("TPA_asm:"sv) ||
           line.starts_with("TPA_reasm:"sv) ||
           line.starts_with("TPA_specdb:"sv);
}

/**********************************************************/
static void GetEmblDescr(ParserPtr pp, const DataBlk& entry, CBioseq& bioseq)
{
    IndexblkPtr ibp;

    char*  offset;
    string gbdiv;

    bool is_htg = false;

    size_t len;

    ibp = pp->entrylist[pp->curindx];

    /* pp->source == NCBI then no embl-block, only GB-block
     */

    /* DE data ==> descr_title
     */
    string title;

    if (SrchNodeType(entry, ParFlat_DE, &len, &offset)) {
        string str = GetBlkDataReplaceNewLine(string_view(offset, len), ParFlat_COL_DATA_EMBL);

        for (size_t pos = 0; pos < str.size();) {
            pos = str.find(";;", pos);
            if (pos == string::npos)
                break;
            ++pos;
            size_t j = 0;
            for (size_t i = pos; i < str.size() && str[i] == ';'; ++i)
                ++j;
            str.erase(pos, j);
        }

        while (! str.empty()) {
            char c = str.back();
            if (c == ' ' || c == ';')
                str.pop_back();
            else
                break;
        }

        if (ibp->is_tpa == false && pp->source != Parser::ESource::EMBL &&
            str.starts_with("TPA:"sv)) {
            FtaErrPost(SEV_REJECT, ERR_DEFINITION_ShouldNotBeTPA, "This is apparently _not_ a TPA record, but the special \"TPA:\" prefix is present on its definition line. Entry dropped.");
            ibp->drop = true;
            return;
        }

        if (ibp->is_tsa == false && str.starts_with("TSA:"sv)) {
            FtaErrPost(SEV_REJECT, ERR_DEFINITION_ShouldNotBeTSA, "This is apparently _not_ a TSA record, but the special \"TSA:\" prefix is present on its definition line. Entry dropped.");
            ibp->drop = true;
            return;
        }

        if (ibp->is_tls == false && str.starts_with("TLS:"sv)) {
            FtaErrPost(SEV_REJECT, ERR_DEFINITION_ShouldNotBeTLS, "This is apparently _not_ a TLS record, but the special \"TLS:\" prefix is present on its definition line. Entry dropped.");
            ibp->drop = true;
            return;
        }

        if (str.starts_with("TPA:"sv)) {
            string_view str1;
            if (ibp->assembly)
                str1 = "TPA_asm:"sv;
            else if (ibp->specialist_db)
                str1 = "TPA_specdb:"sv;
            else if (ibp->inferential)
                str1 = "TPA_inf:"sv;
            else if (ibp->experimental)
                str1 = "TPA_exp:"sv;

            if (! str1.empty())
                str.replace(0, 4, str1);
        }

        CRef<CSeqdesc> descr(new CSeqdesc);
        descr->SetTitle(str);
        bioseq.SetDescr().Set().push_back(descr);

        title = str;
    }

    if (SrchNodeType(entry, ParFlat_PR, &len, &offset))
        fta_get_project_user_object(bioseq.SetDescr().Set(), offset, Parser::EFormat::EMBL, &ibp->drop, pp->source);

    if (ibp->is_tpa &&
        (title.empty() || ! s_HasTPAPrefix(title))) {
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

    if (ibp->is_tls && (title.empty() || ! title.starts_with("TLS:"))) {
        FtaErrPost(SEV_REJECT, ERR_DEFINITION_MissingTLS, "This is apparently a TLS record, but it lacks the required \"TLS:\" prefix on its definition line. Entry dropped.");
        ibp->drop = true;
        return;
    }

    /* RN data ==> pub  should be before GBblock because we need patent ref
     */
    auto& chain = TrackNodes(entry);

    TStringList names;
    for (auto& ref_blk : chain) {
        if (ref_blk.mType != ParFlat_REF_END)
            continue;

        CRef<CPubdesc> pubdesc = DescrRefs(pp, ref_blk, ParFlat_COL_DATA_EMBL);
        if (pubdesc.NotEmpty()) {
            CRef<CSeqdesc> descr(new CSeqdesc);
            descr->SetPub(*pubdesc);
            bioseq.SetDescr().Set().push_back(descr);
            if (pp->medserver > 0) {
                GetCitSubLastNames(descr->GetPub().GetPub(), names);
            }
        }
    }

    for (auto& ref_blk : chain) {
        if (ref_blk.mType != ParFlat_REF_NO_TARGET)
            continue;

        CRef<CPubdesc> pubdesc = DescrRefs(pp, ref_blk, ParFlat_COL_DATA_EMBL);
        if (pubdesc.NotEmpty()) {
            CRef<CSeqdesc> descr(new CSeqdesc);
            descr->SetPub(*pubdesc);
            bioseq.SetDescr().Set().push_back(descr);
            if (pp->medserver > 0) {
                GetCitSubLastNames(descr->GetPub().GetPub(), names);
            }
        }
    }
    /* OS data ==> descr_org
     */
    CBioSource* bio_src = nullptr;
    COrg_ref*   org_ref = nullptr;

    for (auto& descr : bioseq.SetDescr().Set()) {
        if (descr->IsSource()) {
            bio_src = &(descr->SetSource());
            if (bio_src->IsSetOrg())
                org_ref = &bio_src->SetOrg();
            break;
        }
    }

    /* MolInfo, 3rd or 4th token in the ID line
     */
    CRef<CMolInfo> mol_info = GetEmblMolInfo(pp, entry, org_ref);

    TStringList dr_ena,
        dr_biosample,
        dr_pubmed;

    CRef<CEMBL_block> embl_block =
        GetDescrEmblBlock(pp, entry, *mol_info, gbdiv, bio_src, dr_ena, dr_biosample, dr_pubmed);

    if (! pp->medserver && ! dr_pubmed.empty()) {
        FtaErrPost(SEV_WARNING, ERR_DRXREF_PMIDsNotProcessed,
                   "DR-Line cross-references to PubMed exist, but cannot be processed because PubMed Lookups are disabled.");
    }

    if (pp->medserver == 1) {
        for (const string& val : dr_pubmed) {
            CRef<CPubdesc> pubdesc = EmblDescrRefsDr(pp, ENTREZ_ID_FROM(int, fta_atoi(val)));
            if (! pubdesc) {
                FtaErrPost(SEV_ERROR, ERR_DRXREF_PMIDNotFoundInPubMed,
                           "PMID \"{}\" cited by DR-Line cross reference does not exist in PubMed.",
                           val);
                continue;
            }

            CRef<CSeqdesc> descr(new CSeqdesc);
            descr->SetPub(*pubdesc);
            bioseq.SetDescr().Set().push_back(descr);
            if (! names.empty()) {
                CitSubPubmedDrNamesCheck(descr->GetPub().GetPub(), names, val);
            }
        }
    }

    if (pp->source == Parser::ESource::EMBL && embl_block.NotEmpty())
        fta_create_imgt_misc_feat(bioseq, *embl_block, ibp);

    if ((pp->source == Parser::ESource::DDBJ || pp->source == Parser::ESource::EMBL) &&
        ibp->is_contig && ! mol_info->IsSetTech()) {
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

        if (mol_info->IsSetTech() && (mol_info->GetTech() == CMolInfo::eTech_htgs_0 || mol_info->GetTech() == CMolInfo::eTech_htgs_1 ||
                                      mol_info->GetTech() == CMolInfo::eTech_htgs_2))
            is_htg = true;
    } else {
        mol_info.Reset();
    }

    CRef<CUser_object> dbuop;
    if (! dr_ena.empty() || ! dr_biosample.empty())
        fta_build_ena_user_object(bioseq.SetDescr().Set(), dr_ena, dr_biosample, dbuop);

    if (embl_block.Empty()) {
        ibp->drop = true;
        return;
    }

    if (NStr::StartsWith(ibp->division, "CON"sv, NStr::eNocase))
        fta_add_hist(pp, bioseq, embl_block->SetExtra_acc(), Parser::ESource::EMBL, CSeq_id::e_Embl, true, ibp->acnum);
    else
        fta_add_hist(pp, bioseq, embl_block->SetExtra_acc(), Parser::ESource::EMBL, CSeq_id::e_Embl, false, ibp->acnum);

    if (embl_block->GetExtra_acc().empty())
        embl_block->ResetExtra_acc();

    CRef<CGB_block> gbb;

    if (pp->source == Parser::ESource::NCBI || (! embl_block->IsSetDiv() && ! gbdiv.empty())) {
        gbb = GetEmblGBBlock(pp, entry, gbdiv, bio_src); /* GB-block */
    }

    gbdiv.clear();

    bool hasEmblBlock = false;
    if (pp->source != Parser::ESource::NCBI) {
        CRef<CSeqdesc> descr(new CSeqdesc);
        descr->SetEmbl(*embl_block);
        bioseq.SetDescr().Set().push_back(descr);
        hasEmblBlock = true;
    }

    if (! SrchNodeType(entry, ParFlat_AH, &len, &offset) && ibp->is_tpa && ibp->is_wgs == false) {
        if (ibp->inferential || ibp->experimental) {
            if (! fta_dblink_has_sra(dbuop) &&
                ! TrackNodeType(entry, ParFlat_CC)) {
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
        fta_parse_tpa_tsa_block(bioseq, offset, ibp->acnum, ibp->vernum, len, ParFlat_COL_DATA_EMBL, ibp->is_tpa) == false) {
        ibp->drop = true;
        return;
    }

    /* GB-block and div
     */
    if (pp->taxserver == 1) {
        if (hasEmblBlock && embl_block->IsSetDiv() && embl_block->GetDiv() < 15) {
            if (org_ref && org_ref->IsSetOrgname() && ! org_ref->GetOrgname().IsSetDiv() &&
                (! org_ref->IsSetDb() || ! fta_orgref_has_taxid(org_ref->GetDb()))) {
                org_ref->SetOrgname().SetDiv(ParFlat_GBDIV_array[embl_block->GetDiv()]);
            }

            if (bioseq.IsSetAnnot()) {
                for (auto& pAnnot : bioseq.SetAnnot()) {
                    if (pAnnot->IsFtable()) {
                        for (auto& pFeat : pAnnot->SetData().SetFtable()) {
                            if (pFeat->IsSetData() && pFeat->SetData().IsBiosrc()) {
                                auto& biosrc = pFeat->SetData().SetBiosrc();
                                if (biosrc.IsSetOrg() &&
                                    (! biosrc.GetOrg().IsSetDb() ||
                                     ! fta_orgref_has_taxid(biosrc.GetOrg().GetDb()))) {
                                    biosrc.SetOrg().SetOrgname().SetDiv(ParFlat_GBDIV_array[embl_block->GetDiv()]);
                                }
                            }
                        }
                    }
                }
            }
        } else if (gbb && gbb->IsSetDiv()) {
            fta_fix_orgref_div(bioseq.GetAnnot(), org_ref, *gbb);
        }
    }

    if (gbb) {
        CRef<CSeqdesc> descr(new CSeqdesc);
        descr->SetGenbank(*gbb);
        bioseq.SetDescr().Set().push_back(descr);
    }

    /* all CC data ==> comment
     */
    if (SrchNodeType(entry, ParFlat_CC, &len, &offset)) {
        string comment = GetDescrComment(offset, len, ParFlat_COL_DATA_EMBL, is_htg, ibp->is_pat);
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

    if (pp->no_date)
        return;

    /* DT data ==> create-date, update-date
     */

    CRef<CDate_std> std_creation_date,
        std_update_date;
    GetEmblDate(pp->source, entry, std_creation_date, std_update_date);
    if (std_creation_date.NotEmpty()) {
        CRef<CSeqdesc> descr(new CSeqdesc);
        descr->SetCreate_date().SetStd(*std_creation_date);
        bioseq.SetDescr().Set().push_back(descr);
    }

    if (std_update_date.NotEmpty()) {
        CRef<CSeqdesc> descr(new CSeqdesc);
        descr->SetUpdate_date().SetStd(*std_update_date);
        bioseq.SetDescr().Set().push_back(descr);

        if (std_creation_date.NotEmpty() && std_creation_date->Compare(*std_update_date) == CDate::eCompare_after) {
            string crdate_str, update_str;
            std_creation_date->GetDate(&crdate_str, "%2M-%2D-%4Y");
            std_update_date->GetDate(&crdate_str, "%2M-%2D-%4Y");
            FtaErrPost(SEV_ERROR, ERR_DATE_IllegalDate, "Update-date \"{}\" precedes create-date \"{}\".", update_str, crdate_str);
        }
    }
}

/**********************************************************/
static void FakeEmblBioSources(const DataBlk& entry, CBioseq& bioseq)
{
    unsigned count = 0;

    const auto& chain = TrackNodes(entry);
    for (const auto& os_blk : chain) {
        if (os_blk.mType != ParFlat_OS)
            continue;
        ++count;

        CRef<COrg_ref> org_ref = GetEmblOrgRef(os_blk);
        if (org_ref.Empty())
            continue;

        CRef<CBioSource> bio_src(new CBioSource);
        bio_src->SetOrg(*org_ref);

        string& taxname_str = org_ref->SetTaxname();
        size_t  off_pos     = 0;
        if (GetGenomeInfo(*bio_src, taxname_str) && bio_src->GetGenome() != CBioSource::eGenome_plasmid) {
            while (taxname_str[off_pos] != ' ' && off_pos < taxname_str.size())
                ++off_pos;
            while (taxname_str[off_pos] == ' ' && off_pos < taxname_str.size())
                ++off_pos;
        }

        taxname_str = taxname_str.substr(off_pos);
        if (taxname_str == "Unknown.") {
            taxname_str = taxname_str.substr(0, taxname_str.size() - 1);
        }

        for (const auto& subdbp : os_blk.GetSubBlocks()) {
            if (subdbp.mType == ParFlat_OG) {
                GetGenomeInfo(*bio_src, subdbp.mBuf.ptr + ParFlat_COL_DATA_EMBL);
                continue;
            }
            if (subdbp.mType != ParFlat_OC || ! subdbp.mBuf.ptr ||
                subdbp.mBuf.len < ParFlat_COL_DATA_EMBL)
                continue;

            string lineage(subdbp.mBuf.ptr + ParFlat_COL_DATA_EMBL, subdbp.mBuf.len - ParFlat_COL_DATA_EMBL);
            while (! lineage.empty()) {
                auto it = lineage.find("\nOC   ");
                if (it == string::npos)
                    break;
                lineage.erase(it, 5);
            }
            while (! lineage.empty()) {
                char c = lineage.back();
                if (c != ' ' && c != '\t' && c != '\n' && c != '.' && c != ';')
                    break;
                lineage.pop_back();
            }
            if (! lineage.empty() && ! org_ref->IsSetOrgname()) {
                org_ref->SetOrgname().SetLineage(lineage);
            }
        }

        CRef<CSeqdesc> descr(new CSeqdesc);
        descr->SetSource(*bio_src);
        bioseq.SetDescr().Set().push_front(descr);
    }

    if (count == 0) {
        FtaErrPost(SEV_WARNING, ERR_ORGANISM_NoOrganism, "No Organism data in Embl format file");
        return;
    }
}

/**********************************************************/
static void EmblGetDivision(IndexblkPtr ibp, const DataBlk& entry)
{
    const char* p;
    const char* q;

    p = StringChr(entry.mBuf.ptr, ';');
    if (! p)
        p = entry.mBuf.ptr;
    else {
        q = StringChr(p + 1, ';');
        if (q)
            p = q;
    }
    while (*p == ' ' || *p == ';')
        p++;

    StringNCpy(ibp->division, p, 3);
    ibp->division[3] = '\0';
}

/**********************************************************/
static void EmblGetDivisionNewID(IndexblkPtr ibp, const DataBlk& entry)
{
    const char* p;
    Int4        i;

    for (i = 0, p = entry.mBuf.ptr; *p != '\0' && i < 4; p++)
        if (*p == ';' && p[1] == ' ')
            i++;

    while (*p == ' ')
        p++;

    i = fta_StringMatch(ParFlat_Embl_dataclass_array, p);
    if (i < 0) {
        p = StringChr(p, ';');
        if (p)
            for (p++; *p == ' ';)
                p++;
    } else if (i == 0)
        p = "CON";

    if (! p)
        p = "   ";

    StringNCpy(ibp->division, p, 3);
    ibp->division[3] = '\0';
}


CRef<CSeq_entry> CEmbl2Asn::xGetEntry()
{
    CRef<CSeq_entry> pResult;
    if (mParser.curindx >= mParser.indx) {
        return pResult;
    }

    bool        seq_long = false;
    Int4        i        = mParser.curindx;
    IndexblkPtr ibp      = mParser.entrylist[i];

    err_install(ibp, mParser.accver);
    if (! ibp->drop) {
        string sEntry(LoadEntry(&mParser, ibp));
        if (sEntry.empty()) {
            FtaDeletePrefix(PREFIX_LOCUS | PREFIX_ACCESSION);
            NCBI_THROW(CException, eUnknown, "Unable to load entry");
        }
        auto  pEntry(MakeEntry(std::move(sEntry)));
        auto& entry = *pEntry;
        auto  ebp   = entry.GetEntryData();
        char* ptr   = entry.mBuf.ptr; /* points to beginning of the
                                       memory line */
        char* eptr  = ptr + entry.mBuf.len;
        Int2  curkw = ParFlat_ID;

        // TODO: below is a potentially infinite cycle!!!!
        while (curkw != ParFlatEM_END) {
            /* ptr points to current keyword's memory line */
            ptr = GetEmblBlock(ebp->chain, ptr, &curkw, mParser.format, eptr);
        }

        if (ibp->embl_new_ID)
            EmblGetDivisionNewID(ibp, entry);
        else
            EmblGetDivision(ibp, entry);

        if (StringEqu(ibp->division, "TSA")) {
            if (ibp->tsa_allowed == false)
                FtaErrPost(SEV_WARNING, ERR_TSA_UnexpectedPrimaryAccession, "The record with accession \"{}\" is not expected to have a TSA division code.", ibp->acnum);
            ibp->is_tsa = true;
        }

        if (! CheckEmblContigEverywhere(ibp, mParser.source)) {
            ibp->drop = true;
            FtaErrPost(SEV_ERROR, ERR_ENTRY_Skipped, "Entry skipped: \"{}|{}\".", ibp->locusname, ibp->acnum);
            mParser.curindx++;
            return pResult;
        }

        if (ptr >= eptr) {
            ibp->drop = true;
            FtaErrPost(SEV_ERROR, ERR_FORMAT_MissingEnd, "Missing end of the entry, entry dropped");
            FtaErrPost(SEV_ERROR, ERR_ENTRY_Skipped, "Entry skipped: \"{}|{}\".", ibp->locusname, ibp->acnum);
            mParser.curindx++;
            return pResult;
        }
        GetEmblSubBlock(ibp->bases, mParser.source, entry);

        CRef<CBioseq> bioseq = CreateEntryBioseq(&mParser);
        AddNIDSeqId(*bioseq, entry, ParFlat_NI, ParFlat_COL_DATA_EMBL, mParser.source);

        ebp->seq_entry.Reset(new CSeq_entry);
        ebp->seq_entry->SetSeq(*bioseq);

        if (! mParser.accver) {
            GetReleaseInfo(entry);
        }

        if (! s_GetEmblInst(&mParser, entry, GetDNAConvTable())) {
            ibp->drop = true;
            FtaErrPost(SEV_REJECT, ERR_SEQUENCE_BadData, "Bad sequence data, entry dropped");
            FtaErrPost(SEV_ERROR, ERR_ENTRY_Skipped, "Entry skipped: \"{}|{}\".", ibp->locusname, ibp->acnum);
            mParser.curindx++;
            return pResult;
        }


        FakeEmblBioSources(entry, *bioseq);
        GetScope().AddBioseq(*bioseq);

        LoadFeat(&mParser, entry, *bioseq);

        if (! bioseq->IsSetAnnot() && ibp->drop) {
            FtaErrPost(SEV_ERROR, ERR_ENTRY_Skipped, "Entry skipped: \"{}|{}\".", ibp->locusname, ibp->acnum);
            mParser.curindx++;
            return pResult;
        }

        GetEmblDescr(&mParser, entry, *bioseq);

        if (ibp->drop) {
            FtaErrPost(SEV_ERROR, ERR_ENTRY_Skipped, "Entry skipped: \"{}|{}\".", ibp->locusname, ibp->acnum);
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

        if (entry.mpQscore.empty() && mParser.accver) {
            if (mParser.ff_get_qscore)
                entry.mpQscore = (*mParser.ff_get_qscore)(ibp->acnum, ibp->vernum);
            else if (mParser.ff_get_qscore_pp)
                entry.mpQscore = (*mParser.ff_get_qscore_pp)(ibp->acnum, ibp->vernum, &mParser);
            if (mParser.qsfd && ibp->qslength > 0)
                entry.mpQscore = GetQSFromFile(mParser.qsfd, ibp);
        }

        if (! QscoreToSeqAnnot(entry.mpQscore, *bioseq, ibp->acnum, ibp->vernum, false, false)) {
            if (mParser.ign_bad_qs == false) {
                ibp->drop = true;
                FtaErrPost(SEV_ERROR, ERR_QSCORE_FailedToParse, "Error while parsing QScore. Entry dropped.");
                FtaErrPost(SEV_ERROR, ERR_ENTRY_Skipped, "Entry skipped: \"{}|{}\".", ibp->locusname, ibp->acnum);
                mParser.curindx++;
                return pResult;
            }
            FtaErrPost(SEV_ERROR, ERR_QSCORE_FailedToParse, "Error while parsing QScore.");
        }

        entry.mpQscore.clear();

        /* add PatentSeqId if patent is found in reference
         */
        if (ibp->psip.NotEmpty()) {
            CRef<CSeq_id> id(new CSeq_id);
            id->SetPatent(*ibp->psip);
            bioseq->SetId().push_back(id);
            ibp->psip.Reset();
        }

        if (no_reference(*bioseq) && mParser.debug == false) {
            ibp->drop = true;
            FtaErrPost(SEV_ERROR, ERR_REFERENCE_No_references, "No reference for the entry, entry dropped");
            FtaErrPost(SEV_ERROR, ERR_ENTRY_Skipped, "Entry skipped: \"{}|{}\".", ibp->locusname, ibp->acnum);
            mParser.curindx++;
            return pResult;
        }

        TEntryList seq_entries;
        seq_entries.push_back(ebp->seq_entry);
        ebp->seq_entry.Reset();

        if (mParser.limit != 0 && ibp->bases > (size_t)mParser.limit) {
            if (ibp->htg == 4 || ibp->htg == 1 || ibp->htg == 2) {
                FtaErrPost(SEV_WARNING, ERR_ENTRY_LongHTGSSequence, "HTGS Phase 0/1/2 sequence {}|{} exceeds length limit {}: entry has been processed regardless of this problem", ibp->locusname, ibp->acnum, mParser.limit);
            } else
                seq_long = true;
        }

        pResult = OutputEmblAsn(seq_long, &mParser, seq_entries);
        if (! pResult) {
            ibp->drop = true;
        }
        else if (! ibp->drop) {
            mTotals.Succeeded++;
        }
        GetScope().ResetDataAndHistory();
    } /* if, not drop */

    if (! ibp->drop) {
        FtaErrPost(SEV_INFO, ERR_ENTRY_Parsed, "OK - entry parsed successfully: \"{}|{}\".", ibp->locusname, ibp->acnum);
    } else {
        FtaErrPost(SEV_ERROR, ERR_ENTRY_Skipped, "Entry skipped: \"{}|{}\".", ibp->locusname, ibp->acnum);
    }

    mParser.curindx++;
    return pResult;
}


void CEmbl2Asn::PostTotals()
{
    FtaErrPost(SEV_INFO, ERR_ENTRY_ParsingComplete, 
            "COMPLETED : SUCCEEDED = %d; SKIPPED = %d.", mTotals.Succeeded, mParser.indx-mTotals.Succeeded);

}


/**********************************************************/
string_view GetEmblDiv(Uint1 num)
{
    if (num > 15)
        return {};
    return ParFlat_Embl_DIV_array[num];
}

/**********************************************************/
CRef<CEMBL_block> XMLGetEMBLBlock(ParserPtr pp, const char* entry, CMolInfo& mol_info, string& gbdiv, CBioSource* bio_src, TStringList& dr_ena, TStringList& dr_biosample, TStringList& dr_pubmed)
{
    CRef<CEMBL_block> embl(new CEMBL_block),
        ret;

    IndexblkPtr ibp;
    char*       bptr;

    CEMBL_block::EDiv div;

    bool pat_ref = false;
    bool est_kwd = false;
    bool sts_kwd = false;
    bool gss_kwd = false;
    bool htc_kwd = false;
    bool fli_kwd = false;
    bool wgs_kwd = false;
    bool tpa_kwd = false;
    bool env_kwd = false;
    bool mga_kwd = false;
    bool tsa_kwd = false;
    bool tls_kwd = false;
    bool cancelled;

    char* tempdiv;
    Int4  i;
    Char  dataclass[4];

    ibp = pp->entrylist[pp->curindx];

    bool if_cds = XMLCheckCDS(entry, ibp->xip);

    if (ibp->psip.NotEmpty())
        pat_ref = true;

    if (! ibp->keywords.empty()) {
        embl->SetKeywords().swap(ibp->keywords);
        ibp->keywords.clear();
    } else
        XMLGetKeywords(entry, ibp->xip, embl->SetKeywords());

    for (const string& key : embl->GetKeywords()) {
        fta_keywords_check(key, &est_kwd, &sts_kwd, &gss_kwd, &htc_kwd, &fli_kwd, &wgs_kwd, &tpa_kwd, &env_kwd, &mga_kwd, &tsa_kwd, &tls_kwd);
    }

    if (ibp->env_sample_qual == false && env_kwd) {
        FtaErrPost(SEV_REJECT, ERR_KEYWORD_ENV_NoMatchingQualifier, "This record utilizes the ENV keyword, but there are no /environmental_sample qualifiers among its source features.");
        return ret;
    }

    bptr         = StringSave(XMLFindTagValue(entry, ibp->xip, INSDSEQ_DIVISION));
    div          = static_cast<CEMBL_block::TDiv>(fta_StringMatch(ParFlat_Embl_DIV_array, bptr));
    dataclass[0] = '\0';
    if (bptr) {
        bptr[3] = '\0';
        StringCpy(dataclass, bptr);
    }
    if (div < 0) {
        FtaErrPost(SEV_REJECT, ERR_DIVISION_UnknownDivCode, "Unknown division code \"{}\" found in Embl flatfile. Record rejected.", bptr);
        if (bptr)
            MemFree(bptr);
        return ret;
    }

    if (bptr)
        MemFree(bptr);

    /* Embl has recently (7-19-93, email) decided to change the name of
     * its "UNA"==10 division to "UNC"==16 (for "unclassified")
     */
    if (div == 16)
        div = CEMBL_block::eDiv_una;

    StringCpy(ibp->division, ParFlat_GBDIV_array[div]);

    /* 06-10-96 new HUM division replaces the PRI
     * it's temporarily mapped to 'other' in asn.1 embl-block.
     * Divisions GSS, HUM, HTG, CON, ENV and MUS are mapped to other.
     */
    int thtg = (div == 18) ? CEMBL_block::eDiv_pri : div;
    gbdiv    = ParFlat_GBDIV_array[thtg];

    if (div <= CEMBL_block::eDiv_sts)
        embl->SetDiv(div);

    const char* p = gbdiv.c_str();
    if (ibp->is_tpa &&
        (StringEqu(p, "EST") || StringEqu(p, "GSS") ||
         StringEqu(p, "PAT") || StringEqu(p, "HTG"))) {
        FtaErrPost(SEV_REJECT, ERR_DIVISION_BadTPADivcode, "Division code \"{}\" is not legal for TPA records. Entry dropped.", p);
        return ret;
    }

    if (ibp->is_tsa && ! StringEqu(p, "TSA")) {
        FtaErrPost(SEV_REJECT, ERR_DIVISION_BadTSADivcode, "Division code \"{}\" is not legal for TSA records. Entry dropped.", p);
        return ret;
    }

    cancelled = IsCancelled(embl->GetKeywords());

    if (div == 19) /* HTG */
    {
        if (! HasHtg(embl->GetKeywords())) {
            FtaErrPost(SEV_ERROR, ERR_DIVISION_MissingHTGKeywords, "Division is HTG, but entry lacks HTG-related keywords. Entry dropped.");
            return ret;
        }
        tempdiv = StringSave("HTG");
    } else
        tempdiv = nullptr;

    fta_check_htg_kwds(embl->SetKeywords(), ibp, mol_info);

    XMLDefVsHTGKeywords(mol_info.GetTech(), entry, ibp->xip, cancelled);
    if ((mol_info.GetTech() == CMolInfo::eTech_htgs_0 || mol_info.GetTech() == CMolInfo::eTech_htgs_1 ||
         mol_info.GetTech() == CMolInfo::eTech_htgs_2) &&
        ! gbdiv.empty()) {
        gbdiv.clear();
    }

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
        FtaErrPost(SEV_REJECT, ERR_KEYWORD_MissingTSA, "This is apparently a TSA record, but it lacks the required \"TPA\" and/or \"Transcriptome Shotgun Assembly\" keywords. Entry dropped.");
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

    CMolInfo::TTech tech = mol_info.GetTech();
    if (tech == CMolInfo::eTech_htgs_0 || tech == CMolInfo::eTech_htgs_1 ||
        tech == CMolInfo::eTech_htgs_2 || tech == CMolInfo::eTech_htgs_3) {
        RemoveHtgPhase(embl->SetKeywords());
    }

    if (auto kw = XMLConcatSubTags(entry, ibp->xip, INSDSEQ_KEYWORDS, ';')) {
        if (! est_kwd && kw->find("EST") != string::npos) {
            FtaErrPost(SEV_WARNING, ERR_KEYWORD_ESTSubstring, "Keyword {} has substring EST, but no official EST keywords found", *kw);
        }
        if (! sts_kwd && kw->find("STS") != string::npos) {
            FtaErrPost(SEV_WARNING, ERR_KEYWORD_STSSubstring, "Keyword {} has substring STS, but no official STS keywords found", *kw);
        }
        if (! gss_kwd && kw->find("GSS") != string::npos) {
            FtaErrPost(SEV_WARNING, ERR_KEYWORD_GSSSubstring, "Keyword {} has substring GSS, but no official GSS keywords found", *kw);
        }
    }
    if (! ibp->is_contig) {
        bool            drop = false;
        CMolInfo::TTech tech = mol_info.GetTech();

        check_div(ibp->is_pat, pat_ref, est_kwd, sts_kwd, gss_kwd, if_cds, gbdiv, &tech, ibp->bases, pp->source, drop);
        if (tech != CMolInfo::eTech_unknown)
            mol_info.SetTech(tech);
        else
            mol_info.ResetTech();

        if (drop) {
            return ret;
        }
    } else if (gbdiv == "CON") {
        gbdiv.clear();
    }

    bool is_htc_div = (gbdiv == "HTC");
    bool has_htc    = HasHtc(embl->GetKeywords());

    if (is_htc_div && ! has_htc) {
        FtaErrPost(SEV_ERROR, ERR_DIVISION_MissingHTCKeyword, "This record is in the HTC division, but lacks the required HTC keyword.");
        return ret;
    }
    if (! is_htc_div && has_htc) {
        FtaErrPost(SEV_ERROR, ERR_DIVISION_InvalidHTCKeyword, "This record has the special HTC keyword, but is not in HTC division. If this record has graduated out of HTC, then the keyword should be removed.");
        return ret;
    }

    if (is_htc_div) {
        char* r = StringSave(XMLFindTagValue(entry, ibp->xip, INSDSEQ_MOLTYPE));
        if (r) {
            p = r;
            if (*r == 'm' || *r == 'r')
                p = r + 1;
            else if (StringEquN(r, "pre-", 4))
                p = r + 4;
            else if (StringEquN(r, "transcribed ", 12))
                p = r + 12;

            if (! fta_StartsWith(p, "RNA"sv)) {
                FtaErrPost(SEV_ERROR, ERR_DIVISION_HTCWrongMolType, "All HTC division records should have a moltype of pre-RNA, mRNA or RNA.");
                MemFree(r);
                return ret;
            }
            MemFree(r);
        }
    }

    if (fli_kwd)
        mol_info.SetTech(CMolInfo::eTech_fli_cdna);

    /* will be used in flat file database
     */
    if (! gbdiv.empty()) {
        if (gbdiv == "EST") {
            ibp->EST = true;
            mol_info.SetTech(CMolInfo::eTech_est);
        } else if (gbdiv == "STS") {
            ibp->STS = true;
            mol_info.SetTech(CMolInfo::eTech_sts);
        } else if (gbdiv == "GSS") {
            ibp->GSS = true;
            mol_info.SetTech(CMolInfo::eTech_survey);
        } else if (gbdiv == "HTC") {
            ibp->HTC = true;
            mol_info.SetTech(CMolInfo::eTech_htc);
            gbdiv.clear();
        } else if ((gbdiv == "SYN") && bio_src &&
                   bio_src->IsSetOrigin() && bio_src->GetOrigin() == CBioSource::eOrigin_synthetic) {
            gbdiv.clear();
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
        fta_remove_keywords(mol_info.GetTech(), embl->SetKeywords());

    if (ibp->is_tpa)
        fta_remove_tpa_keywords(embl->SetKeywords());

    if (ibp->is_tsa)
        fta_remove_tsa_keywords(embl->SetKeywords(), pp->source);

    if (ibp->is_tls)
        fta_remove_tls_keywords(embl->SetKeywords(), pp->source);

    ibp->wgssec[0] = '\0';
    GetExtraAccession(ibp, pp->allow_uwsec, pp->source, embl->SetExtra_acc());


    CRef<CDate_std> std_creation_date, std_update_date;
    if (char* p = StringSave(XMLFindTagValue(entry, ibp->xip, INSDSEQ_CREATE_DATE))) {
        std_creation_date = GetUpdateDate(p, pp->source);
        embl->SetCreation_date().SetStd(*std_creation_date);
        MemFree(p);
    }
    if (char* p = StringSave(XMLFindTagValue(entry, ibp->xip, INSDSEQ_UPDATE_DATE))) {
        std_update_date = GetUpdateDate(p, pp->source);
        embl->SetUpdate_date().SetStd(*std_update_date);
        MemFree(p);
    }

    if (std_update_date.Empty() && std_creation_date.NotEmpty())
        embl->SetUpdate_date().SetStd(*std_creation_date);

    GetEmblBlockXref(DataBlk(), &ibp->xip, entry, dr_ena, dr_biosample, &ibp->drop, *embl, dr_pubmed, pp->ignore_pubmed_dr);

    if (StringEqu(dataclass, "ANN") || StringEqu(dataclass, "CON")) {
        if (StringLen(ibp->acnum) == 8 && fta_StartsWith(ibp->acnum, "CT"sv)) {
            bool found = false;
            for (const string& acc : embl->SetExtra_acc()) {
                if (fta_if_wgs_acc(acc) == 0 &&
                    (acc[0] == 'C' || acc[0] == 'U')) {
                    found = true;
                    break;
                }
            }
            if (found)
                mol_info.SetTech(CMolInfo::eTech_wgs);
        }
    }

    return embl;
}

END_NCBI_SCOPE
