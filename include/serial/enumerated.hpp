#ifndef ENUMERATED__HPP
#define ENUMERATED__HPP

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
* Author: Eugene Vasilchenko
*
* File Description:
*   !!! PUT YOUR DESCRIPTION HERE !!!
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.9  2000/07/03 18:42:33  vasilche
* Added interface to typeinfo via CObjectInfo and CConstObjectInfo.
* Reduced header dependency.
*
* Revision 1.8  2000/05/24 20:08:11  vasilche
* Implemented XML dump.
*
* Revision 1.7  2000/03/07 14:05:29  vasilche
* Added stream buffering to ASN.1 binary input.
* Optimized class loading/storing.
* Fixed bugs in processing OPTIONAL fields.
*
* Revision 1.6  2000/02/01 21:44:34  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Added buffering to CObjectIStreamAsn.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
*
* Revision 1.5  2000/01/10 19:46:31  vasilche
* Fixed encoding/decoding of REAL type.
* Fixed encoding/decoding of StringStore.
* Fixed encoding/decoding of NULL type.
* Fixed error reporting.
* Reduced object map (only classes).
*
* Revision 1.4  1999/12/17 19:04:52  vasilche
* Simplified generation of GetTypeInfo methods.
*
* Revision 1.3  1999/10/28 15:37:37  vasilche
* Fixed null choice pointers handling.
* Cleaned enumertion interface.
*
* Revision 1.2  1999/10/18 20:11:15  vasilche
* Enum values now have long type.
* Fixed template generation for enums.
*
* Revision 1.1  1999/09/24 18:20:05  vasilche
* Removed dependency on NCBI toolkit.
*
*
* ===========================================================================
*/

#include <serial/typeinfo.hpp>
#include <serial/stdtypes.hpp>
#include <serial/enumvalues.hpp>

BEGIN_NCBI_SCOPE

class CEnumeratedTypeInfo : public CPrimitiveTypeInfo
{
    typedef CPrimitiveTypeInfo CParent;
public:
    // values should exist for all live time of our instance
    CEnumeratedTypeInfo(const CEnumeratedTypeValues* values,
                        size_t size = sizeof(int));

    virtual EValueType GetValueType(void) const;

    const CEnumeratedTypeValues& Values(void) const
        {
            return m_Values;
        }

    virtual size_t GetSize(void) const;
    virtual TObjectPtr Create(void) const;
    
    virtual bool IsDefault(TConstObjectPtr object) const;
    virtual bool Equals(TConstObjectPtr , TConstObjectPtr ) const;
    virtual void SetDefault(TObjectPtr dst) const;
    virtual void Assign(TObjectPtr dst, TConstObjectPtr src) const;

    virtual bool IsSigned(void) const;
    virtual long GetValueLong(TConstObjectPtr objectPtr) const;
    virtual unsigned long GetValueULong(TConstObjectPtr objectPtr) const;
    virtual void SetValueLong(TObjectPtr objectPtr, long value) const;
    virtual void SetValueULong(TObjectPtr objectPtr, unsigned long value) const;
    virtual void GetValueString(TConstObjectPtr objectPtr, string& value) const;
    virtual void SetValueString(TObjectPtr objectPtr, const string& value) const;

protected:
    virtual void SkipData(CObjectIStream& in) const;
    virtual void ReadData(CObjectIStream& in, TObjectPtr object) const;
    virtual void WriteData(CObjectOStream& out, TConstObjectPtr object) const;

private:
    const CPrimitiveTypeInfo* m_ValueType;
    const CEnumeratedTypeValues& m_Values;
};

template<typename T>
inline
CEnumeratedTypeInfo* CreateEnumeratedTypeInfo(const T& ,
                                              const CEnumeratedTypeValues* values)
{
    return new CEnumeratedTypeInfo(values, sizeof(T));
}

END_NCBI_SCOPE

#endif  /* ENUMERATED__HPP */
