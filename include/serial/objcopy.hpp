#ifndef OBJCOPY__HPP
#define OBJCOPY__HPP

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
#include <serial/serialdef.hpp>
#include <serial/typeinfo.hpp>
#include <serial/objostr.hpp>
#include <serial/objistr.hpp>
#include <serial/hookdatakey.hpp>
#include <serial/objhook.hpp>
#include <serial/pathhook.hpp>


/** @addtogroup ObjStreamSupport
 *
 * @{
 */


BEGIN_NCBI_SCOPE

class CContainerTypeInfo;
class CClassTypeInfo;
class CChoiceTypeInfo;
class CAliasTypeInfo;
class CMemberInfo;

class CCopyObjectHook;
class CCopyClassMemberHook;
class CCopyChoiceVariantHook;

class NCBI_XSERIAL_EXPORT CObjectStreamCopier
{
public:
    CObjectStreamCopier(CObjectIStream& in, CObjectOStream& out);
    ~CObjectStreamCopier(void);

    CObjectIStream& In(void) const;
    CObjectOStream& Out(void) const;

    void ResetLocalHooks(void);

    // main copy
    void Copy(const CObjectTypeInfo& type);

    enum ENoFileHeader {
        eNoFileHeader
    };
    // copy without source typename
    void Copy(TTypeInfo type, ENoFileHeader noFileHeader);

    // copy object
    void CopyObject(TTypeInfo type);

    void CopyExternalObject(TTypeInfo type);

    // primitive types copy
    void CopyString(void);
    void CopyStringStore(void);
    void CopyByteBlock(void);

    void CopyAnyContentObject(void);

    // complex types copy
    void CopyNamedType(TTypeInfo namedTypeInfo, TTypeInfo objectType);

    void CopyPointer(TTypeInfo declaredType);

    void CopyContainer(const CContainerTypeInfo* containerType);

    void CopyClassRandom(const CClassTypeInfo* classType);
    void CopyClassSequential(const CClassTypeInfo* classType);

    void CopyChoice(const CChoiceTypeInfo* choiceType);
    void CopyAlias(const CAliasTypeInfo* aliasType);

    typedef CObjectIStream::TFailFlags TFailFlags;
    void ThrowError1(const char* file, int line,
                     TFailFlags fail, const char* message);
    void ThrowError1(const char* file, int line,
                     TFailFlags fail, const string& message);
#define ThrowError(flag, mess) ThrowError1(__FILE__, __LINE__,flag,mess)
    void ExpectedMember(const CMemberInfo* memberInfo);
    void DuplicatedMember(const CMemberInfo* memberInfo);

    void SetPathCopyObjectHook( const string& path, CCopyObjectHook*        hook);
    void SetPathCopyMemberHook( const string& path, CCopyClassMemberHook*   hook);
    void SetPathCopyVariantHook(const string& path, CCopyChoiceVariantHook* hook);
    void SetPathHooks(CObjectStack& stk, bool set);

private:
    CObjectIStream& m_In;
    CObjectOStream& m_Out;
    CStreamPathHook<CMemberInfo*, CCopyClassMemberHook*>   m_PathCopyMemberHooks;
    CStreamPathHook<CVariantInfo*,CCopyChoiceVariantHook*> m_PathCopyVariantHooks;
    CStreamObjectPathHook<CCopyObjectHook*>                m_PathCopyObjectHooks;

public:
    // hook support
    CLocalHookSet<CCopyObjectHook> m_ObjectHookKey;
    CLocalHookSet<CCopyClassMemberHook> m_ClassMemberHookKey;
    CLocalHookSet<CCopyChoiceVariantHook> m_ChoiceVariantHookKey;
};


/* @ */


#include <serial/objcopy.inl>

END_NCBI_SCOPE

#endif  /* OBJCOPY__HPP */



/* ---------------------------------------------------------------------------
* $Log$
* Revision 1.15  2004/04/30 13:28:39  gouriano
* Remove obsolete function declarations
*
* Revision 1.14  2004/01/05 14:24:08  gouriano
* Added possibility to set serialization hooks by stack path
*
* Revision 1.13  2003/10/21 21:08:45  grichenk
* Fixed aliases-related bug in XML stream
*
* Revision 1.12  2003/08/13 15:47:02  gouriano
* implemented serialization of AnyContent objects
*
* Revision 1.11  2003/07/29 18:47:46  vasilche
* Fixed thread safeness of object stream hooks.
*
* Revision 1.10  2003/04/15 16:18:10  siyan
* Added doxygen support
*
* Revision 1.9  2003/03/10 18:52:37  gouriano
* use new structured exceptions (based on CException)
*
* Revision 1.8  2002/12/23 18:38:51  dicuccio
* Added WIn32 export specifier: NCBI_XSERIAL_EXPORT.
* Moved all CVS logs to the end.
*
* Revision 1.7  2002/10/25 14:49:29  vasilche
* NCBI C Toolkit compatibility code extracted to libxcser library.
* Serial streams flags names were renamed to fXxx.
*
* Names of flags
*
* Revision 1.6  2001/05/17 14:56:51  lavr
* Typos corrected
*
* Revision 1.5  2000/10/20 15:51:23  vasilche
* Fixed data error processing.
* Added interface for constructing container objects directly into output stream.
* object.hpp, object.inl and object.cpp were split to
* objectinfo.*, objecttype.*, objectiter.* and objectio.*.
*
* Revision 1.4  2000/10/13 16:28:31  vasilche
* Reduced header dependency.
* Avoid use of templates with virtual methods.
* Reduced amount of different maps used.
* All this lead to smaller compiled code size (libraries and programs).
*
* Revision 1.3  2000/09/29 16:18:13  vasilche
* Fixed binary format encoding/decoding on 64 bit compulers.
* Implemented CWeakMap<> for automatic cleaning map entries.
* Added cleaning local hooks via CWeakMap<>.
* Renamed ReadTypeName -> ReadFileHeader, ENoTypeName -> ENoFileHeader.
* Added some user interface methods to CObjectIStream, CObjectOStream and
* CObjectStreamCopier.
*
* Revision 1.2  2000/09/18 20:00:03  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* Revision 1.1  2000/09/01 13:15:59  vasilche
* Implemented class/container/choice iterators.
* Implemented CObjectStreamCopier for copying data without loading into memory.
*
* ===========================================================================
*/
