#ifndef CLASSSTR_HPP
#define CLASSSTR_HPP

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
*   C++ class info: includes, used classes, C++ code etc.
*/

#include <serial/datatool/typestr.hpp>
#include <corelib/ncbiutil.hpp>
#include <serial/datatool/namespace.hpp>
#include <list>

BEGIN_NCBI_SCOPE

class CClassCode;
class CDataType;

class CClassTypeStrings : public CTypeStrings
{
    typedef CTypeStrings CParent;
public:
    struct SMemberInfo {
        string externalName; // logical name
        string cName; // C++ type code
        string mName; // member name
        string tName; // typedef name
        AutoPtr<CTypeStrings> type; // value type
        string ptrType; // "*" or "NCBI_NS_NCBI::CRef"
        string valueName; // value name (mName or '*'+mName)
        bool optional;  // have OPTIONAL or DEFAULT attribute
        bool ref;       // implement member via CRef
        bool haveFlag;  // need additional boolean flag 'isSet'
        bool canBeNull; // pointer type can be NULL pointer
        bool delayed;
        int memberTag;
        string defaultValue; // DEFAULT value code
        bool noPrefix;
        bool attlist;
        bool noTag;
        bool simple;
        const CDataType* dataType;
        bool nonEmpty;
        SMemberInfo(const string& name, const AutoPtr<CTypeStrings>& type,
                    const string& pointerType,
                    bool optional, const string& defaultValue,
                    bool delayed, int tag,
                    bool noPrefx, bool attlst, bool noTg, bool simpl,
                    const CDataType* dataTp, bool nEmpty);
    };
    typedef list<SMemberInfo> TMembers;

    CClassTypeStrings(const string& externalName, const string& className);
    ~CClassTypeStrings(void);

    void SetClassNamespace(const CNamespace& ns);

    const string& GetExternalName(void) const
        {
            return m_ExternalName;
        }
    // DT added to avoid conflict with the standard Windows interfaces
    const string& GetClassNameDT(void) const
        {
            return m_ClassName;
        }

    void SetParentClass(const string& className, const CNamespace& ns,
                        const string& fileName);

    void AddMember(const string& name, const AutoPtr<CTypeStrings>& type,
                   const string& pointerType,
                   bool optional, const string& defaultValue,
                   bool delayed, int tag,
                   bool noPrefix, bool attlist, bool noTag, bool simple,
                   const CDataType* dataType, bool nonEmpty);
    void AddMember(const AutoPtr<CTypeStrings>& type, int tag, bool nonEmpty, bool noPrefix)
        {
            AddMember(NcbiEmptyString, type, NcbiEmptyString,
                      false, NcbiEmptyString, false, tag,
                      noPrefix,false,false,false,0,nonEmpty);
        }

    string GetCType(const CNamespace& ns) const;
    string GetPrefixedCType(const CNamespace& ns,
                            const string& methodPrefix) const;
    string GetRef(const CNamespace& ns) const;

    EKind GetKind(void) const;

    void SetObject(bool isObject)
        {
            m_IsObject = isObject;
        }

    bool HaveUserClass(void) const
        {
            return m_HaveUserClass;
        }
    void SetHaveUserClass(bool haveUserClass)
        {
            m_HaveUserClass = haveUserClass;
        }

    bool HaveTypeInfo(void) const
        {
            return m_HaveTypeInfo;
        }
    void SetHaveTypeInfo(bool haveTypeInfo)
        {
            m_HaveTypeInfo = haveTypeInfo;
        }

    void GenerateTypeCode(CClassContext& ctx) const;
    string GetResetCode(const string& var) const;

    void GenerateUserHPPCode(CNcbiOstream& out) const;
    void GenerateUserCPPCode(CNcbiOstream& out) const;

protected:
    virtual void GenerateClassCode(CClassCode& code,
                                   CNcbiOstream& getters,
                                   const string& methodPrefix,
                                   bool haveUserClass,
                                   const string& classPrefix) const;
    bool x_IsNullType(TMembers::const_iterator i) const;
    bool x_IsNullWithAttlist(TMembers::const_iterator i) const;

private:
    bool m_IsObject;
    bool m_HaveUserClass;
    bool m_HaveTypeInfo;
    string m_ExternalName;
    string m_ClassName;
    string m_ParentClassName;
    CNamespace m_ParentClassNamespace;
    string m_ParentClassFileName;
public:
    TMembers m_Members;
};

class CClassRefTypeStrings : public CTypeStrings
{
public:
    CClassRefTypeStrings(const string& className, const CNamespace& ns,
                         const string& fileName);

