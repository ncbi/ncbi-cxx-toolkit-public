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
*   Type description of 'SET OF' and 'SEQUENCE OF'
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.35  2005/02/09 14:35:50  gouriano
* Corrected formatting when writing DTD
*
* Revision 1.34  2005/02/02 19:08:36  gouriano
* Corrected DTD generation
*
* Revision 1.33  2005/01/26 18:53:36  gouriano
* Undo the previous change - it introduced other problems
*
* Revision 1.32  2004/12/10 15:13:39  gouriano
* Create unnamed TypeInfo in CUniSequenceDataType::CreateTypeInfo
*
* Revision 1.31  2004/05/19 17:24:18  gouriano
* Corrected generation of C++ code by DTD for containers
*
* Revision 1.30  2004/05/17 21:03:14  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 1.29  2004/05/12 18:33:01  gouriano
* Added type conversion check (when using _type DEF file directive)
*
* Revision 1.28  2004/04/02 16:56:33  gouriano
* made it possible to create named CTypeInfo for containers
*
* Revision 1.27  2003/06/24 20:55:42  gouriano
* corrected code generation and serialization of non-empty unnamed containers (XML)
*
* Revision 1.26  2003/06/16 14:41:05  gouriano
* added possibility to convert DTD to XML schema
*
* Revision 1.25  2003/05/14 14:42:22  gouriano
* added generation of XML schema
*
* Revision 1.24  2003/03/11 20:06:47  kuznets
* iterate -> ITERATE
*
* Revision 1.23  2003/03/10 18:55:19  gouriano
* use new structured exceptions (based on CException)
*
* Revision 1.22  2003/02/10 17:56:14  gouriano
* make it possible to disable scope prefixes when reading and writing objects generated from ASN specification in XML format, or when converting an ASN spec into DTD.
*
* Revision 1.21  2002/01/22 22:01:15  grichenk
* Fixed generation of list<> from "SET OF"
*
* Revision 1.20  2002/01/16 21:30:18  grichenk
* ANS.1 SET OF is implemented as list<> by default
*
* Revision 1.19  2000/11/15 20:34:56  vasilche
* Added user comments to ENUMERATED types.
* Added storing of user comments to ASN.1 module definition.
*
* Revision 1.18  2000/11/14 21:41:27  vasilche
* Added preserving of ASN.1 definition comments.
*
* Revision 1.17  2000/11/08 17:02:53  vasilche
* Added generation of modular DTD files.
*
* Revision 1.16  2000/11/07 17:26:26  vasilche
* Added module names to CTypeInfo and CEnumeratedTypeValues
* Added possibility to set include directory for whole module
*
* Revision 1.15  2000/11/01 20:38:59  vasilche
* OPTIONAL and DEFAULT are not permitted in CHOICE.
* Fixed code generation for DEFAULT.
*
* Revision 1.14  2000/10/13 16:28:45  vasilche
* Reduced header dependency.
* Avoid use of templates with virtual methods.
* Reduced amount of different maps used.
* All this lead to smaller compiled code size (libraries and programs).
*
* Revision 1.13  2000/08/25 15:59:25  vasilche
* Renamed directory tool -> datatool.
*
* Revision 1.12  2000/06/16 16:31:41  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.11  2000/05/24 20:09:30  vasilche
* Implemented DTD generation.
*
* Revision 1.10  2000/04/10 18:39:00  vasilche
* Fixed generation of map<> from SEQUENCE/SET OF SEQUENCE.
*
* Revision 1.9  2000/04/07 19:26:37  vasilche
* Added namespace support to datatool.
* By default with argument -oR datatool will generate objects in namespace
* NCBI_NS_NCBI::objects (aka ncbi::objects).
* Datatool's classes also moved to NCBI namespace.
*
* Revision 1.8  2000/02/01 21:48:09  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
* Changed class generation.
* Moved datatool headers to include/internal/serial/tool.
*
* Revision 1.7  2000/01/10 19:46:47  vasilche
* Fixed encoding/decoding of REAL type.
* Fixed encoding/decoding of StringStore.
* Fixed encoding/decoding of NULL type.
* Fixed error reporting.
* Reduced object map (only classes).
*
* Revision 1.6  1999/12/17 19:05:19  vasilche
* Simplified generation of GetTypeInfo methods.
*
* Revision 1.5  1999/12/03 21:42:14  vasilche
* Fixed conflict of enums in choices.
*
* Revision 1.4  1999/12/01 17:36:29  vasilche
* Fixed CHOICE processing.
*
* Revision 1.3  1999/11/16 15:41:18  vasilche
* Added plain pointer choice.
* By default we use C pointer instead of auto_ptr.
* Start adding initializers.
*
* Revision 1.2  1999/11/15 19:36:21  vasilche
* Fixed warnings on GCC
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <serial/stltypes.hpp>
#include <serial/autoptrinfo.hpp>
#include <serial/datatool/exceptions.hpp>
#include <serial/datatool/unitype.hpp>
#include <serial/datatool/blocktype.hpp>
#include <serial/datatool/statictype.hpp>
#include <serial/datatool/stlstr.hpp>
#include <serial/datatool/value.hpp>
#include <serial/datatool/reftype.hpp>

