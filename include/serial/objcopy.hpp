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
*   ObjectStreamCopier reads serial data object from one stream and
*   immediately writes it into another one, usually using different encoding
*   format. Converted data is not stored in memory.
*/

#include <corelib/ncbistd.hpp>
#include <serial/typeinfo.hpp>
#include <serial/objostr.hpp>
#include <serial/objistr.hpp>
#include <serial/objhook.hpp>
#include <serial/impl/pathhook.hpp>


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

/////////////////////////////////////////////////////////////////////////////
///
/// CObjectStreamCopier --
///
/// Read serial data object from one stream and immediately write
/// it into another one, usually using different encoding format.
/// The benefit of using Copier is that converted data is not stored in memory
class NCBI_XSERIAL_EXPORT CObjectStreamCopier
{
public:
    /// Constructor
    ///
    /// @param in
    ///   Input stream reader
    /// @param out
    ///   Output stream writer    
    CObjectStreamCopier(CObjectIStream& in, CObjectOStream& out);
    
    /// Destructor
    ~CObjectStreamCopier(void);

    CObjectIStream& In(void) const;
    CObjectOStream& Out(void) const;

    void ResetLocalHooks(void);

    /// Copy data
    ///
    /// @param type
    ///   Serial class type description
    void Copy(const CObjectTypeInfo& type);

    enum ENoFileHeader {
        eNoFileHeader
    };
    /// Copy data when the input file header is already read
    ///
    /// @param type
    ///   Type information
    /// @param noFileHeader
    ///   Omit file header in the input stream
    void Copy(TTypeInfo type, ENoFileHeader noFileHeader);

    /// Copy object, omitting file header both
    /// in input and output streams
    ///
    /// @param type
    ///   Type information
    void CopyObject(TTypeInfo type);

    void CopyExternalObject(TTypeInfo type);

    // primitive types copy
    void CopyString(EStringType type = eStringTypeVisible);
    void CopyStringStore(void);
    void CopyByteBlock(void);

    void CopyAnyContentObject(void);

    // complex types copy
    void CopyNamedType(TTypeInfo namedTypeInfo, TTypeInfo objectType);

    void CopyPointer(TTypeInfo declaredType);
    bool CopyNullPointer(void);

    void CopyContainer(const CContainerTypeInfo* containerType);

    void CopyClassRandom(const CClassTypeInfo* classType);
    void CopyClassSequential(const CClassTypeInfo* classType);

    void CopyChoice(const CChoiceTypeInfo* choiceType);
    void CopyAlias(const CAliasTypeInfo* aliasType);

    typedef CObjectIStream::TFailFlags TFailFlags;
    void ThrowError1(const CDiagCompileInfo& diag_info,
                     TFailFlags fail, const char* message);
    void ThrowError1(const CDiagCompileInfo& diag_info,
                     TFailFlags fail, const string& message);
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


#include <serial/impl/objcopy.inl>

END_NCBI_SCOPE

#endif  /* OBJCOPY__HPP */
