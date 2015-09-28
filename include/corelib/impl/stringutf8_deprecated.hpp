#ifndef CORELIB___STRINGUTF8_DEPRECATED__HPP
#define CORELIB___STRINGUTF8_DEPRECATED__HPP

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
 * Author:  Andrei Gourianov
 *
 *
 */

#define  STRINGUTF8_DEFINITION      1
#define  STRINGUTF8_OBSOLETE_STATIC 0

#if STRINGUTF8_DEFINITION
/////////////////////////////////////////////////////////////////////////////
///
/// CStringUTF8 --
///
///   An UTF-8 string.
///   Stores character data in UTF-8 encoding form.
///   Being initialized, converts source characters into UTF-8.
///   Can convert data back into a particular encoding form (non-UTF8)
///   Supported encodings:
///      ISO 8859-1 (Latin1)
///      Microsoft Windows code page 1252
///      UCS-2, UCS-4 (no surrogates)


// On MSVC2010, we cannot export CStringUTF8
// So, all its methods must be inline
#if !defined(NCBI_COMPILER_MSVC)
#  define __EXPORT_CTOR_STRINGUTF8__ 1
#endif
//#  define __EXPORT_IMPL_STRINGUTF8__ 1
//#  define __EXPORT_CTOR_STRINGUTF8__ 1


#if defined(__EXPORT_IMPL_STRINGUTF8__) || defined(__EXPORT_CTOR_STRINGUTF8__)
#  define NCBI_STRINGUTF8_EXPORT NCBI_XNCBI_EXPORT
#else
#  define NCBI_STRINGUTF8_EXPORT
#endif

class NCBI_STRINGUTF8_EXPORT CStringUTF8_DEPRECATED : public string
{
public:

    /// How to verify the character encoding of the source data
    enum EValidate {
        eNoValidate,
        eValidate
    };

    /// How to interpret zeros in the source character buffer -
    /// as end of string, or as part of the data
    enum ECharBufferType {
        eZeroTerminated, ///< Character buffer is zero-terminated
        eCharBuffer      ///< Zeros are part of the data
    };

    CStringUTF8_DEPRECATED(void) {
    }

    ~CStringUTF8_DEPRECATED(void) {
    }

    /// Copy constructor.
    ///
    /// @param src
    ///   Source UTF-8 string
    /// @param validate
    ///   Verify that the source character encoding is really UTF-8
    CStringUTF8_DEPRECATED(const CStringUTF8_DEPRECATED& src, EValidate validate = eNoValidate);

    /// Constructor from a C/C++ string
    ///
    /// @param src
    ///   Source string
    /// @param encoding
    ///   Character encoding of the source string
    /// @param validate
    ///   Verify the character encoding of the source
    /// @deprecated  Use utility class CUtf8 instead
    CStringUTF8_DEPRECATED(const CTempString src);
    CStringUTF8_DEPRECATED(const char* src );
    CStringUTF8_DEPRECATED(const string& src);
    CStringUTF8_DEPRECATED(const CTempString src,
                  EEncoding encoding,
                  EValidate validate = eNoValidate);
    CStringUTF8_DEPRECATED(const char* src,
                EEncoding encoding,
                EValidate validate = eNoValidate);
    CStringUTF8_DEPRECATED(const string& src,
                EEncoding encoding,
                EValidate validate = eNoValidate);

    /// Constructor from Unicode string
    ///
    /// @param src
    ///   Source string
    /// @deprecated  Use utility class CUtf8 instead
    CStringUTF8_DEPRECATED(const TStringUnicode& src);
#if NCBITOOLKIT_USE_LONG_UCS4
    CStringUTF8_DEPRECATED(const TStringUCS4&    src);
#endif
    CStringUTF8_DEPRECATED(const TStringUCS2&    src);
#if defined(HAVE_WSTRING)
    CStringUTF8_DEPRECATED(const wstring&        src);
#endif

