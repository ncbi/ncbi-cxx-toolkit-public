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
*   Type description for compound types: SET, SEQUENCE and CHOICE
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.56  2005/02/09 16:05:10  gouriano
* Corrected schema generation for elements with mixed content
*
* Revision 1.55  2005/02/09 14:33:25  gouriano
* Corrected formatting when writing DTD
*
* Revision 1.54  2005/02/02 19:08:36  gouriano
* Corrected DTD generation
*
* Revision 1.53  2005/01/12 18:05:10  gouriano
* Corrected generation of XML schema for sequence of choice types,
* and simple types with default
*
* Revision 1.52  2004/09/27 18:28:40  gouriano
* Improved diagnostics when creating DataMember with no name
*
* Revision 1.51  2004/06/18 15:27:35  gouriano
* Improved diagnostics
*
* Revision 1.50  2004/05/17 21:03:13  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 1.49  2003/11/21 16:59:11  gouriano
* Correct conversion of ASN spec into XML schema in case of containers
*
* Revision 1.48  2003/06/24 20:55:42  gouriano
* corrected code generation and serialization of non-empty unnamed containers (XML)
*
* Revision 1.47  2003/06/16 14:41:05  gouriano
* added possibility to convert DTD to XML schema
*
* Revision 1.46  2003/05/14 14:42:22  gouriano
* added generation of XML schema
*
* Revision 1.45  2003/04/10 20:13:41  vakatov
* Rollback the "uninitialized member" verification -- it still needs to
* be worked upon...
*
* Revision 1.43  2003/03/11 20:06:47  kuznets
* iterate -> ITERATE
*
* Revision 1.42  2003/03/10 18:55:18  gouriano
* use new structured exceptions (based on CException)
*
* Revision 1.41  2003/02/10 17:56:16  gouriano
* make it possible to disable scope prefixes when reading and writing objects generated from ASN specification in XML format, or when converting an ASN spec into DTD.
*
* Revision 1.40  2002/12/17 16:26:34  gouriano
* added new flags to CMemberInfo in CDataContainerType::CreateClassInfo
*
* Revision 1.39  2002/11/19 19:48:29  gouriano
* added support of XML attributes of choice variants
*
* Revision 1.38  2002/11/14 21:02:54  gouriano
* added support of XML attribute lists
*
* Revision 1.37  2002/10/15 13:58:04  gouriano
* use "noprefix" flag
*
* Revision 1.36  2002/02/21 17:17:13  grichenk
* Prohibited unnamed members in ASN.1 specifications
*
* Revision 1.35  2001/06/11 14:35:02  grichenk
* Added support for numeric tags in ASN.1 specifications and data streams.
*
* Revision 1.34  2001/05/17 15:07:11  lavr
* Typos corrected
*
* Revision 1.33  2001/02/15 21:39:14  kholodov
* Modified: pointer to parent CDataMember added to CDataType class.
* Modified: default value for BOOLEAN type in DTD is copied from ASN.1 spec.
*
* Revision 1.32  2000/11/29 17:42:42  vasilche
* Added CComment class for storing/printing ASN.1/XML module comments.
* Added srcutil.hpp file to reduce file dependency.
*
* Revision 1.31  2000/11/20 17:26:31  vasilche
* Fixed warnings on 64 bit platforms.
* Updated names of config variables.
*
* Revision 1.30  2000/11/15 20:34:54  vasilche
* Added user comments to ENUMERATED types.
* Added storing of user comments to ASN.1 module definition.
*
* Revision 1.29  2000/11/14 21:41:24  vasilche
* Added preserving of ASN.1 definition comments.
*
* Revision 1.28  2000/11/09 18:14:43  vasilche
* Fixed nonstandard behaviour of 'for' statement on MS VC.
*
* Revision 1.27  2000/11/08 17:02:50  vasilche
* Added generation of modular DTD files.
*
* Revision 1.26  2000/11/07 17:26:24  vasilche
* Added module names to CTypeInfo and CEnumeratedTypeValues
* Added possibility to set include directory for whole module
*
* Revision 1.25  2000/10/03 17:22:49  vasilche
* Reduced header dependency.
* Reduced size of debug libraries on WorkShop by 3 times.
* Fixed tag allocation for parent classes.
* Fixed CObject allocation/deallocation in streams.
* Moved instantiation of several templates in separate source file.
*
* Revision 1.24  2000/09/19 14:10:26  vasilche
* Added files to MSVC project
* Updated shell scripts to use new datattool path on MSVC
* Fixed internal compiler error on MSVC
*
* Revision 1.23  2000/09/18 20:00:28  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* Revision 1.22  2000/08/25 15:59:19  vasilche
* Renamed directory tool -> datatool.
*
* Revision 1.21  2000/08/15 19:45:27  vasilche
* Added Read/Write hooks:
* CReadObjectHook/CWriteObjectHook for objects of specified type.
* CReadClassMemberHook/CWriteClassMemberHook for specified members.
* CReadChoiceVariantHook/CWriteChoiceVariant for specified choice variants.
* CReadContainerElementHook/CWriteContainerElementsHook for containers.
*
* Revision 1.20  2000/07/03 18:42:57  vasilche
* Added interface to typeinfo via CObjectInfo and CConstObjectInfo.
*
* Revision 1.19  2000/06/16 16:31:37  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.18  2000/05/24 20:57:14  vasilche
* Use new macro _DEBUG_ARG to avoid warning about unused argument.
*
* Revision 1.17  2000/05/24 20:09:27  vasilche
* Implemented DTD generation.
*
* Revision 1.16  2000/05/03 14:38:17  vasilche
* SERIAL: added support for delayed reading to generated classes.
* DATATOOL: added code generation for delayed reading.
*
* Revision 1.15  2000/04/17 19:11:07  vasilche
* Fixed failed assertion.
* Removed redundant namespace specifications.
*
* Revision 1.14  2000/04/12 15:36:48  vasilche
* Added -on <namespace> argument to datatool.
* Removed unnecessary namespace specifications in generated files.
*
* Revision 1.13  2000/04/07 19:26:23  vasilche
* Added namespace support to datatool.
* By default with argument -oR datatool will generate objects in namespace
* NCBI_NS_NCBI::objects (aka ncbi::objects).
* Datatool's classes also moved to NCBI namespace.
*
* Revision 1.12  2000/03/14 14:43:10  vasilche
* All OPTIONAL members implemented via CRef<> by default.
*
* Revision 1.11  2000/03/10 15:00:45  vasilche
* Fixed OPTIONAL members reading.
*
* Revision 1.10  2000/03/07 14:06:30  vasilche
* Added generation of reference counted objects.
*
* Revision 1.9  2000/02/01 21:47:53  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
* Changed class generation.
* Moved datatool headers to include/internal/serial/tool.
*
* Revision 1.8  1999/12/21 17:18:33  vasilche
* Added CDelayedFostream class which rewrites file only if contents is changed.
*
* Revision 1.7  1999/12/03 21:42:10  vasilche
* Fixed conflict of enums in choices.
*
* Revision 1.6  1999/12/01 17:36:24  vasilche
* Fixed CHOICE processing.
*
* Revision 1.5  1999/11/18 17:13:05  vasilche
* Fixed generation of ENUMERATED CHOICE and VisibleString.
* Added generation of initializers to zero for primitive types and pointers.
*
* Revision 1.4  1999/11/16 15:41:16  vasilche
* Added plain pointer choice.
* By default we use C pointer instead of auto_ptr.
* Start adding initializers.
*
* Revision 1.3  1999/11/15 20:31:37  vasilche
* Fixed error on GCC
*
* Revision 1.2  1999/11/15 19:36:13  vasilche
* Fixed warnings on GCC
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <serial/datatool/exceptions.hpp>
#include <serial/datatool/blocktype.hpp>
#include <serial/datatool/unitype.hpp>
#include <serial/datatool/reftype.hpp>
#include <serial/datatool/statictype.hpp>
#include <serial/autoptrinfo.hpp>
#include <serial/datatool/value.hpp>
#include <serial/datatool/classstr.hpp>
#include <serial/datatool/module.hpp>
#include <serial/datatool/srcutil.hpp>
#include <serial/classinfo.hpp>
#include <serial/member.hpp>
#include <typeinfo>

