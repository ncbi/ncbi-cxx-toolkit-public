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
*   Type description for enumerated types
*/

#include <ncbi_pch.hpp>
#include <serial/datatool/enumtype.hpp>
#include <serial/datatool/blocktype.hpp>
#include <serial/datatool/value.hpp>
#include <serial/datatool/enumstr.hpp>
#include <serial/datatool/module.hpp>
#include <serial/datatool/srcutil.hpp>
#include <serial/enumerated.hpp>

BEGIN_NCBI_SCOPE

CEnumDataType::CEnumDataType(void)
{
    ForbidVar("_type", "string");
}

const char* CEnumDataType::GetASNKeyword(void) const
{
    return "ENUMERATED";
}

const char* CEnumDataType::GetDEFKeyword(void) const
{
    return "_ENUMERATED_";
}

string CEnumDataType::GetXMLContents(void) const
{
    string content("\n");
    ITERATE ( TValues, i, m_Values ) {
        if (i != m_Values.begin()) {
            content += " |\n";
        }
        content += "        " + i->GetName();
    }
    return content;
}

bool CEnumDataType::IsInteger(void) const
{
    return false;
}

CEnumDataType::TValue& CEnumDataType::AddValue(const string& valueName,
                                               TEnumValueType value)
{
    m_Values.push_back(TValue(valueName, value));
    return m_Values.back();
}

void CEnumDataType::PrintASN(CNcbiOstream& out, int indent) const
{
    out << GetASNKeyword() << " {";
    ++indent;
    ITERATE ( TValues, i, m_Values ) {
        PrintASNNewLine(out, indent);
        TValues::const_iterator next = i;
        bool last = ++next == m_Values.end();

        bool oneLineComment = i->GetComments().OneLine();
        if ( !oneLineComment )
            i->GetComments().PrintASN(out, indent);
        out << CDataTypeModule::ToAsnId(i->GetName()) << " (" << i->GetValue() << ")";
        if ( !last )
            out << ',';
        if ( oneLineComment )
            i->GetComments().PrintASN(out, indent, CComments::eOneLine);
    }
    --indent;
    PrintASNNewLine(out, indent);
    m_LastComments.PrintASN(out, indent, CComments::eMultiline);
    out << "}";
}

void CEnumDataType::PrintSpecDumpExtra(CNcbiOstream& out, int indent) const
{
    ++indent;
    ITERATE ( TValues, i, m_Values ) {
        PrintASNNewLine(out, indent);
        out << "V,,";
        out << GetFullName() << ':' << i->GetName() << ',' << i->GetValue();
        i->GetComments().PrintASN(out, indent, CComments::eNoEOL);
    }
}

// XML schema generator submitted by
// Marc Dumontier, Blueprint initiative, dumontier@mshri.on.ca
// modified by Andrei Gourianov, gouriano@ncbi
void CEnumDataType::PrintXMLSchema(CNcbiOstream& out,
    int indent, bool contents_only) const
{
    string tag(XmlTagName());
    string use("required");
    string value("value");
    bool inAttlist= false;
    list<string> opentag, closetag;

    if (GetEnforcedStdXml() &&
        GetParentType() && 
        GetParentType()->GetDataMember() &&
        GetParentType()->GetDataMember()->Attlist()) {
        const CDataMember* mem = GetDataMember();
        inAttlist = true;
        value = tag;
        if (mem->Optional()) {
            use = "optional";
            if (mem->GetDefault()) {
                use += "\" default=\"" + mem->GetDefault()->GetXmlString();
            }
        } else {
            use = "required";
        }
    }
    if (!inAttlist) {
        const CDataMember* mem = GetDataMember();
        string tmp = "<xs:element name=\"" + tag + "\"";
        if (mem && mem->Optional()) {
            tmp += " minOccurs=\"0\"";
            if (mem->GetDefault()) {
                use = "optional";
            }
        }
        opentag.push_back(tmp + ">");
        closetag.push_front("</xs:element>");
        opentag.push_back("<xs:complexType>");
        closetag.push_front("</xs:complexType>");
        if(IsInteger()) {
            opentag.push_back("<xs:simpleContent>");
            closetag.push_front("</xs:simpleContent>");
            opentag.push_back("<xs:extension base=\"xs:integer\">");
            closetag.push_front("</xs:extension>");
            use = "optional";
        }
    }
    string tmp = "<xs:attribute name=\"" + value + "\" use=\"" + use + "\"";
    if (!inAttlist) {
        const CDataMember* mem = GetDataMember();
        if (mem && mem->Optional() && mem->GetDefault()) {
            tmp += " default=\"" + mem->GetDefault()->GetXmlString() + "\"";
        }
    }
    opentag.push_back(tmp + ">");
    closetag.push_front("</xs:attribute>");
    opentag.push_back("<xs:simpleType>");
    closetag.push_front("</xs:simpleType>");
    opentag.push_back("<xs:restriction base=\"xs:string\">");
    closetag.push_front("</xs:restriction>");

    ITERATE ( list<string>, s, opentag ) {
        PrintASNNewLine(out, indent++) << *s;
    }
    bool haveComments = false;
    ITERATE ( TValues, i, m_Values ) {
        if ( !i->GetComments().Empty() ) {
            haveComments = true;
            break;
        }
    }
    if ( haveComments ) {
        out << "\n<!--\n";
        ITERATE ( TValues, i, m_Values ) {
            if ( !i->GetComments().Empty() ) {
                i->GetComments().Print(out, "    "+i->GetName()+"\t- ",
                                       "\n        ", "\n");
            }
        }
        out << "-->";
    }
    ITERATE ( TValues, i, m_Values ) {
        PrintASNNewLine(out, indent) <<
            "<xs:enumeration value=\"" << i->GetName() << "\"/>";
    }
    ITERATE ( list<string>, s, closetag ) {
        PrintASNNewLine(out, --indent) << *s;
    }
    m_LastComments.PrintDTD(out, CComments::eMultiline);
}

