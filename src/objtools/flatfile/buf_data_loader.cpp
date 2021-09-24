/* buf_data_loader.cpp
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
 * File Name:  buf_data_loader.cpp
 *
 * Author: Alexey Dobronadezhdin
 *
 * File Description:
 * -----------------
 *      Data loader for buffer based flat file parsing.
 *
 */
#include <ncbi_pch.hpp>

#include "ftacpp.hpp"

#include <objects/general/Object_id.hpp>
#include <objects/general/Dbtag.hpp>
#include <objmgr/impl/tse_loadlock.hpp>
#include <objmgr/scope.hpp>
#include <objects/seqcode/Seq_code_type.hpp>

#include "index.h"
#include "embl.h"
#include "sprot.h"
#include "genbank.h"
#include <objtools/flatfile/flatfile_parse_info.hpp>
#include <objtools/flatfile/flatfile_parser.hpp>
#include <objtools/flatfile/flat2err.h>

#include "utilfun.h"
#include "ftaerr.hpp"
#include "ftablock.h"
#include "gb_ascii.h"
#include "asci_blk.h"
#include "buf_data_loader.h"

#ifdef THIS_FILE
#    undef THIS_FILE
#endif
#define THIS_FILE "buf_data_loader.cpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

CBuffer_DataLoader::CBuffer_DataLoader(const string&, Parser* parser) :
    m_parser(parser)
{}

CDataLoader::TTSE_LockSet CBuffer_DataLoader::GetRecords(const CSeq_id_Handle& idh, EChoice choice)
{
    TTSE_LockSet locks;

    switch (choice) {
        case eBlob:
        case eBioseq:
        case eCore:
        case eBioseqCore:
        case eSequence:
        case eAll:
        {
            TBlobId blob_id = GetBlobId(idh);
            if (blob_id) {
                locks.insert(GetBlobById(blob_id));
            }
            break;
        }
        default:
            break;
    }

    return locks;
}

bool CBuffer_DataLoader::CanGetBlobById() const
{
    return true;
}

CDataLoader::TBlobId CBuffer_DataLoader::GetBlobId(const CSeq_id_Handle& idh)
{
    TBlobId blob_id = new CBlobIdSeq_id(idh);
    return blob_id;
}

CDataLoader::TTSE_Lock CBuffer_DataLoader::GetBlobById(const TBlobId& blob_id)
{
    CTSE_LoadLock lock = GetDataSource()->GetTSE_LoadLock(blob_id);
    if (!lock.IsLoaded()) {
        const CBlobIdSeq_id& id = dynamic_cast<const CBlobIdSeq_id&>(*blob_id).GetValue();
        x_LoadData(id.GetValue(), lock);

        // Mark TSE info as loaded
        lock.SetLoaded();
    }
    return lock;
}

static char* get_sequence_text(ParserPtr parser, const string& accession, int version)
{
    char* ret = nullptr;
    if (parser) {
        if (!parser->accver) {
            if (parser->ff_get_entry)
                ret = (*parser->ff_get_entry)(accession.c_str());
            else if (parser->ff_get_entry_pp)
                ret = (*parser->ff_get_entry_pp)(accession.c_str(), parser);
        }
        else {
            if (parser->ff_get_entry_v)
                ret = (*parser->ff_get_entry_v)(accession.c_str(), version);
            else if (parser->ff_get_entry_v_pp)
                ret = (*parser->ff_get_entry_v_pp)(accession.c_str(), version, parser);
        }
    }

    return ret;
}

static bool get_accession_from_id(const CSeq_id& id, string& accession, int& version)
{
    bool ret = false;
    if (id.IsGenbank() || id.IsEmbl() || id.IsPir() || id.IsSwissprot() || id.IsDdbj() ||
        id.IsTpg() || id.IsTpe() || id.IsTpd()) {

        const CTextseq_id* text_id = id.GetTextseq_Id();
        if (text_id && text_id->IsSetAccession()) {

            version = text_id->IsSetVersion() ? text_id->GetVersion() : 0;
            accession = text_id->GetAccession();
            ret = true;
        }
    }
    else if (id.IsGeneral()) {
        if (id.GetGeneral().IsSetDb() && id.GetGeneral().GetDb() == "GSDB" && id.GetGeneral().IsSetTag() && id.GetGeneral().GetTag().IsId()) {
            accession = NStr::IntToString(id.GetGeneral().GetTag().GetId());
            version = 0;
            ret = true;
        }
    }

    return ret;
}

