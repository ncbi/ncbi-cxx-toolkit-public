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
 * File Name: ref.h
 *
 * Author: Karl Sirotkin, Hsiu-Chuan Chen
 *
 * File Description:
 *
 */

#ifndef _REF_
#define _REF_

enum ERefRetType {
    ParFlat_MISSING_JOURNAL       = 0,
    ParFlat_UNPUB_JOURNAL         = 1,
    ParFlat_MONOGRAPH_NOT_JOURNAL = 2,
    ParFlat_NORMAL_JOURNAL        = 3,
    ParFlat_SYMPOSIUM_CITATION    = 4,
    ParFlat_SUBMITTED             = 5,
    ParFlat_THESIS_CITATION       = 6,
    ParFlat_THESIS_IN_PRESS       = 7,
    ParFlat_IN_PRESS              = 8,
    ParFlat_PATENT_CITATION       = 9,
    ParFlat_BOOK_CITATION         = 10,
    ParFlat_GEN_CITATION          = 11,
    ParFlat_ONLINE_CITATION       = 12,
};

enum ERefBlockType {
    ParFlat_ReftypeIgnore  = 0,
    ParFlat_ReftypeNoParse = 1,
    ParFlat_ReftypeThesis  = 2,
    ParFlat_ReftypeArticle = 3,
    ParFlat_ReftypeSubmit  = 4,
    ParFlat_ReftypeBook    = 5,
    ParFlat_ReftypePatent  = 6,
    ParFlat_ReftypeUnpub   = 7,
};

BEGIN_NCBI_SCOPE

CRef<objects::CPub>     journal(ParserPtr pp, char* bptr, char* eptr, CRef<objects::CAuth_list>& auth_list, CRef<objects::CTitle::C_E>& title, bool has_muid, CRef<objects::CCit_art>& cit_art, Int4 er);
Int4                    fta_remark_is_er(const string& str);
CRef<objects::CPubdesc> sp_refs(ParserPtr pp, const DataBlk& dbp, Uint2 col_data);
CRef<objects::CPubdesc> DescrRefs(ParserPtr pp, DataBlk& dbp, Uint2 col_data);
CRef<objects::CPubdesc> EmblDescrRefsDr(ParserPtr pp, TEntrezId pmid);
void                    fta_pub_lookup(ParserPtr pp, CRef<objects::CPubdesc> desc);

END_NCBI_SCOPE
#endif
