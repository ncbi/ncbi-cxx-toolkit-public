#ifndef CORELIB___STRINGUTF8_DEPRECATED__INL
#define CORELIB___STRINGUTF8_DEPRECATED__INL

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
 *     CStringUTF8_DEPRECATED - implementation
 *
 */


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



inline
CStringUTF8_DEPRECATED::CStringUTF8_DEPRECATED(const CStringUTF8_DEPRECATED& src, EValidate validate)
    : string(src)
{
    if (validate == eValidate) {
        CUtf8::x_Validate(*this);
    }
}

inline CStringUTF8_DEPRECATED& CStringUTF8_DEPRECATED::operator= (const CStringUTF8_DEPRECATED& src) {
    string::operator= (src);
    return *this;
}

inline CStringUTF8_DEPRECATED& CStringUTF8_DEPRECATED::operator+= (const CStringUTF8_DEPRECATED& src) {
    string::operator+= (src);
    return *this;
}

template <typename TChar> inline
basic_string<TChar> CStringUTF8_DEPRECATED::AsBasicString(const CTempString src) {
    return CUtf8::AsBasicString<TChar>(src,nullptr,CUtf8::eNoValidate);
}

#if !defined(__EXPORT_CTOR_STRINGUTF8__)

inline CStringUTF8_DEPRECATED::CStringUTF8_DEPRECATED(const CTempString src) {
    assign( CUtf8::AsUTF8(src, eEncoding_ISO8859_1, CUtf8::eNoValidate));
}
inline CStringUTF8_DEPRECATED::CStringUTF8_DEPRECATED(const char* src ) {
    assign( CUtf8::AsUTF8(src, eEncoding_ISO8859_1, CUtf8::eNoValidate));
}
inline CStringUTF8_DEPRECATED::CStringUTF8_DEPRECATED(const string& src) {
    assign( CUtf8::AsUTF8(src, eEncoding_ISO8859_1, CUtf8::eNoValidate));
}


inline
CStringUTF8_DEPRECATED::CStringUTF8_DEPRECATED(
    const CTempString src, EEncoding encoding,EValidate validate) {
    assign( CUtf8::AsUTF8(src, encoding, validate == CStringUTF8_DEPRECATED::eValidate ? CUtf8::eValidate : CUtf8::eNoValidate));
}
inline
CStringUTF8_DEPRECATED::CStringUTF8_DEPRECATED(
    const char* src, EEncoding encoding, EValidate validate) {
    assign( CUtf8::AsUTF8(src, encoding, validate == CStringUTF8_DEPRECATED::eValidate ? CUtf8::eValidate : CUtf8::eNoValidate));
}
inline
CStringUTF8_DEPRECATED::CStringUTF8_DEPRECATED(
    const string& src, EEncoding encoding, EValidate validate) {
    assign( CUtf8::AsUTF8(src, encoding, validate == CStringUTF8_DEPRECATED::eValidate ? CUtf8::eValidate : CUtf8::eNoValidate));
}
inline CStringUTF8_DEPRECATED::CStringUTF8_DEPRECATED(const TStringUnicode& src) {
    assign( CUtf8::AsUTF8(src));
}
#if NCBITOOLKIT_USE_LONG_UCS4
inline CStringUTF8_DEPRECATED::CStringUTF8_DEPRECATED(const TStringUCS4& src) {
    assign( CUtf8::AsUTF8(src));
}
#endif
inline CStringUTF8_DEPRECATED::CStringUTF8_DEPRECATED(const TStringUCS2& src) {
    assign( CUtf8::AsUTF8(src));
}
#if defined(HAVE_WSTRING)
inline CStringUTF8_DEPRECATED::CStringUTF8_DEPRECATED(const wstring& src) {
    assign( CUtf8::AsUTF8(src));
}
#endif
inline CStringUTF8_DEPRECATED::CStringUTF8_DEPRECATED(const TUnicodeSymbol* src) {
    assign( CUtf8::AsUTF8(src));
}
#if NCBITOOLKIT_USE_LONG_UCS4
inline CStringUTF8_DEPRECATED::CStringUTF8_DEPRECATED(const TCharUCS4* src) {
    assign( CUtf8::AsUTF8(src));
}
#endif
inline CStringUTF8_DEPRECATED::CStringUTF8_DEPRECATED(const TCharUCS2* src) {
    assign( CUtf8::AsUTF8(src));
}
#if defined(HAVE_WSTRING)
inline CStringUTF8_DEPRECATED::CStringUTF8_DEPRECATED(const wchar_t* src) {
    assign( CUtf8::AsUTF8(src));
}
#endif

