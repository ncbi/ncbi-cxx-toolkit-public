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
 * File Description:  UTF8 converter functions
 *
 */

#include <ncbi_pch.hpp>
#include <util/utf8.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(utf8)

// Translation tables.
// I've put codes from ASCII-7 table here. So in this table should be only 
// 7-bit characters and two special characters - 0x00 (unable to translate) 
// and 0xFF (character should be skipped).

static unsigned char tblTrans[] =
{
    // Latin Base
 // 0    1    2    3    4    5    6    7    8    9    A    B    C    D    E    F
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0 , // 08
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0 , // 09
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  'a',  0,  '"',  0,   0,  '-', // 0A
   0xFF, 0,  '2', '3','\'',  0,   0,  '.',  0,  '1', 'o',  0,  '"',  0,   0,   0 , // 0B
   'A', 'A', 'A', 'A', 'A', 'A', 'A', 'C', 'E', 'E', 'E', 'E', 'I', 'I', 'I', 'I', // 0C
   'D', 'N', 'O', 'O', 'O', 'O', 'O', 'x', 'O', 'U', 'U', 'U', 'U', 'Y',  0,  'B', // 0D 
   'a', 'a', 'a', 'a', 'a', 'a', 'a', 'c', 'e', 'e', 'e', 'e', 'i', 'i', 'i', 'i', // 0E 
   'o', 'n', 'o', 'o', 'o', 'o', 'o', '-', 'o', 'u', 'u', 'u', 'u', 'y',  0,  'y', // 0F 
    // Latin A
 // 0    1    2    3    4    5    6    7    8    9    A    B    C    D    E    F
   'A', 'a', 'A', 'a', 'A', 'a', 'C', 'c', 'C', 'c', 'C', 'c', 'C', 'c', 'D', 'd', // 10 
   'D', 'd', 'E', 'e', 'E', 'e', 'E', 'e', 'E', 'e', 'E', 'e', 'G', 'g', 'G', 'g', // 11 
   'G', 'g', 'G', 'g', 'H', 'h', 'H', 'h', 'I', 'i', 'I', 'i', 'I', 'i', 'I', 'i', // 12 
   'I', 'i', 'J', 'j', 'J', 'j', 'K', 'k', 'k', 'L', 'l', 'L', 'l', 'L', 'l', 'L', // 13 
   'l', 'L', 'l', 'N', 'n', 'N', 'n', 'N', 'n', 'n', 'N', 'n', 'O', 'o', 'O', 'o', // 14 
   'O', 'o', 'O', 'o', 'R', 'r', 'R', 'r', 'R', 'r', 'S', 's', 'S', 's', 'S', 's', // 15 
   'S', 's', 'T', 't', 'T', 't', 'T', 't', 'U', 'u', 'U', 'u', 'U', 'u', 'U', 'u', // 16 
   'U', 'u', 'U', 'u', 'W', 'w', 'Y', 'y', 'Y', 'Z', 'z', 'Z', 'z', 'Z', 'z',  0 , // 17 
    // Latin B
 // 0    1    2    3    4    5    6    7    8    9    A    B    C    D    E    F
   'b', 'B',  0 ,  0 ,  0 ,  0 ,  0 , 'C', 'c', 'D', 'D',  0 ,  0 ,  0 ,  0 ,  0 , // 18 
   'E', 'F', 'f', 'G',  0 ,  0 ,  0 , 'I', 'K', 'k',  0 ,  0 ,  0 , 'N', 'n',  0 , // 19 
   'O', 'o',  0 ,  0 , 'P', 'p', 'R',  0 ,  0 ,  0 ,  0 , 't', 'T', 't', 'T', 'U', // 1A 
   'u',  0 ,  0 , 'Y', 'y', 'Z', 'z', 'Z',  0 ,  0 , 'z',  0 ,  0 ,  0 ,  0 ,  0 , // 1B 
    0 ,  0 ,  0 , '!', 'D', 'd', 'd', 'L', 'L', 'l', 'N', 'N', 'n', 'A', 'a', 'I', // 1C 
   'i', 'O', 'o', 'U', 'u', 'U', 'u', 'U', 'u', 'U', 'u', 'U', 'u',  0 , 'A', 'a', // 1D 
   'A', 'a', 'A', 'a', 'G', 'g', 'G', 'g', 'K', 'k', 'O', 'o', 'O', 'o', 'Z', 'z', // 1E
   'j', 'D', 'D', 'd', 'G', 'g',  0 ,  0 , 'N', 'n', 'A', 'a',  0,   0 , 'O', 'o', // 1F 
   'A', 'a', 'A', 'a', 'E', 'e', 'E', 'e', 'I', 'i', 'I', 'i', 'O', 'o', 'O', 'o', // 20 
   'R', 'r', 'R', 'r', 'U', 'u', 'U', 'u', 'S', 's', 'T', 't',  0 ,  0 , 'H', 'h', // 21 
    0 ,  0 ,  0 ,  0 , 'Z', 'z', 'A', 'a', 'E', 'e', 'O', 'o', 'O', 'o', 'O', 'o', // 22 
   'O', 'o', 'Y', 'y',  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 , // 23
    0 ,  0,   0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 , // 24
    // IPA Extensions
 // 0    1    2    3    4    5    6    7    8    9    A    B    C    D    E    F
    0 , 'a',  0 ,  0 ,  0 ,  0 , 'd', 'd',  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 , // 25
   'g', 'g', 'G',  0 ,  0 ,  0 , 'h' ,'h', 'i', 'i', 'I',  0 ,  0 ,  0 ,  0 ,  0 , // 26
    0,  'm',  0,  'n', 'N',  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0 , // 27
   'R',  0,  's',  0,   0,   0,   0,   0,  't', 'u',  0,   0,   0,   0,   0,  'Y', // 28
   'Z', 'Z', 'z',  'z', 0,   0,   0,   0,  'O', 'B',  0,  'G', 'H', 'j',  0,  'L', // 29
   'q',  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0 , // 2A
    // Spacing Modifiers
 // 0    1    2    3    4    5    6    7    8    9    A    B    C    D    E    F
   'h', 'h', 'j', 'r',  0 ,  0 ,  0 , 'w', 'y','\'', '"','\'','\'','\'','\'','\'', // 2B
   '?', '?', '<', '>', '^', 'v', '^', 'v','\'', '-','\'', '`','\'', '_','\'', '`', // 2C
    0,   0, '\'','\'',  0 ,  0 , '+', '-', '~', '.', '.',  0,  '~', '"' , 0 , 'x', // 2D
    0 ,  0,   0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 , // 2E
    0 , 'l', 's', 'x',  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 , 'v' ,'=', '"',  0   // 2F

};

