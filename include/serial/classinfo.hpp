#ifndef CLASSINFO__HPP
#define CLASSINFO__HPP

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
* Revision 1.1  1999/05/19 19:56:23  vasilche
* Commit just in case.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/typeinfo.hpp>
#include <list>
#include <map>

BEGIN_NCBI_SCOPE

class CObjectIStream;
class CObjectOStream;
class CObjectList;
class CTypeInfo;
class CMemberInfo;

class CMemberInfo {
public:
    typedef CTypeInfo::TObjectPtr TObjectPtr;
    typedef CTypeInfo::TConstObjectPtr TConstObjectPtr;
    typedef CTypeInfo::TTypeInfo TTypeInfo;

    // default constructor for using in map
    CMemberInfo(void)
        { }

    // superclass member
    CMemberInfo(offset_t offset, TTypeInfo info)
        : m_Offset(offset), m_Info(info)
        { }
    
    // common member
    CMemberInfo(const string& name, offset_t offset, TTypeInfo info)
        : m_Name(name), m_Offset(offset), m_Info(info)
        { }

    const string& GetName(void) const
        { return m_Name; }

    offset_t GetOffset(void) const
        { return m_Offset; }

    size_t GetSize(void) const
        { return m_Info->GetSize(); }

    TTypeInfo GetTypeInfo(void) const
        { return m_Info; }

    TObjectPtr GetMemeberPtr(TObjectPtr object) const
        { return static_cast<char*>(object) + m_Offset; }
    TConstObjectPtr GetMemberPtr(TConstObjectPtr object) const
        { return static_cast<const char*>(object) + m_Offset; }

private:
    string m_Name;
    offset_t m_Offset;
    TTypeInfo m_Info;
};

class CClassInfoTmpl : public CTypeInfo {
    typedef CTypeInfo CParent;
public:
    typedef map<string, CMemberInfo> TMembers;
    typedef map<size_t, CMemberInfo> TMembersByOffset;
    typedef TMembers::const_iterator TMemberIterator;

    CClassInfoTmpl(const type_info& ti, size_t size,
                   const type_info& pti, offset_t offset,
                   void* (*creator)(void));

    virtual size_t GetSize(void) const;

    virtual TObjectPtr Create(void) const;

    CClassInfoTmpl* AddMember(const CMemberInfo& member);

    const CMemberInfo& FindMember(const string& name) const;
    TMemberIterator MemberBegin(void) const;
    TMemberIterator MemberEnd(void) const;

protected:
    virtual TTypeInfo GetRealDataTypeInfo(TConstObjectPtr object) const = 0;

    virtual void ReadData(CObjectIStream& in, TObjectPtr object) const;

    virtual void CollectMembers(CObjectList& list,
                                TConstObjectPtr object) const;

    virtual void WriteData(CObjectOStream& out,
                           TConstObjectPtr object) const;

private:
    size_t m_Size;
    TObjectPtr (*m_Creator)(void);

    TMembers m_Members;
    TMembersByOffset m_MembersByOffset;
};

template<class CLASS, class PCLASS = void>
class CClassInfo : public CClassInfoTmpl
{
public:
    CClassInfo(void)
        : CClassInfoTmpl(typeid(CLASS), sizeof(CLASS),
                         typeid(PCLASS),
                         size_t(static_cast<const PCLASS*>
                                (reinterpret_cast<const CLASS*>(0))),
                         &sm_Create)
        {
        }

protected:

    static TObjectPtr sm_Create(void)
        {
            return new CLASS();
        }

    virtual TTypeInfo GetRealDataTypeInfo(TConstObjectPtr object) const
        {
            return GetTypeInfo(typeid(*static_cast<const CLASS*>(object)).name());
        }

    virtual CTypeInfo* CreatePointerTypeInfo(void) const
        {
            return new CPointerTypeInfo<CLASS>(this);
        }
};

#define NEW_CLASS_INFO(CLASS, PCLASS) \
    new CClassInfo(typeid(CLASS).name(), \
                   new CMemberInfo(typeid(PCLASS).name(), \
                                   size_t(static_cast<PCLASS>, reinterpret_cast<CLASS*>(0)) \
                                          ), \
                          CLASS::s_Create); \


//#include <serial/classinfo.inl>

END_NCBI_SCOPE

#endif  /* CLASSINFO__HPP */
