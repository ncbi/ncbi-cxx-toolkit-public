#ifndef STLSTR_HPP
#define STLSTR_HPP

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
*/

#include <serial/datatool/typestr.hpp>
#include <corelib/ncbiutil.hpp>
#include <serial/datatool/classctx.hpp>

BEGIN_NCBI_SCOPE

class CNamespace;

class CTemplate1TypeStrings : public CTypeStrings
{
    typedef CTypeStrings CParent;
public:
    CTemplate1TypeStrings(const string& templateName,
                          CTypeStrings* type,
                          const string& namespaceName);
    CTemplate1TypeStrings(const string& templateName,
                          AutoPtr<CTypeStrings> type,
                          const string& namespaceName);
    ~CTemplate1TypeStrings(void);

    EKind GetKind(void) const;

    const string& GetTemplateName(void) const
        {
            return m_TemplateName;
        }
    const CTypeStrings* GetArg1Type(void) const
        {
            return m_Arg1Type.get();
        }

    string GetCType(const CNamespace& ns) const;
    string GetPrefixedCType(const CNamespace& ns,
                            const string& methodPrefix) const;
    string GetRef(const CNamespace& ns) const;

    string GetIsSetCode(const string& var) const;

    void GenerateTypeCode(CClassContext& ctx) const;

protected:
    void AddTemplateInclude(CClassContext::TIncludes& hpp) const;

    virtual string GetRefTemplate(void) const;
    virtual const CNamespace& GetTemplateNamespace(void) const;

private:
    string m_TemplateName;
    AutoPtr<CTypeStrings> m_Arg1Type;
};

class CSetTypeStrings : public CTemplate1TypeStrings
{
    typedef CTemplate1TypeStrings CParent;
public:
    CSetTypeStrings(const string& templateName,
                    AutoPtr<CTypeStrings> type,
                    const string& namespaceName);
    ~CSetTypeStrings(void);

    string GetDestructionCode(const string& expr) const;
    string GetResetCode(const string& var) const;
};

class CListTypeStrings : public CTemplate1TypeStrings
{
    typedef CTemplate1TypeStrings CParent;
public:
    CListTypeStrings(const string& templateName,
                     AutoPtr<CTypeStrings> type,
                     const string& namespaceName,
                     bool externalSet = false);
    ~CListTypeStrings(void);

    string GetDestructionCode(const string& expr) const;
    string GetResetCode(const string& var) const;

protected:
    string GetRefTemplate(void) const;

private:
    bool m_ExternalSet;
};

class CTemplate2TypeStrings : public CTemplate1TypeStrings
{
    typedef CTemplate1TypeStrings CParent;
public:
    CTemplate2TypeStrings(const string& templateName,
                          AutoPtr<CTypeStrings> type1,
                          AutoPtr<CTypeStrings> type2,
                          const string& namespaceName);
    ~CTemplate2TypeStrings(void);

    const CTypeStrings* GetArg2Type(void) const
        {
            return m_Arg2Type.get();
        }

    string GetCType(const CNamespace& ns) const;
    string GetPrefixedCType(const CNamespace& ns,
                            const string& methodPrefix) const;
    string GetRef(const CNamespace& ns) const;

    void GenerateTypeCode(CClassContext& ctx) const;

private:
    AutoPtr<CTypeStrings> m_Arg2Type;
};

class CMapTypeStrings : public CTemplate2TypeStrings
{
    typedef CTemplate2TypeStrings CParent;
public:
    CMapTypeStrings(const string& templateName,
                    AutoPtr<CTypeStrings> keyType,
                    AutoPtr<CTypeStrings> valueType,
                    const string& namespaceName);
    ~CMapTypeStrings(void);

    string GetDestructionCode(const string& expr) const;
    string GetResetCode(const string& var) const;
};

class CVectorTypeStrings : public CTypeStrings
{
    typedef CTypeStrings CParent;
public:
    CVectorTypeStrings(const string& charType,
                       const string& namespaceName,
                       const CComments& comments);
    ~CVectorTypeStrings(void);

    EKind GetKind(void) const;

    string GetCType(const CNamespace& ns) const;
    string GetPrefixedCType(const CNamespace& ns,
                            const string& methodPrefix) const;
    string GetRef(const CNamespace& ns) const;

    void GenerateTypeCode(CClassContext& ctx) const;

    string GetResetCode(const string& var) const;

private:
    string m_CharType;
};

END_NCBI_SCOPE

#endif
/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.12  2006/10/18 13:13:02  gouriano
* Added comments into typestrings and generated code
*
* Revision 1.11  2006/05/09 15:16:14  gouriano
* Added XML namespace definition possibility
*
* Revision 1.10  2003/04/29 18:29:34  gouriano
* object data member initialization verification
*
* Revision 1.9  2000/08/25 15:58:48  vasilche
* Renamed directory tool -> datatool.
*
* Revision 1.8  2000/07/11 20:36:02  vasilche
* Removed unnecessary generation of namespace references for enum members.
* Removed obsolete methods.
*
* Revision 1.7  2000/06/16 16:31:13  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.6  2000/04/17 19:11:05  vasilche
* Fixed failed assertion.
* Removed redundant namespace specifications.
*
* Revision 1.5  2000/04/07 19:26:13  vasilche
* Added namespace support to datatool.
* By default with argument -oR datatool will generate objects in namespace
* NCBI_NS_NCBI::objects (aka ncbi::objects).
* Datatool's classes also moved to NCBI namespace.
*
* Revision 1.4  2000/03/07 14:06:06  vasilche
* Added generation of reference counted objects.
*
* Revision 1.3  2000/02/03 20:16:20  vasilche
* Fixed bug in type info generation for templates.
*
* Revision 1.2  2000/02/02 16:23:41  vasilche
* Added missing namespace macros to generated files.
*
* Revision 1.1  2000/02/01 21:46:23  vasilche
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
