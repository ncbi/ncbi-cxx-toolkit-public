#ifndef DBAPI_DRIVER___TYPES__HPP
#define DBAPI_DRIVER___TYPES__HPP

/* $Id$
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
 * Author:  Vladimir Soussov
 *
 * File Description:  DB types
 *
 */


#include <corelib/ncbitime.hpp>
#include <corelib/ncbi_param.hpp>


/** @addtogroup DbTypes
 *
 * @{
 */


BEGIN_NCBI_SCOPE


// Set of supported types
//

enum EDB_Type {
    eDB_Int,
    eDB_SmallInt,
    eDB_TinyInt,
    eDB_BigInt,
    eDB_VarChar,
    eDB_Char,
    eDB_VarBinary,
    eDB_Binary,
    eDB_Float,
    eDB_Double,
    eDB_DateTime,
    eDB_SmallDateTime,
    eDB_Text,
    eDB_Image,
    eDB_Bit,
    eDB_Numeric,
    eDB_LongChar,
    eDB_LongBinary,

    eDB_UnsupportedType
};


enum EBulkEnc {
    eBulkEnc_RawBytes,    // Raw bytes, perhaps already encoded in UCS-2.
    eBulkEnc_RawUCS2,     // Directly specified UCS-2, for nvarchar or ntext.
    eBulkEnc_UCS2FromChar // Character data to be reencoded as UCS-2.
};


/////////////////////////////////////////////////////////////////////////////

/// Convenience extension of basic_string, supporting implicit
/// conversion to const TChar* in most situations (but alas not within
/// variadic argument lists, as for printf and the like).
template <typename TChar>
class CGenericSqlString : public basic_string<TChar>
{
public:
    typedef basic_string<TChar>               TBasicString;
    typedef typename TBasicString::size_type  size_type;
    typedef typename TBasicString::value_type value_type;

    CGenericSqlString(void) { }
    CGenericSqlString(const TBasicString& s) : TBasicString(s) { }
    CGenericSqlString(const value_type* data) : TBasicString(data) { }
    CGenericSqlString(const value_type* data, size_type len)
        : TBasicString(data, len) { }

    size_type byte_count(void) const
        { return this->size() * sizeof(value_type); }

    NCBI_DEPRECATED NCBI_DBAPIDRIVER_EXPORT
    operator const value_type*(void) const;
    // { return this->c_str(); } // defined out-of-line to reduce warnings
};

typedef CGenericSqlString<char>    CSqlString;
#ifdef HAVE_WSTRING
typedef CGenericSqlString<wchar_t> CWSqlString;
#endif


/////////////////////////////////////////////////////////////////////////////
class NCBI_DBAPIDRIVER_EXPORT CWString
{
public:
    CWString(void);
    CWString(const CWString& str);

    explicit CWString(const char* str,
                      string::size_type size = string::npos,
                      EEncoding enc = eEncoding_Unknown);
    explicit CWString(const string& str,
                      EEncoding enc = eEncoding_Unknown);
#ifdef HAVE_WSTRING
    explicit CWString(const wchar_t* str,
                      wstring::size_type size = wstring::npos);
    explicit CWString(const wstring& str);
#endif

    ~CWString(void);