    /// Constructor from Unicode character sequence
    ///
    /// @param src
    ///   Source zero-terminated character buffer
    /// @deprecated  Use utility class CUtf8 instead
    CStringUTF8_DEPRECATED(const TUnicodeSymbol* src);
#if NCBITOOLKIT_USE_LONG_UCS4
    CStringUTF8_DEPRECATED(const TCharUCS4*      src);
#endif
    CStringUTF8_DEPRECATED(const TCharUCS2*      src);
#if defined(HAVE_WSTRING)
    CStringUTF8_DEPRECATED(const wchar_t*        src);
#endif

    /// Constructor from Unicode character sequence
    ///
    /// @param type
    ///   How to interpret zeros in the source character buffer -
    ///   as end of string, or as part of the data
    /// @param src
    ///   Source character buffer
    /// @param char_count
    ///   Number of TChars in the buffer
    /// @deprecated  Use utility class CUtf8 instead
    CStringUTF8_DEPRECATED(ECharBufferType type,
                const TUnicodeSymbol* src, SIZE_TYPE char_count);
#if NCBITOOLKIT_USE_LONG_UCS4
    CStringUTF8_DEPRECATED(ECharBufferType type,
                const TCharUCS4*      src, SIZE_TYPE char_count);
#endif
    CStringUTF8_DEPRECATED(ECharBufferType type,
                const TCharUCS2*      src, SIZE_TYPE char_count);
#if defined(HAVE_WSTRING)
    CStringUTF8_DEPRECATED(ECharBufferType type,
                const wchar_t*        src, SIZE_TYPE char_count);
#endif

    /// Assign UTF8 string
    CStringUTF8_DEPRECATED& operator= (const CStringUTF8_DEPRECATED&  src);

    /// Assign Unicode C++ string
    ///
    /// @param src
    ///   Source string
    /// @deprecated  Use utility class CUtf8 instead
    CStringUTF8_DEPRECATED& operator= (const TStringUnicode& src);
#if NCBITOOLKIT_USE_LONG_UCS4
    CStringUTF8_DEPRECATED& operator= (const TStringUCS4&    src);
#endif
    CStringUTF8_DEPRECATED& operator= (const TStringUCS2&    src);
#if defined(HAVE_WSTRING)
    CStringUTF8_DEPRECATED& operator= (const wstring&        src);
#endif

    /// Assign Unicode C string
    ///
    /// @param src
    ///   Source zero-terminated character buffer
    /// @deprecated  Use utility class CUtf8 instead
    CStringUTF8_DEPRECATED& operator= (const TUnicodeSymbol* src);
#if NCBITOOLKIT_USE_LONG_UCS4
    CStringUTF8_DEPRECATED& operator= (const TCharUCS4*      src);
#endif
    CStringUTF8_DEPRECATED& operator= (const TCharUCS2*      src);
#if defined(HAVE_WSTRING)
    CStringUTF8_DEPRECATED& operator= (const wchar_t*        src);
#endif

    /// Append UTF8 string
    CStringUTF8_DEPRECATED& operator+= (const CStringUTF8_DEPRECATED& src);

    /// Append Unicode C++ string
    ///
    /// @param src
    ///   Source string
    /// @deprecated  Use utility class CUtf8 instead
    CStringUTF8_DEPRECATED& operator+= (const TStringUnicode& src);
#if NCBITOOLKIT_USE_LONG_UCS4
    CStringUTF8_DEPRECATED& operator+= (const TStringUCS4&    src);
#endif
    CStringUTF8_DEPRECATED& operator+= (const TStringUCS2&    src);
#if defined(HAVE_WSTRING)
    CStringUTF8_DEPRECATED& operator+= (const wstring&        src);
#endif

    /// Append Unicode C string
    ///
    /// @param src
    ///   Source zero-terminated character buffer
    /// @deprecated  Use utility class CUtf8 instead
    CStringUTF8_DEPRECATED& operator+= (const TUnicodeSymbol* src);
#if NCBITOOLKIT_USE_LONG_UCS4
    CStringUTF8_DEPRECATED& operator+= (const TCharUCS4*      src);
#endif
    CStringUTF8_DEPRECATED& operator+= (const TCharUCS2*      src);
#if defined(HAVE_WSTRING)
    CStringUTF8_DEPRECATED& operator+= (const wchar_t*        src);
#endif

