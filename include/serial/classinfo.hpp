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
* Revision 1.30  2000/06/01 19:06:55  vasilche
* Added parsing of XML data.
*
* Revision 1.29  2000/05/24 20:08:11  vasilche
* Implemented XML dump.
*
* Revision 1.28  2000/04/03 18:47:09  vasilche
* Added main include file for generated headers.
* serialimpl.hpp is included in generated sources with GetTypeInfo methods
*
* Revision 1.27  2000/03/29 15:55:19  vasilche
* Added two versions of object info - CObjectInfo and CConstObjectInfo.
* Added generic iterators by class -
* 	CTypeIterator<class>, CTypeConstIterator<class>,
* 	CStdTypeIterator<type>, CStdTypeConstIterator<type>,
* 	CObjectsIterator and CObjectsConstIterator.
*
* Revision 1.26  2000/03/07 14:05:29  vasilche
* Added stream buffering to ASN.1 binary input.
* Optimized class loading/storing.
* Fixed bugs in processing OPTIONAL fields.
*
* Revision 1.25  2000/02/17 20:02:27  vasilche
* Added some standard serialization exceptions.
* Optimized text/binary ASN.1 reading.
* Fixed wrong encoding of StringStore in ASN.1 binary format.
* Optimized logic of object collection.
*
* Revision 1.24  2000/02/01 21:44:34  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Added buffering to CObjectIStreamAsn.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
*
* Revision 1.23  2000/01/10 19:46:30  vasilche
* Fixed encoding/decoding of REAL type.
* Fixed encoding/decoding of StringStore.
* Fixed encoding/decoding of NULL type.
* Fixed error reporting.
* Reduced object map (only classes).
*
* Revision 1.22  2000/01/05 19:43:43  vasilche
* Fixed error messages when reading from ASN.1 binary file.
* Fixed storing of integers with enumerated values in ASN.1 binary file.
* Added TAG support to key/value of map.
* Added support of NULL variant in CHOICE.
*
* Revision 1.21  1999/12/17 19:04:52  vasilche
* Simplified generation of GetTypeInfo methods.
*
* Revision 1.20  1999/10/04 16:22:08  vasilche
* Fixed bug with old ASN.1 structures.
*
* Revision 1.19  1999/09/14 18:54:02  vasilche
* Fixed bugs detected by gcc & egcs.
* Removed unneeded includes.
*
* Revision 1.18  1999/09/01 17:37:59  vasilche
* Fixed vector<char> implementation.
* Added explicit naming of class info.
* Moved IMPLICIT attribute from member info to class info.
*
* Revision 1.17  1999/08/31 17:50:03  vasilche
* Implemented several macros for specific data types.
* Added implicit members.
* Added multimap and set.
*
* Revision 1.16  1999/08/13 20:22:57  vasilche
* Fixed lot of bugs in datatool
*
* Revision 1.15  1999/08/13 15:53:42  vasilche
* C++ analog of asntool: datatool
*
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
#include <corelib/ncbiobj.hpp>
#include <serial/typeinfo.hpp>
#include <serial/memberlist.hpp>
#include <map>
#include <list>
#include <vector>
#include <memory>

BEGIN_NCBI_SCOPE

class CObjectIStream;
class CObjectOStream;
class COObjectList;
class CMemberId;
class CMemberInfo;

class CClassInfoTmpl : public CTypeInfo {
    typedef CTypeInfo CParent;
public:
    typedef CMembers::TIndex TIndex;
    typedef vector<pair<CMemberId, CTypeRef> > TSubClasses;
    typedef map<TTypeInfo, bool> TContainedTypes;

    CClassInfoTmpl(const type_info& ti, size_t size);
    CClassInfoTmpl(const string& name, const type_info& ti, size_t size);
    CClassInfoTmpl(const char* name, const type_info& ti, size_t size);
    virtual ~CClassInfoTmpl(void);

    const type_info& GetId(void) const
        { return m_Id; }

    virtual size_t GetSize(void) const;

    virtual bool IsDefault(TConstObjectPtr object) const;
    virtual bool Equals(TConstObjectPtr object1,
                        TConstObjectPtr object2) const;
    virtual void SetDefault(TObjectPtr dst) const;
    virtual void Assign(TObjectPtr dst, TConstObjectPtr src) const;

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

