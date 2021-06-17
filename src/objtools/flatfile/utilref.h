/* utilref.h
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
 * File Name:  utilref.h
 *
 * Author: Karl Sirotkin, Hsiu-Chuan Chen
 *
 * File Description:
 * -----------------
 */

#ifndef _UTILREF_
#define _UTILREF_

#define GB_REF   0
#define EMBL_REF 1
#define SP_REF   2
#define PIR_REF  3
#define PDB_REF  4
#define ML_REF   5

//#include <objtools/flatfile/asci_blk.h>

#include <objects/biblio/Auth_list.hpp>
#include <objects/biblio/Author.hpp>
#include <objects/general/Name_std.hpp>

BEGIN_NCBI_SCOPE

Int4         valid_pages_range(char* pages, const Char* title, Int4 er, bool inpress);
ValNodePtr   get_tokens(char* str, const Char *delimeter);

void DealWithGenes(TEntryList& seq_entries, ParserPtr pp);
void GetNameStdFromMl(objects::CName_std& namestd, const Char* token);

CRef<objects::CCit_gen> get_error(char* bptr, CRef<objects::CAuth_list>& auth_list, CRef<objects::CTitle::C_E>& title);
CRef<objects::CDate> get_date(const Char* year);
void get_auth_consortium(char* cons, CRef<objects::CAuth_list>& auths);
void get_auth(char* pt, Uint1 format, char* jour, CRef<objects::CAuth_list>& auths);
void get_auth_from_toks(ValNodePtr tokens, Uint1 format, CRef<objects::CAuth_list>& auths);
CRef<objects::CAuthor> get_std_auth(const Char* token, Uint1 format);

END_NCBI_SCOPE

#endif