    /// Assign C/C++ string
    ///
    /// @param src
    ///   Source string
    /// @param encoding
    ///   Character encoding of the source string
    /// @param validate
    ///   Verify the character encoding of the source
    /// @deprecated  Use utility class CUtf8 instead
    CStringUTF8_DEPRECATED& Assign(const CTempString src,
                        EEncoding        encoding,
                        EValidate        validate = eNoValidate);

    /// Assign Unicode C++ string
    ///
    /// @param src
    ///   Source string
    /// @deprecated  Use utility class CUtf8 instead
    CStringUTF8_DEPRECATED& Assign(const TStringUnicode& src);
#if NCBITOOLKIT_USE_LONG_UCS4
    CStringUTF8_DEPRECATED& Assign(const TStringUCS4&    src);
#endif
    CStringUTF8_DEPRECATED& Assign(const TStringUCS2&    src);
#if defined(HAVE_WSTRING)
    CStringUTF8_DEPRECATED& Assign(const wstring&        src);
#endif

    /// Assign Unicode C string
    ///
    /// @param src
    ///   Source zero-terminated character buffer
    /// @deprecated  Use utility class CUtf8 instead
    CStringUTF8_DEPRECATED& Assign(const TUnicodeSymbol* src);
#if NCBITOOLKIT_USE_LONG_UCS4
    CStringUTF8_DEPRECATED& Assign(const TCharUCS4*      src);
#endif
    CStringUTF8_DEPRECATED& Assign(const TCharUCS2*      src);
#if defined(HAVE_WSTRING)
    CStringUTF8_DEPRECATED& Assign(const wchar_t*        src);
#endif

    /// Assign Unicode C string or character buffer
    ///
    /// @param type
    ///   How to interpret zeros in the source character buffer -
    ///   as end of string, or as part of the data
    /// @param src
    ///   Source character buffer
    /// @param char_count
    ///   Number of TChars in the buffer
    /// @deprecated  Use utility class CUtf8 instead
    CStringUTF8_DEPRECATED& Assign(ECharBufferType type,
                        const TUnicodeSymbol* src, SIZE_TYPE char_count);
#if NCBITOOLKIT_USE_LONG_UCS4
    CStringUTF8_DEPRECATED& Assign(ECharBufferType type,
                        const TCharUCS4*      src, SIZE_TYPE char_count);
#endif
    CStringUTF8_DEPRECATED& Assign(ECharBufferType type,
                        const TCharUCS2*      src, SIZE_TYPE char_count);
#if defined(HAVE_WSTRING)
    CStringUTF8_DEPRECATED& Assign(ECharBufferType type,
                        const wchar_t*        src, SIZE_TYPE char_count);
#endif

    /// Assign a single character
    ///
    /// @param ch
    ///   Character
    /// @param encoding
    ///   Character encoding
    /// @deprecated  Use utility class CUtf8 instead
    CStringUTF8_DEPRECATED& Assign(char ch, EEncoding encoding);

    /// Append a C/C++ string
    ///
    /// @param src
    ///   Source string
    /// @param encoding
    ///   Character encoding of the source string
    /// @param validate
    ///   Verify the character encoding of the source
    /// @deprecated  Use utility class CUtf8 instead
    CStringUTF8_DEPRECATED& Append(const CTempString src,
                        EEncoding encoding,
                        EValidate validate = eNoValidate);

    /// Append Unicode C++ string
    ///
    /// @param src
    ///   Source string
    /// @deprecated  Use utility class CUtf8 instead
    CStringUTF8_DEPRECATED& Append(const TStringUnicode& src);
#if NCBITOOLKIT_USE_LONG_UCS4
    CStringUTF8_DEPRECATED& Append(const TStringUCS4&    src);
#endif
    CStringUTF8_DEPRECATED& Append(const TStringUCS2&    src);
#if defined(HAVE_WSTRING)
    CStringUTF8_DEPRECATED& Append(const wstring&        src);
#endif