static unsigned char tblTransA[] =
{
    // Spacing Modifiers
 // 0    1    2    3    4    5    6    7    8    9    A    B    C    D    E    F
   'A', 'a', 'B', 'b', 'B', 'b', 'B', 'b', 'C', 'c', 'D', 'd', 'D', 'd', 'D', 'd', // 1E0
   'D', 'd', 'D', 'd', 'E', 'e', 'E', 'e', 'E', 'e', 'E', 'e', 'E', 'e', 'F', 'f', // 1E1
   'G', 'g', 'H', 'h', 'H', 'h', 'H', 'h', 'H', 'h', 'H', 'h', 'I', 'i', 'I', 'i', // 1E2
   'K', 'k', 'K', 'k', 'K', 'k', 'L', 'l', 'L', 'l', 'L', 'l', 'L', 'l', 'M', 'm', // 1E3
   'M', 'm', 'M', 'm', 'N', 'n', 'N', 'n', 'N', 'n', 'N', 'n', 'O', 'o', 'O', 'o', // 1E4
   'O', 'o', 'O', 'o', 'P', 'p', 'P', 'p', 'R', 'r', 'R', 'r', 'R', 'r', 'R', 'r', // 1E5
   'S', 's', 'S', 's', 'S', 's', 'S', 's', 'S', 's', 'T', 't', 'T', 't', 'T', 't', // 1E6
   'T', 't', 'U', 'u', 'U', 'u', 'U', 'u', 'U', 'u', 'U', 'u', 'V', 'v', 'V', 'v', // 1E7
   'W', 'w', 'W', 'w', 'W', 'w', 'W', 'w', 'W', 'w', 'X', 'x', 'X', 'x', 'Y', 'y', // 1E8
   'Z', 'z', 'Z', 'z', 'Z', 'z', 'h', 't', 'w', 'y', 'a', 'f',  0 ,  0 ,  0 ,  0 , // 1E9
   'A', 'a', 'A', 'a', 'A', 'a', 'A', 'a', 'A', 'a', 'A', 'a', 'A', 'a', 'A', 'a', // 1EA
   'A', 'a', 'A', 'a', 'A', 'a', 'A', 'a', 'E', 'e', 'E', 'e', 'E', 'e', 'E', 'e', // 1EB
   'E', 'e', 'E', 'e', 'E', 'e', 'E', 'e', 'I', 'i', 'I', 'i', 'O', 'o', 'O', 'o', // 1EC
   'O', 'o', 'O', 'o', 'O', 'o', 'O', 'o', 'O', 'o', 'O', 'o', 'O', 'o', 'O', 'o', // 1ED
   'O', 'o', 'O', 'o', 'U', 'u', 'U', 'u', 'U', 'u', 'U', 'u', 'U', 'u', 'U', 'u', // 1EE
   'U', 'u', 'Y', 'y', 'Y', 'y', 'Y', 'y', 'Y', 'y',  0 ,  0 ,  0 ,  0,   0,   0   // 1EF

};

