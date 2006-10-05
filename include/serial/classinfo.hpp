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
#include <serial/classinfob.hpp>
#include <serial/member.hpp>
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
    const CMemberInfo* GetMemberInfo(const string& name) const;

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



/* ---------------------------------------------------------------------------
* $Log$
* Revision 1.47  2006/10/05 19:23:03  gouriano
* Some headers moved into impl
*
* Revision 1.46  2005/10/19 13:49:37  vasilche
* Fixed MayContainType() for type graph with cycles.
*
* Revision 1.45  2004/03/25 15:56:27  gouriano
* Added possibility to copy and compare serial object non-recursively
*
* Revision 1.44  2003/06/24 20:54:13  gouriano
* corrected code generation and serialization of non-empty unnamed containers (XML)
*
* Revision 1.43  2003/04/29 18:29:06  gouriano
* object data member initialization verification
*
* Revision 1.42  2003/04/15 14:14:57  siyan
* Added doxygen support
*
* Revision 1.41  2003/04/10 20:13:37  vakatov
* Rollback the "uninitialized member" verification -- it still needs to
* be worked upon...
*
* Revision 1.39  2002/12/26 19:33:06  gouriano
* changed XML I/O streams to properly handle object copying
*
* Revision 1.38  2002/12/23 18:38:50  dicuccio
* Added WIn32 export specifier: NCBI_XSERIAL_EXPORT.
* Moved all CVS logs to the end.
*
* Revision 1.37  2002/11/14 20:44:09  gouriano
* moved AddMember method into the base class
*
* Revision 1.36  2000/10/13 16:28:29  vasilche
* Reduced header dependency.
* Avoid use of templates with virtual methods.
* Reduced amount of different maps used.
* All this lead to smaller compiled code size (libraries and programs).
*
* Revision 1.35  2000/10/03 17:22:30  vasilche
* Reduced header dependency.
* Reduced size of debug libraries on WorkShop by 3 times.
* Fixed tag allocation for parent classes.
* Fixed CObject allocation/deallocation in streams.
* Moved instantiation of several templates in separate source file.
*
* Revision 1.34  2000/09/18 19:59:59  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* Revision 1.33  2000/09/01 13:15:58  vasilche
* Implemented class/container/choice iterators.
* Implemented CObjectStreamCopier for copying data without loading into memory.
*
* Revision 1.32  2000/07/03 18:42:32  vasilche
* Added interface to typeinfo via CObjectInfo and CConstObjectInfo.
* Reduced header dependency.
*
* Revision 1.31  2000/06/16 16:31:04  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
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