static int add_entry(ParserPtr pp, const char* acc, Int2 vernum, DataBlkPtr entry)
{
    int i = 0;
    for (; i < pp->indx; i++) {
        if (StringCmp(pp->entrylist[i]->acnum, acc) == 0 &&
            (!pp->accver || pp->entrylist[i]->vernum == vernum))
            break;
    }

    if (i < pp->indx)
        return i;

    IndexblkPtr* temp = (IndexblkPtr*) MemNew((pp->indx + 1) * sizeof(IndexblkPtr));
    copy(pp->entrylist, pp->entrylist + pp->indx, temp);
    MemFree(pp->entrylist);
    pp->entrylist = temp;

    IndexblkPtr cur_block = (IndexblkPtr)MemNew(sizeof(Indexblk));
    StringCpy(cur_block->acnum, acc);
    cur_block->vernum = vernum;
    cur_block->ppp = pp;

    if (pp->format == Parser::EFormat::GenBank) {
        char* q = entry->mOffset;
        if (q != NULL && entry->len != 0 && StringNCmp(q, "LOCUS ", 6) == 0) {
            char* p = StringChr(q, '\n');
            if (p != NULL)
                *p = '\0';
            if (StringLen(q) > 78 && q[28] == ' ' && q[63] == ' ' &&
                q[67] == ' ') {
                cur_block->lc.bases = ParFlat_COL_BASES_NEW;
                cur_block->lc.bp = ParFlat_COL_BP_NEW;
                cur_block->lc.strand = ParFlat_COL_STRAND_NEW;
                cur_block->lc.molecule = ParFlat_COL_MOLECULE_NEW;
                cur_block->lc.topology = ParFlat_COL_TOPOLOGY_NEW;
                cur_block->lc.div = ParFlat_COL_DIV_NEW;
                cur_block->lc.date = ParFlat_COL_DATE_NEW;
            }
            else {
                cur_block->lc.bases = ParFlat_COL_BASES;
                cur_block->lc.bp = ParFlat_COL_BP;
                cur_block->lc.strand = ParFlat_COL_STRAND;
                cur_block->lc.molecule = ParFlat_COL_MOLECULE;
                cur_block->lc.topology = ParFlat_COL_TOPOLOGY;
                cur_block->lc.div = ParFlat_COL_DIV;
                cur_block->lc.date = ParFlat_COL_DATE;
            }
            if (p != NULL)
                *p = '\n';
        }
    }

    pp->entrylist[pp->indx] = cur_block;
    pp->indx++;
    return pp->indx - 1;
}

static void AddToIndexBlk(DataBlkPtr entry, IndexblkPtr ibp, Parser::EFormat format)
{
    char* div;
    char* eptr;
    char* offset;

    if (format != Parser::EFormat::GenBank && format != Parser::EFormat::EMBL)
        return;

    offset = entry->mOffset;
    size_t len = entry->len;

    if (offset == NULL || len == 0)
        return;

    if (format == Parser::EFormat::GenBank) {
        div = offset + ibp->lc.div;
        StringNCpy(ibp->division, div, 3);
        ibp->division[3] = '\0';

        div = offset + ibp->lc.bases;
        eptr = offset + len - 1;
        while (div < eptr && *div == ' ')
            div++;
        ibp->bases = atoi(div);
        return;
    }

    div = StringChr(offset, '\n');
    if (div != NULL) {
        eptr = div;
        len = eptr - offset + 1;
    }
    else
        eptr = offset + len - 1;

    if (len > 5 && StringNCmp(eptr - 3, "BP.", 3) == 0)
        eptr -= 4;

    while (*eptr == ' ' && eptr > offset)
        eptr--;
    while (isdigit(*eptr) != 0 && eptr > offset)
        eptr--;
    ibp->bases = atoi(eptr + 1);
    while (*eptr == ' ' && eptr > offset)
        eptr--;
    if (*eptr == ';')
        eptr--;
    while (isalpha(*eptr) != 0 && eptr > offset)
        eptr--;

    StringNCpy(ibp->division, eptr + 1, 3);
    ibp->division[3] = '\0';
}