// Macro for return character together with status
// Using in functions returning status their work
//
#define RETURN_S(ch,res)\
{\
    if (status) *status = res;\
    return ch;\
}

// Macro for return character together with status and length 
// Using in functions returning status and length their work
//
#define RETURN_LS(ch,len,res)\
{\
    if (seq_len) *seq_len = len;\
    if (status) *status = res;\
    return ch;\
}


// Convert first UTF-8 symbol of "src" into ASCII-7 character.
// "ascii_table" specifies whether to use ASCII-7 translation tables.
// Length of the retrieved UTF-8 symbol is returned in "*seq_len"
// (if "seq_len" is not NULL).
// Return resulting ASCII-7 character.
// NOTE:  If the UTF-8 symbol has no ASCII-7 equivalent, then return
//        kOutrangeChar or hSkipChar.
//
char StringToChar(const string&      src,
                  size_t*            seq_len,
                  bool               ascii_table,
                  EConversionStatus* status)
{
    long              dst_code;  // UTF-code symbol code
    unsigned char     dst_char;  // Result character
    EConversionStatus stat;      // Temporary status     

    // Process one UTF character
    dst_code = StringToCode(src, seq_len, &stat);
    if (status) *status = stat;
    // If it was happily
    if (stat == eSuccess) {
        // Conversion
        if (ascii_table) {
            // Convert into appropriate 7-bit character via conversion table 
            dst_char = CodeToChar(dst_code, status);
            return dst_char;
        }    
        else
        {
            // if character greater than 127 (0x7F) than substitute it 
            // with kOutrangeChar, else leave it as is.
            if (dst_code > 0x7F) {
                RETURN_S (kOutrangeChar, eOutrangeChar);
            }
        }
    }
    // Was error translate char
    return (char)dst_code;
}


// Convert UTF-8 string "src" into the ASCII-7 string with
// graphically similar characters -- using StringToChar().
// Return resulting ASCII-7 string.
//
string StringToAscii(const string& src, bool ascii_table)
{
    string  dst;      // String to result 
    char    ch;       // Temporary UTF symbol code
    size_t  utf_len;  // Length of UTF symbol
    size_t  src_len;  // Length source string

    src_len = src.size();

    for (size_t i = 0; i < src_len; )
    {
        // Process one UTF character
        ch = StringToChar(src.data() + i, &utf_len, ascii_table);
        // Add character to the result vector
        if ( ch != kSkipChar ) dst += ch;
        i += utf_len;
    }
    return dst;
}