BEGIN_NCBI_SCOPE

class CAnyTypeClassInfo : public CClassTypeInfo
{
public:
    CAnyTypeClassInfo(const string& name, size_t count)
        : CClassTypeInfo(sizeof(AnyType) * count, name,
                         TObjectPtr(0), &CreateAnyTypeClass,
                         typeid(void), &GetAnyTypeClassId)
        {
        }

    const AnyType* GetAnyTypePtr(size_t index) const
        {
            return &static_cast<AnyType*>(0)[index];
        }
    const bool* GetSetFlagPtr(size_t index)
        {
            return &GetAnyTypePtr(index)->booleanValue;
        }

protected:
    static TObjectPtr CreateAnyTypeClass(TTypeInfo objectType)
        {
            size_t size = objectType->GetSize();
            TObjectPtr obj = new char[size];
            memset(obj, 0, size);
            return obj;
        }
    static const type_info* GetAnyTypeClassId(TConstObjectPtr /*objectPtr*/)
        {
            return 0;
        }
};

void CDataMemberContainerType::AddMember(const AutoPtr<CDataMember>& member)
{
    m_Members.push_back(member);
}

void CDataMemberContainerType::PrintASN(CNcbiOstream& out, int indent) const
{
    out << GetASNKeyword() << " {";
    ++indent;
    ITERATE ( TMembers, i, m_Members ) {
        PrintASNNewLine(out, indent);
        const CDataMember& member = **i;
        TMembers::const_iterator next = i;
        bool last = ++next == m_Members.end();
        member.PrintASN(out, indent, last);
    }
    --indent;
    PrintASNNewLine(out, indent);
    m_LastComments.PrintASN(out, indent, CComments::eMultiline);
    out << "}";
}

