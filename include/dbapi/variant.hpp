#ifndef DBAPI___VARIANT__HPP
#define DBAPI___VARIANT__HPP

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
 * Author:  Michael Kholodov
 *
 * File Description:  CVariant class implementation
 *
 */

#include <corelib/ncbiobj.hpp>
#include <corelib/ncbitype.h>
#include <corelib/ncbitime.hpp>
#include <dbapi/driver/types.hpp>
#include <dbapi/driver/interfaces.hpp>


/** @addtogroup DbVariant
 *
 * @{
 */


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//
//  CVariantException::
//
//

//class NCBI_DBAPI_EXPORT CVariantException : public std::exception
class NCBI_DBAPI_EXPORT CVariantException : EXCEPTION_VIRTUAL_BASE public CException
{

public:
    enum EErrCode {
         eVariant
    };

    CVariantException(const string& message);

    virtual const char* GetErrCodeString(void) const;

    NCBI_EXCEPTION_DEFAULT(CVariantException, CException);
};


/////////////////////////////////////////////////////////////////////////////
//
//  EDateTimeFormat::
//
//  DateTime format
//

enum EDateTimeFormat {
    eShort,
    eLong
};


/////////////////////////////////////////////////////////////////////////////
//
//  CVariant::
//
//  CVariant data type
//

class NCBI_DBAPI_EXPORT CVariant
{
public:
    // Contructors to create CVariant from various primitive types
    explicit CVariant(Int8 v);
    explicit CVariant(Int4 v);
    explicit CVariant(Int2 v);
    explicit CVariant(Uint1 v);
    explicit CVariant(float v);
    explicit CVariant(double v);
    explicit CVariant(bool v);
    explicit CVariant(const string& v);
    explicit CVariant(const char* s);

    // Factories for different types
    // NOTE: pass p = 0 to make NULL value
    static CVariant BigInt       (Int8 *p);
    static CVariant Int          (Int4 *p);
    static CVariant SmallInt     (Int2 *p);
    static CVariant TinyInt      (Uint1 *p);
    static CVariant Float        (float *p);
    static CVariant Double       (double *p);
    static CVariant Bit          (bool *p);
    static CVariant LongChar     (const char *p, size_t len = 0);
    static CVariant VarChar      (const char *p, size_t len = 0);
    static CVariant Char         (size_t size, const char *p);
    static CVariant LongBinary   (size_t maxSize, const void *p, size_t len);
    static CVariant VarBinary    (const void *p, size_t len);
    static CVariant Binary       (size_t size, const void *p, size_t len);
    static CVariant SmallDateTime(CTime *p);
    static CVariant DateTime     (CTime *p);
    static CVariant Numeric      (unsigned int precision,
                                  unsigned int scale,
                                  const char* p);

    // Make "placeholder" CVariant by type, containing NULL value
    CVariant(EDB_Type type, size_t size = 0);

    // Make DATETIME representation in long and short forms
    CVariant(const class CTime& v, EDateTimeFormat fmt);

    // Make CVariant from internal CDB_Object
    explicit CVariant(CDB_Object* obj);

    // Copy constructor
    CVariant(const CVariant& v);

    // Destructor
    ~CVariant();

    // Get methods
    EDB_Type GetType() const;

    Int8          GetInt8(void) const;
    string        GetString(void) const;
    Int4          GetInt4(void) const;
    Int2          GetInt2(void) const;
    Uint1         GetByte(void) const;
    float         GetFloat(void) const;
    double        GetDouble(void) const;
    bool          GetBit(void) const;
    string        GetNumeric(void) const;
    const CTime&  GetCTime(void) const;

    // Get the argument as default, if the column is NULL
    string AsNotNullString(const string& v) const;

    // Status info
    bool IsNull() const;

    // NULLify
    void SetNull();

    // operators
    CVariant& operator=(const CVariant& v);
    CVariant& operator=(const Int8& v);
    CVariant& operator=(const Int4& v);
    CVariant& operator=(const Int2& v);
    CVariant& operator=(const Uint1& v);
    CVariant& operator=(const float& v);
    CVariant& operator=(const double& v);
    CVariant& operator=(const string& v);
    CVariant& operator=(const char* v);
    CVariant& operator=(const bool& v);
    CVariant& operator=(const CTime& v);



    // Get pointer to the data buffer
    // NOTE: internal use only!
    CDB_Object* GetData() const;

    // Get pointer to the data buffer, throws CVariantException if buffer is 0
    // NOTE: internal use only!
    CDB_Object* GetNonNullData() const;

    // Methods to work with BLOB data (Text and Image)
    size_t GetBlobSize() const;
    size_t Read(void* buf, size_t len) const;
    size_t Append(const void* buf, size_t len);
    // Truncates from buffer end to buffer start.
    // Truncates everything if no argument
    void Truncate(size_t len = kMax_UInt);
    // Moves the internal position pointer
    bool MoveTo(size_t pos) const;

    void SetITDescriptor(I_ITDescriptor* descr);
    I_ITDescriptor& GetITDescriptor(void) const;

protected:
    // Set methods
    void SetData(CDB_Object* o);

private:

//    void VerifyType(bool e) const;
    void CheckNull() const;

    void x_Verify_AssignType(EDB_Type db_type, const char* cxx_type) const;
    void x_Inapplicable_Method(const char* method) const;

    class CDB_Object* m_data;
    auto_ptr<I_ITDescriptor> m_descr;
};

bool NCBI_DBAPI_EXPORT operator==(const CVariant& v1, const CVariant& v2);
bool NCBI_DBAPI_EXPORT operator<(const CVariant& v1, const CVariant& v2);


//================================================================
inline
CDB_Object* CVariant::GetData() const {
    return m_data;
}

inline
EDB_Type CVariant::GetType() const
{
    return m_data->GetType();
}


//inline
//void CVariant::VerifyType(bool e) const
//{
//    if( !e ) {
//#ifdef _DEBUG
//        _TRACE("CVariant::VerifyType(): Invalid type");
//        _ASSERT(0);
//#else
//        NCBI_THROW(CVariantException, eVariant, "Invalid type");
//#endif
//    }
//}


inline void
CVariant::SetITDescriptor(I_ITDescriptor* descr)
{
    m_descr.reset(descr);
}

inline I_ITDescriptor&
CVariant::GetITDescriptor(void) const
{
    return *m_descr;
}

inline
void CVariant::x_Verify_AssignType(EDB_Type db_type, const char* cxx_type) const
{
    if( db_type != GetType() )
    {
        NCBI_THROW(CVariantException, eVariant,
             "Cannot assign type '" + string(cxx_type) + "' to type '"
             + string(CDB_Object::GetTypeName(GetType())) + "'");
    }
}

inline
void CVariant::x_Inapplicable_Method(const char* method) const
{
    NCBI_THROW(CVariantException, eVariant,
        "CVariant::" + string(method) + " is not applicable to type '"
        + string(CDB_Object::GetTypeName(GetType())) + "'");
}


inline
bool operator!=(const CVariant& v1, const CVariant& v2)
{
    return !(v1 == v2);
}

inline
bool operator>(const CVariant& v1, const CVariant& v2)
{
    return v2 < v1;
}

inline
bool operator<=(const CVariant& v1, const CVariant& v2)
{
    return v1 < v2 || v1 == v2;
}

inline
bool operator>=(const CVariant& v1, const CVariant& v2)
{
    return v2 < v1 || v1 == v2;
}


END_NCBI_SCOPE

#endif // DBAPI___VARIANT__HPP