inline
CStringUTF8_DEPRECATED::CStringUTF8_DEPRECATED(
    ECharBufferType type, const TUnicodeSymbol* src, SIZE_TYPE char_count) {
    assign( CUtf8::AsUTF8(src, type == eCharBuffer ? char_count : NPOS));
}
#if NCBITOOLKIT_USE_LONG_UCS4
inline
CStringUTF8_DEPRECATED::CStringUTF8_DEPRECATED(
    ECharBufferType type, const TCharUCS4* src, SIZE_TYPE char_count) {
    assign( CUtf8::AsUTF8(src, type == eCharBuffer ? char_count : NPOS));
}
#endif
inline
CStringUTF8_DEPRECATED::CStringUTF8_DEPRECATED(
    ECharBufferType type, const TCharUCS2* src, SIZE_TYPE char_count) {
    assign( CUtf8::AsUTF8(src, type == eCharBuffer ? char_count : NPOS));
}
inline
CStringUTF8_DEPRECATED::CStringUTF8_DEPRECATED(
    ECharBufferType type, const wchar_t* src, SIZE_TYPE char_count) {
    assign( CUtf8::AsUTF8(src, type == eCharBuffer ? char_count : NPOS));
}
#endif // __EXPORT_CTOR_STRINGUTF8__

#if !defined(__EXPORT_IMPL_STRINGUTF8__)

inline string CStringUTF8_DEPRECATED::AsLatin1(const char* substitute_on_error) const
{
    return CUtf8::AsSingleByteString(*this,eEncoding_ISO8859_1,substitute_on_error);
}
inline wstring CStringUTF8_DEPRECATED::AsUnicode(const wchar_t* substitute_on_error) const
{
    return CUtf8::AsBasicString<wchar_t>(*this,substitute_on_error,CUtf8::eNoValidate);
}
inline TStringUCS2 CStringUTF8_DEPRECATED::AsUCS2(const TCharUCS2* substitute_on_error) const
{
    return CUtf8::AsBasicString<TCharUCS2>(*this,substitute_on_error,CUtf8::eNoValidate);
}
#if  STRINGUTF8_OBSOLETE_STATIC
inline
CStringUTF8_DEPRECATED CStringUTF8_DEPRECATED::TruncateSpaces(const CTempString str,
                                    NStr::ETrunc side)
{
    CStringUTF8_DEPRECATED res;
    res.assign( CUtf8::TruncateSpaces(str,side));
    return res;
}
#endif
inline CStringUTF8_DEPRECATED& CStringUTF8_DEPRECATED::operator= (const TStringUnicode& src) {
    assign( CUtf8::AsUTF8(src));
    return *this;
}
#if NCBITOOLKIT_USE_LONG_UCS4
inline CStringUTF8_DEPRECATED& CStringUTF8_DEPRECATED::operator= (const TStringUCS4& src) {
    assign( CUtf8::AsUTF8(src));
    return *this;
}
#endif
inline CStringUTF8_DEPRECATED& CStringUTF8_DEPRECATED::operator= (const TStringUCS2& src) {
    assign( CUtf8::AsUTF8(src));
    return *this;
}
#if defined(HAVE_WSTRING)
inline CStringUTF8_DEPRECATED& CStringUTF8_DEPRECATED::operator= (const wstring& src) {
    assign( CUtf8::AsUTF8(src));
    return *this;
}
#endif
inline CStringUTF8_DEPRECATED& CStringUTF8_DEPRECATED::operator= (const TUnicodeSymbol* src) {
    assign( CUtf8::AsUTF8(src));
    return *this;
}
#if NCBITOOLKIT_USE_LONG_UCS4
inline CStringUTF8_DEPRECATED& CStringUTF8_DEPRECATED::operator= (const TCharUCS4* src) {
    assign( CUtf8::AsUTF8(src));
    return *this;
}
#endif
inline CStringUTF8_DEPRECATED& CStringUTF8_DEPRECATED::operator= (const TCharUCS2* src) {
    assign( CUtf8::AsUTF8(src));
    return *this;
}
#if defined(HAVE_WSTRING)
inline CStringUTF8_DEPRECATED& CStringUTF8_DEPRECATED::operator= (const wchar_t* src) {
    assign( CUtf8::AsUTF8(src));
    return *this;
}
#endif
inline CStringUTF8_DEPRECATED& CStringUTF8_DEPRECATED::operator+= (const TStringUnicode& src) {
    append( CUtf8::AsUTF8(src));
    return *this;
}
#if NCBITOOLKIT_USE_LONG_UCS4
inline CStringUTF8_DEPRECATED& CStringUTF8_DEPRECATED::operator+= (const TStringUCS4& src) {
    append( CUtf8::AsUTF8(src));
    return *this;
}
#endif
inline CStringUTF8_DEPRECATED& CStringUTF8_DEPRECATED::operator+= (const TStringUCS2& src) {
    append( CUtf8::AsUTF8(src));
    return *this;
}
#if defined(HAVE_WSTRING)
inline CStringUTF8_DEPRECATED& CStringUTF8_DEPRECATED::operator+= (const wstring& src) {
    append( CUtf8::AsUTF8(src));
    return *this;
}
#endif
inline CStringUTF8_DEPRECATED& CStringUTF8_DEPRECATED::operator+= (const TUnicodeSymbol* src) {
    append( CUtf8::AsUTF8(src));
    return *this;
}
#if NCBITOOLKIT_USE_LONG_UCS4
inline CStringUTF8_DEPRECATED& CStringUTF8_DEPRECATED::operator+= (const TCharUCS4* src) {
    append( CUtf8::AsUTF8(src));
    return *this;
}
#endif
inline CStringUTF8_DEPRECATED& CStringUTF8_DEPRECATED::operator+= (const TCharUCS2* src) {
    append( CUtf8::AsUTF8(src));
    return *this;
}
#if defined(HAVE_WSTRING)
inline CStringUTF8_DEPRECATED& CStringUTF8_DEPRECATED::operator+= (const wchar_t* src) {
    append( CUtf8::AsUTF8(src));
    return *this;
}
#endif
inline CStringUTF8_DEPRECATED& CStringUTF8_DEPRECATED::Assign(
    const CTempString src, EEncoding encoding, EValidate validate) {
    assign( CUtf8::AsUTF8(src, encoding, validate == CStringUTF8_DEPRECATED::eValidate ? CUtf8::eValidate : CUtf8::eNoValidate));
    return *this;
}