BEGIN_NCBI_SCOPE

CUniSequenceDataType::CUniSequenceDataType(const AutoPtr<CDataType>& element)
{
    SetElementType(element);
    m_NonEmpty = false;
    m_NoPrefix = false;
    ForbidVar("_type", "short");
    ForbidVar("_type", "int");
    ForbidVar("_type", "long");
    ForbidVar("_type", "unsigned");
    ForbidVar("_type", "unsigned short");
    ForbidVar("_type", "unsigned int");
    ForbidVar("_type", "unsigned long");
    ForbidVar("_type", "string");
}

const char* CUniSequenceDataType::GetASNKeyword(void) const
{
    return "SEQUENCE";
}

void CUniSequenceDataType::SetElementType(const AutoPtr<CDataType>& type)
{
    if ( GetElementType() )
        NCBI_THROW(CDatatoolException,eInvalidData,
            "double element type " + LocationString());
    m_ElementType = type;
}

void CUniSequenceDataType::PrintASN(CNcbiOstream& out, int indent) const
{
    out << GetASNKeyword() << " OF ";
    GetElementType()->PrintASNTypeComments(out, indent + 1);
    GetElementType()->PrintASN(out, indent);
}

void CUniSequenceDataType::PrintDTDElement(CNcbiOstream& out, bool contents_only) const
{
    const CDataType* typeElem = GetElementType();
    const CReferenceDataType* typeRef =
        dynamic_cast<const CReferenceDataType*>(typeElem);
    const CStaticDataType* typeStatic = 0;
    if (GetEnforcedStdXml()) {
        typeStatic = dynamic_cast<const CStaticDataType*>(typeElem);
    }
    string tag(XmlTagName());
    string userType =
        typeRef ? typeRef->UserTypeXmlTagName() : typeElem->XmlTagName();

    if (tag == userType || (GetEnforcedStdXml() && !typeRef)) {
        if (!GetParentType() || typeStatic) {
            out << "\n<!ELEMENT " << tag << ' ';
        }
        if (typeStatic) {
            out << '(';
            typeStatic->PrintDTDElement(out, true);
            out << ")>";
        } else {
            if (!GetParentType()) {
                out << '(';
            }
            typeElem->PrintDTDElement(out,true);
            if (!GetParentType()) {
                out << ')';
                if (m_NonEmpty) {
                    out << '+';
                } else {
                    out << '*';
                }
            }
        }
        if (!GetParentType()) {
            out << ">";
        }
        return;
    }
    out <<
        "\n<!ELEMENT "<< tag << ' ';
    if ( typeRef ) {
        out <<"(" << typeRef->UserTypeXmlTagName() << "*)";
    } else {
        if (typeStatic) {
            out << "(" << typeStatic->GetXMLContents() << ")";
        } else {
            out <<"(" << typeElem->XmlTagName() << "*)";
        }
    }
    out << ">";
}

void CUniSequenceDataType::PrintDTDExtra(CNcbiOstream& out) const
{
    const CDataType* typeElem = GetElementType();
    const CReferenceDataType* typeRef =
        dynamic_cast<const CReferenceDataType*>(typeElem);
    string tag(XmlTagName());
    string userType =
        typeRef ? typeRef->UserTypeXmlTagName() : typeElem->XmlTagName();

    if (tag == userType || (GetEnforcedStdXml() && !typeRef)) {
        const CStaticDataType* typeStatic =
            dynamic_cast<const CStaticDataType*>(typeElem);
        if (!typeStatic) {
            typeElem->PrintDTDExtra(out);
        }
        return;
    }

    if ( !typeRef ) {
        if ( GetParentType() == 0 )
            out << '\n';
        typeElem->PrintDTD(out);
    }
}

// XML schema generator submitted by
// Marc Dumontier, Blueprint initiative, dumontier@mshri.on.ca
// modified by Andrei Gourianov, gouriano@ncbi
void CUniSequenceDataType::PrintXMLSchemaElement(CNcbiOstream& out) const
{
    const CDataType* typeElem = GetElementType();
    const CReferenceDataType* typeRef =
        dynamic_cast<const CReferenceDataType*>(typeElem);
    string tag(XmlTagName());
    string userType =
        typeRef ? typeRef->UserTypeXmlTagName() : typeElem->XmlTagName();

    if (tag == userType || (GetEnforcedStdXml() && !typeRef)) {
        const CStaticDataType* typeStatic =
            dynamic_cast<const CStaticDataType*>(typeElem);
        if (typeStatic) {
            typeStatic->PrintXMLSchemaElementWithTag( out, tag);
        } else {
            typeElem->PrintXMLSchemaElement(out);
        }
        return;
    }

    out << "<xs:element name=\"" << tag << "\">\n"
        << "  <xs:complexType>\n"
        << "    <xs:sequence>\n"
        << "      <xs:element ref=\"" << userType
        << "\" minOccurs=\"0\" maxOccurs=\"unbounded\"/>\n"
        << "    </xs:sequence>\n"
        << "  </xs:complexType>\n"
        << "</xs:element>\n";
}