void CDataMemberContainerType::PrintDTDElement(CNcbiOstream& out, bool contents_only) const
{
    string tag = XmlTagName();
    bool hasAttlist= false, isAttlist= false;
    bool hasNotag= false, isOptional= false;
    bool isSimple= false, isSeq= false;
    string indent("    ");

    if (GetEnforcedStdXml()) {
        ITERATE ( TMembers, i, m_Members ) {
            if (i->get()->Attlist()) {
                hasAttlist = true;
                break;
            }
        }
        if (GetDataMember()) {
            isAttlist = GetDataMember()->Attlist();
            hasNotag   = GetDataMember()->Notag();
        }
        if (GetMembers().size()==1) {
            const CDataMember* member = GetMembers().front().get();
            const CUniSequenceDataType* uniType =
                dynamic_cast<const CUniSequenceDataType*>(member->GetType());
            if (uniType && member->Notag()) {
                isSeq = true;
            }
        }
        if (hasAttlist && GetMembers().size()==2) {
            ITERATE ( TMembers, i, GetMembers() ) {
                if (i->get()->Attlist()) {
                    continue;
                }
                if (i->get()->SimpleType()) {
                    isSimple = true;
                    i->get()->GetType()->PrintDTDElement(out);
                } else {
                    const CUniSequenceDataType* uniType =
                        dynamic_cast<const CUniSequenceDataType*>(i->get()->GetType());
                    if (uniType && i->get()->Notag()) {
                        isSeq = true;
                    }
                }
            }
        }
    }

    if (isAttlist) {
        ITERATE ( TMembers, i, m_Members ) {
            out << indent;
            i->get()->GetType()->PrintDTDElement(out);
        }
        return;
    }
    if (!isSimple) {
        if (!contents_only) {
            out << "\n<!ELEMENT " << tag << " ";
            if (!isSeq) {
                out << "(";
            }
        }
        bool need_separator = false;
        ITERATE ( TMembers, i, m_Members ) {
            if (need_separator) {
                out << XmlMemberSeparator();
            }
            need_separator = true;
            const CDataMember& member = **i;
            string member_name( member.GetType()->XmlTagName());
            const CUniSequenceDataType* uniType =
                dynamic_cast<const CUniSequenceDataType*>(member.GetType());
            if (GetEnforcedStdXml()) {
                if (member.Attlist()) {
                    need_separator = false;
                    continue;
                }
                if (member.Notag()) {
                    const CStaticDataType* statType = 
                        dynamic_cast<const CStaticDataType*>(member.GetType());
                    if (!statType) {
                        out << "(";
                    } else {
                        out << "\n        ";
                    }
                    member.GetType()->PrintDTDElement(out,true);
                    if (!statType) {
                        out << ")";
                    }
                } else {
                    out << "\n        " << member_name;
                }
            } else {
                out << "\n        " << member_name;
            }
            if (uniType) {
                const CStaticDataType* elemType =
                    dynamic_cast<const CStaticDataType*>(uniType->GetElementType());
                if ((elemType || member.NoPrefix()) && GetEnforcedStdXml()) {
                    if ( member.Optional() ) {
                        out << '*';
                    } else {
                        out << '+';
                    }
                } else {
                    if ( member.Optional() ) {
                        out << '?';
                    }
                }
            } else {
                if ( member.Optional() ) {
                    out << '?';
                }
            }
        }
        if (!contents_only) {
            if (!isSeq) {
                out << ")";
            }
            out << ">";
        }
    }
    if (hasAttlist) {
        out << "\n<!ATTLIST " << tag << '\n';
        ITERATE ( TMembers, i, m_Members ) {
            const CDataMember& member = **i;
            if (member.Attlist()) {
                member.GetType()->PrintDTDElement(out);
                break;
            }
        }
        out << indent << ">";
    }
    if (!contents_only && !isSimple) {
        out << '\n';
    }
}