CRef<CBioseq> get_bioseq(ParserPtr pp, DataBlkPtr entry, const CSeq_id& id)
{
    IndexblkPtr ibp;
    EntryBlkPtr ebp;
    char*     ptr;
    char*     eptr;

    ibp = pp->entrylist[pp->curindx];
    ebp = (EntryBlkPtr)entry->mpData;
    ptr = entry->mOffset;
    eptr = ptr + entry->len;

    CRef<CBioseq> bioseq(new CBioseq);
    CRef<CSeq_id> new_id(new CSeq_id);
    new_id->Assign(id);
    bioseq->SetId().push_back(new_id);

    bioseq->SetInst().SetRepr(CSeq_inst::eRepr_raw);
    bioseq->SetInst().SetTopology(CSeq_inst::eTopology_linear);
    bioseq->SetInst().SetStrand(CSeq_inst::eStrand_ss);
    bioseq->SetInst().SetLength(static_cast<CSeq_inst::TLength>(ibp->bases));

    bool res = false;
    if (pp->format == Parser::EFormat::EMBL) {

        bioseq->SetInst().SetMol(CSeq_inst::eMol_na);

        Int2 curkw = ParFlat_ID;
        while (curkw != ParFlatEM_END) {
            ptr = GetEmblBlock(&ebp->chain, ptr, &curkw, pp->format, eptr);
        }
        if (ptr < eptr) {

            if (!ibp->is_contig) {
                auto molconv = GetDNAConv();
                res = GetSeqData(pp, *entry, *bioseq, ParFlat_SQ, molconv.get(), eSeq_code_type_iupacna);
             //   MemFree(molconv);
            }
            else {
                pp->farseq = true;
                res = GetEmblInstContig(*entry, *bioseq, pp);
                pp->farseq = false;
            }
        }
    }
    else if (pp->format == Parser::EFormat::GenBank) {
        bioseq->SetInst().SetMol(CSeq_inst::eMol_na);

        Int2 curkw = ParFlat_LOCUS;
        while (curkw != ParFlat_END) {
            ptr = GetGenBankBlock(&ebp->chain, ptr, &curkw, eptr);
        }
        if (ptr < eptr) {

            res = false;
            if (!ibp->is_contig) {
                auto molconv = GetDNAConv();
                res = GetSeqData(pp, *entry, *bioseq, ParFlat_ORIGIN, molconv.get(), eSeq_code_type_iupacna);
               // MemFree(molconv);
            }
            else {
                pp->farseq = true;
                res = GetGenBankInstContig(entry, *bioseq, pp);
                pp->farseq = false;
            }
        }
    }
    else if (pp->format == Parser::EFormat::SPROT) {
        bioseq->SetInst().SetMol(CSeq_inst::eMol_aa);
        Int2 curkw = ParFlat_ID;
        while (curkw != ParFlatSP_END) {
            ptr = GetEmblBlock(&ebp->chain, ptr, &curkw, pp->format, eptr);
        }

        if (ptr < eptr) {
            auto molconv = GetProteinConv();
            res = GetSeqData(pp, *entry, *bioseq, ParFlat_SQ, molconv.get(), eSeq_code_type_iupacna);
        //    MemFree(molconv);
        }
    }

    if (!res)
        bioseq.Reset();

    return bioseq;
}