    CWString& operator=(const CWString& str);

public:
    operator const CSqlString&(void) const
    {
        if (!(GetAvailableValueType() & eString)) {
            x_MakeString();
        }

        return m_String;
    }
    NCBI_DEPRECATED operator char*(void) const;
    NCBI_DEPRECATED operator const char*(void) const;
    const char* AsCString(EEncoding str_enc = eEncoding_Unknown) const
    {
        if (!(GetAvailableValueType() & eChar)) {
            x_MakeString(str_enc);
        }

        return m_Char;
    }
#ifdef HAVE_WSTRING
    operator const wstring&(void) const
    {
        if (!(GetAvailableValueType() & eWString)) {
            x_MakeWString();
        }

        return m_WString;
    }
    NCBI_DEPRECATED operator wchar_t*(void) const;
    NCBI_DEPRECATED operator const wchar_t*(void) const;
    const wchar_t* AsCWString(EEncoding str_enc = eEncoding_Unknown) const
    {
        if (!(GetAvailableValueType() & eWChar)) {
            x_MakeWString(str_enc);
        }

        return m_WChar;
    }
#endif

public:
    // str_enc - expected string encoding.
    const string& AsLatin1(EEncoding str_enc = eEncoding_Unknown) const
    {
        if (!(GetAvailableValueType() & eString)) {
            x_MakeString(str_enc);
        }

        return m_String;
    }
    const string& AsUTF8(EEncoding str_enc = eEncoding_Unknown) const
    {
        if (!(GetAvailableValueType() & eUTF8String)) {
            x_MakeUTF8String(str_enc);
        }

        return m_UTF8String;
    }
    const TStringUCS2& AsUCS2_LE(EEncoding str_enc = eEncoding_Unknown) const
    {
        if (!(GetAvailableValueType() & eUCS2LEString)) {
            x_MakeUCS2LEString(str_enc);
        }

        return m_UCS2LEString;
    }
#ifdef HAVE_WSTRING
    const wstring& AsUnicode(EEncoding str_enc = eEncoding_Unknown) const
    {
        if (!(GetAvailableValueType() & eWString)) {
            x_MakeWString(str_enc);
        }

        return m_WString;
    }
#endif
    const string& ConvertTo(EEncoding to_enc,
                            EEncoding from_enc = eEncoding_Unknown) const
    {
        if (to_enc == eEncoding_UTF8) {
            return AsUTF8(from_enc);
        }

        return AsLatin1(from_enc);
    }

    size_t GetSymbolNum(void) const;

public:
    void Clear(void);
    void Assign(const char* str,
                string::size_type size = string::npos,
                EEncoding enc = eEncoding_Unknown);
    void Assign(const string& str,
                EEncoding enc = eEncoding_Unknown);
#ifdef HAVE_WSTRING
    void Assign(const wchar_t* str,
                wstring::size_type size = wstring::npos);
    void Assign(const wstring& str);
#endif

protected:
    int GetAvailableValueType(void) const
    {
        return m_AvailableValueType;
    }

    void x_MakeString(EEncoding str_enc = eEncoding_Unknown) const;
#ifdef HAVE_WSTRING
    void x_MakeWString(EEncoding str_enc = eEncoding_Unknown) const;
#endif
    void x_MakeUTF8String(EEncoding str_enc = eEncoding_Unknown) const;
    void x_MakeUCS2LEString(EEncoding str_enc = eEncoding_Unknown) const;

    void x_CalculateEncoding(EEncoding str_enc) const;
    void x_UTF8ToString(EEncoding str_enc = eEncoding_Unknown) const;
    void x_StringToUTF8(EEncoding str_enc = eEncoding_Unknown) const;

protected:
    enum {
        eChar         = 0x1,
        eWChar        = 0x2,
        eString       = 0x4,
        eWString      = 0x8,
        eUTF8String   = 0x10,
        eUCS2LEString = 0x20
    };

    mutable int             m_AvailableValueType;
    mutable EEncoding       m_StringEncoding; // Source string encoding.
    mutable const char*     m_Char;
#ifdef HAVE_WSTRING
    mutable const wchar_t*  m_WChar;
#endif
    mutable CSqlString      m_String;
#ifdef HAVE_WSTRING
    mutable wstring         m_WString;
#endif
    mutable CStringUTF8     m_UTF8String;
    mutable TStringUCS2     m_UCS2LEString;
};

/////////////////////////////////////////////////////////////////////////////
//
//  CDB_Object::
//
// Base class for all "type objects" to support database NULL value
// and provide the means to get the type and to clone the object.
//

class NCBI_DBAPIDRIVER_EXPORT CDB_Object
{
public:
    CDB_Object(bool is_null = true);
    virtual ~CDB_Object();

    bool IsNULL() const  { return m_Null; }
    virtual void AssignNULL();