void CUniSequenceDataType::PrintXMLSchemaExtra(CNcbiOstream& out) const
{
    const CDataType* typeElem = GetElementType();
    const CReferenceDataType* typeRef =
        dynamic_cast<const CReferenceDataType*>(typeElem);
    string tag(XmlTagName());
    string userType =
        typeRef ? typeRef->UserTypeXmlTagName() : typeElem->XmlTagName();

    if (tag == userType || (GetEnforcedStdXml() && !typeRef)) {
        const CStaticDataType* typeStatic =
            dynamic_cast<const CStaticDataType*>(typeElem);
        if (!typeStatic) {
            typeElem->PrintXMLSchemaExtra(out);
        }
        return;
    }

    if ( !typeRef ) {
        if ( GetParentType() == 0 )
            out << '\n';
        typeElem->PrintXMLSchema(out);
    }
}

void CUniSequenceDataType::FixTypeTree(void) const
{
    CParent::FixTypeTree();
    m_ElementType->SetParent(this, "E");
    m_ElementType->SetInSet(this);
}

bool CUniSequenceDataType::CheckType(void) const
{
    return m_ElementType->Check();
}

bool CUniSequenceDataType::CheckValue(const CDataValue& value) const
{
    const CBlockDataValue* block =
        dynamic_cast<const CBlockDataValue*>(&value);
    if ( !block ) {
        value.Warning("block of values expected");
        return false;
    }
    bool ok = true;
    ITERATE ( CBlockDataValue::TValues, i, block->GetValues() ) {
        if ( !m_ElementType->CheckValue(**i) )
            ok = false;
    }
    return ok;
}

TObjectPtr CUniSequenceDataType::CreateDefault(const CDataValue& ) const
{
    NCBI_THROW(CDatatoolException,eNotImplemented,
        "SET/SEQUENCE OF default not implemented");
}

CTypeInfo* CUniSequenceDataType::CreateTypeInfo(void)
{
    return UpdateModuleName(CStlClassInfo_list<AnyType>::CreateTypeInfo(
//        m_ElementType->GetTypeInfo().Get()));
        m_ElementType->GetTypeInfo().Get(), GlobalName()));
}

bool CUniSequenceDataType::NeedAutoPointer(TTypeInfo /*typeInfo*/) const
{
    return true;
}

AutoPtr<CTypeStrings> CUniSequenceDataType::GetFullCType(void) const
{
    AutoPtr<CTypeStrings> tData = GetElementType()->GetFullCType();
    CTypeStrings::AdaptForSTL(tData);
    string templ = GetAndVerifyVar("_type");
    if ( templ.empty() )
        templ = "list";
    return AutoPtr<CTypeStrings>(new CListTypeStrings(templ, tData));
}

CUniSetDataType::CUniSetDataType(const AutoPtr<CDataType>& elementType)
    : CParent(elementType)
{
}

const char* CUniSetDataType::GetASNKeyword(void) const
{
    return "SET";
}

CTypeInfo* CUniSetDataType::CreateTypeInfo(void)
{
    return UpdateModuleName(CStlClassInfo_list<AnyType>::CreateSetTypeInfo(
        GetElementType()->GetTypeInfo().Get(), GlobalName()));
}

AutoPtr<CTypeStrings> CUniSetDataType::GetFullCType(void) const
{
    string templ = GetAndVerifyVar("_type");
    const CDataSequenceType* seq =
        dynamic_cast<const CDataSequenceType*>(GetElementType());
    if ( seq && seq->GetMembers().size() == 2 ) {
        const CDataMember& keyMember = *seq->GetMembers().front();
        const CDataMember& valueMember = *seq->GetMembers().back();
        if ( !keyMember.Optional() && !valueMember.Optional() ) {
            AutoPtr<CTypeStrings> tKey = keyMember.GetType()->GetFullCType();
            if ( tKey->CanBeKey() ) {
                AutoPtr<CTypeStrings> tValue = valueMember.GetType()->GetFullCType();
                CTypeStrings::AdaptForSTL(tValue);
                if ( templ.empty() )
                    templ = "multimap";
                return AutoPtr<CTypeStrings>(new CMapTypeStrings(templ,
                                                                 tKey,
                                                                 tValue));
            }
        }
    }
    AutoPtr<CTypeStrings> tData = GetElementType()->GetFullCType();
    CTypeStrings::AdaptForSTL(tData);
    if ( templ.empty() ) {
        if ( tData->CanBeKey() ) {
            templ = "list";
        }
        else {
            return AutoPtr<CTypeStrings>(new CListTypeStrings("list", tData, true));
        }
    }
    return AutoPtr<CTypeStrings>(new CListTypeStrings(templ, tData, true));
}

END_NCBI_SCOPE
