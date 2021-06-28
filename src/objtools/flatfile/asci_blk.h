/* asci_blk.h
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
 * File Name:  asci_blk.h
 *
 * Author: Karl Sirotkin, Hsiu-Chuan Chen
 *
 * File Description:
 * -----------------
 *
 */

#ifndef _ASCIIBLOCK_
#define _ASCIIBLOCK_

#include "ftablock.h"

BEGIN_NCBI_SCOPE

CRef<objects::CSeq_id> StrToSeqId(char* pch, bool pid);
CRef<objects::CSeq_id> MakeAccSeqId(char* acc, Uint1 seqtype, bool accver, Int2 vernum, bool is_nuc, bool is_tpa);
CRef<objects::CPatent_seq_id> MakeUsptoPatSeqId(char *acc);
CRef<objects::CSeq_id> MakeLocusSeqId(char* locus, Uint1 seqtype);
CRef<objects::CBioseq> CreateEntryBioseq(ParserPtr pp, bool is_nuc);

void StripSerialNumbers(TEntryList& seq_entries);
void PackEntries(TEntryList& seq_entries);

Int4        ScanSequence(bool warn, char** seqptr, std::vector<char>& bsp, unsigned char* conv, Char replacechar, int* numns);
char*     GetGenBankBlock(DataBlkPtr* chain, char* ptr, short* retkw, char* eptr);
void        GetGenBankSubBlock(DataBlkPtr entry, size_t bases);
char*     GetEmblBlock(DataBlkPtr* chain, char* ptr, short* retkw, Parser::EFormat format, char* eptr);
void        GetEmblSubBlock(size_t bases, Parser::ESource source, DataBlkPtr entry);
// LCOV_EXCL_START
// Excluded per Mark's request on 12/14/2016
char*     GetPrfBlock(DataBlkPtr* chain, char* ptr, short* retkw, char* eptr);
// LCOV_EXCL_STOP
void        BuildSubBlock(DataBlkPtr dbp, Int2 subtype, const char *subkw);
void        GetLenSubNode(DataBlkPtr dbp);
char*     SrchNodeSubType(DataBlkPtr entry, Int2 type, Int2 subtype, size_t* len);
char*     GetDescrComment(char* offset, size_t len, Int2 col_data, bool is_htg, bool is_pat);
void        GetExtraAccession(IndexblkPtr ibp, bool allow_uwsec, Parser::ESource source, TAccessionList& accessions);
void        GetSequenceOfKeywords(DataBlkPtr entry, Int2 type, Int2 col_data, TKeywordList& keywords);

bool GetSeqData(ParserPtr pp, DataBlkPtr entry, objects::CBioseq& cpp_bsp,
                      Int4 nodetype, unsigned char* seqconv, Uint1 seq_data_type);
    
unique_ptr<unsigned char[]>   GetDNAConv(void);
unique_ptr<unsigned char[]>   GetProteinConv(void);
void   GetSeqExt(ParserPtr pp, objects::CSeq_loc& seq_loc);

// LCOV_EXCL_START
// Excluded per Mark's request on 12/14/2016
void BuildBioSegHeader(ParserPtr pp, TEntryList& entries,
                       const objects::CSeq_loc& seqloc);
// LCOV_EXCL_STOP

bool        IsSegBioseq(const objects::CSeq_id* id);
char*     check_div(bool pat_acc, bool pat_ref, bool est_kwd,
                            bool sts_kwd, bool gss_kwd, bool if_cds, char* div, unsigned char* tech, size_t bases,
                            Parser::ESource source, bool& drop);
void        EntryCheckDivCode(TEntryList& seq_entries, ParserPtr pp);
void        AddNIDSeqId(objects::CBioseq& bioseq, DataBlkPtr entry, Int2 type, Int2 coldata, Parser::ESource source);
void        DefVsHTGKeywords(Uint1 tech, DataBlkPtr entry, Int2 what, Int2 ori, bool cancelled);
void        XMLDefVsHTGKeywords(Uint1 tech, char* entry, XmlIndexPtr xip, bool cancelled);
void        CheckHTGDivision(char* div, Uint1 tech);
void        fta_sort_biosource(objects::CBioSource& bio);
bool        fta_EntryCheckGBBlock(TEntryList& seq_entries);
void        ShrinkSpaces(char* line);
void        fta_sort_descr(TEntryList& seq_entries);
void        fta_sort_seqfeat_cit(TEntryList& seq_entries);
bool        XMLCheckCDS(char* entry, XmlIndexPtr xip);
void        fta_set_strandedness(TEntryList& seq_entries);

bool GetEmblInstContig(DataBlkPtr entry, objects::CBioseq& bioseq, ParserPtr pp);

void        fta_fix_orgref_div(const objects::CBioseq::TAnnot& annots, objects::COrg_ref& org_ref, objects::CGB_block& gbb);

char*     GetEmblDiv(Uint1 num);
const objects::CSeq_descr& GetDescrPointer(const objects::CSeq_entry& entry);

END_NCBI_SCOPE

#endif