    virtual EDB_Type    GetType() const = 0;
    virtual CDB_Object* Clone()   const = 0;
    virtual void AssignValue(const CDB_Object& v) = 0;

    // Create and return a new object (with internal value NULL) of type "type".
    // NOTE:  "size" matters only for eDB_Char, eDB_Binary, eDB_LongChar, eDB_LongBinary.
    static CDB_Object* Create(EDB_Type type, size_t size = 1);

    // Get human-readable type name for db_type
    static const char* GetTypeName(EDB_Type db_type,
                                   bool throw_on_unknown = true);

    string GetLogString(void) const;

protected:
    void SetNULL(bool flag = true) { m_Null = flag; }

private:
    bool m_Null;
};



/////////////////////////////////////////////////////////////////////////////
//
//  CDB_Int::
//  CDB_SmallInt::
//  CDB_TinyInt::
//  CDB_BigInt::
//  CDB_VarChar::
//  CDB_Char::
//  CDB_VarBinary::
//  CDB_Binary::
//  CDB_Float::
//  CDB_Double::
//  CDB_Stream::
//  CDB_Image::
//  CDB_Text::
//  CDB_SmallDateTime::
//  CDB_DateTime::
//  CDB_Bit::
//  CDB_Numeric::
//
// Classes to represent objects of different types (derived from CDB_Object::)
//


/////////////////////////////////////////////////////////////////////////////
class NCBI_DBAPIDRIVER_EXPORT CDB_Int : public CDB_Object
{
public:
    CDB_Int();
    CDB_Int(const Int4& i);
    virtual ~CDB_Int(void);

    CDB_Int& operator= (const Int4& i) {
        SetNULL(false);
        m_Val  = i;
        return *this;
    }

    Int4  Value()   const  { return IsNULL() ? 0 : m_Val; }
    void* BindVal() const  { return (void*) &m_Val; }

    virtual EDB_Type    GetType() const;
    virtual CDB_Object* Clone() const;
    virtual void AssignValue(const CDB_Object& v);

protected:
    Int4 m_Val;
};



/////////////////////////////////////////////////////////////////////////////
class NCBI_DBAPIDRIVER_EXPORT CDB_SmallInt : public CDB_Object
{
public:
    CDB_SmallInt();
    CDB_SmallInt(const Int2& i);
    virtual ~CDB_SmallInt(void);

    CDB_SmallInt& operator= (const Int2& i) {
        SetNULL(false);
        m_Val = i;
        return *this;
    }

    Int2  Value()   const  { return IsNULL() ? 0 : m_Val; }
    void* BindVal() const  { return (void*) &m_Val; }

    virtual EDB_Type    GetType() const;
    virtual CDB_Object* Clone() const;
    virtual void AssignValue(const CDB_Object& v);

protected:
    Int2 m_Val;
};



/////////////////////////////////////////////////////////////////////////////
class NCBI_DBAPIDRIVER_EXPORT CDB_TinyInt : public CDB_Object
{
public:
    CDB_TinyInt();
    CDB_TinyInt(const Uint1& i);
    virtual ~CDB_TinyInt(void);

    CDB_TinyInt& operator= (const Uint1& i) {
        SetNULL(false);
        m_Val = i;
        return *this;
    }

    Uint1 Value()   const  { return IsNULL() ? 0 : m_Val; }
    void* BindVal() const  { return (void*) &m_Val; }

    virtual EDB_Type    GetType() const;
    virtual CDB_Object* Clone()   const;
    virtual void AssignValue(const CDB_Object& v);

protected:
    Uint1 m_Val;
};



/////////////////////////////////////////////////////////////////////////////
class NCBI_DBAPIDRIVER_EXPORT CDB_BigInt : public CDB_Object
{
public:
    CDB_BigInt();
    CDB_BigInt(const Int8& i);
    virtual ~CDB_BigInt(void);

    CDB_BigInt& operator= (const Int8& i) {
        SetNULL(false);
        m_Val = i;
        return *this;
    }

