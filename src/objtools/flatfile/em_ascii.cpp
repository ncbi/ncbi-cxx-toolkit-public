/* em_ascii.c
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
 * File Name:  em_ascii.c
 *
 * Author: Karl Sirotkin, Hsiu-Chuan Chen
 *
 * File Description:
 * -----------------
 *      Preprocessing embl from blocks in memory to asn.
 * Build EMBL format entry block.
 *
 */
#include <ncbi_pch.hpp>

#include "ftacpp.hpp"

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
#include "ftanet.h"
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

#ifdef THIS_FILE
#    undef THIS_FILE
#endif
#define THIS_FILE "em_ascii.cpp"


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

/* For new stile of ID line in EMBL data check the "data class"
 * field first to figure out division code
 */
static const char *ParFlat_Embl_dataclass_array[] = {
    "ANN", "CON", "PAT", "EST", "GSS", "HTC", "HTG", "STS", "TSA", NULL
};

/* order by EMBL-block in asn.all
 */
static const char *ParFlat_Embl_DIV_array[] = {
    "FUN", "INV", "MAM", "ORG", "PHG", "PLN", "PRI", "PRO", "ROD",
    "SYN", "UNA", "VRL", "VRT", "PAT", "EST", "STS", "UNC", "GSS",
    "HUM", "HTG", "HTC", "CON", "ENV", "MUS", "TGN", "TSA", NULL
};

/* correspond "DIV" genbank string. Must have the same number
 * of elements !
 */
static const char *ParFlat_GBDIV_array[] = {
    "PLN", "INV", "MAM", "UNA", "PHG", "PLN", "PRI", "BCT", "ROD",
    "SYN", "UNA", "VRL", "VRT", "PAT", "EST", "STS", "UNA", "GSS",
    "PRI", "HTG", "HTC", "CON", "ENV", "ROD", "SYN", "TSA", NULL
};

static const char *ParFlat_DBname_array[] = {
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
    NULL
};

