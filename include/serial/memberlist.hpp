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
#include <serial/memberid.hpp>
#include <vector>
#include <map>

BEGIN_NCBI_SCOPE

class CMemberInfo;

class CMembers {
public:
    typedef CMemberId::TTag TTag;
    typedef unsigned TIndex;
    typedef vector<CMemberId> TMembers;
    typedef map<string, TIndex> TMembersByName;
    typedef map<TTag, TIndex> TMembersByTag;

    CMembers(void);
    ~CMembers(void);

    bool Empty(void) const
        {
            return m_Members.size() == 0;
        }

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

    void AddMember(const CMemberId& id);

    const CMemberId& GetMemberId(TIndex index) const
        { return m_Members[index]; }
    CMemberId GetCompleteMemberId(TIndex index) const;

    TIndex FindMember(const string& name) const;
    TIndex FindMember(TTag tag) const;
    TIndex FindMember(const CMemberId& id) const;

private:
    TMembers m_Members;
    mutable auto_ptr<TMembersByName> m_MembersByName;
    mutable auto_ptr<TMembersByTag> m_MembersByTag;
};

//#include <serial/memberlist.inl>

END_NCBI_SCOPE

#endif  /* MEMBERLIST__HPP */