void CEnumDataType::PrintDTDElement(CNcbiOstream& out, bool contents_only) const
{
    string tag(XmlTagName());
    string content(GetXMLContents());
    if (GetParentType() && 
        GetParentType()->GetDataMember() &&
        GetParentType()->GetDataMember()->Attlist()) {
        const CDataMember* mem = GetDataMember();
        out << "\n    " << tag << " (" << content << ") ";
        if (mem->GetDefault()) {
            out << "\"" << mem->GetDefault()->GetXmlString() << "\"";
        } else {
            if (mem->Optional()) {
                out << "#IMPLIED";
            } else {
                out << "#REQUIRED";
            }
        }
    } else {
        out <<
            "\n<!ELEMENT " << tag << " ";
        if ( IsInteger() )
            out << "(%INTEGER;)>";
        else
            out << "%ENUM;>";
    }
}

void CEnumDataType::PrintDTDExtra(CNcbiOstream& out) const
{
    bool haveComments = false;
    ITERATE ( TValues, i, m_Values ) {
        if ( !i->GetComments().Empty() ) {
            haveComments = true;
            break;
        }
    }
    if ( haveComments ) {
        out << "\n\n<!--\n";
        ITERATE ( TValues, i, m_Values ) {
            if ( !i->GetComments().Empty() ) {
                i->GetComments().Print(out, "    "+i->GetName()+"\t- ",
                                       "\n        ", "\n");
            }
        }
        out << "-->";
    }
    out <<
        "\n<!ATTLIST "<<XmlTagName()<<" value (\n";
    ITERATE ( TValues, i, m_Values ) {
        if ( i != m_Values.begin() )
            out << " |\n";
        out << "        " << i->GetName();
    }
    out << "\n        ) ";
    if ( IsInteger() )
        out << "#IMPLIED";
    else
        out << "#REQUIRED";
    out << " >\n";
    m_LastComments.PrintDTD(out, CComments::eMultiline);
}

bool CEnumDataType::CheckValue(const CDataValue& value) const
{
    const CIdDataValue* id = dynamic_cast<const CIdDataValue*>(&value);
    if ( id ) {
        ITERATE ( TValues, i, m_Values ) {
            if ( i->GetName() == id->GetValue() )
                return true;
        }
        value.Warning("illegal ENUMERATED value: " + id->GetValue());
        return false;
    }

    const CIntDataValue* intValue =
        dynamic_cast<const CIntDataValue*>(&value);
    if ( !intValue ) {
        value.Warning("ENUMERATED or INTEGER value expected");
        return false;
    }

    if ( !IsInteger() ) {
        ITERATE ( TValues, i, m_Values ) {
            if ( i->GetValue() == intValue->GetValue() )
                return true;
        }
        value.Warning("illegal INTEGER value: " + intValue->GetValue());
        return false;
    }

    return true;
}

