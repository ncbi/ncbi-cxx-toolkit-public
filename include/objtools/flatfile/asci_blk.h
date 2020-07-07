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

#include <objtools/flatfile/ftablock.h>

ncbi::CRef<ncbi::objects::CSeq_id> StrToSeqId PROTO((CharPtr pch, bool pid));
ncbi::CRef<ncbi::objects::CSeq_id> MakeAccSeqId(CharPtr acc, Uint1 seqtype, bool accver, Int2 vernum, bool is_nuc, bool is_tpa);
ncbi::CRef<ncbi::objects::CSeq_id> MakeLocusSeqId PROTO((CharPtr locus, Uint1 seqtype));
ncbi::CRef<ncbi::objects::CBioseq> CreateEntryBioseq PROTO((ParserPtr pp, bool is_nuc));

void StripSerialNumbers(TEntryList& seq_entries);
void PackEntries(TEntryList& seq_entries);

Int4        ScanSequence(bool warn, CharPtr PNTR seqptr, std::vector<char>& bsp, Uint1Ptr conv, Char replacechar, Int4Ptr numns);
CharPtr     GetGenBankBlock PROTO((DataBlkPtr PNTR chain, CharPtr ptr, Int2 PNTR retkw, CharPtr eptr));
void        GetGenBankSubBlock PROTO((DataBlkPtr entry, size_t bases));
CharPtr     GetEmblBlock PROTO((DataBlkPtr PNTR chain, CharPtr ptr, Int2 PNTR retkw, Int2 format, CharPtr eptr));
void        GetEmblSubBlock PROTO((size_t bases, Int2 source, DataBlkPtr entry));
// LCOV_EXCL_START
// Excluded per Mark's request on 12/14/2016
CharPtr     GetPrfBlock PROTO((DataBlkPtr PNTR chain, CharPtr ptr, Int2 PNTR retkw, CharPtr eptr));
// LCOV_EXCL_STOP
void        BuildSubBlock PROTO((DataBlkPtr dbp, Int2 subtype, const char *subkw));
void        GetLenSubNode PROTO((DataBlkPtr dbp));
CharPtr     SrchNodeSubType PROTO((DataBlkPtr entry, Int2 type, Int2 subtype, size_t PNTR len));
CharPtr     GetDescrComment PROTO((CharPtr offset, size_t len, Int2 col_data, bool is_htg, bool is_pat));
void        GetExtraAccession PROTO((IndexblkPtr ibp, bool allow_uwsec, Int2 source, TAccessionList& accessions));
void        GetSequenceOfKeywords PROTO((DataBlkPtr entry, Int2 type, Int2 col_data, TKeywordList& keywords));

bool GetSeqData PROTO((ParserPtr pp, DataBlkPtr entry, ncbi::objects::CBioseq& cpp_bsp,
                      Int4 nodetype, Uint1Ptr seqconv, Uint1 seq_data_type);
    
Uint1Ptr    GetDNAConv PROTO((void)));
Uint1Ptr    GetProteinConv PROTO((void));
void   GetSeqExt PROTO((ParserPtr pp, ncbi::objects::CSeq_loc& seq_loc));

// LCOV_EXCL_START
// Excluded per Mark's request on 12/14/2016
void BuildBioSegHeader(ParserPtr pp, TEntryList& entries,
                       const ncbi::objects::CSeq_loc& seqloc);
// LCOV_EXCL_STOP

bool        IsSegBioseq PROTO((const ncbi::objects::CSeq_id* id));
CharPtr     check_div PROTO((bool pat_acc, bool pat_ref, bool est_kwd,
                            bool sts_kwd, bool gss_kwd, bool if_cds, CharPtr div, Uint1Ptr tech, size_t bases,
                            Int2 source, bool& drop));
void        EntryCheckDivCode PROTO((TEntryList& seq_entries, ParserPtr pp));
void        AddNIDSeqId PROTO((ncbi::objects::CBioseq& bioseq, DataBlkPtr entry, Int2 type, Int2 coldata, Int2 source));
void        DefVsHTGKeywords PROTO((Uint1 tech, DataBlkPtr entry, Int2 what, Int2 ori, bool cancelled));
void        XMLDefVsHTGKeywords PROTO((Uint1 tech, CharPtr entry, XmlIndexPtr xip, bool cancelled));
void        CheckHTGDivision PROTO((CharPtr div, Uint1 tech));
void        fta_sort_biosource PROTO((ncbi::objects::CBioSource& bio));
bool        fta_EntryCheckGBBlock PROTO((TEntryList& seq_entries));
void        ShrinkSpaces PROTO((CharPtr line));
void        fta_sort_descr PROTO((TEntryList& seq_entries));
void        fta_sort_seqfeat_cit PROTO((TEntryList& seq_entries));
bool        XMLCheckCDS PROTO((CharPtr entry, XmlIndexPtr xip));
void        fta_set_strandedness PROTO((TEntryList& seq_entries));

bool GetEmblInstContig PROTO((DataBlkPtr entry, ncbi::objects::CBioseq& bioseq, ParserPtr pp));

void        fta_fix_orgref_div PROTO((const ncbi::objects::CBioseq::TAnnot& annots, ncbi::objects::COrg_ref& org_ref, ncbi::objects::CGB_block& gbb));

CharPtr     GetEmblDiv PROTO((Uint1 num));
const ncbi::objects::CSeq_descr& GetDescrPointer PROTO((const ncbi::objects::CSeq_entry& entry));

#endif
