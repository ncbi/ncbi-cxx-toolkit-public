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
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <serial/impl/stdtypeinfo.hpp>
#include <serial/impl/typeref.hpp>
#include <serial/typeinfo.hpp>
#include <serial/impl/memberlist.hpp>
#include <map>
#include <set>
#include <memory>


/** @addtogroup TypeInfoCPP
 *
 * @{
 */


BEGIN_NCBI_SCOPE

class NCBI_XSERIAL_EXPORT CClassTypeInfoBase : public CTypeInfo {
    typedef CTypeInfo CParent;
public:
    typedef map<TTypeInfo, EMayContainType> TContainedTypes;

protected:
    CClassTypeInfoBase(ETypeFamily typeFamily, size_t size, const char* name,
                       const void* nonCObject, TTypeCreate createFunc,
                       const type_info& ti);
    CClassTypeInfoBase(ETypeFamily typeFamily, size_t size, const char* name,
                       const CObject* cObject, TTypeCreate createFunc,
                       const type_info& ti);
    CClassTypeInfoBase(ETypeFamily typeFamily, size_t size, const string& name,
                       const void* nonCObject, TTypeCreate createFunc,
                       const type_info& ti);
    CClassTypeInfoBase(ETypeFamily typeFamily, size_t size, const string& name,
                       const CObject* cObject, TTypeCreate createFunc,
                       const type_info& ti);
    
public:
    virtual ~CClassTypeInfoBase(void);

    CMemberInfo* AddMember(const char* memberId,
                           const void* memberPtr, const CTypeRef& memberType);
    CMemberInfo* AddMember(const CMemberId& memberId,
                           const void* memberPtr, const CTypeRef& memberType);

    const CItemsInfo& GetItems(void) const;
    const CItemInfo* GetItemInfo(const string& name) const;

    const type_info& GetId(void) const;

    // PreRead/PostRead/PreWrite/PostWrite
    typedef void (*TPreReadFunction)(TTypeInfo info, TObjectPtr object);
    typedef void (*TPostReadFunction)(TTypeInfo info, TObjectPtr object);
    typedef void (*TPreWriteFunction)(TTypeInfo info, TConstObjectPtr object);
    typedef void (*TPostWriteFunction)(TTypeInfo info, TConstObjectPtr object);

    void SetPreReadFunction(TPreReadFunction func);
    void SetPostReadFunction(TPostReadFunction func);
    void SetPreWriteFunction(TPreWriteFunction func);
    void SetPostWriteFunction(TPostWriteFunction func);

public:
    // finds type info (throws runtime_error if absent)
    static TTypeInfo GetClassInfoByName(const string& name);
    static TTypeInfo GetClassInfoById(const type_info& id);

    typedef set<string> TRegModules;
    typedef set<string> TRegClasses;
    static void RegisterModule(const string& module);
    static void GetRegisteredModuleNames(TRegModules& modules);
    static void GetRegisteredClassNames(const string& module, TRegClasses& names);

    const CObject* GetCObjectPtr(TConstObjectPtr objectPtr) const;

    // iterators interface
    virtual EMayContainType GetMayContainType(TTypeInfo type) const;

    // helping member iterator class (internal use)
    class CIterator : public CItemsInfo::CIterator
    {
        typedef CItemsInfo::CIterator CParent;
    public:
        CIterator(const CClassTypeInfoBase* type);
        CIterator(const CClassTypeInfoBase* type, TMemberIndex index);
    };

protected:
    friend class CIterator;
    CItemsInfo& GetItems(void);

    virtual EMayContainType CalcMayContainType(TTypeInfo typeInfo) const;

private:
    const type_info* m_Id;

    CItemsInfo m_Items;

    mutable auto_ptr<TContainedTypes> m_ContainedTypes;

    // class mapping
    typedef set<CClassTypeInfoBase*> TClasses;
    typedef map<const type_info*, const CClassTypeInfoBase*,
        CLessTypeInfo> TClassesById;
    typedef multimap<string, const CClassTypeInfoBase*> TClassesByName;

    static TClasses* sm_Classes;
    static TClassesById* sm_ClassesById;
    static TClassesByName* sm_ClassesByName;
    static set<string>* sm_Modules;

    void InitClassTypeInfoBase(const type_info& id);
    void Register(void);
    void Deregister(void);
    static TClasses& Classes(void);
    static TClassesById& ClassesById(void);
    static TClassesByName& ClassesByName(void);
};


/* @} */


#include <serial/impl/classinfob.inl>

END_NCBI_SCOPE

#endif  /* CLASSINFOB__HPP */
