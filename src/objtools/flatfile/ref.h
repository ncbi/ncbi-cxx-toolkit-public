/* ref.h
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
 * File Name:  ref.h
 *
 * Author: Karl Sirotkin, Hsiu-Chuan Chen
 *
 * File Description:
 * -----------------
 *
 */

#ifndef _REF_
#define _REF_

#define ParFlat_MISSING_JOURNAL       0
#define ParFlat_UNPUB_JOURNAL         1
#define ParFlat_MONOGRAPH_NOT_JOURNAL 2
#define ParFlat_NORMAL_JOURNAL        3
#define ParFlat_SYMPOSIUM_CITATION    4
#define ParFlat_SUBMITTED             5
#define ParFlat_THESIS_CITATION       6
#define ParFlat_THESIS_IN_PRESS       7
#define ParFlat_IN_PRESS              8
#define ParFlat_PATENT_CITATION       9
#define ParFlat_BOOK_CITATION         10
#define ParFlat_GEN_CITATION          11
#define ParFlat_ONLINE_CITATION       12

#define PAPER_MEDIUM                  1
#define TAPE_MEDIUM                   2
#define FLOPPY_MEDIUM                 3
#define EMAIL_MEDIUM                  4
#define OTHER_MEDIUM                  255

#define ParFlat_Authors               20
#define ParFlat_Journal               21
#define ParFlat_Book                  22
#define ParFlat_Citation              23
#define ParFlat_Title                 24
#define ParFlat_Submission            25
#define ParFlat_Description           26
#define ParFlat_Contents              27
#define ParFlat_Comment               28
#define ParFlat_Ignore                29

#define ParFlat_Cit_let_manuscript    1
#define ParFlat_Cit_let_letter        2
#define ParFlat_Cit_let_thesis        3

#define ParFlat_Cit_book_othertype    0
#define ParFlat_Cit_proc_othertype    1
#define ParFlat_Cit_let_othertype     2

#define ParFlat_Cit_art_journal       1
#define ParFlat_Cit_art_book          2
#define ParFlat_Cit_art_proc          3

#define ParFlat_Author_std            1
#define ParFlat_Author_ml             2
#define ParFlat_Author_str            3

#define ParFlat_ReftypeIgnore         0
#define ParFlat_ReftypeNoParse        1
#define ParFlat_ReftypeThesis         2
#define ParFlat_ReftypeArticle        3
#define ParFlat_ReftypeSubmit         4
#define ParFlat_ReftypeBook           5
#define ParFlat_ReftypePatent         6
#define ParFlat_ReftypeUnpub          7

BEGIN_NCBI_SCOPE

CRef<objects::CPub> journal(ParserPtr pp, char* bptr, char* eptr, CRef<objects::CAuth_list>& auth_list,
                                        CRef<objects::CTitle::C_E>& title, bool has_muid, CRef<objects::CCit_art>& cit_art, Int4 er);
                                        
Int4 fta_remark_is_er(const char* str);

CRef<objects::CPubdesc> sp_refs(ParserPtr pp, DataBlkPtr dbp, Int4 col_data);
CRef<objects::CPubdesc> gb_refs_common(ParserPtr pp, DataBlkPtr dbp, Int4 col_data,
                                                        bool bParser, DataBlkPtr** ppInd, bool& no_auth);

CRef<objects::CPubdesc> DescrRefs(ParserPtr pp, DataBlkPtr dbp, Int4 col_data);

END_NCBI_SCOPE
#endif
