#ifndef MEMBER__HPP
#define MEMBER__HPP

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
* Revision 1.1  1999/06/24 14:44:38  vasilche
* Added binary ASN.1 output.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/serialdef.hpp>
#include <serial/typeref.hpp>

BEGIN_NCBI_SCOPE

class CClassInfoTmpl;

class CMemberInfo {
public:
    typedef int TTag;

    // default constructor for using in map
    CMemberInfo(void);

    // superclass member
    CMemberInfo(size_t offset, const CTypeRef& type);
    
    // common member
    CMemberInfo(const string& name, size_t offset, const CTypeRef& type);

    const string& GetName(void) const;

    size_t GetOffset(void) const;

    TTag GetTag(void) const;
    CMemberInfo* SetTag(TTag tag);

    bool Optional(void) const;
    TConstObjectPtr GetDefault(void) const;
    CMemberInfo* SetDefault(TConstObjectPtr def);

    TTypeInfo GetTypeInfo(void) const;

    size_t GetSize(void) const;

    TObjectPtr GetMember(TObjectPtr object) const;
    TConstObjectPtr GetMember(TConstObjectPtr object) const;
    TObjectPtr GetContainer(TObjectPtr object) const;
    TConstObjectPtr GetContainer(TConstObjectPtr object) const;

    size_t GetEndOffset(void) const;

private:
    string m_Name;
    size_t m_Offset;
    TTag m_Tag;
    TConstObjectPtr m_Default;
    CTypeRef m_Type;

    friend class CClassInfoTmpl;
};

#include <serial/member.inl>

END_NCBI_SCOPE

#endif  /* MEMBER__HPP */
