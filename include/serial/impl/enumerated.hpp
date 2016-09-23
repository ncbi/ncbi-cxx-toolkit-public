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

#include <serial/impl/stdtypes.hpp>
#include <serial/enumvalues.hpp>
#include <limits>


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
// C++ enums are not signed and not unsigned
    bool is_signed = std::is_signed<T>::value;
    if (std::is_enum<T>::value) {
        for (const auto& v : values->GetValues()) {
            if (v.second < 0) {
                is_signed = true;
                break;
            }
        }
    }
    return new CEnumeratedTypeInfo(sizeof(T), values, is_signed);
}


END_NCBI_SCOPE

/* @} */

#endif  /* ENUMERATED__HPP */