    string GetClassName(void) const;
    string GetCType(const CNamespace& ns) const;
    string GetPrefixedCType(const CNamespace& ns,
                            const string& methodPrefix) const;
    string GetRef(const CNamespace& ns) const;

    EKind GetKind(void) const;
    const CNamespace& GetNamespace(void) const;

    string GetResetCode(const string& var) const;

    void GenerateTypeCode(CClassContext& ctx) const;
    void GeneratePointerTypeCode(CClassContext& ctx) const;

private:
    string m_ClassName;
    CNamespace m_Namespace;
    string m_FileName;
};

END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.24  2005/04/26 14:18:50  vasilche
* Allow allocation of objects in CObjectMemoryPool.
*
* Revision 1.23  2004/05/19 17:23:25  gouriano
* Corrected generation of C++ code by DTD for containers
*
* Revision 1.22  2004/03/08 20:08:02  gouriano
* Correct namespaces of generated classes
*
* Revision 1.21  2003/11/20 14:32:12  gouriano
* changed generated C++ code so NULL data types have no value
*
* Revision 1.20  2003/06/24 20:53:39  gouriano
* corrected code generation and serialization of non-empty unnamed containers (XML)
*
* Revision 1.19  2003/04/29 18:29:33  gouriano
* object data member initialization verification
*
* Revision 1.18  2002/11/19 19:47:49  gouriano
* added support of XML attributes of choice variants
*
* Revision 1.17  2002/11/14 21:07:11  gouriano
* added support of XML attribute lists
*
* Revision 1.16  2002/11/13 00:44:00  ucko
* Made type info declaration optional (but on by default); CVS logs to end
*
* Revision 1.15  2002/10/15 13:53:08  gouriano
* use "noprefix" flag
*
* Revision 1.14  2002/08/14 17:14:22  grichenk
* Fixed function name conflict on Win32: renamed
* GetClassName() -> GetClassNameDT()
*
* Revision 1.13  2001/06/11 14:34:58  grichenk
* Added support for numeric tags in ASN.1 specifications and data streams.
*
* Revision 1.12  2000/08/25 15:58:46  vasilche
* Renamed directory tool -> datatool.
*
* Revision 1.11  2000/07/11 20:36:01  vasilche
* Removed unnecessary generation of namespace references for enum members.
* Removed obsolete methods.
*
* Revision 1.10  2000/06/16 16:31:13  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.9  2000/05/03 14:38:10  vasilche
* SERIAL: added support for delayed reading to generated classes.
* DATATOOL: added code generation for delayed reading.
*
* Revision 1.8  2000/04/17 19:11:04  vasilche
* Fixed failed assertion.
* Removed redundant namespace specifications.
*
* Revision 1.7  2000/04/12 15:36:40  vasilche
* Added -on <namespace> argument to datatool.
* Removed unnecessary namespace specifications in generated files.
*
* Revision 1.6  2000/04/07 19:26:07  vasilche
* Added namespace support to datatool.
* By default with argument -oR datatool will generate objects in namespace
* NCBI_NS_NCBI::objects (aka ncbi::objects).
* Datatool's classes also moved to NCBI namespace.
*
* Revision 1.5  2000/03/14 18:32:54  vasilche
* Fixed class includes generation.
*
* Revision 1.4  2000/03/07 20:04:56  vasilche
* Added NewInstance method to generated classes.
*
* Revision 1.3  2000/03/07 14:06:05  vasilche
* Added generation of reference counted objects.
*
* Revision 1.2  2000/02/17 20:05:02  vasilche
* Inline methods now will be generated in *_Base.inl files.
* Fixed processing of StringStore.
* Renamed in choices: Selected() -> Which(), E_choice -> E_Choice.
* Enumerated values now will preserve case as in ASN.1 definition.
*
* Revision 1.1  2000/02/01 21:46:16  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
* Changed class generation.
* Moved datatool headers to include/internal/serial/tool.
*
* Revision 1.5  2000/01/10 19:46:47  vasilche
* Fixed encoding/decoding of REAL type.
* Fixed encoding/decoding of StringStore.
* Fixed encoding/decoding of NULL type.
* Fixed error reporting.
* Reduced object map (only classes).
*
* Revision 1.4  1999/12/01 17:36:28  vasilche
* Fixed CHOICE processing.
*
* Revision 1.3  1999/11/16 15:41:17  vasilche
* Added plain pointer choice.
* By default we use C pointer instead of auto_ptr.
* Start adding initializers.
*
* Revision 1.2  1999/11/15 19:36:21  vasilche
* Fixed warnings on GCC
*
* ===========================================================================
*/

#endif
