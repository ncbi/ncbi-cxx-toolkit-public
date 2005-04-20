#ifndef UTIL_UNICODE__H
#define UTIL_UNICODE__H

/*  $Id$
 * ==========================================================================
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
 * ==========================================================================
 *
 * Author: Aleksey Vinokurov
 *
 * File Description:
 *    Unicode transformation library
 *
 */

#include <corelib/ncbistd.hpp>
#include <string>


/** @addtogroup utf8
 *
 * @{
 */


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(utf8)

/// Types of substitutors.
enum ESubstType
{
    eSkip = 0,      ///< Unicode to be skipped in translation. Usually it is combined mark.
    eAsIs,          ///< Unicodes which should go into the text as is.
    eString,        ///< String of symbols.
    eHTML,          ///< HTML tag or, for example, HTML entity.
    ePicture,       ///< Path to the picture, or maybe picture itself.
    eOther          ///< Something else.
};

/// Structure to keep substititutions for the particular unicode character.
typedef struct
{
    const char* Subst;  ///< Substitutor for unicode.
    ESubstType  Type;   ///< Type of the substitutor.
} SUnicodeTranslation;

typedef SUnicodeTranslation TUnicodePlan[256];
typedef TUnicodePlan* TUnicodeTable[256];
typedef unsigned int TUnicode;


/// Convert Unicode character into ASCII string.
///
/// @param character
///   character to translate
/// @param table
///   Table to use in translation. If Table is not specified,
///   the internal default one will be used.
/// @return
///   Pointer to substitute structure
NCBI_XUTIL_EXPORT
const SUnicodeTranslation*
UnicodeToAscii(TUnicode character, const TUnicodeTable* table=0);

/// Convert UTF8 into Unicode character.
///
/// @param utf
///   Start of UTF8 character buffer
/// @param unicode
///   Pointer to Unicode character to store the result in
/// @return
///   Length of the translated UTF8 or 0 in case of error.
NCBI_XUTIL_EXPORT
size_t UTF8ToUnicode(const char* utf, TUnicode* unicode);

/// Convert Unicode character into UTF8.
///
/// @param unicode
///   Unicode character
/// @param buffer
///   UTF8 buffer to store the result
/// @param buf_length
///   UTF8 buffer size
/// @return
///   Length of the generated UTF8 sequence
NCBI_XUTIL_EXPORT
size_t UnicodeToUTF8(TUnicode unicode, char *buffer, size_t buf_length);

/// Convert Unicode character into UTF8.
///
/// @param unicode
///   Unicode character
/// @return
///   UTF8 buffer as a string
NCBI_XUTIL_EXPORT
string UnicodeToUTF8(TUnicode unicode);

/// Convert UTF8 into ASCII character buffer.
///
/// Decode UTF8 buffer and substitute all Unicodes with appropriate
/// symbols or words from dictionary.
/// @param src
///   UTF8 buffer to decode
/// @param dst
///   Buffer to put the result in
/// @param dst_len
///   Length of the destignation buffer
/// @param table
///   Table to use in translation. If Table is not specified,
///   the internal default one will be used.
/// @return
///   Length of decoded string or -1 if buffer is too small
NCBI_XUTIL_EXPORT
ssize_t UTF8ToAscii(const char* src, char* dst, size_t dst_len,
                    const TUnicodeTable* table=0);

/// Convert UTF8 into ASCII string.
///
/// Decode UTF8 buffer and substitute all Unicodes with appropriate
/// symbols or words from dictionary.
/// @param src
///   UTF8 buffer to decode
/// @param table
///   Table to use in translation. If Table is not specified,
///   the internal default one will be used.
/// @return
///   String with decoded text
NCBI_XUTIL_EXPORT
string UTF8ToAsciiString(const char* src, const TUnicodeTable* table=0);


END_SCOPE(utf8)
END_NCBI_SCOPE

/* @} */


/*
 * ==========================================================================
 * $Log$
 * Revision 1.3  2005/04/20 20:06:59  lavr
 * More changes to use [s]size_t instead of plain int's
 *
 * Revision 1.2  2005/04/20 18:40:09  lavr
 * Change buffer sizes from being 'int' to 'size_t'
 *
 * Revision 1.1  2004/05/06 18:14:53  gouriano
 * Imported from pubmed/xmldb
 *
 * ==========================================================================
 */

#endif  /* UTIL_UNICODE__H */