// Convert first UTF-8 symbol of "src" into a Unicode symbol code.
// Length of the retrieved UTF-8 symbol is returned in "*seq_len"
// (if "seq_len" is not NULL).
// Return resulting Unicode symbol code.
// NOTE:  If the UTF-8 symbol has no Unicode equivalent, then return
//        kOutrangeChar or hSkipChar.
//
long StringToCode(const string&      src,
                  size_t*            seq_len,
                  EConversionStatus* status)
{
    unsigned char ch = src.data()[0];
    size_t utf_len = 0;
    long dst_code = 0;
        
    // If character less then 0x80 we put it as is
    if (ch < 0x80)
    {
        RETURN_LS (ch, 1, eSuccess)
    } 
    else
    {
        // Determine the length of the UTF-8 symbol in bytes
        if      ((ch & 0xFC) == 0xFC) utf_len = 6; // 6 bytes length
        else if ((ch & 0xF8) == 0xF8) utf_len = 5; // 5 bytes length
        else if ((ch & 0xF0) == 0xF0) utf_len = 4; // 4 bytes length
        else if ((ch & 0xE0) == 0xE0) utf_len = 3; // 3 bytes length
        else if ((ch & 0xC0) == 0xC0) utf_len = 2; // 2 bytes length
        else
        {
            // Bad character. Save it as kOutrangeChar
            RETURN_LS (kOutrangeChar, 1, eOutrangeChar)
        }
    }

    // Broken unicode sequence
    if (utf_len > src.size()) {
        RETURN_LS ((long)kSkipChar, 1, eSkipChar);
    }
        
    unsigned char mask = 0xFF;
    mask = (unsigned char)(mask >> (int)utf_len);
    dst_code = ch & mask;

    for (size_t j = 1; j < utf_len; j++)
    {
        dst_code = dst_code << 6;
        ch = src.data()[j];
        ch &= 0x3F;
        dst_code = dst_code | ch;
    }
    // Return result
    RETURN_LS (dst_code, utf_len, eSuccess)
}


// Convert UTF-8 string "src" into the vector of Unicode symbol codes
// using StringToCode().
// Return resulting vector.
//
vector<long> StringToVector (const string& src)
{
    vector<long> dst;      // String to result 
    long         ch;       // Unicode symbol code
    size_t       utf_len;  // Length of Unicode symbol
    size_t       src_len;  // Length of source string

    src_len = src.size();

    for (size_t i = 0; i < src_len; )
    {
        // Process one UTF character
        ch = StringToCode(src.data()+i, &utf_len);
        // Add character to the result vector
        dst.push_back(ch);
        i += utf_len;
    }
    return dst;
}


// Translate Unicode symbol code "src" into graphically similar ASCII-7
// character.
// Return resulting ASCII-7 character.
// NOTE:  If the Unicode symbol has no ASCII-7 equivalent, then return
//        kOutrangeChar or hSkipChar.
//
char CodeToChar(const long src, EConversionStatus* status)
{
    unsigned char ch;

    if (src < 0x80) RETURN_S ((char)src, eSuccess);
    if ((src >= 0x0300) && (src <= 0x036F)) RETURN_S (kSkipChar, eSkipChar);
    if ((src >= 0x1E00) && (src <= 0x1EFF))
    {
      ch = tblTransA[src-0x1E00];
      if (!ch) RETURN_S (kOutrangeChar, eOutrangeChar)
      else     RETURN_S ((char)ch, eSuccess);
    }
    if ((src >= 0xFE20) && (src <= 0xFE2F)) RETURN_S (kSkipChar, eSkipChar);
    if (src > 0x2FF) RETURN_S (kOutrangeChar, eOutrangeChar);

    ch = tblTrans[src-0x80];
    if (!ch) RETURN_S (kOutrangeChar, eOutrangeChar);

    RETURN_S ((char)ch, eSuccess);
}


END_SCOPE(utf8)
END_NCBI_SCOPE
