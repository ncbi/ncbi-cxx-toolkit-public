#ifndef OBJHOOK__HPP
#define OBJHOOK__HPP

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
* Revision 1.2  2000/09/18 20:00:04  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* Revision 1.1  2000/08/15 19:44:39  vasilche
* Added Read/Write hooks:
* CReadObjectHook/CWriteObjectHook for objects of specified type.
* CReadClassMemberHook/CWriteClassMemberHook for specified members.
* CReadChoiceVariantHook/CWriteChoiceVariant for specified choice variants.
* CReadContainerElementHook/CWriteContainerElementsHook for containers.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <serial/serialdef.hpp>

BEGIN_NCBI_SCOPE

class CObjectIStream;
class CObjectOStream;
class CObjectStreamCopier;
class CObjectInfo;
class CConstObjectInfo;
class CObjectTypeInfo;

class CReadObjectHook : public CObject
{
public:
    CReadObjectHook(void)
        {
        }
    CReadObjectHook(ECanDelete canDelete)
        : CObject(canDelete)
        {
        }
    virtual ~CReadObjectHook(void);
    
    virtual void ReadObject(CObjectIStream& in,
                            const CObjectInfo& object) = 0;
};

class CReadClassMemberHook : public CObject
{
public:
    CReadClassMemberHook(void)
        {
        }
    CReadClassMemberHook(ECanDelete canDelete)
        : CObject(canDelete)
        {
        }
    virtual ~CReadClassMemberHook(void);

    virtual void ReadClassMember(CObjectIStream& in,
                                 const CObjectInfo& object,
                                 TMemberIndex memberIndex) = 0;
    virtual void ReadMissingClassMember(CObjectIStream& in,
                                        const CObjectInfo& object,
                                        TMemberIndex memberIndex);
};

class CReadChoiceVariantHook : public CObject
{
public:
    CReadChoiceVariantHook(void)
        {
        }
    CReadChoiceVariantHook(ECanDelete canDelete)
        : CObject(canDelete)
        {
        }
    virtual ~CReadChoiceVariantHook(void);

    virtual void ReadChoiceVariant(CObjectIStream& in,
                                   const CObjectInfo& object,
                                   TMemberIndex memberIndex) = 0;
};

class CReadContainerElementHook : public CObject
{
public:
    CReadContainerElementHook(void)
        {
        }
    CReadContainerElementHook(ECanDelete canDelete)
        : CObject(canDelete)
        {
        }
    virtual ~CReadContainerElementHook(void);

    virtual void ReadContainerElement(CObjectIStream& in,
                                      const CObjectInfo& container) = 0;
};

class CWriteObjectHook : public CObject
{
public:
    CWriteObjectHook(void)
        {
        }
    CWriteObjectHook(ECanDelete canDelete)
        : CObject(canDelete)
        {
        }
    virtual ~CWriteObjectHook(void);
    
    virtual void WriteObject(CObjectOStream& out,
                             const CConstObjectInfo& object) = 0;
};

class CWriteClassMembersHook : public CObject
{
public:
    CWriteClassMembersHook(void)
        {
        }
    CWriteClassMembersHook(ECanDelete canDelete)
        : CObject(canDelete)
        {
        }
    virtual ~CWriteClassMembersHook(void);
    
    virtual void WriteClassMembers(CObjectOStream& out,
                                   const CConstObjectInfo& object) = 0;
};

class CWriteClassMemberHook : public CObject
{
public:
    CWriteClassMemberHook(void)
        {
        }
    CWriteClassMemberHook(ECanDelete canDelete)
        : CObject(canDelete)
        {
        }
    virtual ~CWriteClassMemberHook(void);
    
    virtual void WriteClassMember(CObjectOStream& out,
                                  const CConstObjectInfo& object,
                                  TMemberIndex memberIndex) = 0;
};

class CWriteChoiceVariantHook : public CObject
{
public:
    CWriteChoiceVariantHook(void)
        {
        }
    CWriteChoiceVariantHook(ECanDelete canDelete)
        : CObject(canDelete)
        {
        }
    virtual ~CWriteChoiceVariantHook(void);

    virtual void WriteChoiceVariant(CObjectOStream& in,
                                    const CConstObjectInfo& object,
                                    TMemberIndex memberIndex) = 0;
};

class CWriteContainerElementsHook : public CObject
{
public:
    CWriteContainerElementsHook(void)
        {
        }
    CWriteContainerElementsHook(ECanDelete canDelete)
        : CObject(canDelete)
        {
        }
    virtual ~CWriteContainerElementsHook(void);

    virtual void WriteContainerElements(CObjectOStream& out,
                                        const CConstObjectInfo& container) = 0;
};

class CCopyObjectHook : public CObject
{
public:
    CCopyObjectHook(void)
        {
        }
    CCopyObjectHook(ECanDelete canDelete)
        : CObject(canDelete)
        {
        }
    virtual ~CCopyObjectHook(void);
    
    virtual void CopyObject(CObjectStreamCopier& copier,
                            const CObjectTypeInfo& type) = 0;
};

class CCopyClassMemberHook : public CObject
{
public:
    CCopyClassMemberHook(void)
        {
        }
    CCopyClassMemberHook(ECanDelete canDelete)
        : CObject(canDelete)
        {
        }
    virtual ~CCopyClassMemberHook(void);
    
    virtual void CopyClassMember(CObjectStreamCopier& copier,
                                 const CObjectTypeInfo& type,
                                 TMemberIndex memberIndex) = 0;
    virtual void CopyMissingClassMember(CObjectStreamCopier& copier,
                                        const CObjectTypeInfo& type,
                                        TMemberIndex memberIndex);
};

class CCopyChoiceVariantHook : public CObject
{
public:
    CCopyChoiceVariantHook(void)
        {
        }
    CCopyChoiceVariantHook(ECanDelete canDelete)
        : CObject(canDelete)
        {
        }
    virtual ~CCopyChoiceVariantHook(void);

    virtual void CopyChoiceVariant(CObjectStreamCopier& copier,
                                   const CObjectTypeInfo& objectType,
                                   TMemberIndex memberIndex) = 0;
};

//#include <serial/objhook.inl>

END_NCBI_SCOPE

#endif  /* OBJHOOK__HPP */