inline CStringUTF8_DEPRECATED& CStringUTF8_DEPRECATED::Assign(const TStringUnicode& src) {
    assign( CUtf8::AsUTF8(src));
    return *this;
}
#if NCBITOOLKIT_USE_LONG_UCS4
inline CStringUTF8_DEPRECATED& CStringUTF8_DEPRECATED::Assign(const TStringUCS4& src) {
    assign( CUtf8::AsUTF8(src));
    return *this;
}
#endif
inline CStringUTF8_DEPRECATED& CStringUTF8_DEPRECATED::Assign(const TStringUCS2& src) {
    assign( CUtf8::AsUTF8(src));
    return *this;
}
#if defined(HAVE_WSTRING)
inline CStringUTF8_DEPRECATED& CStringUTF8_DEPRECATED::Assign(const wstring& src) {
    assign( CUtf8::AsUTF8(src));
    return *this;
}
#endif
inline CStringUTF8_DEPRECATED& CStringUTF8_DEPRECATED::Assign(const TUnicodeSymbol* src) {
    assign( CUtf8::AsUTF8(src));
    return *this;
}
#if NCBITOOLKIT_USE_LONG_UCS4
inline CStringUTF8_DEPRECATED& CStringUTF8_DEPRECATED::Assign(const TCharUCS4* src) {
    assign( CUtf8::AsUTF8(src));
    return *this;
}
#endif
inline CStringUTF8_DEPRECATED& CStringUTF8_DEPRECATED::Assign(const TCharUCS2* src) {
    assign( CUtf8::AsUTF8(src));
    return *this;
}
#if defined(HAVE_WSTRING)
inline CStringUTF8_DEPRECATED& CStringUTF8_DEPRECATED::Assign(const wchar_t* src) {
    assign( CUtf8::AsUTF8(src));
    return *this;
}
#endif
inline CStringUTF8_DEPRECATED& CStringUTF8_DEPRECATED::Assign(
    ECharBufferType type, const TUnicodeSymbol* src, SIZE_TYPE char_count) {
    assign( CUtf8::AsUTF8(src, type == eCharBuffer ? char_count : NPOS));
    return *this;
}
#if NCBITOOLKIT_USE_LONG_UCS4
inline CStringUTF8_DEPRECATED& CStringUTF8_DEPRECATED::Assign(
    ECharBufferType type, const TCharUCS4* src, SIZE_TYPE char_count) {
    assign( CUtf8::AsUTF8(src, type == eCharBuffer ? char_count : NPOS));
    return *this;
}
#endif
inline CStringUTF8_DEPRECATED& CStringUTF8_DEPRECATED::Assign(
    ECharBufferType type, const TCharUCS2* src, SIZE_TYPE char_count) {
    assign( CUtf8::AsUTF8(src, type == eCharBuffer ? char_count : NPOS));
    return *this;
}
#if defined(HAVE_WSTRING)
inline CStringUTF8_DEPRECATED& CStringUTF8_DEPRECATED::Assign(
    ECharBufferType type, const wchar_t* src, SIZE_TYPE char_count) {
    assign( CUtf8::AsUTF8(src, type == eCharBuffer ? char_count : NPOS));
    return *this;
}
#endif
inline CStringUTF8_DEPRECATED& CStringUTF8_DEPRECATED::Assign(char ch, EEncoding encoding) {
    assign( CUtf8::AsUTF8( CTempString(&ch,1), encoding, CUtf8::eValidate));
    return *this;
}
inline  CStringUTF8_DEPRECATED& CStringUTF8_DEPRECATED::Append(
    const CTempString src, EEncoding encoding, EValidate validate) {
    append( CUtf8::AsUTF8( src, encoding, validate == CStringUTF8_DEPRECATED::eValidate ? CUtf8::eValidate : CUtf8::eNoValidate));
    return *this;
}
inline  CStringUTF8_DEPRECATED& CStringUTF8_DEPRECATED::Append(const TStringUnicode& src) {
    append( CUtf8::AsUTF8( src));
    return *this;
}
#if NCBITOOLKIT_USE_LONG_UCS4
inline  CStringUTF8_DEPRECATED& CStringUTF8_DEPRECATED::Append(const TStringUCS4& src) {
    append( CUtf8::AsUTF8( src));
    return *this;
}
#endif
inline  CStringUTF8_DEPRECATED& CStringUTF8_DEPRECATED::Append(const TStringUCS2& src) {
    append( CUtf8::AsUTF8( src));
    return *this;
}
#if defined(HAVE_WSTRING)
inline  CStringUTF8_DEPRECATED& CStringUTF8_DEPRECATED::Append(const wstring& src) {
    append( CUtf8::AsUTF8( src));
    return *this;
}
#endif
inline CStringUTF8_DEPRECATED& CStringUTF8_DEPRECATED::Append(const TUnicodeSymbol* src) {
    append( CUtf8::AsUTF8( src));
    return *this;
}
#if NCBITOOLKIT_USE_LONG_UCS4
inline CStringUTF8_DEPRECATED& CStringUTF8_DEPRECATED::Append(const TCharUCS4* src) {
    append( CUtf8::AsUTF8( src));
    return *this;
}
#endif
inline CStringUTF8_DEPRECATED& CStringUTF8_DEPRECATED::Append(const TCharUCS2* src) {
    append( CUtf8::AsUTF8( src));
    return *this;
}
#if defined(HAVE_WSTRING)
inline CStringUTF8_DEPRECATED& CStringUTF8_DEPRECATED::Append(const wchar_t* src) {
    append( CUtf8::AsUTF8( src));
    return *this;
}
#endif
inline  CStringUTF8_DEPRECATED& CStringUTF8_DEPRECATED::Append(
    ECharBufferType type, const TUnicodeSymbol* src, SIZE_TYPE char_count) {
    append( CUtf8::AsUTF8(src, type == eCharBuffer ? char_count : NPOS));
    return *this;
}
#if NCBITOOLKIT_USE_LONG_UCS4
inline CStringUTF8_DEPRECATED& CStringUTF8_DEPRECATED::Append(
    ECharBufferType type, const TCharUCS4* src, SIZE_TYPE char_count) {
    append( CUtf8::AsUTF8(src, type == eCharBuffer ? char_count : NPOS));
    return *this;
}
#endif
inline CStringUTF8_DEPRECATED& CStringUTF8_DEPRECATED::Append(
    ECharBufferType type, const TCharUCS2* src, SIZE_TYPE char_count) {
    append( CUtf8::AsUTF8(src, type == eCharBuffer ? char_count : NPOS));
    return *this;
}
#if defined(HAVE_WSTRING)
inline CStringUTF8_DEPRECATED& CStringUTF8_DEPRECATED::Append(
    ECharBufferType type, const wchar_t* src, SIZE_TYPE char_count) {
    append( CUtf8::AsUTF8(src, type == eCharBuffer ? char_count : NPOS));
    return *this;
}
#endif
inline CStringUTF8_DEPRECATED& CStringUTF8_DEPRECATED::Append(char ch, EEncoding encoding) {
    append( CUtf8::AsUTF8( CTempString(&ch,1), encoding, CUtf8::eValidate));
    return *this;
}
inline CStringUTF8_DEPRECATED& CStringUTF8_DEPRECATED::Append(TUnicodeSymbol ch) {
    append( CUtf8::AsUTF8(&ch, 1));
    return *this;
}
#if  STRINGUTF8_OBSOLETE_STATIC
inline SIZE_TYPE CStringUTF8_DEPRECATED::GetSymbolCount(void) const {
    return CUtf8::GetSymbolCount(*this);
}
#endif
inline bool CStringUTF8_DEPRECATED::IsValid(void) const {
    return CUtf8::MatchEncoding(*this, eEncoding_UTF8);
}
#if  STRINGUTF8_OBSOLETE_STATIC
inline TUnicodeSymbol CStringUTF8_DEPRECATED::Decode(const char*& src) {
    return CUtf8::Decode(src);
}
#ifndef NCBI_COMPILER_WORKSHOP
inline TUnicodeSymbol CStringUTF8_DEPRECATED::Decode(string::const_iterator& src) {
    return CUtf8::Decode(src);
}
#endif
#endif