    bool RandomOrder(void) const
        {
            return m_RandomOrder;
        }
    CClassInfoTmpl* SetRandomOrder(bool random = true)
        {
            m_RandomOrder = random;
            return this;
        }

    bool Implicit(void) const
        {
            return m_Implicit;
        }
    CClassInfoTmpl* SetImplicit(bool implicit = true)
        {
            m_Implicit = implicit;
            return this;
        }

    // finds type info (throws runtime_error if absent)
    static TTypeInfo GetClassInfoByName(const string& name);
    static TTypeInfo GetClassInfoById(const type_info& id);
    static TTypeInfo GetClassInfoBy(const type_info& id,
                                    void (*creator)(void));

    void SetParentClass(TTypeInfo parentClass);

    void AddSubClass(const CMemberId& id, const CTypeRef& type);
    void AddSubClass(const char* id, TTypeInfoGetter getter);
    void AddSubClassNull(const CMemberId& id);
    void AddSubClassNull(const char* id);
    const TSubClasses* SubClasses(void) const
        {
            return m_SubClasses.get();
        }

    virtual const type_info* GetCPlusPlusTypeInfo(TConstObjectPtr object) const;

    virtual bool IsType(TTypeInfo type) const;
    virtual bool MayContainType(TTypeInfo type) const;
    virtual bool HaveChildren(TConstObjectPtr object) const;
    virtual void BeginTypes(CChildrenTypesIterator& cc) const;
    virtual void Begin(CConstChildrenIterator& cc) const;
    virtual void Begin(CChildrenIterator& cc) const;
    virtual bool ValidTypes(const CChildrenTypesIterator& cc) const;
    virtual bool Valid(const CConstChildrenIterator& cc) const;
    virtual bool Valid(const CChildrenIterator& cc) const;
    virtual TTypeInfo GetChildType(const CChildrenTypesIterator& cc) const;
    virtual void GetChild(const CConstChildrenIterator& cc,
                          CConstObjectInfo& child) const;
    virtual void GetChild(const CChildrenIterator& cc,
                          CObjectInfo& child) const;
    virtual void NextType(CChildrenTypesIterator& cc) const;
    virtual void Next(CConstChildrenIterator& cc) const;
    virtual void Next(CChildrenIterator& cc) const;
    virtual void Erase(CChildrenIterator& cc) const;

protected:
    virtual void ReadData(CObjectIStream& in, TObjectPtr object) const;

    virtual void SkipData(CObjectIStream& in) const;

    virtual void WriteData(CObjectOStream& out,
                           TConstObjectPtr object) const;

    TTypeInfo GetRealTypeInfo(TConstObjectPtr object) const;
    TTypeInfo GetParentTypeInfo(void) const;

    void RegisterSubClasses(void) const;
    void UpdateClassInfo(const void* /*object*/)
        {
            // do nothing
        }
    void UpdateClassInfo(const CObject* object);

private:
    const type_info& m_Id;
    size_t m_Size;
    bool m_RandomOrder;
    bool m_Implicit;

    CMembersInfo m_Members;

    const CClassInfoTmpl* m_ParentClassInfo;
    auto_ptr<TSubClasses> m_SubClasses;
    mutable auto_ptr<TContainedTypes> m_ContainedTypes;

    // class mapping
    typedef list<CClassInfoTmpl*> TClasses;
    typedef map<const type_info*, const CClassInfoTmpl*,
        CTypeInfoOrder> TClassesById;
    typedef map<string, const CClassInfoTmpl*> TClassesByName;

    static TClasses* sm_Classes;
    static TClassesById* sm_ClassesById;
    static TClassesByName* sm_ClassesByName;

    void Register(void);
    void Deregister(void) const;
    static TClasses& Classes(void);
    static TClassesById& ClassesById(void);
    static TClassesByName& ClassesByName(void);

};

class CStructInfoTmpl : public CClassInfoTmpl
{
    typedef CClassInfoTmpl CParent;
public:
    CStructInfoTmpl(const string& name, const type_info& id, size_t size)
        : CParent(name, id, size)
        {
        }

