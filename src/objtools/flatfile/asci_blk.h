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
 * File Name: asci_blk.h
 *
 * Author: Karl Sirotkin, Hsiu-Chuan Chen
 *
 * File Description:
 *
 */

#ifndef _ASCIIBLOCK_
#define _ASCIIBLOCK_

#include "ftablock.h"
#include "entry.h"
#include <objects/seqfeat/Org_ref.hpp>

BEGIN_NCBI_SCOPE

CRef<objects::CSeq_id>        StrToSeqId(const char* pch, bool pid);
CRef<objects::CSeq_id>        MakeAccSeqId(const char* acc, Uint1 seqtype, bool accver, Int2 vernum, bool is_tpa=false);
CRef<objects::CPatent_seq_id> MakeUsptoPatSeqId(const char* acc);
CRef<objects::CSeq_id>        MakeLocusSeqId(const char* locus, objects::CSeq_id::E_Choice seqtype);
CRef<objects::CBioseq>        CreateEntryBioseq(ParserPtr pp);

void StripSerialNumbers(TEntryList& seq_entries);
void PackEntries(TEntryList& seq_entries);

Int4       ScanSequence(bool warn, char** seqptr, std::vector<char>& bsp, unsigned char* conv, Char replacechar, int* numns);
char*      GetGenBankBlock(TDataBlkList& chain, char* ptr, short* retkw, char* eptr);
void       xGetGenBankBlocks(Entry& entry);
void       GetGenBankSubBlock(const DataBlk& entry, size_t bases);
void       xGetGenBankSubBlocks(Entry& entry, size_t bases);
char*      GetEmblBlock(TDataBlkList& chain, char* ptr, short* retkw, Parser::EFormat format, char* eptr);
void       GetEmblSubBlock(size_t bases, Parser::ESource source, const DataBlk& entry);
void       BuildSubBlock(DataBlk& dbp, Int2 subtype, string_view subkw);
void       GetLenSubNode(DataBlk& dbp);
char*      SrchNodeSubType(const DataBlk& entry, Int2 type, Int2 subtype, size_t* len);
string     GetDescrComment(const char* offset, size_t len, Uint2 col_data, bool is_htg, bool is_pat);
void       GetExtraAccession(IndexblkPtr ibp, bool allow_uwsec, Parser::ESource source, TAccessionList& accessions);
void       GetSequenceOfKeywords(const DataBlk& entry, int type, Uint2 col_data, TKeywordList& keywords);

bool GetSeqData(ParserPtr pp, const DataBlk& entry, objects::CBioseq& cpp_bsp, Int4 nodetype, unsigned char* seqconv, Uint1 seq_data_type);

unique_ptr<unsigned char[]> GetDNAConv(void);
unique_ptr<unsigned char[]> GetProteinConv(void);
void                        GetSeqExt(ParserPtr pp, objects::CSeq_loc& seq_loc);

unsigned char* const GetDNAConvTable();
unsigned char* const GetProtConvTable();


bool check_div(bool pat_acc, bool pat_ref, bool est_kwd, bool sts_kwd, bool gss_kwd, bool if_cds, string& div, int* tech, size_t bases, Parser::ESource source, bool& drop);
void EntryCheckDivCode(TEntryList& seq_entries, ParserPtr pp);
void AddNIDSeqId(objects::CBioseq& bioseq, const DataBlk& entry, Int2 type, Int2 coldata, Parser::ESource source);
void DefVsHTGKeywords(int tech, const DataBlk& entry, Int2 what, Int2 ori, bool cancelled);
void XMLDefVsHTGKeywords(int tech, const char* entry, const TXmlIndexList& xil, bool cancelled);
void CheckHTGDivision(const char* div, int tech);
void fta_sort_biosource(objects::CBioSource& bio);
bool fta_EntryCheckGBBlock(TEntryList& seq_entries);
void ShrinkSpaces(char* line);
void ShrinkSpaces(string& line);
void fta_sort_descr(TEntryList& seq_entries);
void fta_sort_seqfeat_cit(TEntryList& seq_entries);
bool XMLCheckCDS(const char* entry, const TXmlIndexList& xil);
void fta_set_strandedness(TEntryList& seq_entries);

bool GetEmblInstContig(const DataBlk& entry, objects::CBioseq& bioseq, ParserPtr pp);

bool fta_orgref_has_taxid(const objects::COrg_ref::TDb& dbtags);
void fta_fix_orgref_div(const objects::CBioseq::TAnnot& annots, objects::COrg_ref* org_ref, objects::CGB_block& gbb);

const char*                GetEmblDiv(Uint1 num);
const objects::CSeq_descr& GetDescrPointer(const objects::CSeq_entry& entry);

END_NCBI_SCOPE

#endif
