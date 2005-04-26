#ifndef CHOICE__HPP
#define CHOICE__HPP

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
#include <serial/typeref.hpp>
#include <serial/classinfob.hpp>
#include <serial/variant.hpp>


/** @addtogroup TypeInfoCPP
 *
 * @{
 */


BEGIN_NCBI_SCOPE

class CChoiceTypeInfoReader;
class CClassInfoHelperBase;

class CChoiceTypeInfoFunctions;

class NCBI_XSERIAL_EXPORT CChoiceTypeInfo : public CClassTypeInfoBase
{
    typedef CClassTypeInfoBase CParent;
public:
    typedef TMemberIndex (*TWhichFunction)(const CChoiceTypeInfo* choiceType,
                                           TConstObjectPtr choicePtr);
    typedef void (*TResetFunction)(const CChoiceTypeInfo* choiceType,
                                   TObjectPtr choicePtr);
    typedef void (*TSelectFunction)(const CChoiceTypeInfo* choiceType,
                                    TObjectPtr choicePtr, TMemberIndex index,
                                    CObjectMemoryPool* memPool);
    typedef void (*TSelectDelayFunction)(const CChoiceTypeInfo* choiceType,
                                         TObjectPtr choicePtr,
                                         TMemberIndex index);
    typedef TObjectPtr (*TGetDataFunction)(const CChoiceTypeInfo* choiceType,
                                           TObjectPtr choicePtr,
                                           TMemberIndex index);

    CChoiceTypeInfo(size_t size, const char* name,
                    const void* nonCObject, TTypeCreate createFunc,
                    const type_info& ti,
                    TWhichFunction whichFunc,
                    TSelectFunction selectFunc,
                    TResetFunction resetFunc);
    CChoiceTypeInfo(size_t size, const char* name,
                    const CObject* cObject, TTypeCreate createFunc,
                    const type_info& ti,
                    TWhichFunction whichFunc,
                    TSelectFunction selectFunc,
                    TResetFunction resetFunc);
    CChoiceTypeInfo(size_t size, const string& name,
                    const void* nonCObject, TTypeCreate createFunc,
                    const type_info& ti,
                    TWhichFunction whichFunc,
                    TSelectFunction selectFunc,
                    TResetFunction resetFunc);
    CChoiceTypeInfo(size_t size, const string& name,
                    const CObject* cObject, TTypeCreate createFunc,
                    const type_info& ti,
                    TWhichFunction whichFunc,
                    TSelectFunction selectFunc,
                    TResetFunction resetFunc);

    const CItemsInfo& GetVariants(void) const;
    const CVariantInfo* GetVariantInfo(TMemberIndex index) const;
    const CVariantInfo* GetVariantInfo(const CIterator& i) const;
    const CVariantInfo* GetVariantInfo(const string& name) const;

    CVariantInfo* AddVariant(const char* variantId,
                             const void* variantPtr,
                             const CTypeRef& variantType);
    CVariantInfo* AddVariant(const CMemberId& variantId,
                             const void* variantPtr,
                             const CTypeRef& variantType);

    virtual bool IsDefault(TConstObjectPtr object) const;
    virtual bool Equals(TConstObjectPtr obj1, TConstObjectPtr obj2,
                        ESerialRecursionMode how = eRecursive) const;
    virtual void SetDefault(TObjectPtr dst) const;
    virtual void Assign(TObjectPtr dst, TConstObjectPtr src,
                        ESerialRecursionMode how = eRecursive) const;

    // iterators interface
    TMemberIndex GetIndex(TConstObjectPtr object) const;
    void ResetIndex(TObjectPtr object) const;
    void SetIndex(TObjectPtr object, TMemberIndex index,
                  CObjectMemoryPool* pool = 0) const;
    void SetDelayIndex(TObjectPtr object, TMemberIndex index) const;

    TConstObjectPtr GetData(TConstObjectPtr object, TMemberIndex index) const;
    TObjectPtr GetData(TObjectPtr object, TMemberIndex index) const;

protected:
    void SetSelectDelayFunction(TSelectDelayFunction func);

    friend class CChoiceTypeInfoFunctions;

private:
    void InitChoiceTypeInfoFunctions(void);

protected:
    TWhichFunction m_WhichFunction;
    TResetFunction m_ResetFunction;
    TSelectFunction m_SelectFunction;
    TSelectDelayFunction m_SelectDelayFunction;
};


