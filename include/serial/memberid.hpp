#ifndef MEMBERID__HPP
#define MEMBERID__HPP

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
* Revision 1.12  2000/09/01 13:15:59  vasilche
* Implemented class/container/choice iterators.
* Implemented CObjectStreamCopier for copying data without loading into memory.
*
* Revision 1.11  2000/08/15 19:44:39  vasilche
* Added Read/Write hooks:
* CReadObjectHook/CWriteObjectHook for objects of specified type.
* CReadClassMemberHook/CWriteClassMemberHook for specified members.
* CReadChoiceVariantHook/CWriteChoiceVariant for specified choice variants.
* CReadContainerElementHook/CWriteContainerElementsHook for containers.
*
* Revision 1.10  2000/07/03 18:42:34  vasilche
* Added interface to typeinfo via CObjectInfo and CConstObjectInfo.
* Reduced header dependency.
*
* Revision 1.9  2000/06/16 16:31:05  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.8  2000/05/24 20:08:12  vasilche
* Implemented XML dump.
*
* Revision 1.7  2000/05/09 16:38:32  vasilche
* CObject::GetTypeInfo now moved to CObjectGetTypeInfo::GetTypeInfo to reduce possible errors.
* Added write context to CObjectOStream.
* Inlined most of methods of helping class Member, Block, ByteBlock etc.
*
* Revision 1.6  2000/01/05 19:43:43  vasilche
* Fixed error messages when reading from ASN.1 binary file.
* Fixed storing of integers with enumerated values in ASN.1 binary file.
* Added TAG support to key/value of map.
* Added support of NULL variant in CHOICE.
*
* Revision 1.5  1999/12/17 19:04:52  vasilche
* Simplified generation of GetTypeInfo methods.
*
* Revision 1.4  1999/09/22 20:11:48  vasilche
* Modified for compilation on IRIX native c++ compiler.
*
* Revision 1.3  1999/07/13 20:18:05  vasilche
* Changed types naming.
*
* Revision 1.2  1999/07/02 21:31:43  vasilche
* Implemented reading from ASN.1 binary format.
*
* Revision 1.1  1999/06/30 16:04:23  vasilche
* Added support for old ASN.1 structures.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE

class CMembersInfo;

// CMemberId class holds information about logical object member access:
//     name and/or tag (ASN.1)
// default value of name is empty string
// default value of tag is eNoExplicitTag
class CMemberId {
public:
    typedef int TTag;
    enum {
        eNoExplicitTag = -1,
        eParentTag = 0,
        eFirstTag = 0
    };

    CMemberId(void);
    CMemberId(TTag tag);
    CMemberId(const string& name);
    CMemberId(const string& name, TTag tag);
    CMemberId(const char* name);
    CMemberId(const char* name, TTag tag);
    CMemberId(const CMemberId& id);
    CMemberId& operator=(const CMemberId& id);
    ~CMemberId(void);

    const string& GetName(void) const;     // ASN.1 tag name
    TTag GetExplicitTag(void) const;       // ASN.1 explicit binary tag value
    TTag GetTag(void) const;               // ASN.1 effective binary tag value

    // return visible representation of CMemberId (as in ASN.1)
    string ToString(void) const;

private:
    TTag GetTagLong(void) const;  // update and return effective binary tag
    
    CMembersInfo* m_MemberList; // member list

    // identification
    string m_Name;
    TTag m_ExplicitTag;

    friend class CMembersInfo;

    mutable TTag m_Tag;
};

#include <serial/memberid.inl>

END_NCBI_SCOPE

#endif  /* MEMBERID__HPP */
