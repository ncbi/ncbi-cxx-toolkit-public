#ifndef CLASSINFOB__HPP
#define CLASSINFOB__HPP

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
* Revision 1.3  2000/07/11 20:34:51  vasilche
* File included in all generated headers made lighter.
* Nonnecessary code moved to serialimpl.hpp.
*
* Revision 1.2  2000/07/03 18:42:33  vasilche
* Added interface to typeinfo via CObjectInfo and CConstObjectInfo.
* Reduced header dependency.
*
* Revision 1.1  2000/06/16 16:31:04  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <serial/typeref.hpp>
#include <serial/typeinfo.hpp>
#include <serial/memberlist.hpp>
#include <serial/memberid.hpp>
#include <map>
#include <set>
#include <vector>
#include <memory>

BEGIN_NCBI_SCOPE

class CClassTypeInfoBase : public CTypeInfo {
    typedef CTypeInfo CParent;
public:
    typedef CMembers::TMemberIndex TMemberIndex;
    typedef vector<pair<CMemberId, CTypeRef> > TSubClasses;
    typedef map<TTypeInfo, bool> TContainedTypes;

    typedef TObjectPtr (*TCreateFunction)(TTypeInfo info);
    typedef void (*TPostReadFunction)(TTypeInfo info, TObjectPtr object);
    typedef void (*TPreWriteFunction)(TTypeInfo info, TConstObjectPtr object);

protected:
    CClassTypeInfoBase(const type_info& ti, size_t size);
    CClassTypeInfoBase(const string& name, const type_info& ti, size_t size)
        : CParent(name)
        {
            Init(ti, size);
        }
    CClassTypeInfoBase(const char* name, const type_info& ti, size_t size)
        : CParent(name)
        {
            Init(ti, size);
        }
    
public:
    virtual ~CClassTypeInfoBase(void);

    const type_info& GetId(void) const
        {
            _ASSERT(m_Id);
            return *m_Id;
        }

    virtual size_t GetSize(void) const;

    // returns type info of pointer to this type
    static TTypeInfo GetPointerTypeInfo(const type_info& id,
                                        const CTypeRef& typeRef);

    CMembersInfo& GetMembers(void)
        {
            return m_Members;
        }
    const CMembersInfo& GetMembers(void) const
        {
            return m_Members;
        }
    TMemberIndex GetMembersCount(void) const
        {
            return GetMembers().GetMembersCount();
        }
    const CMemberId& GetMemberId(TMemberIndex index) const
        {
            return GetMembers().GetMemberId(index);
        }
    const CMemberInfo* GetMemberInfo(TMemberIndex index) const
        {
            return GetMembers().GetMemberInfo(index);
        }
    TTypeInfo GetMemberTypeInfo(TMemberIndex index) const
        {
            return GetMemberInfo(index)->GetTypeInfo();
        }

protected:
    friend class CClassInfoHelperBase;

    void SetCreateFunction(TCreateFunction func);

    void DoPostRead(TObjectPtr object) const
        {
            if ( m_PostReadFunction )
                m_PostReadFunction(this, object);
        }
    void DoPreWrite(TConstObjectPtr object) const
        {
            if ( m_PreWriteFunction )
                m_PreWriteFunction(this, object);
        }
public:

    // finds type info (throws runtime_error if absent)
    static TTypeInfo GetClassInfoByName(const string& name);
    static TTypeInfo GetClassInfoById(const type_info& id);
    static TTypeInfo GetClassInfoBy(const type_info& id,
                                    void (*creator)(void));

    bool IsCObject(void) const;
    void SetIsCObject(void);
    void SetIsCObject(const void* /*object*/)
        {
        }
    void SetIsCObject(const CObject* /*object*/)
        {
            SetIsCObject();
        }

    void SetPostReadFunction(TPostReadFunction func);
    void SetPreWriteFunction(TPreWriteFunction func);

    virtual TObjectPtr Create(void) const;

    // iterators interface
    virtual bool MayContainType(TTypeInfo type) const;
    virtual bool IsOrMayContainType(TTypeInfo type) const;

protected:
    virtual bool CalcMayContainType(TTypeInfo typeInfo) const;

private:
    const type_info* m_Id;
    size_t m_Size;
    CMembersInfo m_Members;
    bool m_IsCObject;

    TCreateFunction m_CreateFunction;
    TPostReadFunction m_PostReadFunction;
    TPreWriteFunction m_PreWriteFunction;

    mutable auto_ptr<TContainedTypes> m_ContainedTypes;

    // class mapping
    typedef set<CClassTypeInfoBase*> TClasses;
    typedef map<const type_info*, const CClassTypeInfoBase*,
        CLessTypeInfo> TClassesById;
    typedef map<string, const CClassTypeInfoBase*> TClassesByName;

    static TClasses* sm_Classes;
    static TClassesById* sm_ClassesById;
    static TClassesByName* sm_ClassesByName;

    void Init(const type_info& id, size_t size);
    void Register(void);
    void Deregister(void);
    static TClasses& Classes(void);
    static TClassesById& ClassesById(void);
    static TClassesByName& ClassesByName(void);
};

//#include <serial/classinfob.inl>

END_NCBI_SCOPE

#endif  /* CLASSINFOB__HPP */
