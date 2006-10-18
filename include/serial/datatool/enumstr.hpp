#ifndef ENUMSTR_HPP
#define ENUMSTR_HPP

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
#include <serial/datatool/namespace.hpp>
#include <memory>
#include <list>

BEGIN_NCBI_SCOPE

class CEnumDataTypeValue;

class CEnumTypeStrings : public CTypeStrings
{
    typedef CTypeStrings CParent;
public:
    typedef list<CEnumDataTypeValue> TValues;
    CEnumTypeStrings(const string& externalName, const string& enumName,
                     const string& cType, bool isInteger,
                     const TValues& values, const string& valuesPrefix,
                     const CComments& comments);

    const string& GetExternalName(void) const
        {
            return m_ExternalName;
        }

    void SetEnumNamespace(const CNamespace& ns);

    EKind GetKind(void) const;
    const string& GetEnumName(void) const;

    string GetCType(const CNamespace& ns) const;
    string GetPrefixedCType(const CNamespace& ns,
                            const string& methodPrefix) const;
    string GetRef(const CNamespace& ns) const;
    string GetInitializer(void) const;

    void GenerateTypeCode(CClassContext& ctx) const;

private:
    string m_ExternalName;
    string m_EnumName;
    string m_CType;
    bool m_IsInteger;
    const TValues& m_Values;
    string m_ValuesPrefix;
};

class CEnumRefTypeStrings : public CTypeStrings
{
    typedef CTypeStrings CParent;
public:
    CEnumRefTypeStrings(const string& enumName,
                        const string& cName,
                        const CNamespace& ns,
                        const string& fileName,
                        const CComments& comments);

    EKind GetKind(void) const;
    const CNamespace& GetNamespace(void) const;
    const string& GetEnumName(void) const;

    string GetCType(const CNamespace& ns) const;
    string GetPrefixedCType(const CNamespace& ns,
                            const string& methodPrefix) const;
    string GetRef(const CNamespace& ns) const;
    string GetInitializer(void) const;

    void GenerateTypeCode(CClassContext& ctx) const;

private:
    string m_EnumName;
    string m_CType;
    CNamespace m_Namespace;
    string m_FileName;
};

END_NCBI_SCOPE

#endif
/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.11  2006/10/18 13:13:02  gouriano
* Added comments into typestrings and generated code
*
* Revision 1.10  2003/04/29 18:29:33  gouriano
* object data member initialization verification
*
* Revision 1.9  2000/11/15 20:34:41  vasilche
* Added user comments to ENUMERATED types.
* Added storing of user comments to ASN.1 module definition.
*
* Revision 1.8  2000/08/25 15:58:46  vasilche
* Renamed directory tool -> datatool.
*
* Revision 1.7  2000/07/11 20:36:01  vasilche
* Removed unnecessary generation of namespace references for enum members.
* Removed obsolete methods.
*
* Revision 1.6  2000/06/16 16:31:13  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.5  2000/05/24 20:08:31  vasilche
* Implemented DTD generation.
*
* Revision 1.4  2000/04/17 19:11:04  vasilche
* Fixed failed assertion.
* Removed redundant namespace specifications.
*
* Revision 1.3  2000/04/12 15:36:40  vasilche
* Added -on <namespace> argument to datatool.
* Removed unnecessary namespace specifications in generated files.
*
* Revision 1.2  2000/04/07 19:26:08  vasilche
* Added namespace support to datatool.
* By default with argument -oR datatool will generate objects in namespace
* NCBI_NS_NCBI::objects (aka ncbi::objects).
* Datatool's classes also moved to NCBI namespace.
*
* Revision 1.1  2000/02/01 21:46:17  vasilche
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