static const char *ParFlat_DRname_array[] = {
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
    NULL
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
static void GetEmblDate(Parser::ESource source, const DataBlk& entry,
                        CRef<objects::CDate_std>& crdate, CRef<objects::CDate_std>& update)
{
    char* offset;
    char* eptr;
    size_t  len;

    crdate.Reset();
    update.Reset();
    offset = xSrchNodeType(entry, ParFlat_DT, &len);
    if(offset == NULL)
        return;

    eptr = offset + len;
    crdate = GetUpdateDate(offset + ParFlat_COL_DATA_EMBL, source);
    while(offset < eptr)
    {
        offset = SrchTheChar(offset, eptr,'\n');
        if(offset == NULL)
            break;

        offset++;                       /* newline */
        if(StringNCmp(offset, "DT", 2) == 0)
        {
            update = GetUpdateDate(offset + ParFlat_COL_DATA_EMBL,
                                   source);
            break;
        }
    }
    if (update.Empty())
    {
        update.Reset(new objects::CDate_std);
        update->SetDay(crdate->GetDay());
        update->SetMonth(crdate->GetMonth());
        update->SetYear(crdate->GetYear());
    }
}

/**********************************************************/
static bool OutputEmblAsn(bool seq_long, ParserPtr pp, TEntryList& seq_entries)
{
    DealWithGenes(seq_entries, pp);

    if (seq_entries.empty())
    {
        GetScope().ResetDataAndHistory();
        return false;
    }

    fta_find_pub_explore(pp, seq_entries);

    /* change qual "citation" on features to SeqFeat.cit find citation
     * in the list by serial_number. If serial number not found remove
     * /citation
     */
    ProcessCitations(seq_entries);

    if (pp->convert)
    {
        if (pp->cleanup <= 1)
        {
            FinalCleanup(seq_entries);

            if (pp->qamode && !seq_entries.empty())
                fta_remove_cleanup_user_object(*(*seq_entries.begin()));
        }

        MaybeCutGbblockSource(seq_entries);
    }

    EntryCheckDivCode(seq_entries, pp);

    if (pp->xml_comp)
        fta_set_strandedness(seq_entries);

    if (fta_EntryCheckGBBlock(seq_entries))
    {
        ErrPostStr(SEV_WARNING, ERR_ENTRY_GBBlock_not_Empty,
                   "Attention: GBBlock is not empty");
    }

    if (pp->qamode)
    {
        fta_sort_descr(seq_entries);
        fta_sort_seqfeat_cit(seq_entries);
    }

    if(pp->citat)
    {
        StripSerialNumbers(seq_entries);
    }

    PackEntries(seq_entries);
    CheckDupDates(seq_entries);

    if(seq_long)
    {
        ErrPostEx(SEV_REJECT, ERR_ENTRY_LongSequence,
                  "Sequence %s|%s is longer than limit %ld",
                  pp->entrylist[pp->curindx]->locusname,
                  pp->entrylist[pp->curindx]->acnum, pp->limit);
    }
    else
    {
        pp->entries.splice(pp->entries.end(), seq_entries);
    }

    seq_entries.clear();
    GetScope().ResetDataAndHistory();

    return true;
}

/**********************************************************
 *
 *   static ValNodePtr GetXrefObjId(vnp, str):
 *
 *      A ValNode which points to a ObjectId.
 *
 **********************************************************/
static void SetXrefObjId(objects::CEMBL_xref& xref, const std::string& str)
{
    if (str.empty())
        return;

    objects::CEMBL_xref::TId& ids = xref.SetId();

    bool found = false;
    ITERATE(objects::CEMBL_xref::TId, id, ids)
    {
        if ((*id)->IsStr() && (*id)->GetStr() == str)
        {
            found = true;
            break;
        }
    }

    if (found)
        return;

    CRef<objects::CObject_id> obj_id(new objects::CObject_id);
    obj_id->SetStr(str);

    ids.push_back(obj_id);
}

/**********************************************************
 *
 *   static void GetEmblBlockXref(entry, xip,
 *                                chentry, dr_ena,
 *                                dr_biosample,
 *                                drop):
 *
 *      Return a list of EMBLXrefPtr, one EMBLXrefPtr per
 *   type (DR) line.
 *
 **********************************************************/
static void GetEmblBlockXref(const DataBlk& entry, XmlIndexPtr xip,
                             char* chentry, TStringList& dr_ena,
                             TStringList& dr_biosample, unsigned char* drop,
                             objects::CEMBL_block& embl)
{
    const char    **b;

    const char    *drline;

    char*       bptr;
    char*       eptr;
    char*       ptr;
    char*       xref;
    char*       p;
    char*       q;

    bool       valid_biosample;
    bool       many_biosample;
    size_t     len;

    Int2          col_data;
    Int2          code;

    objects::CEMBL_block::TXref new_xrefs;

    if(xip == NULL)
    {
        bptr = xSrchNodeType(entry, ParFlat_DR, &len);
        col_data = ParFlat_COL_DATA_EMBL;
        xref = NULL;
    }
    else
    {
        bptr = XMLFindTagValue(chentry, xip, INSDSEQ_DATABASE_REFERENCE);
        if(bptr != NULL)
            len = StringLen(bptr);
        col_data = 0;
        xref = bptr;
    }

    if(bptr == NULL)
        return;

    for(eptr = bptr + len; bptr < eptr; bptr = ptr)
    {
        drline = bptr;
        bptr += col_data;       /* bptr points to database_identifier */
        code = fta_StringMatch(ParFlat_DBname_array, bptr);

        std::string name;
        if(code < 0)
        {
            ptr = SrchTheChar(bptr, eptr, ';');
            name.assign(bptr, ptr);

            if (NStr::EqualNocase(name, "MD5"))
            {
                while(ptr < eptr)
                {
                    if (NStr::Equal(ptr, 0, 2, "DR"))
                        break;

                    ptr = SrchTheChar(ptr, eptr, '\n');
                    if(*ptr == '\n')
                        ptr++;
                }
                continue;
            }

            for (b = ParFlat_DRname_array; *b != NULL; b++)
            {
                if (NStr::EqualNocase(name, *b))
                    break;
            }

            if(*b == NULL)
                ErrPostEx(SEV_WARNING, ERR_DRXREF_UnknownDBname,
                          "Encountered a new/unknown database name in DR line: \"%s\".",
                          name.c_str());
            else if (NStr::EqualNocase(*b, "UNIPROT/SWISS-PROT"))
            {
                name = "UniProtKB/Swiss-Prot";
            }
            else if (NStr::EqualNocase(*b, "UNIPROT/TREMBL"))
            {
                name = "UniProtKB/TrEMBL";
            }
        }

        bptr = PointToNextToken(bptr);  /* bptr points to primary_identifier */
        p = SrchTheChar(bptr, eptr, '\n');
        ptr = SrchTheChar(bptr, eptr, ';');

        std::string id, id1;

        if (ptr != NULL && ptr < p)
        {
            id.assign(bptr, ptr);
            CleanTailNoneAlphaCharInString(id);

            bptr = PointToNextToken(ptr);       /* points to
                                                   secondary_identifier */
        }
        if(p != NULL)
        {
            id1.assign(bptr, p);
            CleanTailNoneAlphaCharInString(id1);
        }

        if (id.empty())
        {
            id = id1;
            id1.clear();
        }

        if(name == "BioSample" && !id.empty())
        {
            many_biosample = (!id.empty() && !id1.empty());
            valid_biosample = fta_if_valid_biosample(id.c_str(), false);
            if(!id1.empty() && fta_if_valid_biosample(id1.c_str(), false) == false)
                valid_biosample = false;
            if(many_biosample || !valid_biosample)
            {
                q = NULL;
                if(drline == NULL)
                    drline = "[Empty]";
                else
                {
                    q = StringChr(drline, '\n');
                    if(q != NULL)
                        *q = '\0';
                }
                if(many_biosample)
                    ErrPostEx(SEV_REJECT, ERR_DRXREF_InvalidBioSample,
                              "Multiple BioSample ids provided in the same DR line: \"%s\".",
                              drline);
                if(!valid_biosample)
                    ErrPostEx(SEV_REJECT, ERR_DRXREF_InvalidBioSample,
                              "Invalid BioSample id(s) provided in DR line: \"%s\".",
                              drline);
                *drop = 1;
                if(q != NULL)
                    *q = '\n';
            }
            else
            {
                bool found = false;
                ITERATE(TStringList, val, dr_biosample)
                {
                    if (*val == id)
                    {
                        found = true;
                        break;
                    }
                }

                if (found)
                {
                    ErrPostEx(SEV_WARNING, ERR_DRXREF_DuplicatedBioSamples,
                              "Duplicated BioSample ids found within DR lines contents: \"%s\".",
                              id.c_str());
                }
                else
                {
                    dr_biosample.push_back(id);
                }
            }
        }
        else if(name == "ENA" && !id.empty() && fta_if_valid_sra(id.c_str(), false))
        {
            if (!id.empty() && !id1.empty())
            {
                q = NULL;
                if(drline == NULL)
                    drline = "[Empty]";
                else
                {
                    q = StringChr(drline, '\n');
                    if(q != NULL)
                        *q = '\0';
                }
                ErrPostEx(SEV_REJECT, ERR_DRXREF_InvalidSRA,
                          "Multiple possible SRA ids provided in the same DR line: \"%s\".",
                          drline);
                *drop = 1;
                if(q != NULL)
                    *q = '\n';
            }
            else
            {
                bool found = false;
                ITERATE(TStringList, val, dr_ena)
                {
                    if (*val == id)
                    {
                        found = true;
                        break;
                    }
                }

                if (found)
                {
                    ErrPostEx(SEV_WARNING, ERR_DRXREF_DuplicatedSRA,
                              "Duplicated Sequence Read Archive ids found within DR lines contents: \"%s\".",
                              id.c_str());
                }
                else
                {
                    dr_ena.push_back(id);
                }
            }
        }
        else
        {
            CRef<objects::CEMBL_xref> new_xref(new objects::CEMBL_xref);

            if (code != -1)
                new_xref->SetDbname().SetCode(static_cast<objects::CEMBL_dbname::ECode>(code));
            else
                new_xref->SetDbname().SetName(name);

            if(!id.empty())
                SetXrefObjId(*new_xref, id);

            if (!id1.empty())
                SetXrefObjId(*new_xref, id1);

            new_xrefs.push_back(new_xref);
        }

        ptr = p + 1;

        if(xip != NULL)
            continue;

        /* skip "XX" line
         */
        while(ptr < eptr)
        {
            if(StringNCmp(ptr, "DR", 2) == 0)
                break;

            ptr = SrchTheChar(ptr, eptr, '\n');
            if(*ptr == '\n')
                ptr++;
        }
    }

    if(xref != NULL)
        MemFree(xref);

    if (!new_xrefs.empty())
        embl.SetXref().swap(new_xrefs);
}

static objects::CTextseq_id& SetTextIdRef(objects::CSeq_id& id)
{
    static objects::CTextseq_id noTextId;

    switch (id.Which())
    {
    case objects::CSeq_id::e_Genbank:
        return id.SetGenbank();
    case objects::CSeq_id::e_Embl:
        return id.SetEmbl();
    case objects::CSeq_id::e_Pir:
        return id.SetPir();
    case objects::CSeq_id::e_Swissprot:
        return id.SetSwissprot();
    case objects::CSeq_id::e_Other:
        return id.SetOther();
    case objects::CSeq_id::e_Ddbj:
        return id.SetDdbj();
    case objects::CSeq_id::e_Prf:
        return id.SetPrf();
    case objects::CSeq_id::e_Tpg:
        return id.SetTpg();
    case objects::CSeq_id::e_Tpe:
        return id.SetTpe();
    case objects::CSeq_id::e_Tpd:
        return id.SetTpd();
    case objects::CSeq_id::e_Gpipe:
        return id.SetGpipe();
    case objects::CSeq_id::e_Named_annot_track:
        return id.SetNamed_annot_track();
    default:
        ; // do nothing
    }

    return noTextId;
}

/**********************************************************/
static void GetReleaseInfo(const DataBlk& entry, bool accver)
{
    EntryBlkPtr  ebp;

    char*      offset;
    char*      bptr;
    char*      eptr;

    size_t       len;

    if(accver)
        return;

    ebp = (EntryBlkPtr) entry.mpData;
    objects::CBioseq& bioseq = ebp->seq_entry->SetSeq();
    objects::CTextseq_id& id = SetTextIdRef(*(*(bioseq.SetId().begin())));

    offset = xSrchNodeType(entry, ParFlat_DT, &len);
    if(offset == NULL)
        return;

    eptr = offset + len;
    offset = SrchTheChar(offset, eptr, '\n');
    if(offset == NULL)
        return;

    bptr = SrchTheStr(offset, eptr, "Version");
    if(bptr == NULL)
        return;

    bptr = PointToNextToken(bptr);      /* bptr points to next token */

    id.SetVersion(NStr::StringToInt(bptr, NStr::fAllowTrailingSymbols));
}

/**********************************************************
 *
 *   static OrgRefPtr GetEmblOrgRef(dbp):
 *
 *      >= 1 OS per entry.
 *
 **********************************************************/
static CRef<objects::COrg_ref> GetEmblOrgRef(DataBlkPtr dbp)
{
    char*   bptr;
    char*   eptr;
    char*   ptr;
    char*   str;
    char*   taxname;

    bptr = dbp->mOffset;
    eptr = bptr + dbp->len;

    taxname = str = (char*) MemNew(dbp->len + 1);

    /* get block of OS line data
     */
    while(bptr < eptr && (ptr = SrchTheChar(bptr, eptr, '\n')) != NULL)
    {
        if(StringNCmp(bptr, "XX", 2) != 0)
        {
            bptr += ParFlat_COL_DATA_EMBL;

            if(StringLen(taxname) > 0)
                str = StringMove(str, " ");

            size_t size = ptr - bptr;
            MemCpy(str, bptr, size);
            str += size;
        }

        bptr = ptr + 1;
    }

    CRef<objects::COrg_ref> org_ref;
    if(taxname[0] == '\0')
    {
        MemFree(taxname);
        return org_ref;
    }

    org_ref.Reset(new objects::COrg_ref);
    org_ref->SetTaxname(taxname);

    ptr = StringChr(taxname, '(');
    if(ptr != NULL && ptr > taxname)
    {
        for(ptr--; *ptr == ' ' || *ptr == '\t'; ptr--)
            if(ptr == taxname)
                break;
        if(*ptr != ' ' && *ptr != '\t')
            ptr++;
        if(ptr > taxname)
        {
            *ptr = '\0';
            org_ref->SetCommon(taxname);
        }
    }

    MemFree(taxname);
        
    return org_ref;
}

/**********************************************************/
static void CheckEmblContigEverywhere(IndexblkPtr ibp, Parser::ESource source)
{
    bool condiv = (NStr::CompareNocase(ibp->division, "CON") == 0);

    if(condiv && ibp->segnum != 0)
    {
        ErrPostEx(SEV_ERROR, ERR_DIVISION_ConDivInSegset,
                  "Use of the CON division is not allowed for members of segmented set : %s|%s. Entry skipped.",
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
    else if(condiv && !ibp->is_contig && !ibp->origin)
    {
        ErrPostEx(SEV_ERROR, ERR_FORMAT_MissingContigFeature,
                  "No CONTIG data in GenBank format file, entry dropped.");
        ibp->drop = 1;
    }
    else if(condiv && !ibp->is_contig && ibp->origin)
    {
        ErrPostEx(SEV_WARNING, ERR_DIVISION_ConDivLacksContig,
                  "Division is CON, but CONTIG data have not been found.");
    }
}

/**********************************************************/
bool GetEmblInstContig(const DataBlk& entry, objects::CBioseq& bioseq, ParserPtr pp)
{
    DataBlkPtr dbp;

    char*    p;
    char*    q;
    char*    r;
    bool    locmap;
    bool    sitemap;

    bool    allow_crossdb_featloc;
    int        numerr;

    dbp = TrackNodeType(entry, ParFlat_CO);
    if(dbp == NULL || dbp->mOffset == NULL)
        return true;

    Int4 i = static_cast<Int4>(dbp->len) - ParFlat_COL_DATA_EMBL;
    if(i <= 0)
        return false;

    p = (char*) MemNew(i + 1);
    StringNCpy(p, &dbp->mOffset[ParFlat_COL_DATA_EMBL], i);
    p[i-1] = '\0';
    for(q = p; *q != '\0'; q++)
    {
        if(*q == '\t')
            *q = ' ';
        else if(*q == '\n')
        {
            *q = ' ';
            if(q[1] == 'C' && q[2] == 'O' && q[3] == ' ')
            {
                q[1] = ' ';
                q[2] = ' ';
            }
        }
    }
    for(q = p, r = p; *q != '\0'; q++)
        if(*q != ' ')
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

    if (loc.NotEmpty() && loc->IsMix())
    {
        allow_crossdb_featloc = pp->allow_crossdb_featloc;
        pp->allow_crossdb_featloc = true;

        TSeqLocList locs;
        locs.push_back(loc);

        i = fta_fix_seq_loc_id(locs, pp, p, NULL, true);
        if(i > 999)
            fta_create_far_fetch_policy_user_object(bioseq, i);
        pp->allow_crossdb_featloc = allow_crossdb_featloc;

        XGappedSeqLocsToDeltaSeqs(loc->GetMix(), bioseq.SetInst().SetExt().SetDelta().Set());
        bioseq.SetInst().SetRepr(objects::CSeq_inst::eRepr_delta);
    }
    else
        bioseq.SetInst().ResetExt();

    MemFree(p);
    return true;
}

/**********************************************************
 *
 *   bool GetEmblInst(pp, entry, dnaconv):
 *
 *      Fills in Seq-inst for an entry. Assumes Bioseq
 *   already allocated.
 *
 **********************************************************/
static bool GetEmblInst(ParserPtr pp, const DataBlk& entry, unsigned char* dnaconv)
{
    EntryBlkPtr ebp;
    IndexblkPtr ibp;

    char*     p;
    char*     q;
    char*     r;

    Int4        i;
    Int2        strand;

    ebp = (EntryBlkPtr) entry.mpData;

    objects::CBioseq& bioseq = ebp->seq_entry->SetSeq();

    objects::CSeq_inst& inst = bioseq.SetInst();
    inst.SetRepr(objects::CSeq_inst::eRepr_raw);

    ibp = pp->entrylist[pp->curindx];

    /* p points to 2nd token
     */
    p = PointToNextToken(entry.mOffset + ParFlat_COL_DATA_EMBL);
    p = PointToNextToken(p);            /* p points to 3rd token */

    if (ibp->embl_new_ID)
        p = PointToNextToken(p);

    /* some entries have "circular" before molecule type in embl
     */
    if(StringNICmp(p, "circular", 8) == 0)
    {
        inst.SetTopology(objects::CSeq_inst::eTopology_circular);
        p = PointToNextToken(p);
    }
    else if (ibp->embl_new_ID)
        p = PointToNextToken(p);

    r = StringChr(p, ';');
    if(r != NULL)
        *r = '\0';

    for(i = 0, q = p; *q != '\0'; q++)
    {
        if(*q != ' ')
            continue;

        while(*q == ' ')
            q++;
        if(*q != '\0')
            i++;
        q--;
    }

    if (ibp->embl_new_ID == false && inst.GetTopology() != objects::CSeq_inst::eTopology_circular &&
       StringStr(p, "DNA") == NULL && StringStr(p, "RNA") == NULL &&
       (pp->source != Parser::ESource::EMBL || (StringStr(p, "xxx") == NULL &&
        StringStr(p, "XXX") == NULL)))
    {
        ErrPostEx(SEV_WARNING, ERR_LOCUS_WrongTopology,
                  "Other than circular topology found in EMBL, \"%s\", assign default topology",
                  p);
    }

    /* the "p" must be the mol-type
     */
    if(i == 0 && pp->source == Parser::ESource::NCBI)
    {
        /* source = NCBI can be full variety of strands/mol-type
         */
        strand = CheckSTRAND(p);
        if (strand > 0)
            inst.SetStrand(static_cast<objects::CSeq_inst::EStrand>(strand));
    }

    if(r != NULL)
        *r = ';';

    if (!GetSeqData(pp, entry, bioseq, ParFlat_SQ, dnaconv, objects::eSeq_code_type_iupacna))
        return false;

    if(ibp->is_contig && !GetEmblInstContig(entry, bioseq, pp))
        return false;

    return true;
}

/**********************************************************
 *
 *   static CRef<objects::CEMBL_block> GetDescrEmblBlock(pp, entry, mfp,
 *                                         gbdiv, biosp,
 *                                         dr_ena, dr_biosample):
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
static CRef<objects::CEMBL_block> GetDescrEmblBlock(
    ParserPtr pp, const DataBlk& entry, objects::CMolInfo& mol_info, char** gbdiv,
    objects::CBioSource* bio_src, TStringList& dr_ena, TStringList& dr_biosample)
{
    CRef<objects::CEMBL_block> ret, embl(new objects::CEMBL_block);

    IndexblkPtr  ibp;
    char*      bptr;
    char*      kw;
    char*      kwp;
    Char         dataclass[4];
    Char         ch;
    Int2         div;

    TKeywordList keywords;

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

    /* bptr points to 2nd token
     */
    bptr = PointToNextToken(entry.mOffset + ParFlat_COL_DATA_EMBL);

    if(ibp->embl_new_ID == false)
    {
        if (StringNICmp(bptr, "standard", 8) == 0)
        {
            ;// embl->SetClass(objects::CEMBL_block::eClass_standard);
        }
        else if (StringNICmp(bptr, "unannotated", 11) == 0)
        {
            embl->SetClass(objects::CEMBL_block::eClass_unannotated);
        }
        else if (StringNICmp(bptr, "unreviewed", 10) == 0 ||
                 StringNICmp(bptr, "preliminary", 11) == 0)
        {
            embl->SetClass(objects::CEMBL_block::eClass_other);
        }
        else
        {
            embl->SetClass(objects::CEMBL_block::eClass_not_set);
        }

        bptr = StringChr(bptr, ';');
        if(bptr != NULL)
            bptr = StringChr(bptr + 1, ';');
    }
    else
    {
        bptr = StringChr(bptr, ';');
        if(bptr != NULL)
            bptr = StringChr(bptr + 1, ';');
        if(bptr != NULL)
            bptr = StringChr(bptr + 1, ';');
        if(bptr != NULL)
        {
            while(*bptr == ' ' || *bptr == ';')
                bptr++;
            i = fta_StringMatch(ParFlat_Embl_dataclass_array, bptr);
            if(i < 0)
                bptr = StringChr(bptr, ';');
            else if(i == 0)
                bptr = (char*) "CON";
        }
    }

    if(bptr != NULL)
    {
        while(*bptr == ' ' || *bptr == ';')
            bptr++;
        StringNCpy(dataclass, bptr, 3);
        dataclass[3] = '\0';
        if(StringCmp(dataclass, "TSA") == 0)
            ibp->is_tsa = true;
    }
    else
    {
        bptr = (char*) "   ";
        dataclass[0] = '\0';
    }

    if_cds = check_cds(entry, pp->format);

    if (ibp->psip.NotEmpty())
        pat_ref = true;

    if(!ibp->keywords.empty())
    {
        keywords.swap(ibp->keywords);
        ibp->keywords.clear();
    }
    else
        GetSequenceOfKeywords(entry, ParFlat_KW, ParFlat_COL_DATA_EMBL, keywords);

    embl->SetKeywords() = keywords;
    if(ibp->is_tpa && !fta_tpa_keywords_check(keywords))
    {
        return ret;
    }

    if(ibp->is_tsa && !fta_tsa_keywords_check(keywords, pp->source))
    {
        return ret;
    }

    if(ibp->is_tls && !fta_tls_keywords_check(keywords, pp->source))
    {
        return ret;
    }

    ITERATE(TKeywordList, key, keywords)
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

    div = fta_StringMatch(ParFlat_Embl_DIV_array, bptr);
    if(div < 0)
    {
        ch = bptr[3];
        bptr[3] = '\0';
        ErrPostEx(SEV_REJECT, ERR_DIVISION_UnknownDivCode,
                  "Unknown division code \"%s\" found in Embl flatfile. Record rejected.",
                  bptr);
        bptr[3] = ch;
        return ret;
    }

    /* Embl has recently (7-19-93, email) decided to change the name of
     * its "UNA"==10 division to "UNC"==16 (for "unclassified")
     */
    if(div == 16)
        div = 10;

    StringCpy(ibp->division, ParFlat_GBDIV_array[div]);

    /* 06-10-96 new HUM division replaces the PRI
     * it's temporarily mapped to 'other' in asn.1 embl-block.
     * Divisions GSS, HUM, HTG, CON, ENV and MUS are mapped to other.
     */
    thtg = (div == 18) ? 6 : div;
    *gbdiv = StringSave(ParFlat_GBDIV_array[thtg]);

    if (div <= 15)
        embl->SetDiv(static_cast<objects::CEMBL_block_Base::TDiv>(div));

    p = *gbdiv;
    if(ibp->is_tpa &&
       (StringCmp(p, "EST") == 0 || StringCmp(p, "GSS") == 0 ||
        StringCmp(p, "PAT") == 0 || StringCmp(p, "HTG") == 0))
    {
        ErrPostEx(SEV_REJECT, ERR_DIVISION_BadTPADivcode,
                  "Division code \"%s\" is not legal for TPA records. Entry dropped.",
                  p);
        return ret;
    }

    if(ibp->is_tsa && StringCmp(p, "TSA") != 0)
    {
        ErrPostEx(SEV_REJECT, ERR_DIVISION_BadTSADivcode,
                  "Division code \"%s\" is not legal for TSA records. Entry dropped.",
                  p);
        return ret;
    }

    cancelled = IsCancelled(embl->GetKeywords());

    if(div == 19)                       /* HTG */
    {
        if (!HasHtg(embl->GetKeywords()))
        {
            ErrPostEx(SEV_ERROR, ERR_DIVISION_MissingHTGKeywords,
                      "Division is HTG, but entry lacks HTG-related keywords. Entry dropped.");
            return ret;
        }
        tempdiv = StringSave("HTG");
    }
    else
        tempdiv = NULL;

    fta_check_htg_kwds(embl->SetKeywords(), pp->entrylist[pp->curindx], mol_info);

    DefVsHTGKeywords(mol_info.GetTech(), entry, ParFlat_DE, ParFlat_SQ, cancelled);
    if ((mol_info.GetTech() == objects::CMolInfo::eTech_htgs_0 || mol_info.GetTech() == objects::CMolInfo::eTech_htgs_1 ||
        mol_info.GetTech() == objects::CMolInfo::eTech_htgs_2) && *gbdiv != NULL)
    {
        MemFree(*gbdiv);
        *gbdiv = NULL;
    }

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
        else if((i == 2 && wgs_kwd && tpa_kwd) ||
                (i == 2 && tsa_kwd && tpa_kwd))
        {
        }
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
        RemoveHtgPhase(embl->SetKeywords());
    }

    size_t len = 0;
    bptr = xSrchNodeType(entry, ParFlat_KW, &len);
    if(bptr != NULL)
    {
        kw = GetBlkDataReplaceNewLine(bptr, bptr + len, ParFlat_COL_DATA_EMBL);

        kwp = StringStr(kw, "EST");
        if(kwp != NULL && !est_kwd)
        {
            ErrPostEx(SEV_WARNING, ERR_KEYWORD_ESTSubstring,
                      "Keyword %s has substring EST, but no official EST keywords found",
                      kw);
        }
        kwp = StringStr(kw, "STS");
        if(kwp != NULL && !sts_kwd)
        {
            ErrPostEx(SEV_WARNING, ERR_KEYWORD_STSSubstring,
                      "Keyword %s has substring STS, but no official STS keywords found",
                      kw);
        }
        kwp = StringStr(kw, "GSS");
        if(kwp != NULL && !gss_kwd)
        {
            ErrPostEx(SEV_WARNING, ERR_KEYWORD_GSSSubstring,
                      "Keyword %s has substring GSS, but no official GSS keywords found",
                      kw);
        }
        MemFree(kw);
    }

    if (!ibp->is_contig)
    {
        drop = false;
        Uint1 tech = mol_info.GetTech();
        *gbdiv = check_div(ibp->is_pat, pat_ref, est_kwd, sts_kwd, gss_kwd,
                           if_cds, *gbdiv, &tech, ibp->bases, pp->source,
                           drop);
        if (tech != 0)
            mol_info.SetTech(tech);
        else
            mol_info.ResetTech();

        if(drop)
        {
            return ret;
        }
    }
    else if(*gbdiv != NULL && StringCmp(*gbdiv, "CON") == 0)
    {
        MemFree(*gbdiv);
        *gbdiv = NULL;
    }

    bool has_htc = HasHtc(embl->GetKeywords());

    if (*gbdiv != NULL && StringCmp(*gbdiv, "HTC") == 0 && !has_htc)
    {
        ErrPostEx(SEV_ERROR, ERR_DIVISION_MissingHTCKeyword,
                  "This record is in the HTC division, but lacks the required HTC keyword.");
        return ret;
    }
    if ((*gbdiv == NULL || StringCmp(*gbdiv, "HTC") != 0) && has_htc)
    {
        ErrPostEx(SEV_ERROR, ERR_DIVISION_InvalidHTCKeyword,
                  "This record has the special HTC keyword, but is not in HTC division. If this record has graduated out of HTC, then the keyword should be removed.");
        return ret;
    }

    if(*gbdiv != NULL && StringCmp(*gbdiv, "HTC") == 0)
    {
        p = entry.mOffset + ParFlat_COL_DATA_EMBL;      /* p points to 1st
                                                           token */
        p = PointToNextToken(p);        /* p points to 2nd token */
        p = PointToNextToken(p);        /* p points to 3rd token */

        if(ibp->embl_new_ID)
        {
            p = PointToNextToken(p);
            p = PointToNextToken(p);
        }
        else if(StringNICmp(p, "circular", 8) == 0)
            p = PointToNextToken(p);    /* p points to 4th token */

        if(StringNCmp(p + 1, "s-", 2) == 0)
            p += 3;
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
    if(*gbdiv != NULL)
    {
        if(StringCmp(*gbdiv, "EST") == 0)
        {
            ibp->EST = true;
            mol_info.SetTech(objects::CMolInfo::eTech_est);
        }
        else if(StringCmp(*gbdiv, "STS") == 0)
        {
            ibp->STS = true;
            mol_info.SetTech(objects::CMolInfo::eTech_sts);
        }
        else if(StringCmp(*gbdiv, "GSS") == 0)
        {
            ibp->GSS = true;
            mol_info.SetTech(objects::CMolInfo::eTech_survey);
        }
        else if(StringCmp(*gbdiv, "HTC") == 0)
        {
            ibp->HTC = true;
            mol_info.SetTech(objects::CMolInfo::eTech_htc);
            MemFree(*gbdiv);
            *gbdiv = NULL;
        }
        else if(StringCmp(*gbdiv, "SYN") == 0 && bio_src != NULL &&
                bio_src->IsSetOrigin() && bio_src->GetOrigin() == 5)     /* synthetic */
        {
            MemFree(*gbdiv);
            *gbdiv = NULL;
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
        fta_remove_keywords(mol_info.GetTech(), embl->SetKeywords());

    if(ibp->is_tpa)
        fta_remove_tpa_keywords(embl->SetKeywords());

    if(ibp->is_tsa)
        fta_remove_tsa_keywords(embl->SetKeywords(), pp->source);

    if(ibp->is_tls)
        fta_remove_tls_keywords(embl->SetKeywords(), pp->source);

    if (bio_src != NULL && bio_src->IsSetSubtype())
    {
        ITERATE(objects::CBioSource::TSubtype, subtype, bio_src->GetSubtype())
        {
            if ((*subtype)->GetSubtype() == objects::CSubSource::eSubtype_environmental_sample)
            {
                fta_remove_env_keywords(embl->SetKeywords());
                break;
            }
        }
    }


    CRef<objects::CDate_std> std_creation_date,
                                         std_update_date;

    GetEmblDate(pp->source, entry, std_creation_date, std_update_date);

    embl->SetCreation_date().SetStd(*std_creation_date);
    embl->SetUpdate_date().SetStd(*std_update_date);

    ibp->wgssec[0] = '\0';
    GetExtraAccession(ibp, pp->allow_uwsec, pp->source, embl->SetExtra_acc());

    GetEmblBlockXref(entry, NULL, NULL, dr_ena, dr_biosample, &ibp->drop, *embl);

    if (StringCmp(dataclass, "ANN") == 0 || StringCmp(dataclass, "CON") == 0)
    {
        if(StringLen(ibp->acnum) == 8 &&
           (StringNCmp(ibp->acnum, "CT", 2) == 0 ||
            StringNCmp(ibp->acnum, "CU", 2) == 0))
        {
            bool found = false;
            ITERATE(objects::CEMBL_block::TExtra_acc, acc, embl->SetExtra_acc())
            {
                if (fta_if_wgs_acc(acc->c_str()) == 0 &&
                    ((*acc)[0] == 'C' || (*acc)[0] == 'U'))
                {
                    found = true;
                    break;
                }
            }
            if (found)
                mol_info.SetTech(objects::CMolInfo::eTech_wgs);
        }
    }

    return embl;
}


static bool s_DuplicatesBiosource(const CBioSource& biosource, const char* gbdiv)
{
    return (biosource.IsSetOrg() &&
            biosource.GetOrg().IsSetOrgname() &&
            biosource.GetOrg().GetOrgname().IsSetDiv() &&                   
            NStr::Equal(biosource.GetOrg().GetOrgname().GetDiv(),gbdiv));
}

/**********************************************************/
static CRef<objects::CGB_block> GetEmblGBBlock(ParserPtr pp, const DataBlk& entry,
                                                           char* gbdiv, objects::CBioSource* bio_src)
{
    IndexblkPtr  ibp;

    CRef<objects::CGB_block> gbb(new objects::CGB_block);

    ibp = pp->entrylist[pp->curindx];

    if(pp->source == Parser::ESource::NCBI)
    {
        ibp->wgssec[0] = '\0';
        GetExtraAccession(ibp, pp->allow_uwsec, pp->source, gbb->SetExtra_accessions());
        if(!ibp->keywords.empty())
        {
            gbb->SetKeywords().swap(ibp->keywords);
            ibp->keywords.clear();
        }
        else
            GetSequenceOfKeywords(entry, ParFlat_KW, ParFlat_COL_DATA_EMBL, gbb->SetKeywords());
    }

    if(gbdiv)
    {
        if(NStr::EqualNocase(gbdiv, "ENV") &&
           bio_src && bio_src->IsSetSubtype())
        {
            const auto& subtype = bio_src->GetSubtype();
            const auto it = 
                find_if(begin(subtype), end(subtype), 
                    [](auto pSubSource) { 
                        return pSubSource->GetSubtype() == CSubSource::eSubtype_environmental_sample; 
                        });
            if ((it == subtype.end()) && !s_DuplicatesBiosource(*bio_src,gbdiv)) { // Not found
                gbb->SetDiv(gbdiv);
            }
        }
        else if (!bio_src || 
                 !s_DuplicatesBiosource(*bio_src, gbdiv)) {
            gbb->SetDiv(gbdiv);
        }
    }

    if (!gbb->IsSetExtra_accessions() && !gbb->IsSetKeywords() && !gbb->IsSetDiv())
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
static CRef<objects::CMolInfo> GetEmblMolInfo(ParserPtr pp, const DataBlk& entry,
                                                          const objects::COrg_ref* org_ref)
{
    IndexblkPtr ibp;

    char*     bptr;
    char*     p;
    char*     q;
    char*     r;
    Int4        i;

    ibp = pp->entrylist[pp->curindx];
    bptr = entry.mOffset + ParFlat_COL_DATA_EMBL;       /* bptr points to 1st
                                                           token */
    bptr = PointToNextToken(bptr);      /* bptr points to 2nd token */
    bptr = PointToNextToken(bptr);      /* bptr points to 3rd token */

    if(StringNICmp(bptr, "circular", 8) == 0 || ibp->embl_new_ID)
        bptr = PointToNextToken(bptr);  /* bptr points to 4th token */
    if(ibp->embl_new_ID)
        bptr = PointToNextToken(bptr);  /* bptr points to 5th token */

    r = StringChr(bptr, ';');
    if(r != NULL)
        *r = '\0';

    for(i = 0, q = bptr; *q != '\0'; q++)
    {
        if(*q != ' ')
            continue;

        while(*q == ' ')
            q++;
        if(*q != '\0')
            i++;
        q--;
    }

    if(r != NULL)
        for(p = r + 1; *p == ' ' || *p == ';';)
            p++;
    else
        p = bptr;

    CRef<objects::CMolInfo> mol_info(new objects::CMolInfo);

    if(StringNCmp(p, "EST", 3) == 0)
        mol_info->SetTech(objects::CMolInfo::eTech_est);
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

    if(i == 0 && CheckSTRAND(bptr) >= 0)
        bptr = bptr + 3;

    GetFlatBiomol(mol_info->SetBiomol(), mol_info->GetTech(), bptr, pp, entry, org_ref);
    if (mol_info->GetBiomol() == 0) // not set
        mol_info->ResetBiomol();

    if(r != NULL)
        *r = ';';

    return mol_info;
}

/**********************************************************/
static CRef<objects::CUser_field> fta_create_user_field(const char *tag, TStringList& lst)
{
    CRef<objects::CUser_field> field;
    if (tag == NULL || lst.empty())
        return field;

    field.Reset(new objects::CUser_field);
    field->SetLabel().SetStr(tag);
    field->SetNum(static_cast<objects::CUser_field::TNum>(lst.size()));

    ITERATE(TStringList, item, lst)
    {
        field->SetData().SetStrs().push_back(*item);
    }

    return field;
}

/**********************************************************/
void fta_build_ena_user_object(objects::CSeq_descr::Tdata& descrs, TStringList& dr_ena,
                               TStringList& dr_biosample,
                               CRef<objects::CUser_object>& dbuop)
{
    bool got = false;

    if(dr_ena.empty() && dr_biosample.empty())
        return;

    objects::CUser_object* user_obj_ptr = nullptr;

    NON_CONST_ITERATE(objects::CSeq_descr::Tdata, descr, descrs)
    {
        if (!(*descr)->IsUser() || !(*descr)->GetUser().IsSetType())
            continue;

        const objects::CObject_id& obj_id = (*descr)->GetUser().GetType();

        if (obj_id.IsStr() && obj_id.GetStr() == "DBLink")
        {
            user_obj_ptr = &(*descr)->SetUser();
            got = true;
            break;
        }
    }

    CRef<objects::CUser_field> field_bs;
    if (!dr_biosample.empty())
        field_bs = fta_create_user_field("BioSample", dr_biosample);

    CRef<objects::CUser_field> field_ena;
    if (!dr_ena.empty())
    {
        field_ena = fta_create_user_field("Sequence Read Archive", dr_ena);
    }

    if (field_bs.Empty() && field_ena.Empty())
        return;

    CRef<objects::CUser_object> user_obj;

    if (!got)
    {
        user_obj.Reset(new objects::CUser_object);
        user_obj->SetType().SetStr("DBLink");

        user_obj_ptr = user_obj.GetPointer();
    }

    if (field_bs.NotEmpty())
        user_obj_ptr->SetData().push_back(field_bs);
    if (field_ena.NotEmpty())
        user_obj_ptr->SetData().push_back(field_ena);

    if (!got)
    {
        CRef<objects::CSeqdesc> descr(new objects::CSeqdesc);
        descr->SetUser(*user_obj);
        descrs.push_back(descr);
    }

    if (!got)
        dbuop = user_obj;
    else
    {
        dbuop.Reset(new objects::CUser_object);
        dbuop->Assign(*user_obj_ptr);
    }
}

/**********************************************************/
static void fta_create_imgt_misc_feat(objects::CBioseq& bioseq, objects::CEMBL_block& embl_block,
                                      IndexblkPtr ibp)
{
    if (!embl_block.IsSetXref())
        return;

    objects::CSeq_feat::TDbxref xrefs;
    ITERATE(objects::CEMBL_block::TXref, xref, embl_block.GetXref())
    {
        if (!(*xref)->IsSetDbname() || !(*xref)->GetDbname().IsName() ||
            StringNCmp((*xref)->GetDbname().GetName().c_str(), "IMGT/", 5) != 0)
            continue;

        bool empty = true;
        ITERATE(objects::CEMBL_xref::TId, id, (*xref)->GetId())
        {
            if ((*id)->IsStr() && !(*id)->GetStr().empty())
            {
                empty = false;
                break;
            }
        }

        if (empty)
            continue;

        CRef<objects::CDbtag> tag(new objects::CDbtag);
        tag->SetDb((*xref)->GetDbname().GetName());

        std::string& id_str = tag->SetTag().SetStr();

        bool need_delimiter = false;
        ITERATE(objects::CEMBL_xref::TId, id, (*xref)->GetId())
        {
            if ((*id)->IsStr() && !(*id)->GetStr().empty())
            {
                if (need_delimiter)
                    id_str += "; ";
                else
                    need_delimiter = true;

                id_str += (*id)->GetStr();
            }
        }

        xrefs.push_back(tag);
    }

    if (xrefs.empty())
        return;

    CRef<objects::CSeq_feat> feat(new objects::CSeq_feat);
    objects::CImp_feat& imp = feat->SetData().SetImp();
    imp.SetKey("misc_feature");
    feat->SetDbxref().swap(xrefs);
    feat->SetLocation(*fta_get_seqloc_int_whole(*(*bioseq.SetId().begin()), ibp->bases));

    objects::CBioseq::TAnnot& annot = bioseq.SetAnnot();
    if (annot.empty() || !(*annot.begin())->IsFtable())
    {
        CRef<objects::CSeq_annot> new_annot(new objects::CSeq_annot);
        new_annot->SetData().SetFtable().push_back(feat);

        annot.push_back(new_annot);
    }
    else
    {
        objects::CSeq_annot& old_annot = *(*annot.begin());
        old_annot.SetData().SetFtable().push_front(feat);
    }
}

/**********************************************************/
static void GetEmblDescr(ParserPtr pp, const DataBlk& entry, objects::CBioseq& bioseq)
{
    IndexblkPtr   ibp;
    DataBlkPtr    dbp;

    char*       offset;
    char*       str;
    char*       str1;
    char*       gbdiv;
    char*       p;
    char*       q;

    bool          is_htg = false;

    size_t len;

    ibp = pp->entrylist[pp->curindx];

    /* pp->source == NCBI then no embl-block, only GB-block
     */

    /* DE data ==> descr_title
     */
    str = NULL;
    offset = xSrchNodeType(entry, ParFlat_DE, &len);

    std::string title;

    if(offset != NULL)
    {
        str = GetBlkDataReplaceNewLine(offset, offset + len, ParFlat_COL_DATA_EMBL);

        for (p = str, q = p; *q != '\0';)
        {
            *p++ = *q;
            if(*q++ == ';')
                while(*q == ';')
                    q++;
        }

        *p = '\0';
        if (p > str)
        {
            for(p--; *p == ' ' || *p == ';'; p--)
                if(p == str)
                    break;
            if(*p != ' ' && *p != ';')
                p++;
            *p = '\0';
        }

        if(ibp->is_tpa == false && pp->source != Parser::ESource::EMBL &&
           StringNCmp(str, "TPA:", 4) == 0)
        {
            ErrPostEx(SEV_REJECT, ERR_DEFINITION_ShouldNotBeTPA,
                      "This is apparently _not_ a TPA record, but the special \"TPA:\" prefix is present on its definition line. Entry dropped.");
            ibp->drop = 1;
            return;
        }

        if (ibp->is_tsa == false && StringNCmp(str, "TSA:", 4) == 0)
        {
            ErrPostEx(SEV_REJECT, ERR_DEFINITION_ShouldNotBeTSA,
                      "This is apparently _not_ a TSA record, but the special \"TSA:\" prefix is present on its definition line. Entry dropped.");
            ibp->drop = 1;
            return;
        }

        if (ibp->is_tls == false && StringNCmp(str, "TLS:", 4) == 0)
        {
            ErrPostEx(SEV_REJECT, ERR_DEFINITION_ShouldNotBeTLS,
                      "This is apparently _not_ a TLS record, but the special \"TLS:\" prefix is present on its definition line. Entry dropped.");
            ibp->drop = 1;
            return;
        }

        if(StringNCmp(str, "TPA:", 4) == 0)
        {
            if(ibp->assembly)
                p = (char*) "TPA_asm:";
            else if(ibp->specialist_db)
                p = (char*) "TPA_specdb:";
            else if(ibp->inferential)
                p = (char*) "TPA_inf:";
            else if(ibp->experimental)
                p = (char*) "TPA_exp:";
            else
                p = NULL;

            if(p != NULL)
            {
                str1 = (char*) MemNew(StringLen(p) + StringLen(str));
                StringCpy(str1, p);
                StringCat(str1, str + 4);
                MemFree(str);
                str = str1;
            }
        }

        CRef<objects::CSeqdesc> descr(new objects::CSeqdesc);
        descr->SetTitle(str);
        bioseq.SetDescr().Set().push_back(descr);

        title = str;
        MemFree(str);
        str = NULL;
    }

    offset = xSrchNodeType(entry, ParFlat_PR, &len);
    if(offset != NULL)
        fta_get_project_user_object(bioseq.SetDescr().Set(), offset,
                                    Parser::EFormat::EMBL, &ibp->drop, pp->source);

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

    /* RN data ==> pub  should be before GBblock because we need patent ref
     */
    dbp = TrackNodeType(entry, ParFlat_REF_END);
    for(; dbp != NULL; dbp = dbp->mpNext)
    {
        if(dbp->mType != ParFlat_REF_END)
            continue;

        CRef<objects::CPubdesc> pubdesc = DescrRefs(pp, dbp, ParFlat_COL_DATA_EMBL);
        if (pubdesc.NotEmpty())
        {
            CRef<objects::CSeqdesc> descr(new objects::CSeqdesc);
            descr->SetPub(*pubdesc);
            bioseq.SetDescr().Set().push_back(descr);
        }
    }

    dbp = TrackNodeType(entry, ParFlat_REF_NO_TARGET);
    for(; dbp != NULL; dbp = dbp->mpNext)
    {
        if(dbp->mType != ParFlat_REF_NO_TARGET)
            continue;

        CRef<objects::CPubdesc> pubdesc = DescrRefs(pp, dbp, ParFlat_COL_DATA_EMBL);
        if (pubdesc.NotEmpty())
        {
            CRef<objects::CSeqdesc> descr(new objects::CSeqdesc);
            descr->SetPub(*pubdesc);
            bioseq.SetDescr().Set().push_back(descr);
        }
    }

    /* OS data ==> descr_org
     */
    objects::CBioSource* bio_src = nullptr;
    objects::COrg_ref* org_ref = nullptr;

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

    /* MolInfo, 3rd or 4th token in the ID line
     */
    CRef<objects::CMolInfo> mol_info = GetEmblMolInfo(pp, entry, org_ref);

    gbdiv = NULL;

    TStringList dr_ena,
                dr_biosample;

    CRef<objects::CEMBL_block> embl_block =
        GetDescrEmblBlock(pp, entry, *mol_info, &gbdiv, bio_src, dr_ena, dr_biosample);

    if (pp->source == Parser::ESource::EMBL && embl_block.NotEmpty())
        fta_create_imgt_misc_feat(bioseq, *embl_block, ibp);

    if ((pp->source == Parser::ESource::DDBJ || pp->source == Parser::ESource::EMBL) &&
        ibp->is_contig && !mol_info->IsSetTech())
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

        if (mol_info->IsSetTech() && (mol_info->GetTech() == objects::CMolInfo::eTech_htgs_0 || mol_info->GetTech() == objects::CMolInfo::eTech_htgs_1 ||
            mol_info->GetTech() == objects::CMolInfo::eTech_htgs_2))
            is_htg = true;
    }
    else
    {
        mol_info.Reset();
    }

    CRef<objects::CUser_object> dbuop;
    if (!dr_ena.empty() || !dr_biosample.empty())
        fta_build_ena_user_object(bioseq.SetDescr().Set(), dr_ena, dr_biosample, dbuop);

    if (embl_block.Empty())
    {
        ibp->drop = 1;
        return;
    }

    if(StringNICmp(ibp->division, "CON", 3) == 0)
        fta_add_hist(pp, bioseq, embl_block->SetExtra_acc(), Parser::ESource::EMBL, objects::CSeq_id::e_Embl,
                     true, ibp->acnum);
    else
        fta_add_hist(pp, bioseq, embl_block->SetExtra_acc(), Parser::ESource::EMBL, objects::CSeq_id::e_Embl,
                     false, ibp->acnum);

    if (embl_block->GetExtra_acc().empty())
        embl_block->ResetExtra_acc();

    CRef<objects::CGB_block> gbb;
   
    if  (pp->source == Parser::ESource::NCBI || (!embl_block->IsSetDiv() && gbdiv) ) {
        gbb = GetEmblGBBlock(pp, entry, gbdiv, bio_src);      /* GB-block */
    }

    if(gbdiv != NULL)
        MemFree(gbdiv);
    
    bool hasEmblBlock = false;
    if(pp->source != Parser::ESource::NCBI)
    {
        CRef<objects::CSeqdesc> descr(new objects::CSeqdesc);
        descr->SetEmbl(*embl_block);
        bioseq.SetDescr().Set().push_back(descr);
        hasEmblBlock = true;
    }

    offset = xSrchNodeType(entry, ParFlat_AH, &len);
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
       fta_parse_tpa_tsa_block(bioseq, offset, ibp->acnum, ibp->vernum, len,
                               ParFlat_COL_DATA_EMBL, ibp->is_tpa) == false)
    {
        ibp->drop = 1;
        return;
    }

    /* GB-block and div
     */
    if (pp->taxserver == 1) {
        if (hasEmblBlock && embl_block->IsSetDiv() && embl_block->GetDiv() < 15) {
            if (org_ref->IsSetOrgname() && !org_ref->GetOrgname().IsSetDiv() &&
                (!org_ref->IsSetDb() || !fta_orgref_has_taxid(org_ref->GetDb()))) {
                org_ref->SetOrgname().SetDiv(ParFlat_GBDIV_array[embl_block->GetDiv()]); 
            }

            if (bioseq.IsSetAnnot()) {
                for (auto pAnnot : bioseq.GetAnnot()) {
                    if (pAnnot->IsFtable()) {
                        for (auto pFeat : pAnnot->SetData().SetFtable()) {
                            if (pFeat->IsSetData() && pFeat->SetData().IsBiosrc()) {
                                auto& biosrc = pFeat->SetData().SetBiosrc();
                                if (biosrc.IsSetOrg() &&
                                    (!biosrc.GetOrg().IsSetDb() ||
                                     !fta_orgref_has_taxid(biosrc.GetOrg().GetDb()))) {
                                        biosrc.SetOrg().SetOrgname().SetDiv(ParFlat_GBDIV_array[embl_block->GetDiv()]); 
                                }
                            }
                        }
                    }
                }
            }
        }
        else
        if (gbb && gbb->IsSetDiv()) {
            fta_fix_orgref_div(bioseq.GetAnnot(), *org_ref, *gbb);
        }
    }

    if (gbb)
    {
        CRef<objects::CSeqdesc> descr(new objects::CSeqdesc);
        descr->SetGenbank(*gbb);
        bioseq.SetDescr().Set().push_back(descr);
    }

    /* all CC data ==> comment
     */
    offset = xSrchNodeType(entry, ParFlat_CC, &len);
    if(offset != NULL && len > 0)
    {
        str = GetDescrComment(offset, len, ParFlat_COL_DATA_EMBL,
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

    if (pp->no_date)
        return;

    /* DT data ==> create-date, update-date
     */

    CRef<objects::CDate_std> std_creation_date,
                                         std_update_date;
    GetEmblDate(pp->source, entry, std_creation_date, std_update_date);
    if (std_creation_date.NotEmpty())
    {
        CRef<objects::CSeqdesc> descr(new objects::CSeqdesc);
        descr->SetCreate_date().SetStd(*std_creation_date);
        bioseq.SetDescr().Set().push_back(descr);
    }

    if(std_update_date.NotEmpty())
    {
        CRef<objects::CSeqdesc> descr(new objects::CSeqdesc);
        descr->SetUpdate_date().SetStd(*std_update_date);
        bioseq.SetDescr().Set().push_back(descr);

        if (std_creation_date.NotEmpty() && std_creation_date->Compare(*std_update_date) == objects::CDate::eCompare_after)
        {
            std::string crdate_str,
                        update_str;
            std_creation_date->GetDate(&crdate_str, "%2M-%2D-%4Y");
            std_update_date->GetDate(&crdate_str, "%2M-%2D-%4Y");
            ErrPostEx(SEV_ERROR, ERR_DATE_IllegalDate,
                      "Update-date \"%s\" precedes create-date \"%s\".",
                      update_str.c_str(), crdate_str.c_str());
        }
    }
}

/**********************************************************/
static void FakeEmblBioSources(const DataBlk& entry, objects::CBioseq& bioseq)
{
    DataBlkPtr   dbp;
    DataBlkPtr   subdbp;

    char*      p;
    char*      q;
    Char         ch;

    dbp = TrackNodeType(entry, ParFlat_OS);
    if(dbp == NULL)
    {
        ErrPostStr(SEV_WARNING, ERR_ORGANISM_NoOrganism,
                   "No Organism data in Embl format file");
        return;
    }

    for(; dbp != NULL; dbp = dbp->mpNext)
    {
        if(dbp->mType != ParFlat_OS)
            continue;

        CRef<objects::COrg_ref> org_ref = GetEmblOrgRef(dbp);
        if (org_ref.Empty())
            continue;

        CRef<objects::CBioSource> bio_src(new objects::CBioSource);
        bio_src->SetOrg(*org_ref);

        std::string& taxname_str = org_ref->SetTaxname();
        size_t off_pos = 0;
        if (GetGenomeInfo(*bio_src, taxname_str.c_str()) && bio_src->GetGenome() != 9)  /* ! Plasmid */
        {
            while (taxname_str[off_pos] != ' ' && off_pos < taxname_str.size())
                ++off_pos;
            while (taxname_str[off_pos] == ' ' && off_pos < taxname_str.size())
                ++off_pos;
        }

        taxname_str = taxname_str.substr(off_pos);
        if (taxname_str == "Unknown.")
        {
            taxname_str = taxname_str.substr(0, taxname_str.size() - 1);
        }

        subdbp = (DataBlkPtr) dbp->mpData;
        for(; subdbp != NULL; subdbp = subdbp->mpNext)
        {
            if(subdbp->mType == ParFlat_OG)
            {
                GetGenomeInfo(*bio_src, subdbp->mOffset + ParFlat_COL_DATA_EMBL);
                continue;
            }
            if(subdbp->mType != ParFlat_OC || subdbp->mOffset == NULL ||
               subdbp->len < ParFlat_COL_DATA_EMBL)
                continue;

            ch = subdbp->mOffset[subdbp->len];
            subdbp->mOffset[subdbp->len] = '\0';
            q = StringSave(subdbp->mOffset + ParFlat_COL_DATA_EMBL);
            subdbp->mOffset[subdbp->len] = ch;
            for(p = q; p != NULL;)
            {
                p = StringStr(p, "\nOC   ");
                if(p != NULL)
                    fta_StringCpy(p, p + 5);
            }
            for(p = q; *p != '\0';)
                p++;
            if(p == q)
            {
                MemFree(q);
                continue;
            }
            for(p--;; p--)
            {
                if(*p != ' ' && *p != '\t' && *p != '\n' && *p != '.' &&
                   *p != ';')
                {
                    p++;
                    break;
                }
                if(p == q)
                    break;
            }
            if(p == q)
            {
                MemFree(q);
                continue;
            }
            *p = '\0';

            if (!org_ref->IsSetOrgname())
            {
                org_ref->SetOrgname().SetLineage(q);
            }
            MemFree(q);
        }

        CRef<objects::CSeqdesc> descr(new objects::CSeqdesc);
        descr->SetSource(*bio_src);
        bioseq.SetDescr().Set().push_front(descr);
    }
}

/**********************************************************/
static void EmblGetDivision(IndexblkPtr ibp, const DataBlk& entry)
{
    char* p;
    char* q;

    p = StringChr(entry.mOffset, ';');
    if(p == NULL)
        p = entry.mOffset;
    else
    {
        q = StringChr(p + 1, ';');
        if(q != NULL)
            p = q;
    }
    while(*p == ' ' || *p == ';')
        p++;

    StringNCpy(ibp->division, p, 3);
    ibp->division[3] = '\0';
}

/**********************************************************/
static void EmblGetDivisionNewID(IndexblkPtr ibp, const DataBlk& entry)
{
    char* p;
    Int4    i;

    for(i = 0, p = entry.mOffset; *p != '\0' && i < 4; p++)
        if(*p == ';' && p[1] == ' ')
            i++;

    while(*p == ' ')
        p++;

    i = fta_StringMatch(ParFlat_Embl_dataclass_array, p);
    if(i < 0)
    {
        p = StringChr(p, ';');
        if(p != NULL)
            for(p++; *p == ' ';)
                p++;
    }
    else if(i == 0)
        p = (char*) "CON";

    if(p == NULL)
        p = (char*) "   ";

    StringNCpy(ibp->division, p, 3);
    ibp->division[3] = '\0';
}

/**********************************************************
 *
 *   bool EmblAscii(pp):
 *
 *      Return FALSE if allocate entry block failed.
 *
 **********************************************************/
bool EmblAscii(ParserPtr pp)
{
    Int2        curkw;
    Int4        i;
    Int4        imax;
    Int4        total = 0;
    char*     ptr;
    char*     eptr;
    //DataBlkPtr  entry;
    EntryBlkPtr ebp;


    TEntryList seq_entries;

    objects::CSeq_loc locs;

    bool     reject_set;
    bool     seq_long = false;
    IndexblkPtr ibp;

    auto dnaconv = GetDNAConv();             /* set up sequence alphabets */

    unique_ptr<DataBlk> pEntry;
    for(imax = pp->indx, i = 0; i < imax; i++)
    {
        pp->curindx = i;
        ibp = pp->entrylist[i];

        err_install(ibp, pp->accver);
        if(ibp->drop != 1)
        {
            pEntry.reset(LoadEntry(pp, ibp->offset, ibp->len));
            if(!pEntry)
            {
                FtaDeletePrefix(PREFIX_LOCUS | PREFIX_ACCESSION);
                return false;
            }
            ebp = (EntryBlkPtr) pEntry->mpData;
            ptr = pEntry->mOffset;        /* points to beginning of the
                                           memory line */
            eptr = ptr + pEntry->len;
            curkw = ParFlat_ID;

            //TODO: below is a potentially infinite cycle!!!!
            while(curkw != ParFlatEM_END)
            {
                /* ptr points to current keyword's memory line
                 */
                ptr = GetEmblBlock(&ebp->chain, ptr, &curkw, pp->format, eptr);
            }

            if(ibp->embl_new_ID)
                EmblGetDivisionNewID(ibp, *pEntry);
            else
                EmblGetDivision(ibp, *pEntry);

            if(StringCmp(ibp->division, "TSA") == 0)
            {
                if(ibp->tsa_allowed == false)
                    ErrPostEx(SEV_WARNING, ERR_TSA_UnexpectedPrimaryAccession,
                              "The record with accession \"%s\" is not expected to have a TSA division code.",
                              ibp->acnum);
                ibp->is_tsa = true;
            }

            CheckEmblContigEverywhere(ibp, pp->source);
            if(ibp->drop != 0)
            {
                ErrPostEx(SEV_ERROR, ERR_ENTRY_Skipped,
                          "Entry skipped: \"%s|%s\".",
                          ibp->locusname, ibp->acnum);
                continue;
            }

            if(ptr >= eptr)
            {
                ibp->drop = 1;
                ErrPostStr(SEV_ERROR, ERR_FORMAT_MissingEnd,
                           "Missing end of the entry, entry dropped");
                if(pp->segment == false)
                {
                    ErrPostEx(SEV_ERROR, ERR_ENTRY_Skipped,
                              "Entry skipped: \"%s|%s\".",
                              ibp->locusname, ibp->acnum);
                }
                continue;
            }
            GetEmblSubBlock(ibp->bases, pp->source, *pEntry);

            CRef<objects::CBioseq> bioseq = CreateEntryBioseq(pp, true);
            AddNIDSeqId(*bioseq, *pEntry, ParFlat_NI, ParFlat_COL_DATA_EMBL,
                        pp->source);

            ebp->seq_entry.Reset(new objects::CSeq_entry);
            ebp->seq_entry->SetSeq(*bioseq);
            GetScope().AddBioseq(*bioseq);

            GetReleaseInfo(*pEntry, pp->accver);
            if (!GetEmblInst(pp, *pEntry, dnaconv.get()))
            {
                ibp->drop = 1;
                ErrPostStr(SEV_REJECT, ERR_SEQUENCE_BadData,
                           "Bad sequence data, entry dropped");
                if(pp->segment == false)
                {
                    ErrPostEx(SEV_ERROR, ERR_ENTRY_Skipped,
                              "Entry skipped: \"%s|%s\".",
                              ibp->locusname, ibp->acnum);
                }
                continue;
            }

            FakeEmblBioSources(*pEntry, *bioseq);
            LoadFeat(pp, *pEntry, *bioseq);

            if (!bioseq->IsSetAnnot() && ibp->drop != 0)
            {
                if(pp->segment == false)
                {
                    ErrPostEx(SEV_ERROR, ERR_ENTRY_Skipped,
                              "Entry skipped: \"%s|%s\".",
                              ibp->locusname, ibp->acnum);
                }
                continue;
            }

            GetEmblDescr(pp, *pEntry, *bioseq);

            if (ibp->drop != 0)
            {
                if(pp->segment == false)
                {
                    ErrPostEx(SEV_ERROR, ERR_ENTRY_Skipped,
                              "Entry skipped: \"%s|%s\".",
                              ibp->locusname, ibp->acnum);
                }
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

            if (pEntry->mpQscore == NULL && pp->accver)
            {
                if (pp->ff_get_qscore != NULL)
                    pEntry->mpQscore = (*pp->ff_get_qscore)(ibp->acnum, ibp->vernum);
                else if (pp->ff_get_qscore_pp != NULL)
                    pEntry->mpQscore = (*pp->ff_get_qscore_pp)(ibp->acnum, ibp->vernum, pp);
                if(pp->qsfd != NULL && ibp->qslength > 0)
                    pEntry->mpQscore = GetQSFromFile(pp->qsfd, ibp);
            }

            if(QscoreToSeqAnnot(pEntry->mpQscore, *bioseq, ibp->acnum,
                                ibp->vernum, false, false) == false)
            {
                if(pp->ign_bad_qs == false)
                {
                    ibp->drop = 1;
                    ErrPostEx(SEV_ERROR, ERR_QSCORE_FailedToParse,
                              "Error while parsing QScore. Entry dropped.");
                    if(pp->segment == false)
                    {
                        ErrPostEx(SEV_ERROR, ERR_ENTRY_Skipped,
                                  "Entry skipped: \"%s|%s\".",
                                  ibp->locusname, ibp->acnum);
                    }
                    continue;
                }
                ErrPostEx(SEV_ERROR, ERR_QSCORE_FailedToParse,
                          "Error while parsing QScore.");
            }

            if(pEntry->mpQscore != NULL)
            {
                MemFree(pEntry->mpQscore);
                pEntry->mpQscore = NULL;
            }

            /* add PatentSeqId if patent is found in reference
             */
            if (ibp->psip.NotEmpty())
            {
                CRef<objects::CSeq_id> id(new objects::CSeq_id);
                id->SetPatent(*ibp->psip);
                bioseq->SetId().push_back(id);
                ibp->psip.Reset();
            }

            if(no_reference(*bioseq) && pp->debug == false)
            {
                ibp->drop = 1;
                ErrPostStr(SEV_ERROR, ERR_REFERENCE_No_references,
                           "No reference for the entry, entry dropped");
                if(pp->segment == false)
                {
                    ErrPostEx(SEV_ERROR, ERR_ENTRY_Skipped,
                              "Entry skipped: \"%s|%s\".",
                              ibp->locusname, ibp->acnum);
                }
                continue;
            }

            seq_entries.push_back(ebp->seq_entry);
            ebp->seq_entry.Reset();

            if (pp->segment == false)
            {
                if(pp->limit != 0 && ibp->bases > (size_t) pp->limit)
                {
                    if(ibp->htg == 4 || ibp->htg == 1 || ibp->htg == 2)
                    {
                        ErrPostEx(SEV_WARNING, ERR_ENTRY_LongHTGSSequence,
                                  "HTGS Phase 0/1/2 sequence %s|%s exceeds length limit %ld: entry has been processed regardless of this problem",
                                  ibp->locusname, ibp->acnum, pp->limit);
                    }
                    else
                        seq_long = true;
                }

                if (!OutputEmblAsn(seq_long, pp, seq_entries))
                    ibp->drop = 1;
                else if(ibp->drop == 0)
                    total++;
                seq_long = false;
            }
            else
            {
                GetSeqExt(pp, locs);
            }
            GetScope().ResetHistory();
        } /* if, not drop */

        if (pp->segment == false)
        {
            if(ibp->drop == 0)
            {
                ErrPostEx(SEV_INFO, ERR_ENTRY_Parsed,
                          "OK - entry parsed successfully: \"%s|%s\".",
                          ibp->locusname, ibp->acnum);
            }
            else
            {
                ErrPostEx(SEV_ERROR, ERR_ENTRY_Skipped,
                          "Entry skipped: \"%s|%s\".",
                          ibp->locusname, ibp->acnum);
            }
        }
    } /* while, ascii block entries */

    if (pp->segment)
    {
        /* reject the whole set if any one entry was rejected
         */
        for(reject_set = false, i = 0; i < imax; i++)
        {
            if(pp->entrylist[i]->drop != 0)
            {
                reject_set = true;
                break;
            }
        }
        if(pp->limit != 0 && !reject_set)
        {
            for(seq_long = false, i = 0; i < imax; i++)
            {
                ibp = pp->entrylist[i];
                if(ibp->bases > (size_t) pp->limit && ibp->htg != 1 &&
                   ibp->htg != 2 && ibp->htg != 4)
                {
                    seq_long = true;
                    break;
                }
            }
            if(!seq_long)
            {
                for(i = 0; i < imax; i++)
                {
                    ibp = pp->entrylist[i];
                    if(ibp->bases > (size_t) pp->limit &&
                       (ibp->htg == 1 || ibp->htg == 2 || ibp->htg == 4))
                    {
                        ErrPostEx(SEV_WARNING, ERR_ENTRY_LongHTGSSequence,
                                  "HTGS Phase 0/1/2 sequence %s|%s exceeds length limit %ld: entry has been processed regardless of this problem",
                                  ibp->locusname, ibp->acnum, pp->limit);
                    }
                }
            }
        }
        if(!reject_set)
        {
// LCOV_EXCL_START
// Excluded per Mark's request on 12/14/2016
            BuildBioSegHeader(pp, seq_entries, locs);
// LCOV_EXCL_STOP

            if (!OutputEmblAsn(seq_long, pp, seq_entries))
                reject_set = true;
        }
        if(!reject_set)
        {
            for(i = 0; i < imax; i++)
            {
                ibp = pp->entrylist[i];
                ErrPostEx(SEV_INFO, ERR_ENTRY_Parsed,
                          "OK - entry parsed successfully: \"%s|%s\".",
                          ibp->locusname, ibp->acnum);
            }
            total = imax;
        }
        else
        {
            ErrPostEx(SEV_WARNING, ERR_SEGMENT_Rejected,
                      "Reject the whole segmented set.");
            for(i = 0; i < imax; i++)
            {
                ibp = pp->entrylist[i];
                ErrPostEx(SEV_ERROR, ERR_ENTRY_Skipped,
                          "Entry skipped: \"%s|%s\".",
                          ibp->locusname, ibp->acnum);
            }
        }
    }

    FtaDeletePrefix(PREFIX_LOCUS | PREFIX_ACCESSION);

    ErrPostEx(SEV_INFO, ERR_ENTRY_ParsingComplete,
              "COMPLETED : SUCCEEDED = %d; SKIPPED = %d.",
              total, imax - total);
    return true;
}

/**********************************************************/
char* GetEmblDiv(Uint1 num)
{
    if(num > 15)
        return(NULL);
    return((char*) ParFlat_Embl_DIV_array[num]);
}

/**********************************************************/
CRef<objects::CEMBL_block> XMLGetEMBLBlock(ParserPtr pp, char* entry, objects::CMolInfo& mol_info,
                                                       char** gbdiv, objects::CBioSource* bio_src,
                                                       TStringList& dr_ena, TStringList& dr_biosample)
{
    CRef<objects::CEMBL_block> embl(new objects::CEMBL_block),
                                           ret;

    IndexblkPtr  ibp;
    char*      bptr;
    char*      kw;
    char*      kwp;
    Int2         div;

    bool         pat_ref = false;
    bool         est_kwd = false;
    bool         sts_kwd = false;
    bool         gss_kwd = false;
    bool         htc_kwd = false;
    bool         fli_kwd = false;
    bool         wgs_kwd = false;
    bool         tpa_kwd = false;
    bool         env_kwd = false;
    bool         mga_kwd = false;
    bool         tsa_kwd = false;
    bool         tls_kwd = false;
    bool         cancelled;

    char*      tempdiv;
    Int2         thtg;
    char*      p;
    char*      r;
    Int4         i;
    Char         dataclass[4];

    ibp = pp->entrylist[pp->curindx];

    bool if_cds = XMLCheckCDS(entry, ibp->xip);

    if(ibp->psip.NotEmpty())
        pat_ref = true;

    if (!ibp->keywords.empty())
    {
        embl->SetKeywords().swap(ibp->keywords);
        ibp->keywords.clear();
    }
    else
        XMLGetKeywords(entry, ibp->xip, embl->SetKeywords());

    ITERATE(TKeywordList, key, embl->GetKeywords())
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
    div = fta_StringMatch(ParFlat_Embl_DIV_array, bptr);
    dataclass[0] = '\0';
    if(bptr != NULL)
    {
        bptr[3] = '\0';
        StringCpy(dataclass, bptr);
    }
    if(div < 0)
    {
        ErrPostEx(SEV_REJECT, ERR_DIVISION_UnknownDivCode,
                  "Unknown division code \"%s\" found in Embl flatfile. Record rejected.",
                  bptr);
        if(bptr != NULL)
            MemFree(bptr);
        return ret;
    }

    if(bptr != NULL)
        MemFree(bptr);

    /* Embl has recently (7-19-93, email) decided to change the name of
     * its "UNA"==10 division to "UNC"==16 (for "unclassified")
     */
    if(div == 16)
        div = 10;

    StringCpy(ibp->division, ParFlat_GBDIV_array[div]);

    /* 06-10-96 new HUM division replaces the PRI
     * it's temporarily mapped to 'other' in asn.1 embl-block.
     * Divisions GSS, HUM, HTG, CON, ENV and MUS are mapped to other.
     */
    thtg = (div == 18) ? 6 : div;
    *gbdiv = StringSave(ParFlat_GBDIV_array[thtg]);

    if (div <= 15)
        embl->SetDiv(static_cast<objects::CEMBL_block_Base::TDiv>(div));

    p = *gbdiv;
    if(ibp->is_tpa &&
       (StringCmp(p, "EST") == 0 || StringCmp(p, "GSS") == 0 ||
        StringCmp(p, "PAT") == 0 || StringCmp(p, "HTG") == 0))
    {
        ErrPostEx(SEV_REJECT, ERR_DIVISION_BadTPADivcode,
                  "Division code \"%s\" is not legal for TPA records. Entry dropped.", p);
        return ret;
    }

    if(ibp->is_tsa && StringCmp(p, "TSA") != 0)
    {
        ErrPostEx(SEV_REJECT, ERR_DIVISION_BadTSADivcode,
                  "Division code \"%s\" is not legal for TSA records. Entry dropped.", p);
        return ret;
    }

    cancelled = IsCancelled(embl->GetKeywords());

    if(div == 19)                       /* HTG */
    {
        if (!HasHtg(embl->GetKeywords()))
        {
            ErrPostEx(SEV_ERROR, ERR_DIVISION_MissingHTGKeywords,
                      "Division is HTG, but entry lacks HTG-related keywords. Entry dropped.");
            return ret;
        }
        tempdiv = StringSave("HTG");
    }
    else
        tempdiv = NULL;

    fta_check_htg_kwds(embl->SetKeywords(), ibp, mol_info);

    XMLDefVsHTGKeywords(mol_info.GetTech(), entry, ibp->xip, cancelled);
    if ((mol_info.GetTech() == objects::CMolInfo::eTech_htgs_0 || mol_info.GetTech() == objects::CMolInfo::eTech_htgs_1 ||
        mol_info.GetTech() == objects::CMolInfo::eTech_htgs_2) && *gbdiv != NULL)
    {
        MemFree(*gbdiv);
        *gbdiv = NULL;
    }

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
                  "This is apparently a TSA record, but it lacks the required \"TPA\" and/or \"Transcriptome Shotgun Assembly\" keywords. Entry dropped.");
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
        RemoveHtgPhase(embl->SetKeywords());
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
        kwp = StringStr(kw, "GSS");
        if(kwp != NULL && gss_kwd == false)
        {
            ErrPostEx(SEV_WARNING, ERR_KEYWORD_GSSSubstring,
                      "Keyword %s has substring GSS, but no official GSS keywords found",
                      kw);
        }
        MemFree(kw);
    }
    if(!ibp->is_contig)
    {
        bool drop = false;
        Uint1 tech = mol_info.GetTech();
        *gbdiv = check_div(ibp->is_pat, pat_ref, est_kwd, sts_kwd, gss_kwd,
                           if_cds, *gbdiv, &tech, ibp->bases, pp->source,
                           drop);
        if (tech != 0)
            mol_info.SetTech(tech);
        else
            mol_info.ResetTech();

        if(drop)
        {
            return ret;
        }
    }
    else if(*gbdiv != NULL && StringCmp(*gbdiv, "CON") == 0)
    {
        MemFree(*gbdiv);
        *gbdiv = NULL;
    }

    bool has_htc = HasHtc(embl->GetKeywords());

    if(*gbdiv != NULL && StringCmp(*gbdiv, "HTC") == 0 && !has_htc)
    {
        ErrPostEx(SEV_ERROR, ERR_DIVISION_MissingHTCKeyword,
                  "This record is in the HTC division, but lacks the required HTC keyword.");
        return ret;
    }
    if ((*gbdiv == NULL || StringCmp(*gbdiv, "HTC") != 0) && has_htc)
    {
        ErrPostEx(SEV_ERROR, ERR_DIVISION_InvalidHTCKeyword,
                  "This record has the special HTC keyword, but is not in HTC division. If this record has graduated out of HTC, then the keyword should be removed.");
        return ret;
    }

    if(*gbdiv != NULL && StringCmp(*gbdiv, "HTC") == 0)
    {
        r = XMLFindTagValue(entry, ibp->xip, INSDSEQ_MOLTYPE);
        if(r != NULL)
        {
            p = r;
            if(*r == 'm' || *r == 'r')
                p = r + 1;
            else if(StringNCmp(r, "pre-", 4) == 0)
                p = r + 4;
            else if(StringNCmp(r, "transcribed ", 12) == 0)
                p = r + 12;

            if(StringNCmp(p, "RNA", 3) != 0)
            {
                ErrPostEx(SEV_ERROR, ERR_DIVISION_HTCWrongMolType,
                          "All HTC division records should have a moltype of pre-RNA, mRNA or RNA.");
                MemFree(r);
                return ret;
            }
            MemFree(r);
        }
    }

    if (fli_kwd)
        mol_info.SetTech(objects::CMolInfo::eTech_fli_cdna);

    /* will be used in flat file database
     */
    if(*gbdiv != NULL)
    {
        if(StringCmp(*gbdiv, "EST") == 0)
        {
            ibp->EST = true;
            mol_info.SetTech(objects::CMolInfo::eTech_est);
        }
        else if(StringCmp(*gbdiv, "STS") == 0)
        {
            ibp->STS = true;
            mol_info.SetTech(objects::CMolInfo::eTech_sts);
        }
        else if(StringCmp(*gbdiv, "GSS") == 0)
        {
            ibp->GSS = true;
            mol_info.SetTech(objects::CMolInfo::eTech_survey);
        }
        else if(StringCmp(*gbdiv, "HTC") == 0)
        {
            ibp->HTC = true;
            mol_info.SetTech(objects::CMolInfo::eTech_htc);
            MemFree(*gbdiv);
            *gbdiv = NULL;
        }
        else if(StringCmp(*gbdiv, "SYN") == 0 && bio_src != NULL &&
                bio_src->IsSetOrigin() && bio_src->GetOrigin() == 5)     /* synthetic */
        {
            MemFree(*gbdiv);
            *gbdiv = NULL;
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
        fta_remove_keywords(mol_info.GetTech(), embl->SetKeywords());

    if (ibp->is_tpa)
        fta_remove_tpa_keywords(embl->SetKeywords());

    if (ibp->is_tsa)
        fta_remove_tsa_keywords(embl->SetKeywords(), pp->source);

    if (ibp->is_tls)
        fta_remove_tls_keywords(embl->SetKeywords(), pp->source);

    ibp->wgssec[0] = '\0';
    GetExtraAccession(ibp, pp->allow_uwsec, pp->source, embl->SetExtra_acc());


    p = XMLFindTagValue(entry, ibp->xip, INSDSEQ_CREATE_DATE);
    CRef<objects::CDate_std> std_creation_date,
                                         std_update_date;
    if (p != NULL)
    {
        std_creation_date = GetUpdateDate(p, pp->source);
        embl->SetCreation_date().SetStd(*std_creation_date);
        MemFree(p);
    }
    p = XMLFindTagValue(entry, ibp->xip, INSDSEQ_UPDATE_DATE);
    if(p != NULL)
    {
        std_update_date = GetUpdateDate(p, pp->source);
        embl->SetUpdate_date().SetStd(*std_update_date);
        MemFree(p);
    }

    if(std_update_date.Empty() && std_creation_date.NotEmpty())
        embl->SetUpdate_date().SetStd(*std_creation_date);

    GetEmblBlockXref(DataBlk(), ibp->xip, entry, dr_ena, dr_biosample, &ibp->drop, *embl);

    if (StringCmp(dataclass, "ANN") == 0 || StringCmp(dataclass, "CON") == 0)
    {
        if (StringLen(ibp->acnum) == 8 && StringNCmp(ibp->acnum, "CT", 2) == 0)
        {
            bool found = false;
            ITERATE(objects::CEMBL_block::TExtra_acc, acc, embl->SetExtra_acc())
            {
                if (fta_if_wgs_acc(acc->c_str()) == 0 &&
                   ((*acc)[0] == 'C' || (*acc)[0] == 'U'))
                {
                    found = true;
                    break;
                }
            }
            if (found)
                mol_info.SetTech(objects::CMolInfo::eTech_wgs);
        }
    }

    return embl;
}

END_NCBI_SCOPE
