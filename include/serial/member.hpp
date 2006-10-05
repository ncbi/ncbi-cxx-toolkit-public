#ifndef MEMBER__HPP
#define MEMBER__HPP

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

#include <corelib/ncbistd.hpp>
#include <serial/serialutil.hpp>
#include <serial/item.hpp>
#include <serial/impl/hookdata.hpp>
#include <serial/impl/hookfunc.hpp>
#include <serial/typeinfo.hpp>


/** @addtogroup FieldsComplex
 *
 * @{
 */


BEGIN_NCBI_SCOPE

class CClassTypeInfoBase;

class CObjectIStream;
class CObjectOStream;
class CObjectStreamCopier;

class CReadClassMemberHook;
class CWriteClassMemberHook;
class CSkipClassMemberHook;
class CCopyClassMemberHook;

class CDelayBuffer;

class CMemberInfoFunctions;

class NCBI_XSERIAL_EXPORT CMemberInfo : public CItemInfo
{
    typedef CItemInfo CParent;
public:
    typedef TConstObjectPtr (*TMemberGetConst)(const CMemberInfo* memberInfo,
                                               TConstObjectPtr classPtr);
    typedef TObjectPtr (*TMemberGet)(const CMemberInfo* memberInfo,
                                     TObjectPtr classPtr);

    CMemberInfo(const CClassTypeInfoBase* classType, const CMemberId& id,
                TPointerOffsetType offset, const CTypeRef& type);
    CMemberInfo(const CClassTypeInfoBase* classType, const CMemberId& id,
                TPointerOffsetType offset, TTypeInfo type);
    CMemberInfo(const CClassTypeInfoBase* classType, const char* id,
                TPointerOffsetType offset, const CTypeRef& type);
    CMemberInfo(const CClassTypeInfoBase* classType, const char* id,
                TPointerOffsetType offset, TTypeInfo type);

    const CClassTypeInfoBase* GetClassType(void) const;

    bool Optional(void) const;
    CMemberInfo* SetOptional(void);
    CMemberInfo* SetNoPrefix(void);
    CMemberInfo* SetAttlist(void);
    CMemberInfo* SetNotag(void);
    CMemberInfo* SetAnyContent(void);
    CMemberInfo* SetCompressed(void);

    bool NonEmpty(void) const;
    CMemberInfo* SetNonEmpty(void);

    TConstObjectPtr GetDefault(void) const;
    CMemberInfo* SetDefault(TConstObjectPtr def);

    bool HaveSetFlag(void) const;
    CMemberInfo* SetSetFlag(const bool* setFlag);
    CMemberInfo* SetSetFlag(const Uint4* setFlag);
    CMemberInfo* SetOptional(const bool* setFlag);

    enum ESetFlag {
        eSetNo    = 0,
        eSetMaybe = 1,
        eSetYes   = 3
    };

    /// return current value of 'setFlag'
    ESetFlag GetSetFlag(TConstObjectPtr object) const;

    /// true if 'setFlag' is not eSetNo
    bool GetSetFlagYes(TConstObjectPtr object) const;
    /// true if 'setFlag' is eSetNo
    bool GetSetFlagNo(TConstObjectPtr object) const;

    /// set value of 'setFlag'
    void UpdateSetFlag(TObjectPtr object, ESetFlag value) const;
    /// set 'setFlag' to eSetYes
    void UpdateSetFlagYes(TObjectPtr object) const;
    /// set 'setFlag' to eSetMaybe
    void UpdateSetFlagMaybe(TObjectPtr object) const;
    /// set 'setFlag' to eSetNo and return true if previous value wasn't eSetNo
    bool UpdateSetFlagNo(TObjectPtr object) const;
    bool CompareSetFlags(TConstObjectPtr object1,
                         TConstObjectPtr object2) const;

    bool CanBeDelayed(void) const;
    CMemberInfo* SetDelayBuffer(CDelayBuffer* buffer);
    CDelayBuffer& GetDelayBuffer(TObjectPtr object) const;
    const CDelayBuffer& GetDelayBuffer(TConstObjectPtr object) const;

    void SetParentClass(void);

    // I/O
    void ReadMember(CObjectIStream& in, TObjectPtr classPtr) const;
    void ReadMissingMember(CObjectIStream& in, TObjectPtr classPtr) const;
    void WriteMember(CObjectOStream& out, TConstObjectPtr classPtr) const;
    void CopyMember(CObjectStreamCopier& copier) const;
    void CopyMissingMember(CObjectStreamCopier& copier) const;
    void SkipMember(CObjectIStream& in) const;
    void SkipMissingMember(CObjectIStream& in) const;

    TObjectPtr GetMemberPtr(TObjectPtr classPtr) const;
    TConstObjectPtr GetMemberPtr(TConstObjectPtr classPtr) const;