    Int8 Value() const  { return IsNULL() ? 0 : m_Val; }
    void* BindVal() const  { return (void*) &m_Val; }


    virtual EDB_Type    GetType() const;
    virtual CDB_Object* Clone()   const;
    virtual void AssignValue(const CDB_Object& v);

protected:
    Int8 m_Val;
};


/////////////////////////////////////////////////////////////////////////////
class NCBI_DBAPIDRIVER_EXPORT CDB_String : public CDB_Object
{
public:
    CDB_String(void);
    CDB_String(const CDB_String& other);
    explicit CDB_String(const string& s,
                        EEncoding enc = eEncoding_Unknown);
    explicit CDB_String(const char* s,
                        string::size_type size = string::npos,
                        EEncoding enc = eEncoding_Unknown);
    explicit CDB_String(const string& s,
                        string::size_type size = string::npos,
                        EEncoding enc = eEncoding_Unknown);
    explicit CDB_String(const TStringUCS2& s,
                        TStringUCS2::size_type size = TStringUCS2::npos);
    // Accepting const TCharUCS2* in addition to const char* leads to
    // ambiguity errors when passed NULL.
    // explicit CDB_String(const TCharUCS2* s,
    //                     string::size_type size = TStringUCS2::npos);
    virtual ~CDB_String(void);

public:
    // Assignment operators
    CDB_String& operator= (const CDB_String& other);
    CDB_String& operator= (const string& s);
    CDB_String& operator= (const char* s);
    CDB_String& operator= (const TStringUCS2& s);
    // CDB_String& operator= (const TCharUCS2* s);

public:
    // Conversion operators
    NCBI_DEPRECATED operator const char*(void) const;

    operator const string&(void) const
    {
        return m_WString;
    }

public:
#if defined(HAVE_WSTRING)
    // enc - expected source string encoding.
    const wstring& AsWString(EEncoding enc) const
    {
        return IsNULL() ? kEmptyWStr : m_WString.AsUnicode(enc);
    }

    NCBI_DEPRECATED const wchar_t* AsUnicode(EEncoding enc) const;
#endif

    const string& AsString(void) const
    {
        return IsNULL() ? kEmptyStr : x_GetWString();
    }

    size_t Size(void) const
    {
        return IsNULL() ? 0 : m_WString.GetSymbolNum();
    }

    const char* Data(void) const
    {
        return IsNULL() ? NULL : x_GetWString().data();
    }

    const char* AsCString(void) const
    {
        return IsNULL() ? NULL : x_GetWString().c_str();
    }

    const char* Value(void) const { return AsCString(); }

public:
    // set-value methods
    void Assign(const CDB_String& other);
    void Assign(const char* s,
                string::size_type size = string::npos,
                EEncoding enc = eEncoding_Unknown);
    void Assign(const string& s,
                string::size_type size = string::npos,
                EEncoding enc = eEncoding_Unknown);
    // void Assign(const TCharUCS2* s,
    //             TStringUCS2::size_type size = TStringUCS2::npos);
    void Assign(const TStringUCS2& s,
                TStringUCS2::size_type size = TStringUCS2::npos);

    // Simplify bulk insertion into NVARCHAR columns, which require
    // explicit handling.
    EBulkEnc GetBulkInsertionEnc(void) const { return m_BulkInsertionEnc; }
    void     SetBulkInsertionEnc(EBulkEnc e) { m_BulkInsertionEnc = e;    }
    void     GetBulkInsertionData(CTempString* ts,
                                  bool convert_raw_bytes = false) const;

private:
    CWString m_WString;
    EBulkEnc m_BulkInsertionEnc;

    // Avoid explicit static_cast<const string&>(m_WString) constructs,
    // which trigger internal errors under ICC 12.
    const string& x_GetWString() const { return m_WString; }
};


