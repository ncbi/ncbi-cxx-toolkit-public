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
*
* ---------------------------------------------------------------------------
* $Log$
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

#include <serial/tool/typestr.hpp>
#include <corelib/ncbiutil.hpp>
#include <list>

class CClassCode;

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
        bool optional; // have OPTIONAL or DEFAULT attribute
        string defaultValue; // DEFAULT value code
        SMemberInfo(const string& name, AutoPtr<CTypeStrings> type,
                    const string& pointerType,
                    bool optional, const string& defaultValue);
    };
    typedef list<SMemberInfo> TMembers;

    CClassTypeStrings(const string& externalName, const string& className);
    ~CClassTypeStrings(void);

    const string& GetExternalName(void) const
        {
            return m_ExternalName;
        }
    const string& GetClassName(void) const
        {
            return m_ClassName;
        }

    void SetParentClass(const string& className,
                        const string& namespaceName,
                        const string& fileName);

    void AddMember(const string& name, AutoPtr<CTypeStrings> type,
                   const string& pointerType,
                   bool optional, const string& defaultValue);

    string GetCType(void) const;
    string GetRef(void) const;

    bool CanBeKey(void) const;
    bool CanBeInSTL(void) const;
    bool IsObject(void) const;

    string NewInstance(const string& init) const;

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

    void GenerateTypeCode(CClassContext& ctx) const;
    string GetResetCode(const string& var) const;

    void GenerateUserHPPCode(CNcbiOstream& out) const;
    void GenerateUserCPPCode(CNcbiOstream& out) const;

protected:
    virtual void GenerateClassCode(CClassCode& code,
                                   CNcbiOstream& getters,
                                   const string& methodPrefix,
                                   const string& codeClassName) const;

private:
    bool m_IsObject;
    bool m_HaveUserClass;
    string m_ExternalName;
    string m_ClassName;
    string m_ParentClassName;
    string m_ParentClassNamespaceName;
    string m_ParentClassFileName;
    TMembers m_Members;
};

class CClassRefTypeStrings : public CTypeStrings
{
public:
    CClassRefTypeStrings(const string& className,
                         const string& namespaceName,
                         const string& fileName);

    string GetCType(void) const;
    string GetRef(void) const;

    bool CanBeKey(void) const;
    bool CanBeInSTL(void) const;
    bool IsObject(void) const;

    string NewInstance(const string& init) const;

    string GetResetCode(const string& var) const;

    void GenerateTypeCode(CClassContext& ctx) const;
    void GeneratePointerTypeCode(CClassContext& ctx) const;

private:
    string m_ClassName;
    string m_NamespaceName;
    string m_FileName;
};

#endif