TObjectPtr CEnumDataType::CreateDefault(const CDataValue& value) const
{
    const CIdDataValue* id = dynamic_cast<const CIdDataValue*>(&value);
    if ( id == 0 ) {
        return new TEnumValueType(dynamic_cast<const CIntDataValue&>(value).GetValue());
    }
    ITERATE ( TValues, i, m_Values ) {
        if ( i->GetName() == id->GetValue() )
            return new TEnumValueType(i->GetValue());
    }
    value.Warning("illegal ENUMERATED value: " + id->GetValue());
    return 0;
}

string CEnumDataType::GetDefaultString(const CDataValue& value) const
{
    const CIdDataValue* id = dynamic_cast<const CIdDataValue*>(&value);
    if ( id ) {
        return GetEnumCInfo().valuePrefix + Identifier(id->GetValue(), false);
    }
    else {
        const CIntDataValue* intValue =
            dynamic_cast<const CIntDataValue*>(&value);
        return NStr::IntToString(intValue->GetValue());
    }
}

CTypeInfo* CEnumDataType::CreateTypeInfo(void)
{
    AutoPtr<CEnumeratedTypeValues>
        info(new CEnumeratedTypeValues(GlobalName(), IsInteger()));
    ITERATE ( TValues, i, m_Values ) {
        info->AddValue(i->GetName(), i->GetValue());
    }
    if ( HaveModuleName() )
        info->SetModuleName(GetModule()->GetName());
    return new CEnumeratedTypeInfo(sizeof(TEnumValueType), info.release());
}

string CEnumDataType::DefaultEnumName(void) const
{
    // generate enum name from ASN type or field name
    if ( !GetParentType() ) {
        // root enum
        return 'E' + Identifier(IdName());
    }
    else {
        // internal enum
        return 'E' + Identifier(GetKeyPrefix());
    }
}

CEnumDataType::SEnumCInfo CEnumDataType::GetEnumCInfo(void) const
{
    string typeName = GetAndVerifyVar("_type");
    string enumName;
    if ( !typeName.empty() && typeName[0] == 'E' ) {
        enumName = typeName;
    }
    else {
        // make C++ type name
        enumName = DefaultEnumName();
        if ( typeName.empty() ) {
            if ( IsInteger() )
                typeName = "int";
            else
                typeName = enumName;
        }
    }
    string prefix = GetVar("_prefix");
    if ( prefix.empty() ) {
        prefix = char(tolower((unsigned char) enumName[0])) + enumName.substr(1) + '_';
    }
    return SEnumCInfo(enumName, typeName, prefix);
}

AutoPtr<CTypeStrings> CEnumDataType::GetRefCType(void) const
{
    SEnumCInfo enumInfo = GetEnumCInfo();
    return AutoPtr<CTypeStrings>(new CEnumRefTypeStrings(enumInfo.enumName,
                                                         enumInfo.cType,
                                                         Namespace(),
                                                         FileName(),
                                                         Comments()));
}

AutoPtr<CTypeStrings> CEnumDataType::GetFullCType(void) const
{
    SEnumCInfo enumInfo = GetEnumCInfo();
    AutoPtr<CEnumTypeStrings> 
        e(new CEnumTypeStrings(GlobalName(), enumInfo.enumName,
                               enumInfo.cType, IsInteger(),
                               m_Values, enumInfo.valuePrefix, Comments()));
    return AutoPtr<CTypeStrings>(e.release());
}

AutoPtr<CTypeStrings> CEnumDataType::GenerateCode(void) const
{
    return GetFullCType();
}

const char* CIntEnumDataType::GetASNKeyword(void) const
{
    return "INTEGER";
}

const char* CIntEnumDataType::GetDEFKeyword(void) const
{
    return "_INTEGER_ENUM_";
}

bool CIntEnumDataType::IsInteger(void) const
{
    return true;
}

const char* CBigIntEnumDataType::GetASNKeyword(void) const
{
    return "BigInt";
}

const char* CBigIntEnumDataType::GetDEFKeyword(void) const
{
    return "_BigInt_ENUM_";
}