    /// Append Unicode C string
    ///
    /// @param src
    ///   Source zero-terminated character buffer
    /// @deprecated  Use utility class CUtf8 instead
    CStringUTF8_DEPRECATED& Append(const TUnicodeSymbol* src);
#if NCBITOOLKIT_USE_LONG_UCS4
    CStringUTF8_DEPRECATED& Append(const TCharUCS4*      src);
#endif
    CStringUTF8_DEPRECATED& Append(const TCharUCS2*      src);
#if defined(HAVE_WSTRING)
    CStringUTF8_DEPRECATED& Append(const wchar_t*        src);
#endif

    /// Append Unicode C string or character buffer
    ///
    /// @param type
    ///   How to interpret zeros in the source character buffer -
    ///   as end of string, or as part of the data
    /// @param src
    ///   Source character buffer
    /// @param char_count
    ///   Number of TChars in the buffer
    /// @deprecated  Use utility class CUtf8 instead
    CStringUTF8_DEPRECATED& Append(ECharBufferType type,
                        const TUnicodeSymbol* src, SIZE_TYPE char_count);
#if NCBITOOLKIT_USE_LONG_UCS4
    CStringUTF8_DEPRECATED& Append(ECharBufferType type,
                        const TCharUCS4*      src, SIZE_TYPE char_count);
#endif
    CStringUTF8_DEPRECATED& Append(ECharBufferType type,
                        const TCharUCS2*      src, SIZE_TYPE char_count);
#if defined(HAVE_WSTRING)
    CStringUTF8_DEPRECATED& Append(ECharBufferType type,
                        const wchar_t*        src, SIZE_TYPE char_count);
#endif

    /// Append single character
    ///
    /// @param ch
    ///   Character
    /// @param encoding
    ///   Character encoding
    /// @deprecated  Use utility class CUtf8 instead
    CStringUTF8_DEPRECATED& Append(char ch, EEncoding encoding);

    /// Append single Unicode code point
    ///
    /// @param ch
    ///   Unicode code point
    /// @deprecated  Use utility class CUtf8 instead
    CStringUTF8_DEPRECATED& Append(TUnicodeSymbol ch);

    /// Get the number of symbols (code points) in the string
    ///
    /// @return
    ///   Number of symbols (code points)
    /// @deprecated  Use utility class CUtf8 instead
    SIZE_TYPE GetSymbolCount(void) const;
    
    /// Get the number of symbols (code points) in the string
    ///
    /// @return
    ///   Number of symbols (code points)
    /// @deprecated  Use utility class CUtf8 instead
#if  STRINGUTF8_OBSOLETE_STATIC
    static SIZE_TYPE GetSymbolCount(const CTempString src);
#endif

    /// Get the number of valid UTF-8 symbols (code points) in the buffer
    ///
    /// @param src
    ///   Character buffer
    /// @param buf_size
    ///   The number of bytes in the buffer
    /// @return
    ///   Number of valid symbols (no exception thrown)
    /// @deprecated  Use utility class CUtf8 instead
#if  STRINGUTF8_OBSOLETE_STATIC
    static SIZE_TYPE GetValidSymbolCount(const char* src, SIZE_TYPE buf_size);
#endif

    /// Get the number of valid UTF-8 symbols (code points) in the char buffer
    ///
    /// @param src
    ///   Zero-terminated character buffer, or string
    /// @return
    ///   Number of valid symbols (no exception thrown)
    /// @deprecated  Use utility class CUtf8 instead
#if  STRINGUTF8_OBSOLETE_STATIC
    static SIZE_TYPE GetValidSymbolCount(const CTempString src);
#endif
    
    /// Get the number of valid UTF-8 bytes (code units) in the buffer
    ///
    /// @param src
    ///   Character buffer
    /// @param buf_size
    ///   The number of bytes in the buffer
    /// @return
    ///   Number of valid bytes (no exception thrown)
    /// @deprecated  Use utility class CUtf8 instead
#if  STRINGUTF8_OBSOLETE_STATIC
    static SIZE_TYPE GetValidBytesCount(const char* src, SIZE_TYPE buf_size);
#endif

