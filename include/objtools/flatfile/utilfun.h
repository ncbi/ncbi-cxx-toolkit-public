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


ncbi::CRef<ncbi::objects::CDate_std> get_full_date PROTO((const Char* s, bool is_ref, Int2 source));

/**********************************************************/

/* relative routines for tokenize string
    */
TokenStatBlkPtr TokenString PROTO((CharPtr str, Char delimiter));
TokenStatBlkPtr TokenStringByDelimiter PROTO((CharPtr str, Char delimiter));
void            FreeTokenstatblk PROTO((TokenStatBlkPtr tsbp));
void            FreeTokenblk PROTO((TokenBlkPtr tbp));
bool            ParseAccessionRange PROTO((TokenStatBlkPtr tsbp, Int4 skip));
void            UnwrapAccessionRange PROTO((const ncbi::objects::CGB_block::TExtra_accessions& extra_accs, ncbi::objects::CGB_block::TExtra_accessions& hist));

/* Return array position of the matched length of string in array_string.
    * Return -1 if no match.
    */
Int2            fta_StringMatch PROTO((const Char **array, const Char* text));

/* Return array position of the matched length of string (ignored case)
    * in array_string.
    * Return -1 if no match.
    */
Int2            StringMatchIcase PROTO((const Char **array, const Char* text));

CharPtr         StringIStr PROTO((const Char* where, const Char *what));

/* Return array position of the string in the array_string.
    * Return -1 if no match.
    */
Int2            MatchArrayString PROTO((const Char **array, const Char *text));
Int2            MatchArrayIString PROTO((const Char **array, const Char *text));

/* Return array position of the string in the array_string if any
    * array_string is in the substring of "text".
    * Return -1 if no match.
    */
Int2            MatchArraySubString PROTO((const Char **array, const Char* text));
Int2            MatchArrayISubString PROTO((const Char **array, const Char* text));

/* Return a string which replace newline to blank and skip "XX" line data.
    */
CharPtr         GetBlkDataReplaceNewLine PROTO((CharPtr bptr, CharPtr eptr, Int2 col_data));

/* Delete any tailing ' ', '\n', '\\', ',', ';', '~', '.', ':'
    * characters.
    */
void            CleanTailNoneAlphaChar PROTO((CharPtr str));
void            CleanTailNoneAlphaCharInString PROTO((std::string& str));

CharPtr         PointToNextToken PROTO((CharPtr ptr));

/* Return the current token which ptr points to and ptr will points to
    * next token after the routine return.
    */
CharPtr         GetTheCurrentToken PROTO((CharPtr PNTR ptr));

/* Search The character letter.
    * Return NULL if not found; otherwise, return a pointer points first
    * occurrence The character.
    */
CharPtr         SrchTheChar PROTO((CharPtr bptr, CharPtr eptr, Char letter));

/* Search The string.
    * Return NULL if not found; otherwise, return a pointer points first
    * occurrence The string.
    */
CharPtr         SrchTheStr PROTO((CharPtr bptr, CharPtr eptr, const char *str));

void            CpSeqId PROTO((InfoBioseqPtr ibp, const ncbi::objects::CSeq_id& id));

void            InfoBioseqFree PROTO((InfoBioseqPtr ibp));
Int2            SrchKeyword PROTO((CharPtr ptr, KwordBlk kwl[]));
bool            CheckLineType PROTO((CharPtr ptr, Int4 line, KwordBlk kwl[], bool after_origin));
CharPtr         SrchNodeType PROTO((DataBlkPtr entry, Int4 type, size_t PNTR len));
DataBlkPtr      TrackNodeType PROTO((DataBlkPtr entry, Int2 type));
void            fta_operon_free PROTO((FTAOperonPtr fop));
ValNodePtr      ConstructValNode PROTO((ValNodePtr head, Uint1 choice, Pointer data));
ValNodePtr      ConstructValNodeInt PROTO((ValNodePtr head, Uint1 choice, Int4 data));
bool            fta_is_tpa_keyword PROTO((const Char PNTR str));
bool            fta_tpa_keywords_check PROTO((const TKeywordList& kwds));
bool            fta_is_tsa_keyword PROTO((CharPtr str));
bool            fta_is_tls_keyword PROTO((CharPtr str));
bool            fta_tsa_keywords_check PROTO((const TKeywordList& kwds, Int2 source));
bool            fta_tls_keywords_check PROTO((const TKeywordList& kwds, Int2 source));
bool            fta_check_mga_keywords PROTO((ncbi::objects::CMolInfo& mol_info, const TKeywordList& kwds));
void            fta_StringCpy PROTO((CharPtr dst, CharPtr src));

void            fta_keywords_check PROTO((const Char PNTR str, bool* estk, bool* stsk, bool* gssk,
    bool* htck, bool* flik, bool* wgsk, bool* tpak,
    bool* envk, bool* mgak, bool* tsak, bool* tlsk));

void            fta_remove_keywords PROTO((Uint1 tech, TKeywordList& kwds));
void            fta_remove_tpa_keywords PROTO((TKeywordList& kwds));
void            fta_remove_tsa_keywords PROTO((TKeywordList& kwds, Int2 source));
void            fta_remove_tls_keywords PROTO((TKeywordList& kwds, Int2 source));
void            fta_remove_env_keywords PROTO((TKeywordList& kwds));

bool            IsCancelled PROTO((const TKeywordList& keywords));
bool            HasHtg PROTO((const TKeywordList& keywords));
void            RemoveHtgPhase PROTO((TKeywordList& keywords));
bool            HasHtc PROTO((const TKeywordList& keywords));
bool            SetTextId PROTO((Uint1 seqtype, ncbi::objects::CSeq_id& seqId, ncbi::objects::CTextseq_id& textId));

void            check_est_sts_gss_tpa_kwds PROTO((ValNodePtr kwds, size_t len,
                                                  IndexblkPtr entry,
                                                  bool tpa_check,
                                                  bool &specialist_db,
                                                  bool &inferential,
                                                  bool &experimental,
                                                  bool &assembly));

#endif
