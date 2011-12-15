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
*/

#include <corelib/ncbistd.hpp>
#include <serial/impl/classinfob.hpp>
#include <serial/impl/member.hpp>
#include <list>


/** @addtogroup TypeInfoCPP
 *
 * @{
 */


BEGIN_NCBI_SCOPE

class CObjectIStream;
class CObjectOStream;
class COObjectList;
class CMemberId;
class CMemberInfo;
class CClassInfoHelperBase;
class CObjectInfoMI;

class NCBI_XSERIAL_EXPORT CClassTypeInfo : public CClassTypeInfoBase
{
    typedef CClassTypeInfoBase CParent;
protected:
    typedef const type_info* (*TGetTypeIdFunction)(TConstObjectPtr object);

    enum EClassType {
        eSequential,
        eRandom,
        eImplicit
    };

    friend class CClassInfoHelperBase;

    CClassTypeInfo(size_t size, const char* name,
                   const void* nonCObject, TTypeCreate createFunc,
                   const type_info& ti, TGetTypeIdFunction idFunc);
    CClassTypeInfo(size_t size, const char* name,
                   const CObject* cObject, TTypeCreate createFunc,
                   const type_info& ti, TGetTypeIdFunction idFunc);
    CClassTypeInfo(size_t size, const string& name,
                   const void* nonCObject, TTypeCreate createFunc,
                   const type_info& ti, TGetTypeIdFunction idFunc);
    CClassTypeInfo(size_t size, const string& name,
                   const CObject* cObject, TTypeCreate createFunc,
                   const type_info& ti, TGetTypeIdFunction idFunc);

public:
    typedef list<pair<CMemberId, CTypeRef> > TSubClasses;

    const CItemsInfo& GetMembers(void) const;
    const CMemberInfo* GetMemberInfo(TMemberIndex index) const;
    const CMemberInfo* GetMemberInfo(const CIterator& i) const;
    const CMemberInfo* GetMemberInfo(const CTempString& name) const;

    virtual bool IsDefault(TConstObjectPtr object) const;
    virtual bool Equals(TConstObjectPtr object1, TConstObjectPtr object2,
                        ESerialRecursionMode how = eRecursive) const;
    virtual void SetDefault(TObjectPtr dst) const;
    virtual void Assign(TObjectPtr dst, TConstObjectPtr src,
                        ESerialRecursionMode how = eRecursive) const;

    bool RandomOrder(void) const;
    CClassTypeInfo* SetRandomOrder(bool random = true);

    bool Implicit(void) const;
    CClassTypeInfo* SetImplicit(void);
    bool IsImplicitNonEmpty(void) const;

    void AddSubClass(const CMemberId& id, const CTypeRef& type);
    void AddSubClass(const char* id, TTypeInfoGetter getter);
    void AddSubClassNull(const CMemberId& id);
    void AddSubClassNull(const char* id);
    const TSubClasses* SubClasses(void) const;

    const CClassTypeInfo* GetParentClassInfo(void) const;
    void SetParentClass(TTypeInfo parentClass);

    typedef void (*TPreReadMemberFunction)(CObjectIStream& in,
                                           const CObjectInfoMI& member);
    void SetPreReadMemberFunction(const CTempString& member_name,
                                  TPreReadMemberFunction func);

public:

    // iterators interface
    const type_info* GetCPlusPlusTypeInfo(TConstObjectPtr object) const;

protected:
    void AssignMemberDefault(TObjectPtr object, const CMemberInfo* info) const;
    void AssignMemberDefault(TObjectPtr object, TMemberIndex index) const;
    
    virtual bool IsType(TTypeInfo typeInfo) const;
    virtual bool IsParentClassOf(const CClassTypeInfo* classInfo) const;
    virtual EMayContainType CalcMayContainType(TTypeInfo typeInfo) const;

    virtual TTypeInfo GetRealTypeInfo(TConstObjectPtr object) const;
    void RegisterSubClasses(void) const;

private:
    void InitClassTypeInfo(void);

    EClassType m_ClassType;

    const CClassTypeInfo* m_ParentClassInfo;
    auto_ptr<TSubClasses> m_SubClasses;

    TGetTypeIdFunction m_GetTypeIdFunction;

    const CMemberInfo* GetImplicitMember(void) const;

private:
    void UpdateFunctions(void);

    static void ReadClassSequential(CObjectIStream& in,
                                    TTypeInfo objectType,
                                    TObjectPtr objectPtr);
    static void ReadClassRandom(CObjectIStream& in,
                                TTypeInfo objectType,
                                TObjectPtr objectPtr);
    static void ReadImplicitMember(CObjectIStream& in,
                                   TTypeInfo objectType,
                                   TObjectPtr objectPtr);
    static void WriteClassRandom(CObjectOStream& out,
                                 TTypeInfo objectType,
                                 TConstObjectPtr objectPtr);
    static void WriteClassSequential(CObjectOStream& out,
                                     TTypeInfo objectType,
                                     TConstObjectPtr objectPtr);
    static void WriteImplicitMember(CObjectOStream& out,
                                    TTypeInfo objectType,
                                    TConstObjectPtr objectPtr);
    static void SkipClassSequential(CObjectIStream& in,
                                    TTypeInfo objectType);
    static void SkipClassRandom(CObjectIStream& in,
                                TTypeInfo objectType);
    static void SkipImplicitMember(CObjectIStream& in,
                                   TTypeInfo objectType);
    static void CopyClassSequential(CObjectStreamCopier& copier,
                                    TTypeInfo objectType);
    static void CopyClassRandom(CObjectStreamCopier& copier,
                                TTypeInfo objectType);
    static void CopyImplicitMember(CObjectStreamCopier& copier,
                                   TTypeInfo objectType);
};


/* @} */


#include <serial/impl/classinfo.inl>

END_NCBI_SCOPE

#endif  /* CLASSINFO__HPP */