    /// Get the number of valid UTF-8 bytes (code units) in the char buffer
    ///
    /// @param src
    ///   Zero-terminated character buffer, or string
    /// @return
    ///   Number of valid bytes (no exception thrown)
    /// @deprecated  Use utility class CUtf8 instead
#if  STRINGUTF8_OBSOLETE_STATIC
    static SIZE_TYPE GetValidBytesCount(const CTempString src);
#endif

    /// Check that the character encoding of the string is valid UTF-8
    ///
    /// @return
    ///   Result of the check
    /// @deprecated  Use utility class CUtf8 instead
    bool IsValid(void) const;

    /// Convert to ISO 8859-1 (Latin1) character representation
    ///
    /// Can throw a CStringException if the conversion is impossible
    /// or the string has invalid UTF-8 encoding.
    /// @param substitute_on_error
    ///   If the conversion is impossible, append the provided string
    ///   or, if substitute_on_error equals 0, throw the exception
    /// @deprecated  Use utility class CUtf8 instead
    string AsLatin1(const char* substitute_on_error = 0) const;
    
    /// Convert the string to a single-byte character representation
    ///
    /// Can throw a CStringException if the conversion is impossible
    /// or the string has invalid UTF-8 encoding.
    /// @param encoding
    ///   Desired encoding
    /// @param substitute_on_error
    ///   If the conversion is impossible, append the provided string
    ///   or, if substitute_on_error equals 0, throw the exception
    /// @return
    ///   C++ string
    /// @deprecated  Use utility class CUtf8 instead
    string AsSingleByteString(EEncoding   encoding,
                              const char* substitute_on_error = 0) const;

#if defined(HAVE_WSTRING)
    /// Convert to Unicode (UCS-2 with no surrogates where
    /// sizeof(wchar_t) == 2 and UCS-4 where sizeof(wchar_t) == 4).
    ///
    /// Can throw a CStringException if the conversion is impossible
    /// or the string has invalid UTF-8 encoding.
    /// Defined only if wstring is supported by the compiler.
    ///
    /// @param substitute_on_error
    ///   If the conversion is impossible, append the provided string
    ///   or, if substitute_on_error equals 0, throw the exception
    /// @deprecated  Use utility class CUtf8 instead
    wstring AsUnicode(const wchar_t* substitute_on_error = 0) const;
#endif // HAVE_WSTRING

    /// Convert to UCS-2 for all platforms
    ///
    /// Can throw a CStringException if the conversion is impossible
    /// or the string has invalid UTF-8 encoding.
    ///
    /// @param substitute_on_error
    ///   If the conversion is impossible, append the provided string
    ///   or, if substitute_on_error equals 0, throw the exception
    /// @deprecated  Use utility class CUtf8 instead
    TStringUCS2 AsUCS2(const TCharUCS2* substitute_on_error = 0) const;

    /// Conversion to Unicode string with any base type we need
    /// @deprecated  Use utility class CUtf8 instead
    template <typename TChar> 
    basic_string<TChar> AsBasicString(const TChar* substitute_on_error = 0)
        const;

    /// Conversion to Unicode string with any base type we need
    /// @deprecated  Use utility class CUtf8 instead
    template <typename TChar> 
    static  
    basic_string<TChar> AsBasicString(
        const CTempString src,
        const TChar* substitute_on_error,
        EValidate validate = eNoValidate);

    /// Conversion to Unicode string with any base type we need
    /// @deprecated  Use utility class CUtf8 instead
    template <typename TChar> 
    static basic_string<TChar> AsBasicString(const CTempString src);