void CDataMemberContainerType::PrintDTDExtra(CNcbiOstream& out) const
{
    ITERATE ( TMembers, i, m_Members ) {
        const CDataMember& member = **i;
        if (member.Notag()) {
            member.GetType()->PrintDTDExtra(out);
        } else {
            member.PrintDTD(out);
        }
    }
    m_LastComments.PrintDTD(out, CComments::eMultiline);
}

// XML schema generator submitted by
// Marc Dumontier, Blueprint initiative, dumontier@mshri.on.ca
// modified by Andrei Gourianov, gouriano@ncbi
void CDataMemberContainerType::PrintXMLSchemaElement(CNcbiOstream& out) const
{
    string tag = XmlTagName();
    string asnk = GetASNKeyword();
    string xsdk;
    bool hasAttlist= false, isAttlist= false;
    bool hasNotag= false, isOptional= false, isSeq= false, isMixed=false;
    bool isSimple= false, isSimpleSeq= false;
    bool parent_hasNotag= false;
    string openTag, closeTag1, closeTag2, simpleType, simpleContents;

    if (GetEnforcedStdXml()) {
        ITERATE ( TMembers, i, m_Members ) {
            if (i->get()->Attlist()) {
                hasAttlist = true;
                break;
            }
        }
        if (( hasAttlist && GetMembers().size() > 2) ||
            (!hasAttlist && GetMembers().size() > 1)) {
            ITERATE ( TMembers, i, m_Members ) {
                if (i->get()->Notag()) {
                    const CStringDataType* str =
                        dynamic_cast<const CStringDataType*>(i->get()->GetType());
                    isMixed = (str != 0);
                }
            }
        }
        if (GetDataMember()) {
            isAttlist = GetDataMember()->Attlist();
            hasNotag   = GetDataMember()->Notag();
            isOptional= GetDataMember()->Optional();
            isSeq   = (dynamic_cast<const CUniSequenceDataType*>(GetDataMember()->GetType()) != 0);
        }
        const CUniSequenceDataType* uniType = 
            dynamic_cast<const CUniSequenceDataType*>(GetParentType());
        if (uniType) {
            tag = GetParentType()->XmlTagName();
            isSeq = true;
            if (GetParentType()->GetDataMember()) {
                isOptional = GetParentType()->GetDataMember()->Optional();
                parent_hasNotag = GetParentType()->GetDataMember()->Notag();
            } else {
                isOptional = !uniType->IsNonEmpty();
            }
        }
        if (hasNotag && GetMembers().size()==1) {
            const CDataMember* member = GetMembers().front().get();
            isOptional = member->Optional();
            const CUniSequenceDataType* typeSeq =
                dynamic_cast<const CUniSequenceDataType*>(member->GetType());
            isSeq = (typeSeq != 0);
            if (isSeq) {
                const CDataMemberContainerType* data =
                    dynamic_cast<const CDataMemberContainerType*>(typeSeq->GetElementType());
                if (data) {
                    asnk = data->GetASNKeyword();
                }
            }
        }
        if (hasAttlist && GetMembers().size()==2) {
            ITERATE ( TMembers, i, GetMembers() ) {
                if (i->get()->Attlist()) {
                    continue;
                }
                if (i->get()->SimpleType()) {
                    isSimple = true;
                    const CStaticDataType* statType =
                        dynamic_cast<const CStaticDataType*>(i->get()->GetType());
                    if (!statType) {
                        NCBI_THROW(CDatatoolException,eInvalidData,
                                   string("Wrong element type: ") + tag);
                    }
                    statType->GetXMLSchemaContents(simpleType, simpleContents);
                } else {
                    const CUniSequenceDataType* typeSeq =
                        dynamic_cast<const CUniSequenceDataType*>(i->get()->GetType());
                    isSimpleSeq = (typeSeq != 0);
                    if (isSimpleSeq) {
                        isSeq = true;
                        const CDataMember *mem = typeSeq->GetDataMember();
                        if (mem) {
                            const CDataMemberContainerType* data =
                                dynamic_cast<const CDataMemberContainerType*>(typeSeq->GetElementType());
                            if (data) {
                                asnk = data->GetASNKeyword();
                            }
                            if (mem->Notag()) {
                                isOptional = mem->Optional();
                            } else {
                                isSeq = isSimpleSeq = false;
                            }
                        }
                    }
                }
            }
        }
    }

    x_AddSavedName(tag);
    if(NStr::CompareCase(asnk,"CHOICE")==0) {
        xsdk = "choice";
    } else if(NStr::CompareCase(asnk,"SEQUENCE")==0) {
        xsdk = "sequence";
    }

    if (isAttlist || parent_hasNotag) {
        openTag.erase();
        closeTag1.erase();
        closeTag2.erase();
    } else if (isSimple) {
        openTag   = "<xs:element name=\"" + tag + "\">\n"
                    "  <xs:complexType";
        if (isMixed) {
            openTag += " mixed=\"true\"";
        }
        openTag += ">\n";
        if (!simpleType.empty()) {
            openTag  +="    <xs:simpleContent>\n"
                       "      <xs:extension base=\"" + simpleType + "\">\n";
        }
        closeTag1.erase();
        closeTag2.erase();
        if (!simpleType.empty()) {
            closeTag2+= "      </xs:extension>\n"
                        "    </xs:simpleContent>\n";
        }
        closeTag2+= "  </xs:complexType>\n"
                    "</xs:element>\n";
    } else {
        openTag.erase();
        if (!hasNotag) {
            openTag += "<xs:element name=\"" + tag + "\">\n" +
                       "  <xs:complexType";
            if (isMixed) {
                openTag += " mixed=\"true\"";
            }
            openTag += ">\n";
        }
        openTag     += "    <xs:"  + xsdk;
        if (isOptional) {
            openTag += " minOccurs=\"0\"";
        }
        if (isSeq) {
            openTag += " maxOccurs=\"unbounded\"";
        }
        openTag     += ">\n";
        closeTag1    = "    </xs:" + xsdk + ">\n";
        closeTag2.erase();
        if (!hasNotag) {
            closeTag2+= "  </xs:complexType>\n"
                        "</xs:element>\n";
        }
    }

    out << openTag;
    if (isAttlist) {
        ITERATE ( TMembers, i, m_Members ) {
            i->get()->GetType()->PrintXMLSchemaElement(out);
        }
    } else if (!isSimple) {
        ITERATE ( TMembers, i, m_Members ) {
            const CDataMember& member = **i;
            string member_name( member.GetType()->XmlTagName());
            bool uniseq = false;
            if (GetEnforcedStdXml()) {
                if (member.Attlist()) {
                    continue;
                }
                const CUniSequenceDataType* type =
                    dynamic_cast<const CUniSequenceDataType*>(member.GetType());
                uniseq = (type != 0);
                if (uniseq) {
                    if (isSimpleSeq) {
                        type->PrintXMLSchemaElement(out);
                        continue;
                    }
                    const CReferenceDataType* typeRef =
                        dynamic_cast<const CReferenceDataType*>(type->GetElementType());
                    if (typeRef) {
                        if (type->XmlTagName() != typeRef->UserTypeXmlTagName()) {
                            uniseq = false;
                        }
                    }
                }
                if (member.Notag()) {
                    if (isMixed && dynamic_cast<const CStringDataType*>(member.GetType())) {
                        continue;
                    }
                    member.GetType()->PrintXMLSchemaElement(out);
                    continue;
                }
            }
            out << "      <xs:element ref=\"" << member_name << "\"";
            if ( member.Optional()) {
                out << " minOccurs=\"0\"";
                if (member.GetDefault()) {
                    out << " default=\"" << member.GetDefault()->GetXmlString() << "\"";
                }
            }
            if (uniseq) {
                out << " maxOccurs=\"unbounded\"";
            }
            out << "/>\n";
        }
    }
    out << closeTag1;
    if (hasAttlist) {
        ITERATE ( TMembers, i, m_Members ) {
            const CDataMember& member = **i;
            if (member.Attlist()) {
                member.GetType()->PrintXMLSchemaElement(out);
            }
        }
    }
    out << closeTag2;
}