/* @} */


#include <serial/choice.inl>

END_NCBI_SCOPE

#endif  /* CHOICE__HPP */



/* ---------------------------------------------------------------------------
* $Log$
* Revision 1.24  2005/04/26 14:18:49  vasilche
* Allow allocation of objects in CObjectMemoryPool.
*
* Revision 1.23  2004/03/25 15:56:27  gouriano
* Added possibility to copy and compare serial object non-recursively
*
* Revision 1.22  2003/04/15 14:14:54  siyan
* Added doxygen support
*
* Revision 1.21  2002/12/23 18:38:50  dicuccio
* Added WIn32 export specifier: NCBI_XSERIAL_EXPORT.
* Moved all CVS logs to the end.
*
* Revision 1.20  2002/12/12 21:10:25  gouriano
* implemented handling of complex XML containers
*
* Revision 1.19  2000/10/03 17:22:30  vasilche
* Reduced header dependency.
* Reduced size of debug libraries on WorkShop by 3 times.
* Fixed tag allocation for parent classes.
* Fixed CObject allocation/deallocation in streams.
* Moved instantiation of several templates in separate source file.
*
* Revision 1.18  2000/09/26 18:03:21  vasilche
* Added missing class keyword.
*
* Revision 1.17  2000/09/26 17:38:06  vasilche
* Fixed incomplete choiceptr implementation.
* Removed temporary comments.
*
* Revision 1.16  2000/09/18 19:59:59  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* Revision 1.15  2000/09/01 13:15:57  vasilche
* Implemented class/container/choice iterators.
* Implemented CObjectStreamCopier for copying data without loading into memory.
*
* Revision 1.14  2000/08/15 19:44:38  vasilche
* Added Read/Write hooks:
* CReadObjectHook/CWriteObjectHook for objects of specified type.
* CReadClassMemberHook/CWriteClassMemberHook for specified members.
* CReadChoiceVariantHook/CWriteChoiceVariant for specified choice variants.
* CReadContainerElementHook/CWriteContainerElementsHook for containers.
*
* Revision 1.13  2000/07/03 18:42:32  vasilche
* Added interface to typeinfo via CObjectInfo and CConstObjectInfo.
* Reduced header dependency.
*
* Revision 1.12  2000/06/16 16:31:03  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.11  2000/06/01 19:06:54  vasilche
* Added parsing of XML data.
*
* Revision 1.10  2000/04/28 16:58:01  vasilche
* Added classes CByteSource and CByteSourceReader for generic reading.
* Added delayed reading of choice variants.
*
* Revision 1.9  2000/04/06 16:10:50  vasilche
* Fixed bug with iterators in choices.
* Removed unneeded calls to ReadExternalObject/WriteExternalObject.
* Added output buffering to text ASN.1 data.
*
* Revision 1.8  2000/04/03 18:47:09  vasilche
* Added main include file for generated headers.
* serialimpl.hpp is included in generated sources with GetTypeInfo methods
*
* Revision 1.7  2000/03/07 14:05:28  vasilche
* Added stream buffering to ASN.1 binary input.
* Optimized class loading/storing.
* Fixed bugs in processing OPTIONAL fields.
*
* Revision 1.6  2000/02/17 20:02:27  vasilche
* Added some standard serialization exceptions.
* Optimized text/binary ASN.1 reading.
* Fixed wrong encoding of StringStore in ASN.1 binary format.
* Optimized logic of object collection.
*
* Revision 1.5  2000/02/01 21:44:34  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Added buffering to CObjectIStreamAsn.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
*
* Revision 1.4  1999/12/28 18:55:39  vasilche
* Reduced size of compiled object files:
* 1. avoid inline or implicit virtual methods (especially destructors).
* 2. avoid std::string's methods usage in inline methods.
* 3. avoid string literals ("xxx") in inline methods.
*
* Revision 1.3  1999/12/17 19:04:52  vasilche
* Simplified generation of GetTypeInfo methods.
*
* Revision 1.2  1999/10/08 21:00:39  vasilche
* Implemented automatic generation of unnamed ASN.1 types.
*
* Revision 1.1  1999/09/24 18:20:05  vasilche
* Removed dependency on NCBI toolkit.
*
*
* ===========================================================================
*/