    // hooks
    void SetGlobalReadHook(CReadClassMemberHook* hook);
    void SetLocalReadHook(CObjectIStream& in, CReadClassMemberHook* hook);
    void ResetGlobalReadHook(void);
    void ResetLocalReadHook(CObjectIStream& in);
    void SetPathReadHook(CObjectIStream* in, const string& path,
                         CReadClassMemberHook* hook);

    void SetGlobalWriteHook(CWriteClassMemberHook* hook);
    void SetLocalWriteHook(CObjectOStream& out, CWriteClassMemberHook* hook);
    void ResetGlobalWriteHook(void);
    void ResetLocalWriteHook(CObjectOStream& out);
    void SetPathWriteHook(CObjectOStream* out, const string& path,
                          CWriteClassMemberHook* hook);

    void SetGlobalSkipHook(CSkipClassMemberHook* hook);
    void SetLocalSkipHook(CObjectIStream& in, CSkipClassMemberHook* hook);
    void ResetGlobalSkipHook(void);
    void ResetLocalSkipHook(CObjectIStream& in);
    void SetPathSkipHook(CObjectIStream* in, const string& path,
                         CSkipClassMemberHook* hook);

    void SetGlobalCopyHook(CCopyClassMemberHook* hook);
    void SetLocalCopyHook(CObjectStreamCopier& copier,
                          CCopyClassMemberHook* hook);
    void ResetGlobalCopyHook(void);
    void ResetLocalCopyHook(CObjectStreamCopier& copier);
    void SetPathCopyHook(CObjectStreamCopier* copier, const string& path,
                         CCopyClassMemberHook* hook);

    // default I/O (without hooks)
    void DefaultReadMember(CObjectIStream& in,
                           TObjectPtr classPtr) const;
    void DefaultReadMissingMember(CObjectIStream& in,
                                  TObjectPtr classPtr) const;
    void DefaultWriteMember(CObjectOStream& out,
                            TConstObjectPtr classPtr) const;
    void DefaultCopyMember(CObjectStreamCopier& copier) const;
    void DefaultCopyMissingMember(CObjectStreamCopier& copier) const;
    void DefaultSkipMember(CObjectIStream& in) const;
    void DefaultSkipMissingMember(CObjectIStream& in) const;

    virtual void UpdateDelayedBuffer(CObjectIStream& in,
                                     TObjectPtr classPtr) const;

private:
    // Create parent class object
    TObjectPtr CreateClass(void) const;

    // containing class type info
    const CClassTypeInfoBase* m_ClassType;
    // is optional
    bool m_Optional;
    bool m_NonEmpty;
    // default value
    TConstObjectPtr m_Default;
    // offset of 'SET' flag inside object
    TPointerOffsetType m_SetFlagOffset;
    bool m_BitSetFlag;
    // offset of delay buffer inside object
    TPointerOffsetType m_DelayOffset;

    TMemberGetConst m_GetConstFunction;
    TMemberGet m_GetFunction;

    CHookData<CReadClassMemberHook, SMemberReadFunctions> m_ReadHookData;
    CHookData<CWriteClassMemberHook, TMemberWriteFunction> m_WriteHookData;
    CHookData<CSkipClassMemberHook, SMemberSkipFunctions> m_SkipHookData;
    CHookData<CCopyClassMemberHook, SMemberCopyFunctions> m_CopyHookData;

    void SetReadFunction(TMemberReadFunction func);
    void SetReadMissingFunction(TMemberReadFunction func);
    void SetWriteFunction(TMemberWriteFunction func);
    void SetCopyFunction(TMemberCopyFunction func);
    void SetCopyMissingFunction(TMemberCopyFunction func);
    void SetSkipFunction(TMemberSkipFunction func);
    void SetSkipMissingFunction(TMemberSkipFunction func);

    void UpdateFunctions(void);

    friend class CMemberInfoFunctions;
};

/* @} */


#include <serial/impl/member.inl>

END_NCBI_SCOPE

#endif  /* MEMBER__HPP */