void CDataMemberContainerType::PrintXMLSchemaExtra(CNcbiOstream& out) const
{
    if ( GetParentType() == 0 ) {
        out << "\n";
    }
    ITERATE ( TMembers, i, m_Members ) {
        const CDataMember& member = **i;
        if (member.Notag()) {
            member.GetType()->PrintXMLSchemaExtra(out);
        } else {
            member.PrintXMLSchema(out);
        }
    }                                                                                         
    m_LastComments.PrintDTD(out, CComments::eMultiline);
}


void CDataMemberContainerType::FixTypeTree(void) const
{
    CParent::FixTypeTree();
    ITERATE ( TMembers, i, m_Members ) {
        (*i)->GetType()->SetParent(this, (*i)->GetName());
    }
}

bool CDataMemberContainerType::CheckType(void) const
{
    bool ok = true;
    ITERATE ( TMembers, i, m_Members ) {
        if ( !(*i)->Check() )
            ok = false;
    }
    return ok;
}

TObjectPtr CDataMemberContainerType::CreateDefault(const CDataValue& ) const
{
    NCBI_THROW(CDatatoolException,eNotImplemented,
                 GetASNKeyword() + string(" default not implemented"));
}

const char* CDataContainerType::XmlMemberSeparator(void) const
{
    return ", ";
}

