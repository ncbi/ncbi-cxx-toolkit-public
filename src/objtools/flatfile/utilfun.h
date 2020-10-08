/* utilfun.h
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
 * File Name:  utilfun.h
 *
 * Author: Karl Sirotkin, Hsiu-Chuan Chen
 *
 * File Description:
 * -----------------
 *      Utility functions for parser.
 *
 */

#ifndef _UTILFUN_
#define _UTILFUN_

/* for unknown Moltype
 */
#define Unknown      0

/* for Unknown Keyword type
 */
#define ParFlat_UNKW 999

#include <objects/general/Date_std.hpp>
#include <objects/seqblock/GB_block.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/object_manager.hpp>

#include "ftablock.h"

BEGIN_NCBI_SCOPE

CRef<objects::CDate_std> get_full_date(const Char* s, bool is_ref, Parser::ESource source);

/**********************************************************/

/* relative routines for tokenize string
    */
TokenStatBlk* TokenString(char* str, Char delimiter);
TokenStatBlk* TokenStringByDelimiter(char* str, Char delimiter);
void            FreeTokenstatblk(TokenStatBlk* tsbp);
void            FreeTokenblk(TokenBlk* tbp);
bool            ParseAccessionRange(TokenStatBlk* tsbp, Int4 skip);
void            UnwrapAccessionRange(const objects::CGB_block::TExtra_accessions& extra_accs, objects::CGB_block::TExtra_accessions& hist);

/* Return array position of the matched length of string in array_string.
    * Return -1 if no match.
    */
Int2            fta_StringMatch(const Char **array, const Char* text);

/* Return array position of the matched length of string (ignored case)
    * in array_string.
    * Return -1 if no match.
    */
Int2            StringMatchIcase(const Char **array, const Char* text);

char*         StringIStr(const Char* where, const Char *what);

/* Return array position of the string in the array_string.
    * Return -1 if no match.
    */
Int2            MatchArrayString(const Char **array, const Char *text);
Int2            MatchArrayIString(const Char **array, const Char *text);

/* Return array position of the string in the array_string if any
    * array_string is in the substring of "text".
    * Return -1 if no match.
    */
Int2            MatchArraySubString(const Char **array, const Char* text);
Int2            MatchArrayISubString(const Char **array, const Char* text);

/* Return a string which replace newline to blank and skip "XX" line data.
    */
char*         GetBlkDataReplaceNewLine(char* bptr, char* eptr, Int2 col_data);

/* Delete any tailing ' ', '\n', '\\', ',', ';', '~', '.', ':'
    * characters.
    */
void            CleanTailNoneAlphaChar(char* str);
void            CleanTailNoneAlphaCharInString(std::string& str);

char*         PointToNextToken(char* ptr);

/* Return the current token which ptr points to and ptr will points to
    * next token after the routine return.
    */
char*         GetTheCurrentToken(char** ptr);

/* Search The character letter.
    * Return NULL if not found; otherwise, return a pointer points first
    * occurrence The character.
    */
char*         SrchTheChar(char* bptr, char* eptr, Char letter);

/* Search The string.
    * Return NULL if not found; otherwise, return a pointer points first
    * occurrence The string.
    */
char*         SrchTheStr(char* bptr, char* eptr, const char *str);

void            CpSeqId(InfoBioseq* ibp, const objects::CSeq_id& id);

void            InfoBioseqFree(InfoBioseq* ibp);
Int2            SrchKeyword(char* ptr, KwordBlk kwl[]);
bool            CheckLineType(char* ptr, Int4 line, KwordBlk kwl[], bool after_origin);
char*         SrchNodeType(DataBlk* entry, Int4 type, size_t* len);
DataBlk*      TrackNodeType(DataBlk* entry, Int2 type);
void            fta_operon_free(FTAOperon* fop);
ValNode*      ConstructValNode(ValNode* head, Uint1 choice, void* data);
ValNode*      ConstructValNodeInt(ValNode* head, Uint1 choice, Int4 data);
bool            fta_is_tpa_keyword(const char* str);
bool            fta_tpa_keywords_check(const TKeywordList& kwds);
bool            fta_is_tsa_keyword(char* str);
bool            fta_is_tls_keyword(char* str);
bool            fta_tsa_keywords_check(const TKeywordList& kwds, Parser::ESource source);
bool            fta_tls_keywords_check(const TKeywordList& kwds, Parser::ESource source);
bool            fta_check_mga_keywords(objects::CMolInfo& mol_info, const TKeywordList& kwds);
void            fta_StringCpy(char* dst, char* src);

void            fta_keywords_check(const char* str, bool* estk, bool* stsk, bool* gssk,
    bool* htck, bool* flik, bool* wgsk, bool* tpak,
    bool* envk, bool* mgak, bool* tsak, bool* tlsk);

void            fta_remove_keywords(Uint1 tech, TKeywordList& kwds);
void            fta_remove_tpa_keywords(TKeywordList& kwds);
void            fta_remove_tsa_keywords(TKeywordList& kwds, Parser::ESource source);
void            fta_remove_tls_keywords(TKeywordList& kwds, Parser::ESource source);
void            fta_remove_env_keywords(TKeywordList& kwds);

bool            IsCancelled(const TKeywordList& keywords);
bool            HasHtg(const TKeywordList& keywords);
void            RemoveHtgPhase(TKeywordList& keywords);
bool            HasHtc(const TKeywordList& keywords);
bool            SetTextId(Uint1 seqtype, objects::CSeq_id& seqId, objects::CTextseq_id& textId);

void            check_est_sts_gss_tpa_kwds(ValNodePtr kwds, size_t len,
                                                  IndexblkPtr entry,
                                                  bool tpa_check,
                                                  bool &specialist_db,
                                                  bool &inferential,
                                                  bool &experimental,
                                                  bool &assembly);

namespace objects {
    class CScope;
}

objects::CScope& GetScope();


END_NCBI_SCOPE

#endif
