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
* Revision 1.14  1999/07/20 18:22:53  vasilche
* Added interface to old ASN.1 routines.
* Added fixed choice of subclasses to use for pointers.
*
* Revision 1.13  1999/07/13 20:18:04  vasilche
* Changed types naming.
*
* Revision 1.12  1999/07/07 19:58:44  vasilche
* Reduced amount of data allocated on heap
* Cleaned ASN.1 structures info
*
* Revision 1.11  1999/07/07 18:18:32  vasilche
* Fixed some bugs found by MS VC++
*
* Revision 1.10  1999/07/01 17:55:17  vasilche
* Implemented ASN.1 binary write.
*
* Revision 1.9  1999/06/30 16:04:20  vasilche
* Added support for old ASN.1 structures.
*
* Revision 1.8  1999/06/24 14:44:37  vasilche
* Added binary ASN.1 output.
*
* Revision 1.7  1999/06/17 18:38:48  vasilche
* Fixed order of members in class.
* Added checks for overlapped members.
*
* Revision 1.6  1999/06/15 16:20:00  vasilche
* Added ASN.1 object output stream.
*
* Revision 1.5  1999/06/10 21:06:37  vasilche
* Working binary output and almost working binary input.
*
* Revision 1.4  1999/06/07 20:42:58  vasilche
* Fixed compilation under MS VS
*
* Revision 1.3  1999/06/07 19:59:37  vasilche
* offset_t -> size_t
*
* Revision 1.2  1999/06/04 20:51:31  vasilche
* First compilable version of serialization.
*
* Revision 1.1  1999/05/19 19:56:23  vasilche
* Commit just in case.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/typeinfo.hpp>
#include <serial/memberlist.hpp>
#include <map>
#include <list>
#include <vector>

BEGIN_NCBI_SCOPE

class CObjectIStream;
class CObjectOStream;
class COObjectList;
class CMemberId;
class CMemberInfo;

struct CTypeInfoOrder
{
    // to avoid warning under MSVS, where type_info::before() erroneously
    // returns int, we'll define overloaded functions:
    static bool ToBool(bool b)
        { return b; }
    static bool ToBool(int i)
        { return i != 0; }

    bool operator()(const type_info* i1, const type_info* i2) const
		{ return ToBool(i1->before(*i2)); }
};

class CClassInfoTmpl : public CTypeInfo {
    typedef CTypeInfo CParent;
public:
    enum ERandomOrder {
        eRandomOrder
    };

    typedef CMembers::TIndex TIndex;
    typedef vector<CMemberInfo*> TMembersInfo;
    typedef map<size_t, TIndex> TMembersByOffset;
    typedef vector<CTypeRef> TSubClassesInfo;
    typedef map<const type_info*, TIndex, CTypeInfoOrder> TSubClassesById;

    CClassInfoTmpl(const string& name, const type_info& ti,
                   size_t size, bool randomOrder);
    CClassInfoTmpl(const type_info& ti, size_t size, bool randomOrder);
    CClassInfoTmpl(const type_info& ti, size_t size, bool randomOrder,
                   const CTypeRef& parent, size_t offset);
    virtual ~CClassInfoTmpl(void);

    const type_info& GetId(void) const
        { return m_Id; }

    virtual size_t GetSize(void) const;

    virtual bool Equals(TConstObjectPtr object1, TConstObjectPtr object2) const;

    virtual void Assign(TObjectPtr dst, TConstObjectPtr src) const;

    // returns type info of pointer to this type
    static TTypeInfo GetPointerTypeInfo(const type_info& id,
                                        const CTypeRef& typeRef);

    // AddMember will take ownership of member
    CMemberInfo* AddMember(const string& name, CMemberInfo* member);
    CMemberInfo* AddMember(const CMemberId& id, CMemberInfo* member);

    bool RandomOrder(void) const
        {
            return m_RandomOrder;
        }
    CClassInfoTmpl* SetRandomOrder(bool random = true)
        {
            m_RandomOrder = random;
            return this;
        }

    const TMembersByOffset& GetMembersByOffset(void) const;

    virtual TMemberIndex FindMember(const string& name) const;
    virtual TMemberIndex LocateMember(TConstObjectPtr object,
                                      TConstObjectPtr member,
                                      TTypeInfo memberTypeInfo) const;

    virtual const CMemberId* GetMemberId(TMemberIndex index) const;
    virtual const CMemberInfo* GetMemberInfo(TMemberIndex index) const;

    // finds type info (throws runtime_error if absent)
    static TTypeInfo GetClassInfoByName(const string& name);
    static TTypeInfo GetClassInfoById(const type_info& id);
    static TTypeInfo GetClassInfoBy(const type_info& id, void (*creator)(void));

    CClassInfoTmpl* AddSubClass(const CMemberId& id,
                                const CTypeRef& type);

protected:
    virtual void ReadData(CObjectIStream& in, TObjectPtr object) const;

    virtual void CollectExternalObjects(COObjectList& list,
                                        TConstObjectPtr object) const;

    virtual void WriteData(CObjectOStream& out,
                           TConstObjectPtr object) const;