CTypeInfo* CDataContainerType::CreateTypeInfo(void)
{
    return CreateClassInfo();
}

CClassTypeInfo* CDataContainerType::CreateClassInfo(void)
{
    size_t itemCount = 0;
    // add place for 'isSet' flags
    ITERATE ( TMembers, i, GetMembers() ) {
        ++itemCount;
        CDataMember* mem = i->get();
        if ( mem->Optional() )
            ++itemCount;
    }
    auto_ptr<CAnyTypeClassInfo> typeInfo(new CAnyTypeClassInfo(GlobalName(),
                                                               itemCount));
    size_t index = 0;
    for ( TMembers::const_iterator i = GetMembers().begin();
          i != GetMembers().end(); ++i ) {
        CDataMember* mem = i->get();
        CDataType* memType = mem->GetType();
        TConstObjectPtr memberPtr = typeInfo->GetAnyTypePtr(index++);
        CMemberInfo* memInfo =
            typeInfo->AddMember(mem->GetName(), memberPtr,
                                memType->GetTypeInfo());
        if ( mem->Optional() ) {
            if ( mem->GetDefault() ) {
                TObjectPtr defPtr = memType->CreateDefault(*mem->GetDefault());
                memInfo->SetDefault(defPtr);
            }
            else {
                memInfo->SetOptional();
            }
            memInfo->SetSetFlag(typeInfo->GetSetFlagPtr(index++));
        }
        if (mem->NoPrefix()) {
            memInfo->SetNoPrefix();
        }
        if (mem->Attlist()) {
            memInfo->SetAttlist();
        }
        if (mem->Notag()) {
            memInfo->SetNotag();
        }
    }
    if ( HaveModuleName() )
        typeInfo->SetModuleName(GetModule()->GetName());
    return typeInfo.release();
}