END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.40  2006/10/30 21:03:33  gouriano
* Corrected data spec dump formatting
*
* Revision 1.39  2006/10/30 18:15:40  gouriano
* Added writing data specification in internal format
*
* Revision 1.38  2006/10/18 13:12:36  gouriano
* Added comments into typestrings and generated code
*
* Revision 1.37  2006/08/03 17:21:10  gouriano
* Preserve comments when parsing schema
*
* Revision 1.36  2006/07/24 18:57:39  gouriano
* Preserve comments when parsing DTD
*
* Revision 1.35  2006/06/19 17:34:06  gouriano
* Redesigned generation of XML schema
*
* Revision 1.34  2006/05/16 14:30:13  gouriano
* Corrected generation of ASN spec - to make sure it is valid
*
* Revision 1.33  2005/08/05 15:11:40  gouriano
* Allow DEF file tuneups by data type, not only by name
*
* Revision 1.32  2005/06/20 18:46:56  gouriano
* Corrected generation of schema for enum
*
* Revision 1.31  2005/06/06 15:31:15  lavr
* Explicit (unsigned char) casts in ctype routines
*
* Revision 1.30  2005/02/22 15:07:34  gouriano
* Corrected writing element's default value when generating XML schema
*
* Revision 1.29  2005/02/09 14:33:57  gouriano
* Corrected formatting when writing DTD
*
* Revision 1.28  2005/02/02 19:08:36  gouriano
* Corrected DTD generation
*
* Revision 1.27  2004/07/22 17:49:23  gouriano
* Corrected XML schema generation for named integers
*
* Revision 1.26  2004/05/17 21:03:14  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 1.25  2004/05/12 18:33:01  gouriano
* Added type conversion check (when using _type DEF file directive)
*
* Revision 1.24  2003/06/16 14:41:05  gouriano
* added possibility to convert DTD to XML schema
*
* Revision 1.23  2003/05/14 14:42:22  gouriano
* added generation of XML schema
*
* Revision 1.22  2003/03/11 20:06:47  kuznets
* iterate -> ITERATE
*
* Revision 1.21  2001/05/17 15:07:12  lavr
* Typos corrected
*
* Revision 1.20  2000/12/15 15:38:51  vasilche
* Added support of Int8 and long double.
* Added support of BigInt ASN.1 extension - mapped to Int8.
* Enum values now have type Int4 instead of long.
*
* Revision 1.19  2000/11/29 17:42:44  vasilche
* Added CComment class for storing/printing ASN.1/XML module comments.
* Added srcutil.hpp file to reduce file dependency.
*
* Revision 1.18  2000/11/20 17:26:32  vasilche
* Fixed warnings on 64 bit platforms.
* Updated names of config variables.
*
* Revision 1.17  2000/11/15 20:34:54  vasilche
* Added user comments to ENUMERATED types.
* Added storing of user comments to ASN.1 module definition.
*
* Revision 1.16  2000/11/14 21:41:24  vasilche
* Added preserving of ASN.1 definition comments.
*
* Revision 1.15  2000/11/08 17:02:51  vasilche
* Added generation of modular DTD files.
*
* Revision 1.14  2000/11/07 17:26:25  vasilche
* Added module names to CTypeInfo and CEnumeratedTypeValues
* Added possibility to set include directory for whole module
*
* Revision 1.13  2000/09/18 20:00:29  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* Revision 1.12  2000/08/25 15:59:21  vasilche
* Renamed directory tool -> datatool.
*
* Revision 1.11  2000/05/24 20:09:28  vasilche
* Implemented DTD generation.
*
* Revision 1.10  2000/04/17 19:11:08  vasilche
* Fixed failed assertion.
* Removed redundant namespace specifications.
*
* Revision 1.9  2000/04/12 15:36:51  vasilche
* Added -on <namespace> argument to datatool.
* Removed unnecessary namespace specifications in generated files.
*
* Revision 1.8  2000/04/07 19:26:25  vasilche
* Added namespace support to datatool.
* By default with argument -oR datatool will generate objects in namespace
* NCBI_NS_NCBI::objects (aka ncbi::objects).
* Datatool's classes also moved to NCBI namespace.
*
* Revision 1.7  2000/02/17 20:05:07  vasilche
* Inline methods now will be generated in *_Base.inl files.
* Fixed processing of StringStore.
* Renamed in choices: Selected() -> Which(), E_choice -> E_Choice.
* Enumerated values now will preserve case as in ASN.1 definition.
*
* Revision 1.6  2000/02/01 21:47:57  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
* Changed class generation.
* Moved datatool headers to include/internal/serial/tool.
*
* Revision 1.5  1999/12/21 17:18:33  vasilche
* Added CDelayedFostream class which rewrites file only if contents is changed.
*
* Revision 1.4  1999/12/03 21:42:11  vasilche
* Fixed conflict of enums in choices.
*
* Revision 1.3  1999/12/01 17:36:25  vasilche
* Fixed CHOICE processing.
*
* Revision 1.2  1999/11/15 19:36:14  vasilche
* Fixed warnings on GCC
*
* ===========================================================================
*/
