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
* File Name:  $Id$
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

BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//
//  CVariantException::
//
//  
//
class CVariantException : public exception 
{

public:
    CVariantException(const string& msg);
    
    virtual ~CVariantException() throw();
  
    virtual const char* what() const throw();

private:
    const string m_msg;
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
class CVariant
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
    static CVariant VarChar      (const char *p, size_t len = 0);
    static CVariant Char         (size_t size, const char *p);
    static CVariant VarBinary    (const void *p, size_t len);
    static CVariant Binary       (size_t size, const void *p, size_t len);
    static CVariant SmallDateTime(CTime *p);
    static CVariant DateTime     (CTime *p);
    static CVariant Numeric      (unsigned int precision,
                                  unsigned int scale,
                                  const char* p);

    // Make "placeholder" CVariant by type, containing NULL value
    CVariant(EDB_Type type);

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
    const CTime&  GetCTime(void) const;


    // Status info
    bool IsNull() const;

    // operators
    CVariant& operator=(const CVariant& v);
    bool operator<(const CVariant& v) const;

    // Get pointer to the data buffer
    // NOTE: internal use only!
    CDB_Object* GetData() const;

    // Get pointer to the data buffer, throws CVariantException if buffer is 0
    // NOTE: internal use only!
    CDB_Object* GetNonNullData() const;

protected:
    // Set methods
    void SetData(CDB_Object* o);

private:
    class CDB_Object* m_data;
};


//================================================================

inline
bool operator==(const CVariant& v1,
		const CVariant& v2) 
{
    return !(v1 < v2) && !(v2 < v1);
}

inline
bool operator!=(const CVariant& v1,
		const CVariant& v2) 
{
    return v1 < v2 || v2 < v1;
}

END_NCBI_SCOPE

/*
 * $Log$
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
 *
 *
 */


#endif // DBAPI___VARIANT__HPP
