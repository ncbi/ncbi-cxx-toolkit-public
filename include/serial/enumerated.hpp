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
*/

#include <serial/typeinfo.hpp>
#include <serial/stdtypes.hpp>
#include <serial/enumvalues.hpp>


/** @addtogroup TypeInfoCPP
 *
 * @{
 */


BEGIN_NCBI_SCOPE

class NCBI_XSERIAL_EXPORT CEnumeratedTypeInfo : public CPrimitiveTypeInfo
{
    typedef CPrimitiveTypeInfo CParent;
public:
    // values should exist for all live time of our instance
    CEnumeratedTypeInfo(size_t size, const CEnumeratedTypeValues* values,
                        bool sign = false);

    const CEnumeratedTypeValues& Values(void) const
        {
            return m_Values;
        }

    virtual bool IsDefault(TConstObjectPtr object) const;
    virtual bool Equals(TConstObjectPtr , TConstObjectPtr,
                        ESerialRecursionMode how = eRecursive) const;
    virtual void SetDefault(TObjectPtr dst) const;
    virtual void Assign(TObjectPtr dst, TConstObjectPtr src,
                        ESerialRecursionMode how = eRecursive) const;

    virtual bool IsSigned(void) const;

    virtual Int4 GetValueInt4(TConstObjectPtr objectPtr) const;
    virtual Uint4 GetValueUint4(TConstObjectPtr objectPtr) const;
    virtual void SetValueInt4(TObjectPtr objectPtr, Int4 value) const;
    virtual void SetValueUint4(TObjectPtr objectPtr, Uint4 value) const;

    virtual Int8 GetValueInt8(TConstObjectPtr objectPtr) const;
    virtual Uint8 GetValueUint8(TConstObjectPtr objectPtr) const;
    virtual void SetValueInt8(TObjectPtr objectPtr, Int8 value) const;
    virtual void SetValueUint8(TObjectPtr objectPtr, Uint8 value) const;

    virtual void GetValueString(TConstObjectPtr objectPtr,
                                string& value) const;
    virtual void SetValueString(TObjectPtr objectPtr,
                                const string& value) const;

protected:
    static TObjectPtr CreateEnum(TTypeInfo objectType,
                                 CObjectMemoryPool* memoryPool);
    static void ReadEnum(CObjectIStream& in,
                         TTypeInfo objectType, TObjectPtr objectPtr);
    static void WriteEnum(CObjectOStream& out,
                          TTypeInfo objectType, TConstObjectPtr objectPtr);
    static void SkipEnum(CObjectIStream& in, TTypeInfo objectType);
    static void CopyEnum(CObjectStreamCopier& copier, TTypeInfo objectType);

private:
    const CPrimitiveTypeInfo* m_ValueType;
    const CEnumeratedTypeValues& m_Values;
};

template<typename T>
inline
CEnumeratedTypeInfo* CreateEnumeratedTypeInfo(const T& ,
                                              const CEnumeratedTypeValues* values)
{
    return new CEnumeratedTypeInfo(sizeof(T), values, T(-1) < 0);
}

END_NCBI_SCOPE

#endif  /* ENUMERATED__HPP */


/* @} */


/* ---------------------------------------------------------------------------
* $Log$
* Revision 1.17  2005/04/26 14:18:49  vasilche
* Allow allocation of objects in CObjectMemoryPool.
*
* Revision 1.16  2004/03/25 15:56:27  gouriano
* Added possibility to copy and compare serial object non-recursively
*
* Revision 1.15  2003/04/15 14:15:09  siyan
* Added doxygen support
*
* Revision 1.14  2002/12/23 18:38:50  dicuccio
* Added WIn32 export specifier: NCBI_XSERIAL_EXPORT.
* Moved all CVS logs to the end.
*
* Revision 1.13  2001/01/30 21:40:57  vasilche
* Fixed dealing with unsigned enums.
*
* Revision 1.12  2000/12/15 15:37:59  vasilche
* Added support of Int8 and long double.
* Enum values now have type Int4 instead of long.
*
* Revision 1.11  2000/09/18 20:00:01  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* Revision 1.10  2000/09/01 13:15:58  vasilche
* Implemented class/container/choice iterators.
* Implemented CObjectStreamCopier for copying data without loading into memory.
*
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
