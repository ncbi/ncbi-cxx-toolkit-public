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

    CVariantException(const string& message) 
        : CException(DIAG_COMPILE_INFO, 0, (CException::EErrCode)eVariant,  message)
    {
    }

    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode()) {
        case eVariant:    return "eVariant";
        default:       return CException::GetErrCodeString();
        }
    }
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

protected:
    // Set methods
    void SetData(CDB_Object* o);

private:

    void VerifyType(bool e) const;
    void CheckNull() const;

    class CDB_Object* m_data;
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


inline
void CVariant::VerifyType(bool e) const
{
    if( !e ) {
#ifdef _DEBUG
        _TRACE("CVariant::VerifyType(): Invalid type");
        _ASSERT(0); 
#else
        NCBI_THROW(CVariantException, eVariant, "Invalid type");
#endif
    }
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


/*
 * ===========================================================================
 * $Log$
 * Revision 1.25  2005/05/05 20:09:34  kholodov
 * Fixed: comparison operators
 *
 * Revision 1.24  2005/05/03 19:14:15  kholodov
 * Modified: NULL CVariants return "natural empty values", no exception thrown
 * Added: CVariantException(const string&) constructor for backward compatibility.
 *
 * Revision 1.23  2005/05/02 21:12:22  kholodov
 * Modified: made CException base class for CVariantException
 * Added: throw exception if attempt to get value from NULL column is made
 *
 * Revision 1.22  2004/09/29 14:58:21  kholodov
 * Added: SetNull() method
 *
 * Revision 1.21  2004/04/08 15:57:21  kholodov
 * Multiple bug fixes and optimizations
 *
 * Revision 1.20  2004/02/10 18:52:35  kholodov
 * Modified: made Move() method const
 *
 * Revision 1.19  2003/12/15 20:05:41  ivanov
 * Added export specifier for building DLLs in MS Windows.
 *
 * Revision 1.18  2003/12/10 22:34:04  kholodov
 * Added: MoveTo() method
 *
 * Revision 1.17  2003/12/10 21:13:42  kholodov
 * Added: size argument in CVariant(EDB_Type) constructor
 *
 * Revision 1.16  2003/11/18 16:58:52  kholodov
 * Added: operator=(const char*)
 *
 * Revision 1.15  2003/08/15 19:48:26  kholodov
 * Fixed: const method GetBlobSize()
 *
 * Revision 1.14  2003/08/12 21:11:22  kholodov
 * Added: AsNotNullString() method
 *
 * Revision 1.13  2003/08/01 20:33:02  vakatov
 * Explicitly qualify "exception" with "std::" to avoid a silly name conflict
 * with <math.h> for SUN Forte6u2 compiler
 *
 * Revision 1.12  2003/06/25 22:24:46  kholodov
 * Added: GetBlobSize() method
 *
 * Revision 1.11  2003/05/05 18:33:15  kholodov
 * Added: LONGCHAR and LONGBINARY support
 *
 * Revision 1.10  2003/04/11 17:45:58  siyan
 * Added doxygen support
 *
 * Revision 1.9  2002/09/16 21:04:03  kholodov
 * Modified: CVariant::Assign<> template removed
 *
 * Revision 1.8  2002/09/16 19:31:08  kholodov
 * Added: Numeric datatype support
 * Added: CVariant::operator=() methods for working with bulk insert
 * Added: Methods for writing BLOBs during bulk insert
 *
 * Revision 1.7  2002/05/16 21:59:55  kholodov
 * Added: _TRACE() message to VerifyType()
 *
 * Revision 1.6  2002/04/15 19:12:57  kholodov
 * Added VerifyType() method
 *
 * Revision 1.5  2002/03/13 16:52:21  kholodov
 * Added: Full destructor definition in CVariantException with throw()
 * to conform with the parent's virtual destructor.
 * Modified: Moved CVariantException methods' definitions to variant.cpp file
 *
 * Revision 1.4  2002/02/08 15:50:38  kholodov
 * Modified: integer types used are Int8, Int4, Int2, Uint1
 * Added: factories for CVariants of a particular type
 *
 * Revision 1.3  2002/02/06 22:50:49  kholodov
 * Conditionalized the usage of long long
 *
 * Revision 1.2  2002/02/06 22:21:00  kholodov
 * Added constructor from long long to BigInt type
 *
 * Revision 1.1  2002/01/30 14:51:24  kholodov
 * User DBAPI implementation, first commit
 *
 * ===========================================================================
 */


#endif // DBAPI___VARIANT__HPP