#if  STRINGUTF8_OBSOLETE_STATIC
inline
SIZE_TYPE CStringUTF8_DEPRECATED::GetSymbolCount(const CTempString src)
{
    return CUtf8::GetSymbolCount(src);
}
#endif
#if  STRINGUTF8_OBSOLETE_STATIC
inline
SIZE_TYPE CStringUTF8_DEPRECATED::GetValidSymbolCount(const char* src, SIZE_TYPE buf_size)
{
    return CUtf8::GetValidSymbolCount(CTempString(src, buf_size));
}
inline
SIZE_TYPE CStringUTF8_DEPRECATED::GetValidSymbolCount(const CTempString src)
{
    return CUtf8::GetValidSymbolCount(src);
}
#endif
#if  STRINGUTF8_OBSOLETE_STATIC
inline
SIZE_TYPE CStringUTF8_DEPRECATED::GetValidBytesCount(const char* src, SIZE_TYPE buf_size)
{
    return CUtf8::GetValidBytesCount(CTempString(src,buf_size));
}
inline
SIZE_TYPE CStringUTF8_DEPRECATED::GetValidBytesCount(const CTempString src)
{
    return CUtf8::GetValidBytesCount(src);
}
#endif
inline
string CStringUTF8_DEPRECATED::AsSingleByteString(EEncoding encoding,
    const char* substitute_on_error) const
{
    return CUtf8::AsSingleByteString(*this,encoding,substitute_on_error);
}
#if  STRINGUTF8_OBSOLETE_STATIC
inline
EEncoding CStringUTF8_DEPRECATED::GuessEncoding(const CTempString src)
{
    return CUtf8::GuessEncoding(src);
}
#endif
#if  STRINGUTF8_OBSOLETE_STATIC
inline
bool CStringUTF8_DEPRECATED::MatchEncoding(const CTempString src, EEncoding encoding)
{
    return CUtf8::MatchEncoding(src,encoding);
}
inline
string CStringUTF8_DEPRECATED::EncodingToString(EEncoding encoding)
{
    return CUtf8::EncodingToString(encoding);
}
inline
EEncoding CStringUTF8_DEPRECATED::StringToEncoding(const CTempString str)
{
    return CUtf8::StringToEncoding(str);
}
#endif
#if  STRINGUTF8_OBSOLETE_STATIC
inline
TUnicodeSymbol CStringUTF8_DEPRECATED::CharToSymbol(char ch, EEncoding encoding)
{
    return CUtf8::CharToSymbol(ch,encoding);
}
inline
char CStringUTF8_DEPRECATED::SymbolToChar(TUnicodeSymbol sym, EEncoding encoding)
{
    return CUtf8::SymbolToChar(sym,encoding);
}
#endif
#if  STRINGUTF8_OBSOLETE_STATIC
inline
TUnicodeSymbol  CStringUTF8_DEPRECATED::DecodeFirst(char ch, SIZE_TYPE& more)
{
    return CUtf8::DecodeFirst(ch,more);
}
inline
TUnicodeSymbol  CStringUTF8_DEPRECATED::DecodeNext(TUnicodeSymbol chU, char ch)
{
    return CUtf8::DecodeNext(chU,ch);
}
#endif
#if  STRINGUTF8_OBSOLETE_STATIC
inline
bool  CStringUTF8_DEPRECATED::IsWhiteSpace(TUnicodeSymbol chU)
{
    return CUtf8::IsWhiteSpace(chU);
}
#endif
inline
CStringUTF8_DEPRECATED& CStringUTF8_DEPRECATED::TruncateSpacesInPlace(NStr::ETrunc side)
{
    CUtf8::TruncateSpacesInPlace(*this,side);
    return *this;
}
#if  STRINGUTF8_OBSOLETE_STATIC
inline
CTempString CStringUTF8_DEPRECATED::TruncateSpaces_Unsafe(const CTempString str, NStr::ETrunc side)
{
    return CUtf8::TruncateSpaces_Unsafe(str,side);
}
#endif
inline
void   CStringUTF8_DEPRECATED::x_Validate(void) const
{
    CUtf8::x_Validate(*this);
}
inline
void   CStringUTF8_DEPRECATED::x_AppendChar(TUnicodeSymbol ch)
{
    CUtf8::x_AppendChar(*this, ch);
}
inline
void   CStringUTF8_DEPRECATED::x_Append(const CTempString src,
                EEncoding encoding, EValidate validate)
{
    CUtf8::x_Append(*this, src, encoding, (CUtf8::EValidate)validate);
}
inline
SIZE_TYPE CStringUTF8_DEPRECATED::x_BytesNeeded(TUnicodeSymbol ch)
{
    return CUtf8::x_BytesNeeded(ch);
}
inline
bool   CStringUTF8_DEPRECATED::x_EvalFirst(char ch, SIZE_TYPE& more)
{
    return CUtf8::x_EvalFirst(ch, more);
}
inline
bool   CStringUTF8_DEPRECATED::x_EvalNext(char ch)
{
    return CUtf8::x_EvalNext(ch);
}
#endif // __EXPORT_IMPL_STRINGUTF8__

