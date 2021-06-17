#ifndef UTIL_UTF8__H
#define UTIL_UTF8__H

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
 * Author: Aleksey Vinokurov, Vladimir Ivanov
 *
 * File Description:
 *    UTF8 conversion functions
 *
 */

#include <corelib/ncbistd.hpp>
#include <vector>


/** @addtogroup utf8
 *
 * @{
 */


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(utf8)


// For characters that could not be translated into similar ASCII-7 or
// Unicode character because there is no graphically similar character in
// ASCII-7 table for this one.
//
const char kOutrangeChar = '?';


// 0xFF This means that the character should be skipped in translation to
// ASCII-7. 
// For example, there are a lot of characters which meaning is to modify the
// character next to them.
const char kSkipChar = '\xFF';

// Result (status) conversion Unicode symbols to character
enum EConversionStatus {
    eSuccess,             // Success, result is good
    eSkipChar,            // Result conversion == kSkipChar
    eOutrangeChar         // Result conversion == kOutrangeChar
};


// Convert first UTF-8 symbol of "src" into ASCII-7 character.
// "ascii_table" specifies whether to use ASCII-7 translation tables.
// Length of the retrieved UTF-8 symbol is returned in "*seq_len"
// (if "seq_len" is not NULL).
// Return resulting ASCII-7 character.
// NOTE:  If the UTF-8 symbol has no ASCII-7 equivalent, then return
//        kOutrangeChar or kSkipChar.
//
NCBI_XUTIL_EXPORT
extern char StringToChar(const string&      src,
                         size_t*            seq_len     = 0,
                         bool               ascii_table = true,
                         EConversionStatus* status      = 0);

// Convert UTF-8 string "src" into the ASCII-7 string with
// graphically similar characters -- using StringToChar().
// Return resulting ASCII-7 string.
//
NCBI_XUTIL_EXPORT
extern string StringToAscii(const string& src,
                            bool          ascii_table = true);


// Convert first UTF-8 symbol of "src" into a Unicode symbol code.
// Length of the retrieved UTF-8 symbol is returned in "*seq_len"
// (if "seq_len" is not NULL).
// Return resulting Unicode symbol code.
// NOTE:  If the UTF-8 symbol has no Unicode equivalent, then return
//        kOutrangeChar or kSkipChar.
//
NCBI_XUTIL_EXPORT
extern long StringToCode(const string&      src,
                         size_t*            seq_len = 0,
                         EConversionStatus* status  = 0);

// Convert UTF-8 string "src" into the vector of Unicode symbol codes
// using StringToCode().
// Return resulting vector.
//
NCBI_XUTIL_EXPORT
extern vector<long> StringToVector(const string& src);


// Translate Unicode symbol code "src" into graphically similar ASCII-7
// character.
// Return resulting ASCII-7 character.
// NOTE:  If the Unicode symbol has no ASCII-7 equivalent, then return
//        kOutrangeChar or kSkipChar.
//
NCBI_XUTIL_EXPORT
extern char CodeToChar(const long src, EConversionStatus* status = 0); 


END_SCOPE(utf8)
END_NCBI_SCOPE


/* @} */

#endif  /* UTIL_UTF8__H */