AutoPtr<CTypeStrings> CDataContainerType::GenerateCode(void) const
{
    return GetFullCType();
}

AutoPtr<CTypeStrings> CDataContainerType::GetFullCType(void) const
{
    bool isRootClass = GetParentType() == 0;
    AutoPtr<CClassTypeStrings> code(new CClassTypeStrings(GlobalName(),
                                                          ClassName()));
    bool haveUserClass = isRootClass;
/*
    bool isObject;
    if ( haveUserClass ) {
        isObject = true;
    }
    else {
        isObject = !GetVar("_object").empty();
    }
*/
    code->SetHaveUserClass(haveUserClass);
    code->SetObject(true /*isObject*/ );
    ITERATE ( TMembers, i, GetMembers() ) {
        string defaultCode;
        bool optional = (*i)->Optional();
        const CDataValue* defaultValue = (*i)->GetDefault();
        if ( defaultValue ) {
            defaultCode = (*i)->GetType()->GetDefaultString(*defaultValue);
            _ASSERT(!defaultCode.empty());
        }

        bool delayed = !GetVar((*i)->GetName()+"._delay").empty();
        AutoPtr<CTypeStrings> memberType = (*i)->GetType()->GetFullCType();
        code->AddMember((*i)->GetName(), memberType,
                        (*i)->GetType()->GetVar("_pointer"),
                        optional, defaultCode, delayed,
                        (*i)->GetType()->GetTag(),
                        (*i)->NoPrefix(), (*i)->Attlist(), (*i)->Notag(),
                        (*i)->SimpleType(),(*i)->GetType(),false);
        (*i)->GetType()->SetTypeStr(&(*code));
    }
    SetTypeStr(&(*code));
    SetParentClassTo(*code);
    return AutoPtr<CTypeStrings>(code.release());
}

AutoPtr<CTypeStrings> CDataContainerType::GetRefCType(void) const
{
    return AutoPtr<CTypeStrings>(new CClassRefTypeStrings(ClassName(),
                                                          Namespace(),
                                                          FileName()));
}

const char* CDataSetType::GetASNKeyword(void) const
{
    return "SET";
}

bool CDataSetType::CheckValue(const CDataValue& value) const
{
    const CBlockDataValue* block =
        dynamic_cast<const CBlockDataValue*>(&value);
    if ( !block ) {
        value.Warning("block of values expected");
        return false;
    }

    typedef map<string, const CDataMember*> TReadValues;
    TReadValues mms;
    for ( TMembers::const_iterator m = GetMembers().begin();
          m != GetMembers().end(); ++m ) {
        mms[m->get()->GetName()] = m->get();
    }

    ITERATE ( CBlockDataValue::TValues, v, block->GetValues() ) {
        const CNamedDataValue* currvalue =
            dynamic_cast<const CNamedDataValue*>(v->get());
        if ( !currvalue ) {
            v->get()->Warning("named value expected");
            return false;
        }
        TReadValues::iterator member = mms.find(currvalue->GetName());
        if ( member == mms.end() ) {
            currvalue->Warning("unexpected member");
            return false;
        }
        if ( !member->second->GetType()->CheckValue(currvalue->GetValue()) ) {
            return false;
        }
        mms.erase(member);
    }
    
    for ( TReadValues::const_iterator member = mms.begin();
          member != mms.end(); ++member ) {
        if ( !member->second->Optional() ) {
            value.Warning(member->first + " member expected");
            return false;
        }
    }
    return true;
}