/////////////////////////////////////////////////////////////////////////////
class NCBI_DBAPIDRIVER_EXPORT CDB_VarChar : public CDB_String
{
public:
    CDB_VarChar(void);
    CDB_VarChar(const string& s,
                EEncoding enc = eEncoding_Unknown);
    CDB_VarChar(const char* s,
                EEncoding enc = eEncoding_Unknown);
    CDB_VarChar(const char* s,
                size_t l,
                EEncoding enc = eEncoding_Unknown);
    CDB_VarChar(const TStringUCS2& s, size_t l = TStringUCS2::npos);
    // CDB_VarChar(const TCharUCS2* s, size_t l);
    virtual ~CDB_VarChar(void);

public:
    // assignment operators
    CDB_VarChar& operator= (const string& s)  { return SetValue(s); }
    CDB_VarChar& operator= (const char*   s)  { return SetValue(s); }
    CDB_VarChar& operator= (const TStringUCS2& s)  { return SetValue(s); }
    // CDB_VarChar& operator= (const TCharUCS2*   s)  { return SetValue(s); }

public:
    // set-value methods
    CDB_VarChar& SetValue(const string& s,
                          EEncoding enc = eEncoding_Unknown);
    CDB_VarChar& SetValue(const char* s,
                          EEncoding enc = eEncoding_Unknown);
    CDB_VarChar& SetValue(const char* s, size_t l,
                          EEncoding enc = eEncoding_Unknown);
    CDB_VarChar& SetValue(const TStringUCS2& s);
    // CDB_VarChar& SetValue(const TCharUCS2* s, size_t l);

public:
    virtual EDB_Type    GetType() const;
    virtual CDB_Object* Clone()   const;
    virtual void AssignValue(const CDB_Object& v);
};

/////////////////////////////////////////////////////////////////////////////
class NCBI_DBAPIDRIVER_EXPORT CDB_Char : public CDB_String
{
public:
    CDB_Char(size_t s = 1);
    CDB_Char(size_t s,
             const string& v,
             EEncoding enc = eEncoding_Unknown);
    // This ctor copies a string.
    CDB_Char(size_t s,
             const char* str,
             EEncoding enc = eEncoding_Unknown);
    CDB_Char(size_t s, const TStringUCS2& v);
    // CDB_Char(size_t s, const TCharUCS2* str);
    CDB_Char(const CDB_Char& v);
    virtual ~CDB_Char(void);

public:
    CDB_Char& operator= (const CDB_Char& v);
    CDB_Char& operator= (const string& v);
    // This operator copies a string.
    CDB_Char& operator= (const char* v);
    CDB_Char& operator= (const TStringUCS2& v);
    // CDB_Char& operator= (const TCharUCS2* v);

public:
    // This method copies a string.
    void SetValue(const char* str,
                  size_t len,
                  EEncoding enc = eEncoding_Unknown);
    void SetValue(const TStringUCS2& v, size_t len);
    // void SetValue(const TCharUCS2* v, size_t len);

public:
    virtual EDB_Type    GetType() const;
    virtual CDB_Object* Clone()   const;

    virtual void AssignValue(const CDB_Object& v);

protected:
    size_t      m_Size; // Number of characters (not bytes)
};


/////////////////////////////////////////////////////////////////////////////
#define K8_1 8191

class NCBI_DBAPIDRIVER_EXPORT CDB_LongChar : public CDB_String
{
public:

    CDB_LongChar(size_t s = K8_1);
    CDB_LongChar(size_t s,
                 const string& v,
                 EEncoding enc = eEncoding_Unknown);
    // This ctor copies a string.
    CDB_LongChar(size_t len,
                 const char* str,
                 EEncoding enc = eEncoding_Unknown);
    CDB_LongChar(size_t s, const TStringUCS2& v);
    // CDB_LongChar(size_t len, const TCharUCS2* str);
    CDB_LongChar(const CDB_LongChar& v);
    virtual ~CDB_LongChar();

public:
    CDB_LongChar& operator= (const CDB_LongChar& v);
    CDB_LongChar& operator= (const string& v);
    // This operator copies a string.
    CDB_LongChar& operator= (const char* v);
    CDB_LongChar& operator= (const TStringUCS2& v);
    // CDB_LongChar& operator= (const TCharUCS2* v);

