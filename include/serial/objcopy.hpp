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
*
* ---------------------------------------------------------------------------
* $Log$
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

#include <corelib/ncbistd.hpp>
#include <serial/serialdef.hpp>
#include <serial/typeinfo.hpp>
#include <serial/objostr.hpp>
#include <serial/objistr.hpp>
#include <serial/weakmap.hpp>
#include <serial/objhook.hpp>

BEGIN_NCBI_SCOPE

class CContainerTypeInfo;
class CClassTypeInfo;
class CChoiceTypeInfo;
class CMemberInfo;

class CCopyObjectHook;
class CCopyClassMemberHook;
class CCopyChoiceVariantHook;

class CObjectStreamCopier
{
public:
    CObjectStreamCopier(CObjectIStream& in, CObjectOStream& out);

    CObjectIStream& In(void) const;
    CObjectOStream& Out(void) const;

    // main copy
    void Copy(const CObjectTypeInfo& type);

    enum ENoFileHeader {
        eNoFileHeader
    };
    // copy without source typename
    void Copy(const CObjectTypeInfo& type, ENoFileHeader noFileHeader);
    void Copy(TTypeInfo type, ENoFileHeader noFileHeader);

    // copy object
    void CopyObject(const CObjectTypeInfo& type);
    void CopyObject(TTypeInfo type);

    void CopyExternalObject(TTypeInfo type);

    // primitive types copy
    void CopyString(void);
    void CopyStringStore(void);
    void CopyByteBlock(void);

    // complex types copy
    void CopyNamedType(TTypeInfo namedTypeInfo, TTypeInfo objectType);

    void CopyPointer(TTypeInfo declaredType);

    void CopyContainer(const CContainerTypeInfo* containerType);

    void CopyClassRandom(const CClassTypeInfo* classType);
    void CopyClassSequential(const CClassTypeInfo* classType);

    void CopyChoice(const CChoiceTypeInfo* choiceType);

    void ExpectedMember(const CMemberInfo* memberInfo);

private:
    CObjectIStream& m_In;
    CObjectOStream& m_Out;

public:
    // hook support
    CWeakMapKey< CRef<CCopyObjectHook> > m_ObjectHookKey;
    CWeakMapKey< CRef<CCopyClassMemberHook> > m_ClassMemberHookKey;
    CWeakMapKey< CRef<CCopyChoiceVariantHook> > m_ChoiceVariantHookKey;
};

#include <serial/objcopy.inl>

END_NCBI_SCOPE

#endif  /* OBJCOPY__HPP */
