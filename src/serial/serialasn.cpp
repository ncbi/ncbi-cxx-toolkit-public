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
*   Serialization classes.
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.7  2005/04/26 14:18:50  vasilche
* Allow allocation of objects in CObjectMemoryPool.
*
* Revision 1.6  2004/05/17 21:03:03  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 1.5  2002/10/25 15:15:54  vasilche
* Fixed check for NCBI C toolkit.
*
* Revision 1.4  2002/10/25 15:05:44  vasilche
* Moved more code to libxcser library.
*
* Revision 1.3  2000/10/13 16:28:40  vasilche
* Reduced header dependency.
* Avoid use of templates with virtual methods.
* Reduced amount of different maps used.
* All this lead to smaller compiled code size (libraries and programs).
*
* Revision 1.2  1999/11/22 21:29:27  vasilche
* Fixed compilation on Windows
*
* Revision 1.1  1999/11/22 21:04:42  vasilche
* Cleaned main interface headers. Now generated files should include serial/serialimpl.hpp and user code should include serial/serial.hpp which became might lighter.
*
* Revision 1.3  1999/06/04 20:51:49  vasilche
* First compilable version of serialization.
*
* Revision 1.2  1999/05/19 19:56:57  vasilche
* Commit just in case.
*
* Revision 1.1  1999/03/25 19:12:04  vasilche
* Beginning of serialization library.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#if HAVE_NCBI_C

#include <serial/autoptrinfo.hpp>
#include <serial/asntypes.hpp>
#include <serial/classinfo.hpp>
#include <serial/choice.hpp>
#include <serial/serialimpl.hpp>
#include <asn.h>

BEGIN_NCBI_SCOPE

TTypeInfo COctetStringTypeInfoGetTypeInfo(void)
{
    return COctetStringTypeInfo::GetTypeInfo();
}

TTypeInfo CAutoPointerTypeInfoGetTypeInfo(TTypeInfo type)
{
    return CAutoPointerTypeInfo::GetTypeInfo(type);
}

TTypeInfo CSetOfTypeInfoGetTypeInfo(TTypeInfo type)
{
    return CSetOfTypeInfo::GetTypeInfo(type);
}

TTypeInfo CSequenceOfTypeInfoGetTypeInfo(TTypeInfo type)
{
    return CSequenceOfTypeInfo::GetTypeInfo(type);
}

TTypeInfo COldAsnTypeInfoGetTypeInfo(const string& name,
                                     TAsnNewProc newProc,
                                     TAsnFreeProc freeProc,
                                     TAsnReadProc readProc,
                                     TAsnWriteProc writeProc)
{
    return new COldAsnTypeInfo(name, newProc, freeProc, readProc, writeProc);
}

static TObjectPtr CreateAsnStruct(TTypeInfo info,
                                  CObjectMemoryPool* /*memoryPool*/)
{
    TObjectPtr object = calloc(info->GetSize(), 1);
    if ( !object )
        THROW_TRACE(bad_alloc, ());
    return object;
}

static const type_info* GetNullId(TConstObjectPtr )
{
    return 0;
}

CClassTypeInfo* CClassInfoHelperBase::CreateAsnStructInfo(const char* name,
                                                          size_t size,
                                                          const type_info& id)
{
    return CreateClassInfo(name, size,
                           TConstObjectPtr(0), &CreateAsnStruct,
                           id, &GetNullId);
}

static TMemberIndex WhichAsn(const CChoiceTypeInfo* /*choiceType*/,
                             TConstObjectPtr choicePtr)
{
    const valnode* node = static_cast<const valnode*>(choicePtr);
    return node->choice + (kEmptyChoice - 0);
}

static void SelectAsn(const CChoiceTypeInfo* /*choiceType*/,
                      TObjectPtr choicePtr,
                      TMemberIndex index,
                      CObjectMemoryPool* /*memoryPool*/)
{
    valnode* node = static_cast<valnode*>(choicePtr);
    node->choice = Uint1(index - (kEmptyChoice - 0));
}

static void ResetAsn(const CChoiceTypeInfo* /*choiceType*/,
                     TObjectPtr choicePtr)
{
    valnode* node = static_cast<valnode*>(choicePtr);
    node->choice = 0;
}

CChoiceTypeInfo* CClassInfoHelperBase::CreateAsnChoiceInfo(const char* name)
{
    return CreateChoiceInfo(name, sizeof(valnode),
                            TConstObjectPtr(0), &CreateAsnStruct, typeid(void),
                            &WhichAsn, &SelectAsn, &ResetAsn);
}

END_NCBI_SCOPE

#endif