    // This method copies a string.
    void SetValue(const char* str,
                  size_t len,
                  EEncoding enc = eEncoding_Unknown);
    void SetValue(const TStringUCS2& str, size_t len);
    // void SetValue(const TCharUCS2* str, size_t len);

    //
    size_t      Size()  const  { return IsNULL() ? 0 : m_Size; }
    size_t  DataSize()  const  { return CDB_String::Size(); }

    virtual EDB_Type    GetType() const;
    virtual CDB_Object* Clone()   const;

    virtual void AssignValue(const CDB_Object& v);

protected:
    size_t      m_Size; // Number of characters (not bytes)
};



/////////////////////////////////////////////////////////////////////////////
class NCBI_DBAPIDRIVER_EXPORT CDB_VarBinary : public CDB_Object
{
public:
    CDB_VarBinary();
    CDB_VarBinary(const void* v, size_t l);
    virtual ~CDB_VarBinary(void);

public:
    void SetValue(const void* v, size_t l);

    CDB_VarBinary& operator= (const CDB_VarBinary& v);
   
    //
    const void* Value() const
        { return IsNULL() ? NULL : (void*) m_Value.data(); }
    const void* Data()  const  { return Value(); }
    size_t      Size()  const  { return IsNULL() ? 0 : m_Value.size(); }

    virtual EDB_Type    GetType() const;
    virtual CDB_Object* Clone()   const;
    virtual void AssignValue(const CDB_Object& v);

protected:
    string m_Value;
};



/////////////////////////////////////////////////////////////////////////////
class NCBI_DBAPIDRIVER_EXPORT CDB_Binary : public CDB_Object
{
public:
    CDB_Binary(size_t s = 1);
    CDB_Binary(size_t s, const void* v, size_t v_size);
    CDB_Binary(const CDB_Binary& v);
    virtual ~CDB_Binary();

public:
    void SetValue(const void* v, size_t v_size);

    CDB_Binary& operator= (const CDB_Binary& v);

    //
    const void* Value() const
        { return IsNULL() ? NULL : (void*) m_Value.data(); }
    const void* Data()  const  { return Value(); }
    size_t      Size()  const  { return IsNULL() ? 0 : m_Value.size(); }

    virtual EDB_Type    GetType() const;
    virtual CDB_Object* Clone()   const;

    virtual void AssignValue(const CDB_Object& v);

protected:
    size_t m_Size;
    string m_Value;
};


/////////////////////////////////////////////////////////////////////////////
class NCBI_DBAPIDRIVER_EXPORT CDB_LongBinary : public CDB_Object
{
public:

    CDB_LongBinary(size_t s = K8_1);
    CDB_LongBinary(size_t s, const void* v, size_t v_size);
    CDB_LongBinary(const CDB_LongBinary& v);
    virtual ~CDB_LongBinary();

public:
    void SetValue(const void* v, size_t v_size);

    CDB_LongBinary& operator= (const CDB_LongBinary& v);

    //
    const void* Value() const
        { return IsNULL() ? NULL : (void*) m_Value.data(); }
    const void* Data()  const  { return Value(); }
    size_t      Size()  const  { return IsNULL() ? 0 : m_Value.size(); }
    size_t  DataSize()  const  { return m_DataSize; }

    virtual EDB_Type    GetType() const;
    virtual CDB_Object* Clone()   const;

    virtual void AssignValue(const CDB_Object& v);

protected:
    size_t m_Size;
    size_t m_DataSize;
    string m_Value;
};


/////////////////////////////////////////////////////////////////////////////
class NCBI_DBAPIDRIVER_EXPORT CDB_Float : public CDB_Object
{
public:
    CDB_Float();
    CDB_Float(float i);
    virtual ~CDB_Float(void);

    CDB_Float& operator= (const float& i);
public:

    float Value()   const { return IsNULL() ? 0 : m_Val; }
    void* BindVal() const { return (void*) &m_Val; }