/* ---------------------------------------------------------------------------
* $Log$
* Revision 1.35  2006/10/05 19:23:04  gouriano
* Some headers moved into impl
*
* Revision 1.34  2006/01/19 18:22:34  gouriano
* Added possibility to save bit string data in compressed format
*
* Revision 1.33  2004/01/05 14:24:08  gouriano
* Added possibility to set serialization hooks by stack path
*
* Revision 1.32  2003/10/01 14:40:12  vasilche
* Fixed CanGet() for members wihout 'set' flag.
*
* Revision 1.31  2003/09/16 14:49:15  gouriano
* Enhanced AnyContent objects to support XML namespaces and attribute info items.
*
* Revision 1.30  2003/08/14 20:03:57  vasilche
* Avoid memory reallocation when reading over preallocated object.
* Simplified CContainerTypeInfo iterators interface.
*
* Revision 1.29  2003/07/29 18:47:46  vasilche
* Fixed thread safeness of object stream hooks.
*
* Revision 1.28  2003/06/24 20:54:13  gouriano
* corrected code generation and serialization of non-empty unnamed containers (XML)
*
* Revision 1.27  2003/04/29 18:29:06  gouriano
* object data member initialization verification
*
* Revision 1.26  2003/04/15 14:15:23  siyan
* Added doxygen support
*
* Revision 1.25  2003/04/10 20:13:37  vakatov
* Rollback the "uninitialized member" verification -- it still needs to
* be worked upon...
*
* Revision 1.23  2002/12/23 18:38:51  dicuccio
* Added WIn32 export specifier: NCBI_XSERIAL_EXPORT.
* Moved all CVS logs to the end.
*
* Revision 1.22  2002/11/14 20:47:22  gouriano
* modified constructor to use CClassTypeInfoBase,
* added Attlist and Notag flags
*
* Revision 1.21  2002/09/25 19:38:25  gouriano
* added the possibility of having no tag prefix in XML I/O streams
*
* Revision 1.20  2002/09/09 18:13:59  grichenk
* Added CObjectHookGuard class.
* Added methods to be used by hooks for data
* reading and skipping.
*
* Revision 1.19  2000/10/13 20:22:45  vasilche
* Fixed warnings on 64 bit compilers.
* Fixed missing typename in templates.
*
* Revision 1.18  2000/10/03 17:22:32  vasilche
* Reduced header dependency.
* Reduced size of debug libraries on WorkShop by 3 times.
* Fixed tag allocation for parent classes.
* Fixed CObject allocation/deallocation in streams.
* Moved instantiation of several templates in separate source file.
*
* Revision 1.17  2000/09/29 16:18:12  vasilche
* Fixed binary format encoding/decoding on 64 bit compulers.
* Implemented CWeakMap<> for automatic cleaning map entries.
* Added cleaning local hooks via CWeakMap<>.
* Renamed ReadTypeName -> ReadFileHeader, ENoTypeName -> ENoFileHeader.
* Added some user interface methods to CObjectIStream, CObjectOStream and
* CObjectStreamCopier.
*
* Revision 1.16  2000/09/26 17:38:07  vasilche
* Fixed incomplete choiceptr implementation.
* Removed temporary comments.
*
* Revision 1.15  2000/09/19 14:10:24  vasilche
* Added files to MSVC project
* Updated shell scripts to use new datattool path on MSVC
* Fixed internal compiler error on MSVC
*
* Revision 1.14  2000/09/18 20:00:02  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* Revision 1.13  2000/08/15 19:44:38  vasilche
* Added Read/Write hooks:
* CReadObjectHook/CWriteObjectHook for objects of specified type.
* CReadClassMemberHook/CWriteClassMemberHook for specified members.
* CReadChoiceVariantHook/CWriteChoiceVariant for specified choice variants.
* CReadContainerElementHook/CWriteContainerElementsHook for containers.
*
* Revision 1.12  2000/06/16 20:01:20  vasilche
* Avoid use of unexpected_exception() which is unimplemented on Mac.
*
* Revision 1.11  2000/06/16 19:24:22  vasilche
* Updated MSVC project.
* Fixed error on MSVC with static const class member.
*
* Revision 1.10  2000/06/16 16:31:05  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.9  2000/06/15 19:26:33  vasilche
* Fixed compilation error on Mac.
*
* Revision 1.8  2000/04/28 16:58:01  vasilche
* Added classes CByteSource and CByteSourceReader for generic reading.
* Added delayed reading of choice variants.
*
* Revision 1.7  2000/03/07 14:05:29  vasilche
* Added stream buffering to ASN.1 binary input.
* Optimized class loading/storing.
* Fixed bugs in processing OPTIONAL fields.
*
* Revision 1.6  2000/02/01 21:44:35  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Added buffering to CObjectIStreamAsn.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
*
* Revision 1.5  1999/09/01 17:38:00  vasilche
* Fixed vector<char> implementation.
* Added explicit naming of class info.
* Moved IMPLICIT attribute from member info to class info.
*
* Revision 1.4  1999/08/31 17:50:03  vasilche
* Implemented several macros for specific data types.
* Added implicit members.
* Added multimap and set.
*
* Revision 1.3  1999/08/13 15:53:42  vasilche
* C++ analog of asntool: datatool
*
* Revision 1.2  1999/06/30 16:04:21  vasilche
* Added support for old ASN.1 structures.
*
* Revision 1.1  1999/06/24 14:44:38  vasilche
* Added binary ASN.1 output.
*
* ===========================================================================
*/