CClassTypeInfo* CDataSetType::CreateClassInfo(void)
{
    return CParent::CreateClassInfo()->SetRandomOrder();
}

const char* CDataSequenceType::GetASNKeyword(void) const
{
    return "SEQUENCE";
}

bool CDataSequenceType::CheckValue(const CDataValue& value) const
{
    const CBlockDataValue* block =
        dynamic_cast<const CBlockDataValue*>(&value);
    if ( !block ) {
        value.Warning("block of values expected");
        return false;
    }
    TMembers::const_iterator member = GetMembers().begin();
    CBlockDataValue::TValues::const_iterator cvalue =
        block->GetValues().begin();
    while ( cvalue != block->GetValues().end() ) {
        const CNamedDataValue* currvalue =
            dynamic_cast<const CNamedDataValue*>(cvalue->get());
        if ( !currvalue ) {
            cvalue->get()->Warning("named value expected");
            return false;
        }
        for (;;) {
            if ( member == GetMembers().end() ) {
                currvalue->Warning("unexpected value");
                return false;
            }
            if ( (*member)->GetName() == currvalue->GetName() )
                break;
            if ( !(*member)->Optional() ) {
                currvalue->GetValue().Warning((*member)->GetName() +
                                              " member expected");
                return false;
            }
            ++member;
        }
        if ( !(*member)->GetType()->CheckValue(currvalue->GetValue()) ) {
            return false;
        }
        ++member;
        ++cvalue;
    }
    while ( member != GetMembers().end() ) {
        if ( !(*member)->Optional() ) {
            value.Warning((*member)->GetName() + " member expected");
            return false;
        }
    }
    return true;
}


CDataMember::CDataMember(const string& name, const AutoPtr<CDataType>& type)
    : m_Name(name), m_Type(type), m_Optional(false),
    m_NoPrefix(false), m_Attlist(false), m_Notag(false), m_SimpleType(false)

{
    if ( m_Name.empty() ) {
        m_Notag = true;
/*
        string loc("Invalid identifier name in ASN.1 specification");
        if (type) {
            loc += " (line " + NStr::IntToString(type->GetSourceLine()) + ")";
        }
        NCBI_THROW(CDatatoolException,eInvalidData, loc);
*/
    }
    m_Type->SetDataMember(this);
}

CDataMember::~CDataMember(void)
{
}

void CDataMember::PrintASN(CNcbiOstream& out, int indent, bool last) const
{
    GetType()->PrintASNTypeComments(out, indent);
    bool oneLineComment = m_Comments.OneLine();
    if ( !oneLineComment )
        m_Comments.PrintASN(out, indent);
    out << GetName() << ' ';
    GetType()->PrintASN(out, indent);
    if ( GetDefault() ) {
        GetDefault()->PrintASN(out << " DEFAULT ", indent + 1);
    }
    else if ( Optional() ) {
        out << " OPTIONAL";
    }
    if ( !last )
        out << ',';
    if ( oneLineComment ) {
        out << ' ';
        m_Comments.PrintASN(out, indent, CComments::eOneLine);
    }
}

void CDataMember::PrintDTD(CNcbiOstream& out) const
{
    GetType()->PrintDTD(out, m_Comments);
}

void CDataMember::PrintXMLSchema(CNcbiOstream& out) const
{
    GetType()->PrintXMLSchema(out,m_Comments);
}

bool CDataMember::Check(void) const
{
    if ( !m_Type->Check() )
        return false;
    if ( !m_Default )
        return true;
    return GetType()->CheckValue(*m_Default);
}

void CDataMember::SetDefault(const AutoPtr<CDataValue>& value)
{
    m_Default = value;
}

void CDataMember::SetOptional(void)
{
    m_Optional = true;
}

void CDataMember::SetNoPrefix(void)
{
    m_NoPrefix = true;
}

void CDataMember::SetAttlist(void)
{
    m_Attlist = true;
}
void CDataMember::SetNotag(void)
{
    m_Notag = true;
}
void CDataMember::SetSimpleType(void)
{
    m_SimpleType = true;
}


END_NCBI_SCOPE