    virtual TObjectPtr Create(void) const;
};

template<class Class>
class CStructInfo : public CStructInfoTmpl
{
    typedef CStructInfoTmpl CParent;
public:
    typedef Class TObjectType;
    CStructInfo(const string& name)
        : CParent(name, typeid(Class), sizeof(Class))
        {
        }
    CStructInfo(const char* name)
        : CParent(name, typeid(Class), sizeof(Class))
        {
        }

    virtual const type_info* GetCPlusPlusTypeInfo(TConstObjectPtr object) const
        {
            return &typeid(*static_cast<const TObjectType*>(object));
        }
};

class CCObjectClassInfo : public CClassInfoTmpl
{
    typedef CClassInfoTmpl CParent;
public:
    CCObjectClassInfo(void)
        : CParent(typeid(CObject), sizeof(CObject))
        {
        }

    virtual const type_info* GetCPlusPlusTypeInfo(TConstObjectPtr object) const;
};

class CGeneratedClassInfo : public CClassInfoTmpl
{
    typedef CClassInfoTmpl CParent;
public:
    typedef TObjectPtr (*TCreateFunction)(void);
    typedef const type_info* (*TGetTypeIdFunction)(TConstObjectPtr object);
    typedef void (*TPostReadFunction)(TObjectPtr object);
    typedef void (*TPreWriteFunction)(TConstObjectPtr object);

    CGeneratedClassInfo(const char* name,
                        const type_info& typeId, size_t size,
                        TCreateFunction createFunction,
                        TGetTypeIdFunction getTypeIdFunction);

    void SetPostRead(TPostReadFunction func);
    void SetPreWrite(TPreWriteFunction func);

protected:
    virtual TObjectPtr Create(void) const;
    virtual void ReadData(CObjectIStream& in, TObjectPtr object) const;
    virtual void WriteData(CObjectOStream& out, TConstObjectPtr object) const;
    virtual const type_info* GetCPlusPlusTypeInfo(TConstObjectPtr object) const;

private:
    TCreateFunction m_CreateFunction;
    TGetTypeIdFunction m_GetTypeIdFunction;
    TPostReadFunction m_PostReadFunction;
    TPreWriteFunction m_PreWriteFunction;
};

template<class CLASS>
class CAbstractClassInfo : public CClassInfoTmpl
{
    typedef CClassInfoTmpl CParent;
public:
    typedef CLASS TObjectType;

    CAbstractClassInfo(void)
        : CParent(typeid(TObjectType), sizeof(TObjectType))
        {
            const TObjectType* object = 0;
            UpdateClassInfo(object);
        }
    CAbstractClassInfo(const string& name)
        : CParent(name, typeid(TObjectType), sizeof(TObjectType))
        {
            const TObjectType* object = 0;
            UpdateClassInfo(object);
        }
    CAbstractClassInfo(const char* name)
        : CParent(name, typeid(TObjectType), sizeof(TObjectType))
        {
            const TObjectType* object = 0;
            UpdateClassInfo(object);
        }
    CAbstractClassInfo(const char* name, const type_info& info)
        : CParent(name, info, sizeof(TObjectType))
        {
            const TObjectType* object = 0;
            UpdateClassInfo(object);
        }

    virtual const type_info* GetCPlusPlusTypeInfo(TConstObjectPtr object) const
        {
            return &typeid(*static_cast<const TObjectType*>(object));
        }
};

template<class CLASS>
class CClassInfo : public CAbstractClassInfo<CLASS>
{
    typedef CAbstractClassInfo<CLASS> CParent;
public:
    CClassInfo(void)
        {
        }
    CClassInfo(const string& name)
        : CParent(name)
        {
        }
    CClassInfo(const char* name)
        : CParent(name)
        {
        }
    CClassInfo(const char* name, const type_info& info)
        : CParent(name, info)
        {
        }

    virtual TObjectPtr Create(void) const
        {
            return new TObjectType();
        }
};

//#include <serial/classinfo.inl>

END_NCBI_SCOPE

#endif  /* CLASSINFO__HPP */