    virtual void CollectPointer(COObjectList& objectList,
                                TConstObjectPtr object) const;
    virtual void WritePointer(CObjectOStream& out,
                              TConstObjectPtr object) const;
    virtual TObjectPtr ReadPointer(CObjectIStream& in) const;

    virtual const type_info& GetCPlusPlusTypeInfo(TConstObjectPtr object) const = 0;
    TTypeInfo GetRealTypeInfo(TConstObjectPtr object) const
        {
            return GetClassInfoById(GetCPlusPlusTypeInfo(object));
        }

private:
    size_t m_Size;
    bool m_RandomOrder;

    CMembers m_Members;
    TMembersInfo m_MembersInfo;
    mutable auto_ptr<TMembersByOffset> m_MembersByOffset;

    CMembers m_SubClasses;
    TSubClassesInfo m_SubClassesInfo;
    mutable auto_ptr<TSubClassesById> m_SubClassesById;

    // class mapping
    typedef list<CClassInfoTmpl*> TClasses;
    typedef map<const type_info*, const CClassInfoTmpl*,
        CTypeInfoOrder> TClassesById;
    typedef map<string, const CClassInfoTmpl*> TClassesByName;

    const type_info& m_Id;
    static TClasses* sm_Classes;
    static TClassesById* sm_ClassesById;
    static TClassesByName* sm_ClassesByName;

    void Register(void);
    void Deregister(void) const;
    static TClasses& Classes(void);
    static TClassesById& ClassesById(void);
    static TClassesByName& ClassesByName(void);

    TSubClassesById& SubClassesById(void) const;
};

class CAliasInfo;

class CClassInfoAlias : public CTypeInfo {
    typedef CTypeInfo CParent;
public:
    typedef list<CAliasInfo*> TAliases;
    typedef map<string, const CAliasInfo*> TAliasesByName;

    CClassInfoAlias(const string& name, const CTypeRef& baseTypeInfo);
    virtual ~CClassInfoAlias(void);

    virtual size_t GetSize(void) const;

    virtual TObjectPtr Create(void) const;

    virtual bool Equals(TConstObjectPtr object1, TConstObjectPtr object2) const;

    virtual void Assign(TObjectPtr dst, TConstObjectPtr src) const;

    // AddMember will take ownership of member
    CClassInfoAlias* AddMember(const string& name, const string& realName);

    virtual TMemberIndex FindMember(const string& name) const;
    virtual TMemberIndex LocateMember(TConstObjectPtr object,
                                      TConstObjectPtr member,
                                      TTypeInfo memberTypeInfo) const;

    virtual const CMemberId* GetMemberId(TMemberIndex index) const;
    virtual const CMemberInfo* GetMemberInfo(TMemberIndex index) const;

protected:
    virtual void ReadData(CObjectIStream& in, TObjectPtr object) const;

    virtual void CollectExternalObjects(COObjectList& list,
                                        TConstObjectPtr object) const;

    virtual void WriteData(CObjectOStream& out,
                           TConstObjectPtr object) const;

private:
    size_t m_Size;

    TAliases m_Aliases;
    TAliasesByName m_AliasesByName;
};

class CStructInfoTmpl : public CClassInfoTmpl
{
public:
    CStructInfoTmpl(const string& name, const type_info& id,
                    size_t size, bool randomOrder)
        : CClassInfoTmpl(name, id, size, randomOrder)
        {
        }

    virtual TObjectPtr Create(void) const;
};

template<class Class>
class CStructInfo : public CStructInfoTmpl
{
public:
    typedef Class TObjectType;
    CStructInfo(const string& name)
        : CStructInfoTmpl(name, typeid(Class), sizeof(Class), false)
        {
        }
    CStructInfo(const string& name, ERandomOrder)
        : CStructInfoTmpl(name, typeid(Class), sizeof(Class), true)
        {
        }

    virtual const type_info& GetCPlusPlusTypeInfo(TConstObjectPtr object) const
        {
            return typeid(*static_cast<const TObjectType*>(object));
        }
};

template<class CLASS, class PCLASS = CLASS>
class CClassInfo : public CClassInfoTmpl
{
public:
    typedef CLASS TObjectType;

    CClassInfo(void)
        : CClassInfoTmpl(typeid(TObjectType), sizeof(TObjectType), false)
        {
        }
    CClassInfo(ERandomOrder)
        : CClassInfoTmpl(typeid(TObjectType), sizeof(TObjectType), true)
        {
        }
    CClassInfo(const CTypeRef& pTypeRef)
        : CClassInfoTmpl(typeid(TObjectType), sizeof(TObjectType),
                         pTypeRef,
                         size_t(static_cast<const PCLASS*>
                                (static_cast<const TObjectType*>(0))))
        {
        }

    virtual const type_info& GetCPlusPlusTypeInfo(TConstObjectPtr object) const
        {
            _TRACE("GetCPlusPlusTypeInfo: " << typeid(*static_cast<const TObjectType*>(object)).name());
            return typeid(*static_cast<const TObjectType*>(object));
        }

    virtual TObjectPtr Create(void) const
        {
            return new TObjectType();
        }
};

//#include <serial/classinfo.inl>

END_NCBI_SCOPE

#endif  /* CLASSINFO__HPP */
