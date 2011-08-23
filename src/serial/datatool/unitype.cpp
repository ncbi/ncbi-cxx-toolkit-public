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
*/

#include <ncbi_pch.hpp>
#include <serial/impl/stltypes.hpp>
#include <serial/impl/autoptrinfo.hpp>
#include "exceptions.hpp"
#include "unitype.hpp"
#include "blocktype.hpp"
#include "statictype.hpp"
#include "stlstr.hpp"
#include "value.hpp"
#include "reftype.hpp"
#include "srcutil.hpp"

BEGIN_NCBI_SCOPE

CUniSequenceDataType::CUniSequenceDataType(const AutoPtr<CDataType>& element)
{
    SetElementType(element);
    m_NonEmpty = false;
    m_NoPrefix = false;
    if (element->IsNsQualified() != eNSQNotSet) {
        SetNsQualified( element->IsNsQualified() == eNSQualified);
    }
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

string CUniSequenceDataType::GetSpecKeyword(void) const
{
    return string(GetASNKeyword()) + ' ' +
           GetElementType()->GetSpecKeyword();
}

const char* CUniSequenceDataType::GetDEFKeyword(void) const
{
    return "_SEQUENCE_OF_";
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

void CUniSequenceDataType::PrintSpecDumpExtra(CNcbiOstream& out, int indent) const
{
    GetElementType()->PrintSpecDump(out, indent);
    GetElementType()->Comments().PrintASN(out, indent, CComments::eNoEOL);
}

// XML schema generator submitted by
// Marc Dumontier, Blueprint initiative, dumontier@mshri.on.ca
// modified by Andrei Gourianov, gouriano@ncbi
void CUniSequenceDataType::PrintXMLSchema(CNcbiOstream& out,
    int indent, bool contents_only) const
{
    const CDataType* typeElem = GetElementType();
    const CReferenceDataType* typeRef =
        dynamic_cast<const CReferenceDataType*>(typeElem);
    const CStaticDataType* typeStatic =
        dynamic_cast<const CStaticDataType*>(typeElem);
    const CDataMemberContainerType* typeContainer =
        dynamic_cast<const CDataMemberContainerType*>(typeElem);

    string tag(XmlTagName()), type(typeElem->GetSchemaTypeString());
    string userType = typeRef ? typeRef->UserTypeXmlTagName() : typeElem->XmlTagName();

    if (GetEnforcedStdXml() && (typeStatic || (typeRef && tag == userType))) {
        bool any = dynamic_cast<const CAnyContentDataType*>(typeStatic) != 0;
        PrintASNNewLine(out, indent++);
        if (any) {
            out << "<xs:any processContents=\"lax\"";
            const string& ns = GetNamespaceName();
            if (!ns.empty()) {
                out << " namespace=\"" << ns << "\"";
            }
        } else {
            out << "<xs:element ";
            if (typeRef) {
                out << "ref";
            } else {
                out << "name";
            }
            out << "=\"" << tag << "\"";
            if (typeStatic && !type.empty()) {
                out << " type=\"" << type << "\"";
            }
        }
        if (GetParentType()) {
            bool isOptional = GetDataMember() ?
                GetDataMember()->Optional() : !IsNonEmpty();
            if (isOptional) {
                out << " minOccurs=\"0\"";
            }
            out << " maxOccurs=\"unbounded\"";
        }
        out << "/>";
    } else {
        bool asn_container = false;
        list<string> opentag, closetag;
        string xsdk("sequence");
        if (!contents_only) {
            string asnk("SEQUENCE");
            bool isMixed = false;
            bool isSimpleSeq = false;
            bool isOptional = false;
            if (typeContainer) {
                asnk = typeContainer->GetASNKeyword();
                ITERATE ( CDataMemberContainerType::TMembers, i, typeContainer->GetMembers() ) {
                    if (i->get()->Notag()) {
                        const CStringDataType* str =
                            dynamic_cast<const CStringDataType*>(i->get()->GetType());
                        if (str != 0) {
                            isMixed = true;
                            break;
                        }
                    }
                }
                if (typeContainer->GetMembers().size() == 1) {
                    CDataMemberContainerType::TMembers::const_iterator i = 
                        typeContainer->GetMembers().begin();
                    const CUniSequenceDataType* typeSeq =
                        dynamic_cast<const CUniSequenceDataType*>(i->get()->GetType());
                    isSimpleSeq = (typeSeq != 0);
                    if (isSimpleSeq) {
                        const CDataMember *mem = typeSeq->GetDataMember();
                        if (mem) {
                            const CDataMemberContainerType* data =
                                dynamic_cast<const CDataMemberContainerType*>(typeSeq->GetElementType());
                            if (data) {
                                asnk = data->GetASNKeyword();
                                ITERATE ( CDataMemberContainerType::TMembers, m, data->GetMembers() ) {
                                    if (m->get()->Notag()) {
                                        const CStringDataType* str =
                                            dynamic_cast<const CStringDataType*>(m->get()->GetType());
                                        if (str != 0) {
                                            isMixed = true;
                                            break;
                                        }
                                    }
                                }
                            }
                            if (mem->Notag()) {
                                isOptional = mem->Optional();
                            } else {
                                isSimpleSeq = false;
                            }
                        }
                    }
                }
            }
            if(NStr::CompareCase(asnk,"CHOICE")==0) {
                xsdk = "choice";
            }
            if(NStr::CompareCase(asnk,"SET")==0) {
                xsdk = "all";
            }
            string tmp = "<xs:element name=\"" + tag + "\"";
            if (GetDataMember()) {
                if (GetDataMember()->Optional()) {
                    tmp += " minOccurs=\"0\"";
                }
                if (GetXmlSourceSpec()) {
                    tmp += " maxOccurs=\"unbounded\"";
                }
            }
            opentag.push_back(tmp + ">");
            closetag.push_front("</xs:element>");

            if (typeContainer && !GetXmlSourceSpec() && !GetEnforcedStdXml()) {
                asn_container = true;
                opentag.push_back("<xs:complexType>");
                closetag.push_front("</xs:complexType>");
                opentag.push_back("<xs:sequence minOccurs=\"0\" maxOccurs=\"unbounded\">");
                closetag.push_front("</xs:sequence>");
                opentag.push_back("<xs:element name=\"" + userType + "\">");
                closetag.push_front("</xs:element>");
            }
            tmp = "<xs:complexType";
            if (isMixed) {
                tmp += " mixed=\"true\"";
            }
            opentag.push_back(tmp + ">");
            closetag.push_front("</xs:complexType>");

            tmp = "<xs:" + xsdk;
            if (!GetXmlSourceSpec()) {
                if (!asn_container) {
                    tmp += " minOccurs=\"0\" maxOccurs=\"unbounded\"";
                }
            } else if (!GetDataMember()) {
                if (!IsNonEmpty()) {
                    tmp += " minOccurs=\"0\"";
                }
                tmp += " maxOccurs=\"unbounded\"";
            } else if (isSimpleSeq) {
                if (isOptional) {
                    tmp += " minOccurs=\"0\"";
                }
                tmp += " maxOccurs=\"unbounded\"";
            }
            opentag.push_back(tmp + ">");
            closetag.push_front("</xs:" + xsdk + ">");
            ITERATE ( list<string>, s, opentag ) {
                PrintASNNewLine(out, indent++) << *s;
            }
        }
        typeElem->PrintXMLSchema(out,indent,true);
        if (!contents_only) {
            ITERATE ( list<string>, s, closetag ) {
                PrintASNNewLine(out, --indent) << *s;
            }
        }
    }
}

void CUniSequenceDataType::PrintDTDElement(CNcbiOstream& out, bool contents_only) const
{
    const CDataType* typeElem = GetElementType();
    if (!contents_only) {
        typeElem->PrintDTDTypeComments(out,0);
    }
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
        if (typeRef && !contents_only) {
            typeRef->PrintDTDElement(out,contents_only);
            return;
        }
        if (!GetParentType() || typeStatic || !contents_only) {
            out << "\n<!ELEMENT " << tag << ' ';
        }
        if (typeStatic || !contents_only) {
            out << '(';
            typeElem->PrintDTDElement(out, true);
            out << ")";
            if (!typeStatic && !GetParentType()) {
                if (m_NonEmpty) {
                    out << '+';
                } else {
                    out << '*';
                }
            }
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
        if (!contents_only) {
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

void CUniSequenceDataType::FixTypeTree(void) const
{
    CParent::FixTypeTree();
    m_ElementType->SetParent(this, "E", "E");
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
        if (CDataType::GetXmlSourceSpec()) {
            return m_ElementType->CheckValue(value);
        }
        value.Warning("block of values expected", 18);
        return false;
    }
    bool ok = true;
    ITERATE ( CBlockDataValue::TValues, i, block->GetValues() ) {
        if ( !m_ElementType->CheckValue(**i) )
            ok = false;
    }
    return ok;
}

TObjectPtr CUniSequenceDataType::CreateDefault(const CDataValue&  value) const
{
    if (CDataType::GetXmlSourceSpec()) {
        return m_ElementType->CreateDefault(value);
    }
    NCBI_THROW(CDatatoolException,eNotImplemented,
        "SET/SEQUENCE OF default not implemented");
}

string CUniSequenceDataType::GetDefaultString(const CDataValue& value) const
{
    if (CDataType::GetXmlSourceSpec()) {
        return m_ElementType->GetDefaultString(value);
    }
    return CParent::GetDefaultString(value);
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
    return AutoPtr<CTypeStrings>(new CListTypeStrings(templ, tData, GetNamespaceName(), this));
}

CUniSetDataType::CUniSetDataType(const AutoPtr<CDataType>& elementType)
    : CParent(elementType)
{
}

const char* CUniSetDataType::GetASNKeyword(void) const
{
    return "SET";
}

const char* CUniSetDataType::GetDEFKeyword(void) const
{
    return "_SET_OF_";
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
                                                                 tValue,
                                                                 GetNamespaceName(),
                                                                 this));
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
            return AutoPtr<CTypeStrings>(new CListTypeStrings("list", tData,
                GetNamespaceName(), this, true));
        }
    }
    return AutoPtr<CTypeStrings>(new CListTypeStrings(templ, tData,
        GetNamespaceName(), this, true));
}

END_NCBI_SCOPE
