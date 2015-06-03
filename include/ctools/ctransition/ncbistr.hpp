#ifndef _NCBISTR_
#define _NCBISTR_

/*  $Id$
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
* File Name:  ncbistr.h
*
* Author:  Gish, Kans, Ostell, Schuler, Vakatov, Lavrentiev
*
* Version Creation Date:   1/1/91
*
* C Tolkit Revision: 6.14
*
* File Description:
*   	prototypes for portable string routines
*
* ==========================================================================
*/

#include <corelib/ncbistd.hpp> 
#include <ctools/ctransition/ncbistd.hpp>


/** @addtogroup CToolsBridge
 *
 * @{
 */

BEGIN_CTRANSITION_SCOPE


#undef NLM_EXTERN
#ifdef NLM_IMPORT
#define NLM_EXTERN NLM_IMPORT
#else
#define NLM_EXTERN extern
#endif


#ifdef __cplusplus
extern "C" {
#endif

NLM_EXTERN int LIBCALL Nlm_StrICmp PROTO((const char FAR *a, const char FAR *b));
NLM_EXTERN int LIBCALL Nlm_StrNICmp PROTO((const char FAR *a, const char FAR *b, size_t max));
NLM_EXTERN int LIBCALL Nlm_StrIPCmp PROTO((const char FAR *a, const char FAR *b));
NLM_EXTERN int LIBCALL Nlm_StrNIPCmp PROTO((const char FAR *a, const char FAR *b, size_t max));
NLM_EXTERN Nlm_CharPtr LIBCALL Nlm_StrMove PROTO((char FAR *to, const char FAR *from));
NLM_EXTERN Nlm_CharPtr LIBCALL Nlm_StrSave PROTO((const char FAR *from));
NLM_EXTERN size_t LIBCALL Nlm_StrCnt PROTO((const char FAR *str, const char FAR *x_list));

NLM_EXTERN size_t LIBCALL Nlm_StringLen PROTO((const char *str));
NLM_EXTERN Nlm_CharPtr LIBCALL Nlm_StringCpy PROTO((char FAR *to, const char FAR *from));

NLM_EXTERN Nlm_CharPtr LIBCALL Nlm_StringNCpy PROTO((char FAR *to, const char FAR *from, size_t max));

/*  Copies not more than 'max' bytes from string 'from' to 'to';
 *  The resulting string is guaranteed to be '\0'-terminated
 *  (even if 'from' == NULL) -- unless 'to' == NULL or 'max' <= 0;
 *  'to' fits into the 'max' bytes
 */
NLM_EXTERN Nlm_CharPtr LIBCALL Nlm_StringNCpy_0 PROTO((char FAR *to, const char FAR *from, size_t max));

NLM_EXTERN Nlm_CharPtr LIBCALL Nlm_StringCat PROTO((char FAR *to, const char FAR *from));

/*  Adds (not more than 'max') bytes from string 'from' to the end of
 *  string 'to';
 *  [Attention!]:  then adds '\0' AFTER the last copied non-'\0' byte
 */
NLM_EXTERN Nlm_CharPtr LIBCALL Nlm_StringNCat PROTO((char FAR *to, const char FAR *from, size_t max));

NLM_EXTERN int LIBCALL Nlm_StringCmp PROTO((const char FAR *a, const char FAR *b));
NLM_EXTERN int LIBCALL Nlm_StringNCmp PROTO((const char FAR *a, const char FAR *b, size_t max));
NLM_EXTERN int LIBCALL Nlm_StringICmp PROTO((const char FAR *a, const char FAR *b));
NLM_EXTERN int LIBCALL Nlm_StringNICmp PROTO((const char FAR *a, const char FAR *b, size_t max));
NLM_EXTERN Nlm_CharPtr LIBCALL Nlm_StringChr PROTO((const char FAR *str, int chr));
NLM_EXTERN Nlm_CharPtr LIBCALL Nlm_StringRChr PROTO((const char FAR *str, int chr));
NLM_EXTERN Nlm_CharPtr LIBCALL Nlm_StringPBrk PROTO((const char FAR *str1, const char FAR *str2));
NLM_EXTERN size_t LIBCALL Nlm_StringSpn PROTO((const char FAR *str1, const char FAR *str2));
NLM_EXTERN size_t LIBCALL Nlm_StringCSpn PROTO((const char FAR *str1, const char FAR *str2));
NLM_EXTERN Nlm_CharPtr LIBCALL Nlm_StringStr PROTO((const char FAR *str1, const char FAR *str2));
NLM_EXTERN Nlm_CharPtr LIBCALL Nlm_StringTok PROTO((char FAR *str1, const char FAR *str2));
NLM_EXTERN Nlm_CharPtr LIBCALL  Nlm_StringTokMT PROTO((char FAR *str1, const char FAR *str2, char FAR **tmp));
NLM_EXTERN Nlm_CharPtr LIBCALL Nlm_StringMove PROTO((char FAR *to, const char FAR *from));
NLM_EXTERN Nlm_CharPtr LIBCALL Nlm_StringSave PROTO((const char FAR *from));
NLM_EXTERN Nlm_CharPtr LIBCALL Nlm_StringSaveNoNull PROTO((const char FAR *from));
NLM_EXTERN size_t LIBCALL Nlm_StringCnt PROTO((const char FAR *str, const char FAR *x_list));
NLM_EXTERN char* LIBCALL Nlm_StringUpper PROTO((char *string));
NLM_EXTERN char* LIBCALL Nlm_StringLower PROTO((char *string));

NLM_EXTERN int LIBCALL Nlm_NaturalStringCmp PROTO((Nlm_CharPtr str1, Nlm_CharPtr str2));
NLM_EXTERN int LIBCALL Nlm_NaturalStringICmp PROTO((Nlm_CharPtr str1, Nlm_CharPtr str2));

NLM_EXTERN Nlm_Int2 LIBCALL Nlm_MeshStringICmp PROTO((const char FAR *str1, const char FAR *str2));

NLM_EXTERN Nlm_CharPtr LIBCALL Nlm_StringSearch PROTO((const char FAR *str, const char FAR *sub));
NLM_EXTERN Nlm_CharPtr LIBCALL Nlm_StringISearch PROTO((const char FAR *str, const char FAR *sub));

NLM_EXTERN Nlm_Boolean LIBCALL Nlm_StringHasNoText PROTO((const char FAR *str));
NLM_EXTERN Nlm_Boolean LIBCALL Nlm_StringDoesHaveText PROTO((const char FAR *str));
NLM_EXTERN Nlm_CharPtr LIBCALL Nlm_TrimSpacesAroundString PROTO((Nlm_CharPtr str));
NLM_EXTERN Nlm_CharPtr LIBCALL Nlm_CompressSpaces PROTO((Nlm_CharPtr str));

/* Printing 8-byte integer into platform-independent array of 8 bytes
   memory allocated for this storage */
NLM_EXTERN Nlm_Uint1Ptr LIBCALL Uint8ToBytes(Nlm_Uint8 value);

/* Reading platform-independent array of 8 bytes and transfer it to
   8-bytes integer */
NLM_EXTERN Nlm_Uint8 LIBCALL BytesToUint8(Nlm_Uint1Ptr bytes);

/* Retrieve integer value from "str".
 * On success, "*endptr" will point to the symbol of "str" that
 * appears right after the read number.
 * On conversion error, "*endptr" will be assigned NULL.
 */
NLM_EXTERN Nlm_Int8  LIBCALL Nlm_StringToInt8 (const char* str, const char** endptr);
NLM_EXTERN Nlm_Uint8 LIBCALL Nlm_StringToUint8(const char* str, const char** endptr);

/* Print "value" to "str".
 * Return '\0'-terminated "str" on success.
 * Return NULL if "str_size" is not enough to put the number and '\0'.
 */
NLM_EXTERN char* LIBCALL Nlm_Int8ToString (Nlm_Int8  value, char* str, size_t str_size);
NLM_EXTERN char* LIBCALL Nlm_Uint8ToString(Nlm_Uint8 value, char* str, size_t str_size);

NLM_EXTERN Nlm_Boolean Nlm_StringIsAllDigits PROTO((Nlm_CharPtr str));
NLM_EXTERN Nlm_Boolean Nlm_StringIsAllUpperCase PROTO((Nlm_CharPtr str));
NLM_EXTERN Nlm_Boolean Nlm_StringIsAllLowerCase PROTO((Nlm_CharPtr str));
NLM_EXTERN Nlm_Boolean Nlm_StringIsAllPunctuation PROTO((Nlm_CharPtr str));

/*****************************************************************************
*
*   LabelCopy (to, from, buflen)
*   	Copies the string "from" into "to" for up to "buflen" chars
*   	if "from" is longer than buflen, makes the last character '>'
*   	always puts one '\0' to terminate the string in to
*   	to MUST be one character longer than buflen to leave room for the
*   		last '\0' if from = buflen.
*       returns number of characters transferred to "to"
*
*****************************************************************************/
NLM_EXTERN Nlm_Uint4 LIBCALL Nlm_LabelCopy PROTO((Nlm_CharPtr to, Nlm_CharPtr from, Nlm_Uint4 buflen));

/*****************************************************************************
*
*   LabelCopyExtra (to, from, buflen, prefix, suffix)
*   	Copies the string "from" into "to" for up to "buflen" chars
*   	if all together are longer than buflen, makes the last character '>'
*   	always puts one '\0' to terminate the string in to
*   	to MUST be one character longer than buflen to leave room for the
*   		last '\0' if from = buflen.
*       returns number of characters transferred to "to"
*
*   	if not NULL, puts prefix before from and suffix after from
*   		both contained within buflen
*
*
*****************************************************************************/
NLM_EXTERN Nlm_Uint4 LIBCALL Nlm_LabelCopyExtra PROTO((Nlm_CharPtr to, Nlm_CharPtr from, Nlm_Uint4 buflen, Nlm_CharPtr prefix, Nlm_CharPtr suffix));

NLM_EXTERN void LIBCALL Nlm_LabelCopyNext PROTO((Nlm_CharPtr PNTR to, Nlm_CharPtr from, Nlm_Uint4 PNTR buflen));

/* Some higher-level string manipulation functions */
NLM_EXTERN Nlm_CharPtr LIBCALL StrCpyPtr PROTO ((char FAR *Dest, char FAR *Start, char FAR *Stop));
NLM_EXTERN Nlm_CharPtr LIBCALL StrDupPtr PROTO ((char FAR *Start,char FAR *Stop));
NLM_EXTERN Nlm_CharPtr LIBCALL SkipSpaces PROTO ((char FAR *Line));
NLM_EXTERN Nlm_CharPtr LIBCALL SkipToSpace PROTO ((char FAR *String));
NLM_EXTERN Nlm_CharPtr LIBCALL SkipChar PROTO ((char FAR *String,char Char));
NLM_EXTERN Nlm_CharPtr LIBCALL SkipToChar PROTO ((char FAR *String,char Char));
NLM_EXTERN Nlm_CharPtr LIBCALL SkipPastChar PROTO ((char FAR *String,char Char));
NLM_EXTERN Nlm_CharPtr LIBCALL SkipToString PROTO ((char FAR *String,char FAR *Find));
NLM_EXTERN Nlm_CharPtr LIBCALL NoCaseSkipToString PROTO ((char FAR *String,char FAR *Find));
NLM_EXTERN Nlm_CharPtr LIBCALL SkipPastString PROTO ((char FAR *String,char FAR *Find));
NLM_EXTERN Nlm_CharPtr LIBCALL NoCaseSkipPastString PROTO ((char FAR *String,char FAR *Find));
NLM_EXTERN Nlm_CharPtr LIBCALL SkipSet PROTO ((char FAR *String,char FAR *CharSet));
NLM_EXTERN Nlm_CharPtr LIBCALL SkipToSet PROTO ((char FAR *String,char FAR *CharSet));
NLM_EXTERN Nlm_Boolean LIBCALL StringSub PROTO ((char FAR *String, char Find, char Replace));
NLM_EXTERN Nlm_Boolean LIBCALL StringSubSet PROTO ((char FAR *String,char FAR *FindSet, char Replace));
NLM_EXTERN Nlm_Boolean LIBCALL StringSubString PROTO ((char FAR *String, char FAR *Find, char FAR *Replace, Nlm_Int4 MaxLength));
NLM_EXTERN Nlm_CharPtr LIBCALL StringEnd PROTO ((char FAR *String));
NLM_EXTERN Int4 LIBCALL CountChar PROTO ((char FAR *String, char Char));
NLM_EXTERN Int4 LIBCALL CountStrings PROTO ((char FAR *String, char FAR *Find));
NLM_EXTERN Nlm_CharPtr LIBCALL StripSpaces PROTO ((char FAR *Line));
NLM_EXTERN void LIBCALL CleanSpaces PROTO ((char FAR *Line));

NLM_EXTERN Nlm_Int4 LIBCALL CountSet PROTO ((char FAR *String, char FAR *Set));
NLM_EXTERN Nlm_Int4 LIBCALL StringDiff PROTO ((char FAR *This, char FAR *That));
NLM_EXTERN Nlm_Int4 LIBCALL StringDiffNum PROTO ((char FAR *This, char FAR *That, Nlm_Int4 NumChars));
NLM_EXTERN void LIBCALL TruncateString PROTO ((char FAR *String, Nlm_Int4 Length));
NLM_EXTERN Nlm_CharPtr LIBCALL TruncateStringCopy PROTO ((char FAR *String, Nlm_Int4 Length));
NLM_EXTERN Nlm_Int4 LIBCALL BreakString PROTO ((char FAR *String, Nlm_CharPtr  PNTR Words));
NLM_EXTERN void LIBCALL DeleteChar PROTO ((char FAR *String,char Delete));

/* The resulting string will contain only printable characters(and '\n', '\t'
 * and (if "rn_eol"==TRUE) '\r').
 * If "rn_eol" is TRUE then insert '\r' before '\n' symbols not preceeded
 * by '\r';  otherwise, remove all '\r' symbols.
 * NB: The caller is responsible to free the memory allocated for the result.
 */
NLM_EXTERN Nlm_CharPtr LIBCALL Nlm_StringPrintable PROTO((const Nlm_Char PNTR str, Nlm_Boolean rn_eol));
#define StringPrintable Nlm_StringPrintable

/* Merge all subsequent space symbols into a one space(see #SPACE));
 * remove end-of-line dashes.
 * NB: The caller is responsible to free the memory allocated for the result.
 */
NLM_EXTERN Nlm_CharPtr LIBCALL Nlm_text2stream PROTO((const Nlm_Char FAR PNTR str));
#define text2stream Nlm_text2stream

/* Find the place where the "str" string should be broken to fit
 * "max_col" positions;  non-zero "dash" recommends to replace the
 * "max_col" symbol with a dash.
 * Return the recommended length of the line;  do not skip leading
 * spaces;  the last symbol of the recommended line is not a space.
 */
NLM_EXTERN size_t Nlm_stream2text PROTO((const Nlm_Char FAR PNTR str,
                                         size_t max_col,
                                         int PNTR dash));
#define stream2text Nlm_stream2text

/* Rule the stroke "str" according to the "method";  return the resulting line.
 * The resulting line contains "len" characters sharp, plus terminating '\0'.
 * All space symbols will be replaced by ' '.  The leading and trailing
 * spaces do not count.
 * NB: The caller is responsible to free the memory allocated for the result.
 */
typedef enum
{
  RL_Left,
  RL_Right,
  RL_Center,
  RL_Spread
} Nlm_enumRuleLine;
#define enumRuleLine Nlm_enumRuleLine

NLM_EXTERN Nlm_CharPtr LIBCALL Nlm_rule_line PROTO(
                                              (const Nlm_Char FAR PNTR str,
                                               size_t len,
                                               Nlm_enumRuleLine method));
#define rule_line Nlm_rule_line


/* aliases for ANSI functions */
#define Nlm_StrLen	strlen
#define Nlm_StrCpy	strcpy
#define Nlm_StrNCpy	strncpy
#define Nlm_StrCat	strcat
#define Nlm_StrNCat	strncat
#define Nlm_StrCmp	strcmp
#define Nlm_StrNCmp	strncmp
#define Nlm_StrChr	strchr
#define Nlm_StrRChr	strrchr
#define Nlm_StrCSpn	strcspn
#define Nlm_StrNSet	strnset
#define Nlm_StrPBrk	strpbrk
#define Nlm_StrSet	strset
#define Nlm_StrSpn	strspn
#define Nlm_StrStr	strstr
#define Nlm_StrTok	strtok

#ifdef COMP_MSC
#define Nlm_StrUpper  _strupr
#define Nlm_StrLower  _strlwr
#else
NLM_EXTERN char * LIBCALL Nlm_StrUpper PROTO((char *string));
NLM_EXTERN char * LIBCALL Nlm_StrLower PROTO((char *string));
#endif

#define StrLen	 Nlm_StrLen
#define StrCpy	 Nlm_StrCpy
#define StrNCpy	 Nlm_StrNCpy
#define StrCat	 Nlm_StrCat
#define StrNCat	 Nlm_StrNCat
#define StrCmp	 Nlm_StrCmp
#define StrNCmp	 Nlm_StrNCmp
#define StrICmp  Nlm_StrICmp
#define StrNICmp Nlm_StrNICmp
#define StrIPCmp  Nlm_StrIPCmp
#define StrNIPCmp Nlm_StrNIPCmp
#define StrChr	 Nlm_StrChr
#define StrRChr	 Nlm_StrRChr
#define StrCSpn	 Nlm_StrCSpn
#define StrNSet	 Nlm_StrNSet
#define StrPBrk	 Nlm_StrPBrk
#define StrSet	 Nlm_StrSet
#define StrSpn	 Nlm_StrSpn
#define StrStr	 Nlm_StrStr
#define StrTok	 Nlm_StrTok
#define StrMove  Nlm_StrMove
#define StrSave  Nlm_StrSave
#define StrCnt   Nlm_StrCnt
#define StrUpper Nlm_StrUpper
#define StrLower Nlm_StrLower

#define StringLen	Nlm_StringLen
#define StringCpy	Nlm_StringCpy
#define StringNCpy	Nlm_StringNCpy
#define StringNCpy_0	Nlm_StringNCpy_0
#define StringCat	Nlm_StringCat
#define StringNCat	Nlm_StringNCat
#define StringCmp	Nlm_StringCmp
#define StringNCmp	Nlm_StringNCmp
#define StringICmp	Nlm_StringICmp
#define StringNICmp	Nlm_StringNICmp
#define StringChr   Nlm_StringChr
#define StringRChr  Nlm_StringRChr
#define StringPBrk  Nlm_StringPBrk
#define StringSpn   Nlm_StringSpn
#define StringCSpn  Nlm_StringCSpn
#define StringStr   Nlm_StringStr
#define StringTok   Nlm_StringTok
#define StringTokMT Nlm_StringTokMT
#define StringMove	Nlm_StringMove
#define StringSave	Nlm_StringSave
#define StringSaveNoNull Nlm_StringSaveNoNull
#define StringCnt   Nlm_StringCnt
#define StringUpper Nlm_StringUpper
#define StringLower Nlm_StringLower

#define NaturalStringCmp Nlm_NaturalStringCmp
#define NaturalStringICmp Nlm_NaturalStringICmp

#define MeshStringICmp Nlm_MeshStringICmp

#define StringSearch Nlm_StringSearch
#define StringISearch Nlm_StringISearch

#define StringHasNoText Nlm_StringHasNoText
#define StringDoesHaveText Nlm_StringDoesHaveText
#define TrimSpacesAroundString Nlm_TrimSpacesAroundString
#define CompressSpaces Nlm_CompressSpaces 

#define LabelCopy Nlm_LabelCopy
#define LabelCopyExtra Nlm_LabelCopyExtra

/* Disabled due conflicts with C++ NStr:: versions.
   Replaced with inline functions, see below.

#define StringToInt8  Nlm_StringToInt8
#define StringToUint8 Nlm_StringToUint8
#define Int8ToString  Nlm_Int8ToString
#define Uint8ToString Nlm_Uint8ToString
*/

#define StringIsAllDigits  Nlm_StringIsAllDigits
#define StringIsAllUpperCase  Nlm_StringIsAllUpperCase
#define StringIsAllLowerCase  Nlm_StringIsAllLowerCase
#define StringIsAllPunctuation  Nlm_StringIsAllPunctuation

/* Search with Boyer-Moore algorithm */
	
typedef struct substringdata {
  int             d [256];
  size_t          subLen;
  Nlm_Boolean     caseCounts;
  Nlm_Boolean     initialized;
  const char FAR  *sub;        /* points to the original substring, not copied */
} Nlm_SubStringData, PNTR Nlm_SubStringDataPtr;

NLM_EXTERN Nlm_Boolean Nlm_SetupSubString (
  const char FAR *sub,
  Nlm_Boolean caseCounts,
  Nlm_SubStringData PNTR data
);
NLM_EXTERN Nlm_CharPtr Nlm_SearchSubString (
  const char FAR *str,
  Nlm_SubStringData PNTR data
);

#define SubStringData Nlm_SubStringData
#define SubStringDataPtr Nlm_SubStringDataPtr
#define SetupSubString Nlm_SetupSubString
#define SearchSubString Nlm_SearchSubString

/*----------------------------------------*/
/*      Misc Text Oriented Macros         */
/*----------------------------------------*/

/*** PORTABILITY WARNING:  These macros assume the ASCII character set ***/
#define IS_DIGIT(c)	('0'<=(c) && (c)<='9')
#define IS_UPPER(c)	('A'<=(c) && (c)<='Z')
#define IS_LOWER(c)	('a'<=(c) && (c)<='z')
#define IS_ALPHA(c)	(IS_UPPER(c) || IS_LOWER(c))
#define TO_LOWER(c)	((Nlm_Char)(IS_UPPER(c) ? (c)+' ' : (c)))
#define TO_UPPER(c)	((Nlm_Char)(IS_LOWER(c) ? (c)-' ' : (c)))
#define IS_WHITESP(c) (((c) == ' ') || ((c) == '\n') || ((c) == '\r') || ((c) == '\t'))
#define IS_ALPHANUM(c) (IS_ALPHA(c) || IS_DIGIT(c))
#define IS_PRINT(c)	(' '<=(c) && (c)<='~')


/*----------------------------------------*/
/*      Inline                            */
/*----------------------------------------*/

inline
Nlm_Int8 LIBCALL StringToInt8 (const char* str, const char** endptr)
{
    return Nlm_StringToInt8(str, endptr);
}

inline
Nlm_Uint8 LIBCALL StringToUint8(const char* str, const char** endptr)
{
    return Nlm_StringToUint8(str, endptr);
}

inline
char* LIBCALL Int8ToString (Nlm_Int8  value, char* str, size_t str_size)
{
    return Nlm_Int8ToString(value, str, str_size);
}

inline
char* LIBCALL Uint8ToString(Nlm_Uint8 value, char* str, size_t str_size)
{
    return Nlm_Uint8ToString(value, str, str_size);
}


#ifdef __cplusplus
}
#endif


#undef NLM_EXTERN
#ifdef NLM_EXPORT
#define NLM_EXTERN NLM_EXPORT
#else
#define NLM_EXTERN
#endif

#endif


END_CTRANSITION_SCOPE

/* @} */