template <typename TChar>
basic_string<TChar> CStringUTF8_DEPRECATED::AsBasicString(
    const TChar* substitute_on_error) const
{
    return CUtf8::AsBasicString<TChar>(*this, substitute_on_error, CUtf8::eNoValidate);
}
template <typename TChar> inline
basic_string<TChar> CStringUTF8_DEPRECATED::AsBasicString(
    const CTempString str, const TChar* substitute_on_error, EValidate validate)
{
    return CNotImplemented<TChar>::Cannot_convert_to_nonUnicode_string();
}
template <> inline
basic_string<TUnicodeSymbol> CStringUTF8_DEPRECATED::AsBasicString(
    const CTempString str, const TUnicodeSymbol* substitute_on_error, EValidate validate)
{
    return CUtf8::x_AsBasicString(str,substitute_on_error, (CUtf8::EValidate)validate);
}
#if NCBITOOLKIT_USE_LONG_UCS4
template <> inline
basic_string<TCharUCS4> CStringUTF8_DEPRECATED::AsBasicString(
    const CTempString str, const TCharUCS4* substitute_on_error, EValidate validate)
{
    return CUtf8::x_AsBasicString(str,substitute_on_error, (CUtf8::EValidate)validate);
}
#endif
template <> inline
basic_string<TCharUCS2> CStringUTF8_DEPRECATED::AsBasicString(
    const CTempString str, const TCharUCS2* substitute_on_error, EValidate validate)
{
    return CUtf8::x_AsBasicString(str,substitute_on_error, (CUtf8::EValidate)validate);
}
#if defined(HAVE_WSTRING)
template <> inline
basic_string<wchar_t> CStringUTF8_DEPRECATED::AsBasicString(
    const CTempString str, const wchar_t* substitute_on_error, EValidate validate)
{
    return CUtf8::x_AsBasicString(str,substitute_on_error, (CUtf8::EValidate)validate);
}
#endif

#endif //STRINGUTF8_DEFINITION

#endif  /* CORELIB___STRINGUTF8_DEPRECATED__INL */