    /// Guess the encoding of the C/C++ string
    ///
    /// It can distinguish between UTF-8, Latin1, and Win1252 only
    /// @param src
    ///   Source zero-terminated character buffer
    /// @return
    ///   Encoding
    /// @deprecated  Use utility class CUtf8 instead
#if  STRINGUTF8_OBSOLETE_STATIC
    static EEncoding GuessEncoding(const CTempString src);
#endif
    /// Check the encoding of the C/C++ string
    ///
    /// Check that the encoding of the source is the same, or
    /// is compatible with the specified one
    /// @param src
    ///   Source string
    /// @param encoding
    ///   Character encoding form to check against
    /// @return
    ///   Boolean result: encoding is same or compatible
    /// @deprecated  Use utility class CUtf8 instead
#if  STRINGUTF8_OBSOLETE_STATIC
    static bool MatchEncoding(const CTempString src, EEncoding encoding);
#endif

    /// Give Encoding name as string
    ///
    /// NOTE: 
    ///   Function throws CStringException on attempt to get name of eEncoding_Unknown
    ///
    /// @param encoding
    ///   EEncoding enum
    /// @return
    ///   Encoding name
    /// @deprecated  Use utility class CUtf8 instead
#if  STRINGUTF8_OBSOLETE_STATIC
    static string EncodingToString(EEncoding encoding);
#endif
    
    /// Convert encoding name into EEncoding enum, taking into account synonyms
    /// as per  http://www.iana.org/assignments/character-sets
    ///
    /// NOTE: 
    ///   Function returns eEncoding_Unknown for unsupported encodings
    ///
    /// @param str
    ///   Encoding name
    /// @return
    ///   EEncoding enum
    /// @deprecated  Use utility class CUtf8 instead
#if  STRINGUTF8_OBSOLETE_STATIC
    static EEncoding StringToEncoding(const CTempString str);
#endif
    
    /// Convert encoded character into UTF16
    ///
    /// @param ch
    ///   Encoded character
    /// @param encoding
    ///   Character encoding
    /// @return
    ///   Code point
    /// @deprecated  Use utility class CUtf8 instead
#if  STRINGUTF8_OBSOLETE_STATIC
    static TUnicodeSymbol CharToSymbol(char ch, EEncoding encoding);
#endif
    
    /// Convert Unicode code point into encoded character
    ///
    /// @param sym
    ///   Code point
    /// @param encoding
    ///   Character encoding
    /// @return
    ///   Encoded character
    /// @deprecated  Use utility class CUtf8 instead
#if  STRINGUTF8_OBSOLETE_STATIC
    static char SymbolToChar(TUnicodeSymbol sym, EEncoding encoding);
#endif

    /// Convert sequence of UTF8 code units into Unicode code point
    ///
    /// @param src
    ///   UTF8 zero-terminated buffer
    /// @return
    ///   Unicode code point
    /// @deprecated  Use utility class CUtf8 instead
#if  STRINGUTF8_OBSOLETE_STATIC
    static TUnicodeSymbol Decode(const char*& src);
#ifndef NCBI_COMPILER_WORKSHOP
    /// @deprecated  Use utility class CUtf8 instead
    static TUnicodeSymbol Decode(string::const_iterator& src);
#endif
#endif
    
    /// Determines if a symbol is whitespace
    /// per  http://unicode.org/charts/uca/chart_Whitespace.html
    ///
    /// @param chU
    ///   Unicode code point
    /// @sa
    ///   TruncateSpacesInPlace, TruncateSpaces_Unsafe, TruncateSpaces
    /// @deprecated  Use utility class CUtf8 instead
#if  STRINGUTF8_OBSOLETE_STATIC
    static bool IsWhiteSpace(TUnicodeSymbol chU);
#endif
    
    /// Truncate spaces in the string (in-place)
    ///
    /// @param side
    ///   Which end of the string to truncate spaces from. Default is to
    ///   truncate spaces from both ends (eTrunc_Both).
    /// @return
    ///   Reference to itself
    /// @sa
    ///   IsWhiteSpace, TruncateSpaces_Unsafe, TruncateSpaces
    /// @deprecated  Use utility class CUtf8 instead
    CStringUTF8_DEPRECATED& TruncateSpacesInPlace(NStr::ETrunc side = NStr::eTrunc_Both);

