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
* Revision 1.13  2000/10/03 17:22:42  vasilche
* Reduced header dependency.
* Reduced size of debug libraries on WorkShop by 3 times.
* Fixed tag allocation for parent classes.
* Fixed CObject allocation/deallocation in streams.
* Moved instantiation of several templates in separate source file.
*
* Revision 1.12  2000/09/18 20:00:22  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* Revision 1.11  2000/09/01 13:16:15  vasilche
* Implemented class/container/choice iterators.
* Implemented CObjectStreamCopier for copying data without loading into memory.
*
* Revision 1.10  2000/08/15 19:44:47  vasilche
* Added Read/Write hooks:
* CReadObjectHook/CWriteObjectHook for objects of specified type.
* CReadClassMemberHook/CWriteClassMemberHook for specified members.
* CReadChoiceVariantHook/CWriteChoiceVariant for specified choice variants.
* CReadContainerElementHook/CWriteContainerElementsHook for containers.
*
* Revision 1.9  2000/07/03 18:42:44  vasilche
* Added interface to typeinfo via CObjectInfo and CConstObjectInfo.
* Reduced header dependency.
*
* Revision 1.8  2000/06/16 16:31:19  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.7  2000/05/24 20:08:46  vasilche
* Implemented XML dump.
*
* Revision 1.6  2000/05/09 16:38:38  vasilche
* CObject::GetTypeInfo now moved to CObjectGetTypeInfo::GetTypeInfo to reduce possible errors.
* Added write context to CObjectOStream.
* Inlined most of methods of helping class Member, Block, ByteBlock etc.
*
* Revision 1.5  2000/01/05 19:43:53  vasilche
* Fixed error messages when reading from ASN.1 binary file.
* Fixed storing of integers with enumerated values in ASN.1 binary file.
* Added TAG support to key/value of map.
* Added support of NULL variant in CHOICE.
*
* Revision 1.4  1999/12/17 19:05:02  vasilche
* Simplified generation of GetTypeInfo methods.
*
* Revision 1.3  1999/07/22 19:40:54  vasilche
* Fixed bug with complex object graphs (pointers to members of other objects).
*
* Revision 1.2  1999/07/02 21:31:54  vasilche
* Implemented reading from ASN.1 binary format.
*
* Revision 1.1  1999/06/30 16:04:51  vasilche
* Added support for old ASN.1 structures.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/memberid.hpp>
#include <serial/memberlist.hpp>

BEGIN_NCBI_SCOPE

CMemberId::CMemberId(void)
    : m_Tag(eNoExplicitTag), m_ExplicitTag(false)
{
}

CMemberId::CMemberId(TTag tag, bool explicitTag)
    : m_Tag(tag), m_ExplicitTag(explicitTag)
{
}

CMemberId::CMemberId(const string& name)
    : m_Name(name), m_Tag(eNoExplicitTag), m_ExplicitTag(false)
{
}

CMemberId::CMemberId(const string& name, TTag tag, bool explicitTag)
    : m_Name(name), m_Tag(tag), m_ExplicitTag(explicitTag)
{
}

CMemberId::CMemberId(const char* name)
    : m_Name(name), m_Tag(eNoExplicitTag), m_ExplicitTag(false)
{
    _ASSERT(name);
}

CMemberId::CMemberId(const char* name, TTag tag, bool explicitTag)
    : m_Name(name), m_Tag(tag), m_ExplicitTag(explicitTag)
{
    _ASSERT(name);
}

CMemberId::~CMemberId(void)
{
}

bool CMemberId::HaveParentTag(void) const
{
    return GetTag() == eParentTag && !HaveExplicitTag();
}

void CMemberId::SetParentTag(void)
{
    SetTag(eParentTag, false);
}

string CMemberId::ToString(void) const
{
    if ( !m_Name.empty() )
        return m_Name;
    else
        return '[' + NStr::IntToString(GetTag()) + ']';
}

END_NCBI_SCOPE
