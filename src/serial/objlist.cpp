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
* Revision 1.7  1999/06/30 16:04:57  vasilche
* Added support for old ASN.1 structures.
*
* Revision 1.6  1999/06/24 14:44:57  vasilche
* Added binary ASN.1 output.
*
* Revision 1.5  1999/06/16 20:35:33  vasilche
* Cleaned processing of blocks of data.
* Added input from ASN.1 text format.
*
* Revision 1.4  1999/06/15 16:19:49  vasilche
* Added ASN.1 object output stream.
*
* Revision 1.3  1999/06/10 21:06:48  vasilche
* Working binary output and almost working binary input.
*
* Revision 1.2  1999/06/07 19:30:26  vasilche
* More bug fixes
*
* Revision 1.1  1999/06/04 20:51:46  vasilche
* First compilable version of serialization.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/objlist.hpp>
#include <serial/typeinfo.hpp>
#include <serial/member.hpp>

BEGIN_NCBI_SCOPE

COObjectList::COObjectList(void)
    : m_NextObjectIndex(0)
{
}

COObjectList::~COObjectList(void)
{
}

bool COObjectList::Add(TConstObjectPtr object, TTypeInfo typeInfo)
{
    _TRACE("COObjectList::Add(" << unsigned(object) << ", " << typeInfo->GetName() << ")");
    // note that TObject have reverse sort order
    // just in case typedef in header file will be redefined:
    typedef map<TConstObjectPtr, CORootObjectInfo, greater<TConstObjectPtr> > TObject;

    TConstObjectPtr endOfObject = typeInfo->EndOf(object);

    TObjects::iterator before = m_Objects.lower_bound(object);
    // before->first <= object, (before-1)->first > object
    if ( before == m_Objects.end() ) {
        // there is not objects before then new one
        if ( m_Objects.empty() ) {
            // first object - just insert it
            m_Objects[object] = CORootObjectInfo(typeInfo);
            return true;
        }
    }
    else {
        TTypeInfo beforeTypeInfo = before->second.GetTypeInfo();
        TConstObjectPtr beforeObject = before->first;
        TConstObjectPtr beforeEndOfObject = beforeTypeInfo->EndOf(beforeObject);
        if ( object < beforeEndOfObject ) {
            // object and beforeObject may overlap
            if ( object > beforeObject || endOfObject < beforeEndOfObject ) {
                // object must be inside beforeObject
                if ( !CheckMember(beforeObject, beforeTypeInfo,
                                  object, typeInfo) ) {
                    THROW1_TRACE(runtime_error, "overlapping objects");
                }
                // in this case object completely inside beforeObject so
                // we'll not check anymore
                return false;
            }
            if ( endOfObject == beforeEndOfObject ) {
                // special case - object and beforeObject use the same memory
                // trivial check if types are the same
                if ( typeInfo == beforeTypeInfo ) {
                    // it's ok this object is already in map
                    return false;
                }
                // check who is owner
                if ( CheckMember(beforeObject, beforeTypeInfo,
                                 object, typeInfo) ) {
                    // good beforeObject is owner of object
                    return false;
                }
                if ( !CheckMember(object, typeInfo,
                                  beforeObject, beforeTypeInfo) ) {
                    THROW1_TRACE(runtime_error, "overlapping objects");
                }
                // ok object is owner of beforeObject
                if ( before->second.IsWritten() ) {
                    THROW1_TRACE(runtime_error, "member already written");
                }
                // lets replace it
                before->second = CORootObjectInfo(typeInfo);
                return true;
            }
            // here object == beforeObject && endOfObject > beforeEndOfObject
            // so beforeObject must be inside object
            // increase before to include it in following check
            ++before;
        }
    }
    // our object is smallest -> check for overlapping with first
    TObjects::iterator after = m_Objects.upper_bound(endOfObject);
    // after->first < endOfObject, (after-1) >= endOfObject
    for ( TObjects::iterator i = after; i != before; ++i ) {
        if ( !CheckMember(object, typeInfo, i->first, i->second.m_TypeInfo) ) {
            THROW1_TRACE(runtime_error, "overlapping objects");
        }
        if ( i->second.IsWritten() ) {
            THROW1_TRACE(runtime_error, "member already written");
        }
    }
    // ok all objects from after to before are inside object
    // so replace them by object
    m_Objects.erase(after, before);
    m_Objects[object] = CORootObjectInfo(typeInfo);
    return true;
}

bool COObjectList::CheckMember(TConstObjectPtr owner, TTypeInfo ownerTypeInfo,
                               TConstObjectPtr member, TTypeInfo memberTypeInfo)
{
    while ( owner != member || ownerTypeInfo != memberTypeInfo ) {
        const CMemberInfo* memberInfo =
            ownerTypeInfo->LocateMember(owner, member, memberTypeInfo).second;
        if ( memberInfo == 0 ) {
            return false;
        }
        ownerTypeInfo = memberInfo->GetTypeInfo();
        owner = memberInfo->GetMember(owner);
    }
    return true;
}

void COObjectList::CheckAllWritten(void) const
{
    for ( TObjects::const_iterator i = m_Objects.begin();
          i != m_Objects.end();
          ++i ) {
        if ( !i->second.IsWritten() ) {
            ERR_POST("object not written: " << unsigned(i->first) <<
                     '{' << i->second.GetTypeInfo()->GetName() << '}');
            _TRACE("object not written: " << unsigned(i->first) <<
                   '{' << i->second.GetTypeInfo()->GetName() << '}');
            THROW1_TRACE(runtime_error, "object not written");
        }
    }
}

void COObjectList::SetObject(COObjectInfo& info,
                             TConstObjectPtr member,
                             TTypeInfo memberTypeInfo) const
{
    // note that TObject have reverse sort order
    // just in case typedef in header file will be redefined:
    typedef map<TConstObjectPtr, CORootObjectInfo, greater<TConstObjectPtr> > TObject;

    TObjects::const_iterator root = m_Objects.lower_bound(member);
    // root->first <= object, (root-1)->first > object
    if ( root == m_Objects.end() ) {
        THROW1_TRACE(runtime_error, "object is not collected");
    }

    info.m_RootObject = &*root;
    TConstObjectPtr owner = info.GetRootObject();
    TTypeInfo ownerTypeInfo = info.GetRootObjectInfo().GetTypeInfo();

    while ( owner != member || ownerTypeInfo != memberTypeInfo ) {
        pair<const CMemberId*, const CMemberInfo*> memberInfo =
            ownerTypeInfo->LocateMember(owner, member, memberTypeInfo);
        if ( memberInfo.second == 0 ) {
            THROW1_TRACE(runtime_error, "object is not collected");
        }
        info.m_Members.push_back(memberInfo);
        ownerTypeInfo = memberInfo.second->GetTypeInfo();
        owner = memberInfo.second->GetMember(owner);
    }
}

void COObjectList::RegisterObject(const CORootObjectInfo& info)
{
    const_cast<CORootObjectInfo&>(info).m_Index = m_NextObjectIndex++;
}

END_NCBI_SCOPE
