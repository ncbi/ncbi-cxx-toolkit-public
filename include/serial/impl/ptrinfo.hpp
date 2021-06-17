#ifndef PTRINFO__HPP
#define PTRINFO__HPP

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
#include <serial/impl/typeref.hpp>


/** @addtogroup TypeInfoCPP
 *
 * @{
 */


BEGIN_NCBI_SCOPE

// CTypeInfo for pointers
class NCBI_XSERIAL_EXPORT CPointerTypeInfo : public CTypeInfo
{
    typedef CTypeInfo CParent;
public:
    CPointerTypeInfo(TTypeInfo type);
    CPointerTypeInfo(const CTypeRef& typeRef);
    CPointerTypeInfo(size_t size, TTypeInfo type);
    CPointerTypeInfo(size_t size, const CTypeRef& typeRef);
    CPointerTypeInfo(const string& name, TTypeInfo type);
    CPointerTypeInfo(const string& name, size_t size, TTypeInfo type);

    static TTypeInfo GetTypeInfo(TTypeInfo base);

    TTypeInfo GetPointedType(void) const;
    
    TConstObjectPtr GetObjectPointer(TConstObjectPtr object) const;
    TObjectPtr GetObjectPointer(TObjectPtr object) const;
    void SetObjectPointer(TObjectPtr object, TObjectPtr pointer) const;

    TTypeInfo GetRealDataTypeInfo(TConstObjectPtr object) const;

    virtual EMayContainType GetMayContainType(TTypeInfo type) const override;

    virtual bool IsDefault(TConstObjectPtr object) const override;
    virtual bool Equals(TConstObjectPtr object1, TConstObjectPtr object2,
                        ESerialRecursionMode how = eRecursive) const override;
    virtual void SetDefault(TObjectPtr dst) const override;
    virtual void Assign(TObjectPtr dst, TConstObjectPtr src,
                        ESerialRecursionMode how = eRecursive) const override;

    typedef TObjectPtr (*TGetDataFunction)(const CPointerTypeInfo* objectType,
                                           TObjectPtr objectPtr);
    typedef void (*TSetDataFunction)(const CPointerTypeInfo* objectType,
                                     TObjectPtr objectPtr,
                                     TObjectPtr dataPtr);

    void SetFunctions(TGetDataFunction getFunc, TSetDataFunction setFunc);

    virtual CTypeInfo* SetTag(CAsnBinaryDefs::TLongTag tag,
                CAsnBinaryDefs::ETagClass tagclass = CAsnBinaryDefs::eUniversal,
                CAsnBinaryDefs::ETagType tagtype   = CAsnBinaryDefs::eAutomatic) override;

protected:
    static TObjectPtr GetPointer(const CPointerTypeInfo* objectType,
                                 TObjectPtr objectPtr);
    static void SetPointer(const CPointerTypeInfo* objectType,
                           TObjectPtr objectPtr,
                           TObjectPtr dataPtr);

    static TObjectPtr CreatePointer(TTypeInfo objectType,
                                    CObjectMemoryPool* memoryPool);

    static void ReadPointer(CObjectIStream& in,
                            TTypeInfo objectType,
                            TObjectPtr objectPtr);
    static void WritePointer(CObjectOStream& out,
                             TTypeInfo objectType,
                             TConstObjectPtr objectPtr);
    static void SkipPointer(CObjectIStream& in,
                            TTypeInfo objectType);
    static void CopyPointer(CObjectStreamCopier& copier,
                            TTypeInfo objectType);

protected:
    CTypeRef m_DataTypeRef;
    TGetDataFunction m_GetData;
    TSetDataFunction m_SetData;

private:
    void InitPointerTypeInfoFunctions(void);
};


/* @} */


#include <serial/impl/ptrinfo.inl>

END_NCBI_SCOPE

#endif  /* PTRINFO__HPP */
