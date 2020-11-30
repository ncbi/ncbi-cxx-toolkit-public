/* add.h
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
 * File Name:  add.h
 *
 * Author: Karl Sirotkin, Hsiu-Chuan Chen
 *
 * File Description:
 * -----------------
 *
 */

#ifndef ADD_H
#define ADD_H

#include <objects/seqblock/GB_block.hpp>

BEGIN_NCBI_SCOPE

CRef<objects::CSeq_loc> fta_get_seqloc_int_whole(objects::CSeq_id& seq_id, size_t len);

char*       tata_save(char* str);

bool          no_date(Parser::EFormat format, const TSeqdescList& descrs);
bool          no_reference(const objects::CBioseq& bioseq);

bool          check_cds(DataBlkPtr entry, Parser::EFormat format);
void          err_install(IndexblkPtr ibp, bool accver);
void          SeqToDelta(objects::CBioseq& bioseq, Int2 tech);
void          GapsToDelta(objects::CBioseq& bioseq, GapFeatsPtr gfp, unsigned char* drop);
void          AssemblyGapsToDelta(objects::CBioseq& bioseq, GapFeatsPtr gfp, unsigned char* drop);
bool          fta_strings_same(const char* s1, const char* s2);

bool          fta_check_htg_kwds(TKeywordList& kwds, IndexblkPtr ibp, objects::CMolInfo& mol_info);
                               
bool          fta_parse_tpa_tsa_block(objects::CBioseq& bioseq, char* offset, char* acnum,
                                            Int2 vernum, size_t len, Int2 col_data, bool tpa);
                                    
void fta_get_project_user_object(TSeqdescList& descrs, char* offset,
                                 Parser::EFormat format, unsigned char* drop, Parser::ESource source);

void fta_get_dblink_user_object(TSeqdescList& descrs, char* offset,
                                size_t len, Parser::ESource source, unsigned char* drop,
                                CRef<objects::CUser_object>& dbuop);
                                    
Uint1         fta_check_con_for_wgs(objects::CBioseq& bioseq);

    
Int4          fta_fix_seq_loc_id(TSeqLocList& locs, ParserPtr pp, char* location, char* name, bool iscon);
void          fta_parse_structured_comment(char* str, bool& bad, TUserObjVector& objs);
char*       GetQSFromFile(FILE* fd, IndexblkPtr ibp);
void          fta_remove_cleanup_user_object(objects::CSeq_entry& seq_entry);
void          fta_tsa_tls_comment_dblink_check(const objects::CBioseq& bioseq, bool is_tsa);
void          fta_set_molinfo_completeness(objects::CBioseq& bioseq, IndexblkPtr ibp);
bool          fta_number_is_huge(const char* s);

void          fta_create_far_fetch_policy_user_object(objects::CBioseq& bsp, Int4 num);
bool          fta_if_valid_biosample(const char* id, bool dblink);
bool          fta_if_valid_sra(const char* id, bool dblink);
void          StripECO(char* str);
void          fta_add_hist(ParserPtr pp, objects::CBioseq& bioseq, objects::CGB_block::TExtra_accessions& extra_accs, Parser::ESource source,
                                 Int4 acctype, bool pricon, char* acc);

char* StringRStr(char* where, const char *what);
bool    fta_dblink_has_sra(const CRef<objects::CUser_object>& uop);

namespace objects {
    class CScope;
};

bool g_DoesNotReferencePrimary(const objects::CDelta_ext& delta_ext, const objects::CSeq_id& primary, objects::CScope& scope);

END_NCBI_SCOPE

#endif // ADD_H