    virtual EDB_Type    GetType() const;
    virtual CDB_Object* Clone()   const;
    virtual void AssignValue(const CDB_Object& v);

protected:
    float m_Val;
};



/////////////////////////////////////////////////////////////////////////////
class NCBI_DBAPIDRIVER_EXPORT CDB_Double : public CDB_Object
{
public:
    CDB_Double();
    CDB_Double(double i);
    virtual ~CDB_Double(void);

public:
    CDB_Double& operator= (const double& i);

    //
    double Value()   const  { return IsNULL() ? 0 : m_Val; }
    void*  BindVal() const  { return (void*) &m_Val; }

    virtual EDB_Type    GetType() const;
    virtual CDB_Object* Clone()   const;
    virtual void AssignValue(const CDB_Object& v);

protected:
    double m_Val;
};


/////////////////////////////////////////////////////////////////////////////
class CMemStore;

class NCBI_DBAPIDRIVER_EXPORT CDB_Stream : public CDB_Object
{
public:
    // assignment
    virtual void AssignNULL();
    CDB_Stream&  Assign(const CDB_Stream& v);

    // data manipulations
    virtual size_t Read     (void* buff, size_t nof_bytes);
    virtual size_t Peek     (void* buff, size_t nof_bytes) const;
    virtual size_t Append   (const void* buff, size_t nof_bytes);
    virtual void   Truncate (size_t nof_bytes = kMax_Int);
    virtual bool   MoveTo   (size_t byte_number);

    // current size of data
    virtual size_t Size() const;
    virtual void AssignValue(const CDB_Object& v);

protected:
    // 'ctors
    CDB_Stream();
    virtual ~CDB_Stream();

private:
    // data storage
    CMemStore* m_Store;
};



/////////////////////////////////////////////////////////////////////////////
class NCBI_DBAPIDRIVER_EXPORT CDB_Image : public CDB_Stream
{
public:
    CDB_Image(void);
    virtual ~CDB_Image(void);

public:
    CDB_Image& operator= (const CDB_Image& image);

    virtual EDB_Type    GetType() const;
    virtual CDB_Object* Clone()   const;
};



/////////////////////////////////////////////////////////////////////////////
class NCBI_DBAPIDRIVER_EXPORT CDB_Text : public CDB_Stream
{
public:
    CDB_Text(void);
    virtual ~CDB_Text(void);

public:
    // Get and set the encoding to be used by subsequent Append operations.
    EBulkEnc GetEncoding(void) const { return m_Encoding; }
    void     SetEncoding(EBulkEnc e);

    virtual size_t Append(const void* buff, size_t nof_bytes);
    size_t Append(const char* buff);
    virtual size_t Append(const string& s, EEncoding enc = eEncoding_Unknown);
    virtual size_t Append(const TStringUCS2& s);

    CDB_Text& operator= (const CDB_Text& text);

    virtual EDB_Type    GetType() const;
    virtual CDB_Object* Clone()   const;

private:
    EBulkEnc m_Encoding;
};



/////////////////////////////////////////////////////////////////////////////
class NCBI_DBAPIDRIVER_EXPORT CDB_SmallDateTime : public CDB_Object
{
public:
    CDB_SmallDateTime(CTime::EInitMode mode= CTime::eEmpty);
    CDB_SmallDateTime(const CTime& t);
    CDB_SmallDateTime(Uint2 days, Uint2 minutes);
    virtual ~CDB_SmallDateTime(void);

public:
    CDB_SmallDateTime& operator= (const CTime& t);

    CDB_SmallDateTime& Assign(Uint2 days, Uint2 minutes);
    const CTime& Value(void) const;
    Uint2 GetDays(void) const;
    Uint2 GetMinutes(void) const;

    virtual EDB_Type    GetType(void) const;
    virtual CDB_Object* Clone(void)   const;
    virtual void AssignValue(const CDB_Object& v);

protected:
    mutable CTime        m_NCBITime;
    mutable TDBTimeU     m_DBTime;
    // which of m_NCBITime(0x1), m_DBTime(0x2) is valid;  they both can be valid
    mutable unsigned int m_Status;
};