    /// Truncate spaces in the string
    ///
    /// @param str
    ///   source string, in UTF8 encoding
    /// @param side
    ///   Which end of the string to truncate spaces from. Default is to
    ///   truncate spaces from both ends (eTrunc_Both).
    /// @attention
    ///   The lifespan of the result string is the same as one of the source.
    ///   So, for example, if the source is temporary string, the result
    ///   will be invalid right away (will point to already released memory).
    /// @sa
    ///   IsWhiteSpace, TruncateSpacesInPlace, TruncateSpaces, CTempString
    /// @deprecated  Use utility class CUtf8 instead
#if  STRINGUTF8_OBSOLETE_STATIC
    static CTempString TruncateSpaces_Unsafe
    (const CTempString str, NStr::ETrunc side = NStr::eTrunc_Both);
#endif

    /// Truncate spaces in the string
    ///
    /// @param str
    ///   source string, in UTF8 encoding
    /// @param side
    ///   Which end of the string to truncate spaces from. Default is to
    ///   truncate spaces from both ends (eTrunc_Both).
    /// @sa
    ///   IsWhiteSpace, TruncateSpacesInPlace, TruncateSpaces_Unsafe
    /// @deprecated  Use utility class CUtf8 instead
#if  STRINGUTF8_OBSOLETE_STATIC
    static CStringUTF8_DEPRECATED TruncateSpaces(const CTempString str,
                                      NStr::ETrunc side = NStr::eTrunc_Both);
#endif

    /// Convert first character of UTF8 sequence into Unicode
    ///
    /// @param ch
    ///   character
    /// @param more
    ///   if the character is valid, - how many more characters to expect
    /// @return
    ///   non-zero, if the character is valid
    /// @deprecated  Use utility class CUtf8 instead
#if  STRINGUTF8_OBSOLETE_STATIC
    static TUnicodeSymbol  DecodeFirst(char ch, SIZE_TYPE& more);
#endif

    /// Convert next character of UTF8 sequence into Unicode
    ///
    /// @param ch
    ///   character
    /// @param chU
    ///   Unicode code point
    /// @return
    ///   non-zero, if the character is valid
    /// @deprecated  Use utility class CUtf8 instead
#if  STRINGUTF8_OBSOLETE_STATIC
    static TUnicodeSymbol  DecodeNext(TUnicodeSymbol chU, char ch);
#endif

private:

    void   x_Validate(void) const;

    /// Convert Unicode code point into UTF8 and append
    void   x_AppendChar(TUnicodeSymbol ch);
    /// Convert coded character sequence into UTF8 and append
    void   x_Append(const CTempString src,
                    EEncoding encoding,
                    EValidate validate = eNoValidate);

    /// Convert Unicode character sequence into UTF8 and append
    /// Sequence can be in UCS-4 (TChar == (U)Int4), UCS-2 (TChar == (U)Int2)
    /// or in ISO8859-1 (TChar == char)
    template <typename TIterator>
    void x_Append(TIterator from, TIterator to);

    template <typename TChar>
    void x_Append(const TChar* src, SIZE_TYPE to = NPOS,
                  ECharBufferType type = eZeroTerminated);

    template <typename TChar> static
    basic_string<TChar> x_AsBasicString
    (const CTempString src,
     const TChar* substitute_on_error, EValidate validate);

    template <typename TIterator> static
    TUnicodeSymbol x_Decode(TIterator& src);

    /// Check how many bytes is needed to represent the code point in UTF8
    static SIZE_TYPE x_BytesNeeded(TUnicodeSymbol ch);
    /// Check if the character is valid first code unit of UTF8
    static bool   x_EvalFirst(char ch, SIZE_TYPE& more);
    /// Check if the character is valid non-first code unit of UTF8
    static bool   x_EvalNext(char ch);

    // Template class for better error messages
    // from unimplemented template methods
    template<class Type> class CNotImplemented {};
};
#endif //STRINGUTF8_DEFINITION

#endif  /* CORELIB___STRINGUTF8_DEPRECATED__HPP */