static DataBlkPtr make_entry(char* entry_str)
{
    DataBlkPtr entry = new DataBlk;

    if (entry != NULL) {
        entry->mType = ParFlat_ENTRYNODE;
        entry->mpNext = NULL;         /* assume no segment at this time */
        entry->mOffset = entry_str;
        entry->len = StringLen(entry->mOffset);
        entry->mpData = (EntryBlkPtr)MemNew(sizeof(EntryBlk));
    }

    return entry;
}

static CRef<CBioseq> parse_entry(ParserPtr pp, char* entry_str, const string& accession, int ver, const CSeq_id& id)
{
    CRef<CBioseq> ret;
    DataBlkPtr entry = make_entry(entry_str);

    if (entry == NULL)
        return ret;

    int ix = add_entry(pp, accession.c_str(), ver, entry),
        old_indx = pp->curindx;

    pp->curindx = ix;

    if (pp->entrylist[ix]->bases == 0) {
        AddToIndexBlk(entry, pp->entrylist[ix], pp->format);
    }

    ret = get_bioseq(pp, entry, id);
    delete entry;
    pp->curindx = old_indx;

    return ret;
}

void CBuffer_DataLoader::x_LoadData(const CSeq_id_Handle& idh, CTSE_LoadLock& lock)
{
    _ASSERT(lock);
    _ASSERT(!lock.IsLoaded());

    string accession;
    int version = 0;
    if (get_accession_from_id(*idh.GetSeqId(), accession, version)) {

        char* entry_str = get_sequence_text(m_parser, accession, version);

        if (entry_str) {

            CRef<CBioseq> bioseq = parse_entry(m_parser, entry_str, accession, version, *idh.GetSeqId());
            if (bioseq.NotEmpty()) {
                
                GetScope().AddBioseq(*bioseq);

                CRef<CSeq_entry> entry(new CSeq_entry);
                entry->SetSeq(*bioseq);
                lock->SetSeq_entry(*entry);
            }
        }
    }
}

const string& CBuffer_DataLoader::GetLoaderNameFromArgs(Parser* )
{
    static const string name("FF2ASN_BUF_BASED_LOADER");
    return name;
}

CBuffer_DataLoader::TRegisterLoaderInfo CBuffer_DataLoader::RegisterInObjectManager(
    CObjectManager& om,
    Parser* params,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority  priority)
{
    TLoaderMaker maker(params);
    CDataLoader::RegisterInObjectManager(om, maker, is_default, priority);
    return maker.GetRegisterInfo();
}

END_SCOPE(objects)

size_t CheckOutsideEntry(ParserPtr pp, const char* acc, Int2 vernum)
{
    char* entry_str = objects::get_sequence_text(pp, acc, vernum);
    if (entry_str == NULL)
        return -1;

    DataBlkPtr entry = objects::make_entry(entry_str);
    if (entry == NULL)
        return -1;

    int ix = objects::add_entry(pp, acc, vernum, entry),
        old_indx = pp->curindx;
    pp->curindx = ix;

    EntryBlkPtr ebp = (EntryBlkPtr)entry->mpData;
    char* ptr = entry->mOffset;    /* points to beginning of the memory line */
    char* eptr = ptr + entry->len;
    Int2 curkw = ParFlat_ID;
    while (curkw != ParFlatEM_END) {
        /* ptr points to current keyword's memory line
        */
        ptr = GetEmblBlock(&ebp->chain, ptr, &curkw, pp->format, eptr);
    }

    if (ptr >= eptr) {
        pp->entrylist[pp->curindx]->drop = 1;
        ErrPostEx(SEV_ERROR, ERR_FORMAT_MissingEnd, "Missing end of the entry, entry dropped.");
        MemFree(entry->mOffset);
        delete entry;
        return(-1);
    }

    if (pp->entrylist[ix]->bases == 0) {
        objects::AddToIndexBlk(entry, pp->entrylist[ix], pp->format);
    }

    delete entry;
    pp->curindx = old_indx;
    return pp->entrylist[ix]->bases;
}
END_NCBI_SCOPE
