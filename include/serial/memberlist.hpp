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
* Revision 1.1  1999/06/30 16:04:26  vasilche
* Added support for old ASN.1 structures.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/memberid.hpp>
#include <list>
#include <map>

BEGIN_NCBI_SCOPE

class CMemberInfo;

class CMembers {
public:
    typedef CMemberId::TTag TTag;
    typedef list<pair<CMemberId, const CMemberInfo*> > TMembers;
    typedef map<string, const CMemberInfo*> TMembersByName;
    typedef map<TTag, const CMemberInfo*> TMembersByTag;

    CMembers(void);
    ~CMembers(void);

    const TMembers& GetMembers(void) const
        {
            return m_Members;
        }
    TMembers::const_iterator begin(void) const
        {
            return m_Members.begin();
        }
    TMembers::const_iterator end(void) const
        {
            return m_Members.end();
        }
    const TMembersByName& GetMembersByName(void) const;
    const TMembersByTag& GetMembersByTag(void) const;

    CMemberInfo* AddMember(const CMemberId& id, CMemberInfo* memberInfo);

    const CMemberInfo* FindMember(const string& name) const;
    const CMemberInfo* FindMember(TTag tag) const;
    const CMemberInfo* FindMember(const CMemberId& id) const;

private:
    TMembers m_Members;
    mutable auto_ptr<TMembersByName> m_MembersByName;
    mutable auto_ptr<TMembersByTag> m_MembersByTag;
};

//#include <serial/memberlist.inl>

END_NCBI_SCOPE

#endif  /* MEMBERLIST__HPP */
