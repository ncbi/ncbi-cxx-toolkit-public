#ifndef MEMBERLIST__HPP
#define MEMBERLIST__HPP

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
* Revision 1.17  2000/09/18 20:00:03  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* Revision 1.16  2000/09/01 13:15:59  vasilche
* Implemented class/container/choice iterators.
* Implemented CObjectStreamCopier for copying data without loading into memory.
*
* Revision 1.15  2000/08/15 19:44:39  vasilche
* Added Read/Write hooks:
* CReadObjectHook/CWriteObjectHook for objects of specified type.
* CReadClassMemberHook/CWriteClassMemberHook for specified members.
* CReadChoiceVariantHook/CWriteChoiceVariant for specified choice variants.
* CReadContainerElementHook/CWriteContainerElementsHook for containers.
*
* Revision 1.14  2000/07/03 18:42:34  vasilche
* Added interface to typeinfo via CObjectInfo and CConstObjectInfo.
* Reduced header dependency.
*
* Revision 1.13  2000/06/16 16:31:05  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.12  2000/06/01 19:06:56  vasilche
* Added parsing of XML data.
*
* Revision 1.11  2000/05/24 20:08:12  vasilche
* Implemented XML dump.
*
* Revision 1.10  2000/04/10 21:01:39  vasilche
* Fixed Erase for map/set.
* Added iteratorbase.hpp header for basic internal classes.
*
* Revision 1.9  2000/04/10 18:01:51  vasilche
* Added Erase() for STL types in type iterators.
*
* Revision 1.8  2000/03/29 15:55:20  vasilche
* Added two versions of object info - CObjectInfo and CConstObjectInfo.
* Added generic iterators by class -
* 	CTypeIterator<class>, CTypeConstIterator<class>,
* 	CStdTypeIterator<type>, CStdTypeConstIterator<type>,
* 	CObjectsIterator and CObjectsConstIterator.
*
* Revision 1.7  2000/02/01 21:44:35  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Added buffering to CObjectIStreamAsn.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
*
* Revision 1.6  2000/01/10 19:46:31  vasilche
* Fixed encoding/decoding of REAL type.
* Fixed encoding/decoding of StringStore.
* Fixed encoding/decoding of NULL type.
* Fixed error reporting.
* Reduced object map (only classes).
*
* Revision 1.5  1999/09/22 20:11:48  vasilche
* Modified for compilation on IRIX native c++ compiler.
*
* Revision 1.4  1999/09/14 18:54:03  vasilche
* Fixed bugs detected by gcc & egcs.
* Removed unneeded includes.
*
* Revision 1.3  1999/07/20 18:22:54  vasilche
* Added interface to old ASN.1 routines.
* Added fixed choice of subclasses to use for pointers.
*
* Revision 1.2  1999/07/01 17:55:18  vasilche
* Implemented ASN.1 binary write.
*
* Revision 1.1  1999/06/30 16:04:26  vasilche
* Added support for old ASN.1 structures.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiutil.hpp>
#include <serial/lightstr.hpp>
#include <serial/memberid.hpp>
#include <serial/member.hpp>
#include <vector>
#include <map>

BEGIN_NCBI_SCOPE

class CConstObjectInfo;
class CObjectInfo;

// This class supports sets of members with IDs
class CMembersInfo
{
public:
    typedef CMemberId::TTag TTag;
    typedef vector< AutoPtr<CItemInfo> > TItems;
    typedef map<CLightString, TMemberIndex> TMembersByName;
    typedef map<TTag, TMemberIndex> TMembersByTag;
    typedef map<size_t, TMemberIndex> TMembersByOffset;

    CMembersInfo(void);
    virtual ~CMembersInfo(void);

    bool Empty(void) const
        {
            return m_Items.empty();
        }

    static TMemberIndex FirstIndex(void)
        {
            return kFirstMemberIndex;
        }
    TMemberIndex LastIndex(void) const
        {
            return m_Items.size();
        }

    TMemberIndex Find(const CLightString& name) const;
    TMemberIndex Find(const CLightString& name, TMemberIndex pos) const;
    TMemberIndex Find(TTag tag) const;
    TMemberIndex Find(TTag tag, TMemberIndex pos) const;

    void UpdateTags(void) const;
    size_t GetFirstItemOffset(void) const;

    const CItemInfo* GetItemInfo(TMemberIndex index) const;
    void AddItem(CItemInfo* item);

protected:
    CItemInfo* x_GetItemInfo(TMemberIndex index) const;

private:
    const TMembersByName& GetMembersByName(void) const;
    const TMembersByOffset& GetMembersByOffset(void) const;

    TItems m_Items;
    mutable auto_ptr<TMembersByName> m_MembersByName;
    mutable TMemberIndex m_ZeroTagIndex;
    mutable auto_ptr<TMembersByTag> m_MembersByTag;
    mutable auto_ptr<TMembersByOffset> m_MembersByOffset;

    CMembersInfo(const CMembersInfo&);
    CMembersInfo& operator=(const CMembersInfo&);
};

#include <serial/memberlist.inl>

END_NCBI_SCOPE

#endif  /* MEMBERLIST__HPP */