/////////////////////////////////////////////////////////////////////////////
class NCBI_DBAPIDRIVER_EXPORT CDB_DateTime : public CDB_Object
{
public:
    CDB_DateTime(CTime::EInitMode mode= CTime::eEmpty);
    CDB_DateTime(const CTime& t);
    CDB_DateTime(Int4 d, Int4 s300);
    virtual ~CDB_DateTime(void);

public:
    CDB_DateTime& operator= (const CTime& t);

    CDB_DateTime& Assign(Int4 d, Int4 s300);
    const CTime& Value(void) const;

    Int4 GetDays(void) const;
    Int4 Get300Secs(void) const;

    virtual EDB_Type    GetType(void) const;
    virtual CDB_Object* Clone(void)   const;
    virtual void AssignValue(const CDB_Object& v);

protected:
    mutable CTime        m_NCBITime;
    mutable TDBTimeI     m_DBTime;
    // which of m_NCBITime(0x1), m_DBTime(0x2) is valid;  they both can be valid
    mutable unsigned int m_Status;
};



/////////////////////////////////////////////////////////////////////////////
class NCBI_DBAPIDRIVER_EXPORT CDB_Bit : public CDB_Object
{
public:
    CDB_Bit();
    CDB_Bit(int  i);
    CDB_Bit(bool i);
    virtual ~CDB_Bit(void);

public:
    CDB_Bit& operator= (const int& i);
    CDB_Bit& operator= (const bool& i);

    int   Value()   const  { return IsNULL() ? 0 : (int) m_Val; }
    void* BindVal() const  { return (void*) &m_Val; }

    virtual EDB_Type    GetType() const;
    virtual CDB_Object* Clone()   const;
    virtual void AssignValue(const CDB_Object& v);

protected:
    Uint1 m_Val;
};



/////////////////////////////////////////////////////////////////////////////
class NCBI_DBAPIDRIVER_EXPORT CDB_Numeric : public CDB_Object
{
public:
    CDB_Numeric();
    CDB_Numeric(unsigned int precision, unsigned int scale);
    CDB_Numeric(unsigned int precision, unsigned int scale,
                const unsigned char* arr);
    CDB_Numeric(unsigned int precision, unsigned int scale, bool is_negative,
                const unsigned char* arr);
    CDB_Numeric(unsigned int precision, unsigned int scale, const char* val);
    CDB_Numeric(unsigned int precision, unsigned int scale, const string& val);
    virtual ~CDB_Numeric(void);

public:
    CDB_Numeric& Assign(unsigned int precision, unsigned int scale,
                        const unsigned char* arr);
    CDB_Numeric& Assign(unsigned int precision, unsigned int scale,
                        bool is_negative, const unsigned char* arr);

    CDB_Numeric& operator= (const char* val);
    CDB_Numeric& operator= (const string& val);

    string Value() const;

    Uint1 Precision() const {
        return m_Precision;
    }

    Uint1 Scale() const {
        return m_Scale;
    }

    // This method is for internal use only. It is strongly recommended
    // to refrain from using this method in applications.
    const unsigned char* RawData() const { return m_Body; }

    virtual EDB_Type    GetType() const;
    virtual CDB_Object* Clone()   const;
    virtual void AssignValue(const CDB_Object& v);

protected:
    void x_MakeFromString(unsigned int precision,
                          unsigned int scale,
                          const CTempString& s);
    Uint1         m_Precision;
    Uint1         m_Scale;
    unsigned char m_Body[33];
};


NCBI_PARAM_DECL_EXPORT(NCBI_DBAPIDRIVER_EXPORT, unsigned int, dbapi, max_logged_param_length);
typedef NCBI_PARAM_TYPE(dbapi, max_logged_param_length) TDbapi_MaxLoggedParamLength;


END_NCBI_SCOPE

#endif  /* DBAPI_DRIVER___TYPES__HPP */


/* @} */

