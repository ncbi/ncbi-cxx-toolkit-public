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
*/

#include <ncbi_pch.hpp>
#include "exceptions.hpp"
#include "blocktype.hpp"
#include "unitype.hpp"
#include "reftype.hpp"
#include "statictype.hpp"
#include <serial/impl/autoptrinfo.hpp>
#include "value.hpp"
#include "classstr.hpp"
#include "module.hpp"
#include "srcutil.hpp"
#include <serial/impl/classinfo.hpp>
#include <typeinfo>
#include "aliasstr.hpp"

BEGIN_NCBI_SCOPE

class CAnyTypeClassInfo : public CClassTypeInfo
{
public:
    CAnyTypeClassInfo(const string& name, size_t count)
        : CClassTypeInfo(sizeof(AnyType) * count, name,
                         TObjectPtr(0), &CreateAnyTypeClass,
                         typeid(bool), &GetAnyTypeClassId)
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
    static TObjectPtr CreateAnyTypeClass(TTypeInfo objectType,
                                         CObjectMemoryPool* /*memoryPool*/)
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
    PrintASNTag(out);
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

void CDataMemberContainerType::PrintSpecDumpExtra(CNcbiOstream& out, int indent) const
{
    bool isAttlist = GetDataMember() && GetDataMember()->Attlist();
    if (!GetParentType() || !isAttlist) {
        ++indent;
    }
    ITERATE ( TMembers, i, m_Members ) {
        i->get()->PrintSpecDump(out, indent, isAttlist ? "A" : "F");
    }
    m_LastComments.PrintASN(out, indent, CComments::eNoEOL);
}

// XML schema generator submitted by
// Marc Dumontier, Blueprint initiative, dumontier@mshri.on.ca
// modified by Andrei Gourianov, gouriano@ncbi
void CDataMemberContainerType::PrintXMLSchema(CNcbiOstream& out,
    int indent, bool contents_only) const
{
    string tag = XmlTagName();
    string asnk = GetASNKeyword();
    string xsdk, tmp;
    bool hasAttlist= false, isAttlist= false;
    bool hasNotag= false, isOptionalMember= false, isOptionalContent= false;
    bool isSimple= false, isSimpleSeq= false, isSeq= false, isMixed=false;
    bool isSimpleContainer= false, parent_isSeq= false;
    bool defineAsType = false;
    string simpleType;
    list<string> opentag, closetag1, closetag2;
    CNcbiOstream* os = &out;
    CNcbiOstrstream otype;

    parent_isSeq = (dynamic_cast<const CUniSequenceDataType*>(GetParentType()) != 0);
    if (GetEnforcedStdXml()) {
        defineAsType = GetParentType() && IsReferenced() &&
            GetReferences().front()->IsRefToParent();

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
                    if (str != 0) {
                        isMixed = true;
                        break;
                    }
                }
            }
        }
        if (GetDataMember()) {
            isAttlist = GetDataMember()->Attlist();
            hasNotag   = GetDataMember()->Notag();
            isOptionalMember= GetDataMember()->Optional();
        }
        if (hasNotag && GetMembers().size()==1) {
            const CDataMember* member = GetMembers().front().get();
            isOptionalMember = member->Optional();
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
        if ((hasAttlist && GetMembers().size()==2) ||
            (!hasAttlist && GetMembers().size()==1)) {
            ITERATE ( TMembers, i, GetMembers() ) {
                if (i->get()->Attlist()) {
                    continue;
                }
                if (i->get()->SimpleType()) {
                    isSimple = true;
                    simpleType = i->get()->GetType()->GetSchemaTypeString();
                } else {
                    const CUniSequenceDataType* typeSeq =
                        dynamic_cast<const CUniSequenceDataType*>(i->get()->GetType());
                    bool any = (typeSeq != 0) &&
                        dynamic_cast<const CAnyContentDataType*>(
                            typeSeq->GetElementType()) != 0;
                    const CDataMemberContainerType* data =
                        dynamic_cast<const CDataMemberContainerType*>(i->get()->GetType());
                    isSimpleSeq = !any && (typeSeq != 0 || data != 0);
                    isSimpleContainer = data != 0;
                    if (isSimpleSeq) {
                        isSeq = typeSeq != 0;
                        const CDataMember *mem = i->get()->GetType()->GetDataMember();
                        if (mem) {
                            if (typeSeq) {
                                data = dynamic_cast<const CDataMemberContainerType*>(typeSeq->GetElementType());
                            }
                            if (data) {
                                asnk = data->GetASNKeyword();
                                ITERATE ( TMembers, m, data->m_Members ) {
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
                                isOptionalContent = mem->Optional();
                            } else {
                                isSeq = isSimpleSeq = false;
                            }
                        }
                    } else {
                        if (i->get()->Notag()) {
                            const CStringDataType* str =
                                dynamic_cast<const CStringDataType*>(i->get()->GetType());
                            if (str != 0) {
                                isMixed = true;
                            }
                        }
                    }
                }
            }
        }
    } else {
        if (GetDataMember()) {
            isOptionalMember= GetDataMember()->Optional();
        }
    }
    

    if (!isAttlist && !parent_isSeq) {
        if (!hasNotag) {
            if (!contents_only) {
                tmp = "<xs:element name=\"" + tag + "\"";
                if (isOptionalMember) {
                    tmp += " minOccurs=\"0\"";
                }
                if (IsNillable()) {
                    tmp += " nillable=\"true\"";
                }
                string tname;
                if (defineAsType) {
                    const CDataType* par = GetParentType();
                    while ( par->GetParentType() ) {
                        par = par->GetParentType();
                    }
                    tname = par->GetMemberName() + tag + "_Type";
                    tmp += " type=\"" + tname + "\"/>";
                    PrintASNNewLine(out, indent) << tmp;
                } else {
#if _DATATOOL_USE_SCHEMA_STYLE_COMMENTS
                    PrintASNNewLine(out, indent) << tmp;
                    if (!Comments().PrintSchemaComments(out, indent+1)) {
                        out << '>';
                    }
                    ++indent;
#else
                    tmp += ">";
                    opentag.push_back(tmp);
#endif
                    closetag2.push_front("</xs:element>");
                }

                tmp = "<xs:complexType";
                if (isMixed) {
                    tmp += " mixed=\"true\"";
                }
                if (defineAsType) {
                    tmp += " name=\"" + tname + "\"";
                    os = &otype;
                    indent = 0;
                }
                opentag.push_back(tmp + ">");
                closetag2.push_front("</xs:complexType>");
            }
        }
        if (!isSimple && !isSimpleContainer) {
            if (!contents_only || hasNotag) {
                if(NStr::CompareCase(asnk,"CHOICE")==0) {
                    xsdk = "choice";
                } else if(NStr::CompareCase(asnk,"SEQUENCE")==0) {
                    xsdk = "sequence";
                } else if(NStr::CompareCase(asnk,"SET")==0) {
                    xsdk = "all";
                }
                tmp = "<xs:" + xsdk;
                if (isOptionalContent || (hasNotag && isOptionalMember)) {
                    tmp += " minOccurs=\"0\"";
                }
                if (isSeq) {
                    tmp += " maxOccurs=\"unbounded\"";
                }
                opentag.push_back(tmp + ">");
                closetag1.push_front("</xs:" + xsdk + ">");
            }
        } else if (!simpleType.empty()) {
            opentag.push_back("<xs:simpleContent>");
            closetag2.push_front("</xs:simpleContent>");
            opentag.push_back("<xs:extension base=\"" + simpleType + "\">");
            closetag2.push_front("</xs:extension>");
        }
    }
    ITERATE ( list<string>, s, opentag ) {
        PrintASNNewLine(*os, indent++) << *s;
    }
    if (isAttlist) {
        ITERATE ( TMembers, i, m_Members ) {
            const CDataMember& member = **i;
            member.PrintXMLSchema(*os, indent);
        }
    } else if (!isSimple) {
        ITERATE ( TMembers, i, m_Members ) {
            const CDataMember& member = **i;
            if (member.Attlist()) {
                continue;
            }
            if (isMixed && member.Notag()) {
                if (dynamic_cast<const CStringDataType*>(member.GetType())) {
                    continue;
                }
            }
            member.PrintXMLSchema(*os, indent, isSimpleSeq);
        }
    }
    ITERATE ( list<string>, s, closetag1 ) {
        PrintASNNewLine(*os, --indent) << *s;
    }
    if (hasAttlist) {
        ITERATE ( TMembers, i, m_Members ) {
            const CDataMember& member = **i;
            if (member.Attlist()) {
                member.PrintXMLSchema(*os, indent);
            }
        }
    }
    ITERATE ( list<string>, s, closetag2 ) {
        PrintASNNewLine(*os, --indent) << *s;
    }
    if (defineAsType) {
        GetModule()->AddExtraSchemaOutput( CNcbiOstrstreamToString(otype) );
    }
    m_LastComments.PrintDTD(out, CComments::eMultiline);
}

void CDataMemberContainerType::PrintDTDElement(CNcbiOstream& out, bool contents_only) const
{
    string tag = XmlTagName();
    bool hasAttlist= false, isAttlist= false;
    bool isSimple= false, isSeq= false;

    if (GetEnforcedStdXml()) {
        ITERATE ( TMembers, i, m_Members ) {
            if (i->get()->Attlist()) {
                hasAttlist = true;
                break;
            }
        }
        if (GetDataMember()) {
            isAttlist = GetDataMember()->Attlist();
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
            (*i)->Comments().PrintDTD(out,CComments::eNoEOL);
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
            bool isOptional = member.Optional();
            if (GetEnforcedStdXml()) {
                if (member.Attlist()) {
                    need_separator = false;
                    continue;
                }
                if (member.Notag()) {
                    const CStaticDataType* statType = 
                        dynamic_cast<const CStaticDataType*>(member.GetType());
                    bool need_open = !statType;
                    bool need_newline = !need_open;
                    const CDataMemberContainerType* data =
                        dynamic_cast<const CDataMemberContainerType*>(member.GetType());
                    if (data) {
                        const CDataMember* data_member = data->GetMembers().front().get();
                        if (data_member && data_member->Notag() &&
                            data_member->GetType()->IsUniSeq()) {
                            isOptional = false;
                            need_open = false;
                            need_newline = false;
                        }
                    }
                    if (need_open) {
                        out << "(";
                    }
                    if (need_newline) {
                        out << "\n        ";
                    }
                    member.GetType()->PrintDTDElement(out,true);
                    if (need_open) {
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
                    if ( isOptional ) {
                        out << '*';
                    } else {
                        out << '+';
                    }
                } else {
                    if ( isOptional ) {
                        out << '?';
                    }
                }
            } else {
                if ( isOptional ) {
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
        ITERATE ( TMembers, i, m_Members ) {
            const CDataMember& member = **i;
            if (member.Attlist()) {
                member.GetComments().PrintDTD(out, CComments::eNoEOL);
                out << "\n<!ATTLIST " << tag;
                member.GetType()->PrintDTDElement(out);
                break;
            }
        }
        out << ">";
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

//check tags
// All tags must be different
    bool hasUntagged = false;
    set< pair< CAsnBinaryDefs::TLongTag, CAsnBinaryDefs::ETagClass> > tag_inuse;
    CAsnBinaryDefs::ETagType tagtype = GetTagType();
    bool ischoice = NStr::CompareCase(GetASNKeyword(),"CHOICE")==0;

    if (tagtype == CAsnBinaryDefs::eAutomatic ||
        ischoice) {

        ITERATE ( TMembers, i, m_Members ) {
            const CDataType* itype = (*i)->GetType();
            bool hastag = itype->HasTag();
            CAsnBinaryDefs::TLongTag itag = itype->GetTag();
            CAsnBinaryDefs::ETagClass tagclass = itype->GetTagClass();
            if (!hastag && itype->IsReference()) {
                const CDataType* rtype = itype->Resolve();
                if (rtype) {
                    hastag = rtype->HasTag();
                    itag = rtype->GetTag();
                    tagclass = rtype->GetTagClass();
                }
            }
            if (hastag) {
                pair< CAsnBinaryDefs::TLongTag, CAsnBinaryDefs::ETagClass>
                    tag = make_pair(itag, tagclass);
                if (tag_inuse.find(tag) != tag_inuse.end()) {
                    NCBI_THROW(CDatatoolException,eInvalidData,
                               "Duplicate tag: " + itype->IdName() +
                               " [" + NStr::NumericToString(itag) + "] in " +
                               GetModule()->GetName());
                }
                tag_inuse.insert(tag);
                if (hasUntagged) {
                    NCBI_THROW(CDatatoolException,eInvalidData,
                               "No explicit tag for " + itype->IdName() + " in " +
                               GetModule()->GetName());
                }
            }
            else {
                hasUntagged = true;
                if (!tag_inuse.empty()) {
                    NCBI_THROW(CDatatoolException,eInvalidData,
                               "No explicit tag for " + itype->IdName() + " in " +
                               GetModule()->GetName());
                }
            }
        }
    }
    return ok;
}

TObjectPtr CDataMemberContainerType::CreateDefault(const CDataValue& ) const
{
    NCBI_THROW(CDatatoolException,eNotImplemented,
                 GetASNKeyword() + string(" default not implemented"));
}

bool CDataMemberContainerType::UniElementNameExists(const string& name) const
{
    bool res = false;
    for (TMembers::const_iterator i = m_Members.begin();
        !res && i != m_Members.end(); ++i) {
        const CUniSequenceDataType* mem =
            dynamic_cast<const CUniSequenceDataType*>((*i)->GetType());
        if (mem != 0) {
            const CDataMemberContainerType* elem =
                dynamic_cast<const CDataMemberContainerType*>(mem->GetElementType());
            res = (elem != 0 && elem->GetMemberName() == name);
        }
    }
    return res;
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
        if ( mem->Optional() || mem->GetDefault() || mem->Nillable() )
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
        if ( mem->GetDefault() ) {
            TObjectPtr defPtr = memType->CreateDefault(*mem->GetDefault());
            memInfo->SetDefault(defPtr);
        }
        if ( mem->Optional() ) {
            memInfo->SetOptional();
        }
        if ( mem->Optional() || mem->GetDefault() || mem->Nillable() ) {
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
        if (dynamic_cast<const CAnyContentDataType*>(mem->GetType()) != 0) {
            memInfo->SetAnyContent();
        }
        if (mem->Nillable()) {
            memInfo->SetNillable();
        }
    }
    typeInfo->AssignItemsTags();
    if ( HaveModuleName() )
        typeInfo->SetModuleName(GetModule()->GetName());
    return typeInfo.release();
}

AutoPtr<CTypeStrings> CDataContainerType::GenerateCode(void) const
{
#if 0
    return GetFullCType();
#else
    string alias = GetVar("_fullalias");
    if (alias.empty()) {
        return GetFullCType();
    }
    const CDataType* aliastype = ResolveGlobal(alias);
    if (!aliastype) {
        NCBI_THROW(CDatatoolException,eWrongInput,
            "cannot create type info of _fullalias " + alias);
    }
    AutoPtr<CTypeStrings> dType = aliastype->GetRefCType();
    dType->SetDataType(aliastype);
    AutoPtr<CAliasTypeStrings> code(new CAliasTypeStrings(GlobalName(),
                                                          ClassName(),
                                                          *dType.release(),
                                                          Comments()));
    code->SetNamespaceName( GetNamespaceName());
    code->SetFullAlias();
    return AutoPtr<CTypeStrings>(code.release());
#endif
}

AutoPtr<CTypeStrings> CDataContainerType::GetFullCType(void) const
{
    AutoPtr<CClassTypeStrings> code(new CClassTypeStrings(
        GlobalName(), ClassName(), GetNamespaceName(), this, Comments()));
    return AddMembers(code);
}

AutoPtr<CTypeStrings> CDataContainerType::AddMembers(
    AutoPtr<CClassTypeStrings>& code) const
{
    bool isRootClass = GetParentType() == 0;
    bool haveUserClass = isRootClass;
/*
    bool isObject;
    if ( haveUserClass ) {
        isObject = true;
    }
    else {
        isObject = GetBoolVar("_object");
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

        bool delayed = GetBoolVar((*i)->GetName()+"._delay");
        AutoPtr<CTypeStrings> memberType = (*i)->GetType()->GetFullCType();
        string external_name = (*i)->GetName();
        string member_name = (*i)->GetType()->DefClassMemberName();
        if (member_name.empty()) {
            member_name = external_name;
        }
        code->AddMember(external_name, member_name, memberType,
                        (*i)->GetType()->GetVar("_pointer"),
                        optional, defaultCode, delayed,
                        (*i)->GetType()->GetTag(),
                        (*i)->NoPrefix(), (*i)->Attlist(), (*i)->Notag(),
                        (*i)->SimpleType(),(*i)->GetType(),false,
                        (*i)->Comments());
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
                                                          FileName(),
                                                          Comments()));
}

string CDataContainerType::GetSpecKeyword(void) const
{
    bool hasAttlist = !m_Members.empty() && m_Members.front()->Attlist();
    if (( hasAttlist && m_Members.size() == 2) ||
        (!hasAttlist && m_Members.size() == 1)) {
        const CDataMember* member = m_Members.back().get();
        if (!GetParentType() && (member->SimpleType() || member->Notag())) {
            return member->GetType()->GetSpecKeyword();
        }
    }
    return GetASNKeyword();
}


const char* CDataSetType::GetASNKeyword(void) const
{
    return "SET";
}

const char* CDataSetType::GetDEFKeyword(void) const
{
    return "_SET_";
}

bool CDataSetType::CheckValue(const CDataValue& value) const
{
    const CBlockDataValue* block =
        dynamic_cast<const CBlockDataValue*>(&value);
    if ( !block ) {
        value.Warning("block of values expected", 2);
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
            v->get()->Warning("named value expected", 3);
            return false;
        }
        TReadValues::iterator member = mms.find(currvalue->GetName());
        if ( member == mms.end() ) {
            currvalue->Warning("unexpected member", 4);
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
            value.Warning(member->first + " member expected", 5);
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

const char* CDataSequenceType::GetDEFKeyword(void) const
{
    return "_SEQUENCE_";
}

bool CDataSequenceType::CheckValue(const CDataValue& value) const
{
    const CBlockDataValue* block =
        dynamic_cast<const CBlockDataValue*>(&value);
    if ( !block ) {
        value.Warning("block of values expected", 6);
        return false;
    }
    TMembers::const_iterator member = GetMembers().begin();
    CBlockDataValue::TValues::const_iterator cvalue =
        block->GetValues().begin();
    while ( cvalue != block->GetValues().end() ) {
        const CNamedDataValue* currvalue =
            dynamic_cast<const CNamedDataValue*>(cvalue->get());
        if ( !currvalue ) {
            cvalue->get()->Warning("named value expected", 7);
            return false;
        }
        for (;;) {
            if ( member == GetMembers().end() ) {
                currvalue->Warning("unexpected value", 8);
                return false;
            }
            if ( (*member)->GetName() == currvalue->GetName() )
                break;
            if ( !(*member)->Optional() ) {
                currvalue->GetValue().Warning((*member)->GetName() +
                                              " member expected", 9);
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
            value.Warning((*member)->GetName() + " member expected", 10);
            return false;
        }
    }
    return true;
}

AutoPtr<CTypeStrings> CWsdlDataType::GetFullCType(void) const
{
    AutoPtr<CClassTypeStrings> code(new CWsdlTypeStrings(
        GlobalName(), ClassName(), GetNamespaceName(), this, Comments()));
    code->SetHaveTypeInfo(false);
    return AddMembers(code);
}


CDataMember::CDataMember(const string& name, const AutoPtr<CDataType>& type)
    : m_Name(name), m_Type(type), m_Optional(false),
    m_NoPrefix(false), m_Attlist(false), m_Notag(false), m_SimpleType(false),
    m_Nillable(false)
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
    out << CDataTypeModule::ToAsnId(GetName()) << ' ';
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

void CDataMember::PrintSpecDump(CNcbiOstream& out, int indent, const char* tag) const
{
    if (!SimpleType()) {
        const CDataType* type = GetType();
        if (!Attlist()) {
            bool needTitle= !Notag() || (type->IsStdType() || type->IsPrimitive());
            if (!needTitle) {
                const CDataMemberContainerType* cont =
                    dynamic_cast<const CDataMemberContainerType*>(type);
                if (cont) {
                    needTitle=true;
                    const CDataMemberContainerType::TMembers& members = cont->GetMembers();
                    bool hasAttlist = !members.empty() && members.front()->Attlist();
                    if (( hasAttlist && members.size() == 2) ||
                        (!hasAttlist && members.size() == 1)) {
                        const CDataMember* member = members.back().get();
                        needTitle = !member->GetType()->IsUniSeq();
                        if (!needTitle) {
                            --indent;
                        }
                    }
                } else if (type->IsUniSeq()) {
                    const CDataType* parent = type->GetParentType();
                    needTitle = parent->GetDataMember() != 0;
                    if (!needTitle && parent->IsContainer()) {
                        cont = dynamic_cast<const CDataMemberContainerType*>(parent);
                        needTitle = cont->GetMembers().size() == 1;
                    }
                    if (!needTitle) {
                        const CUniSequenceDataType* uni =
                            dynamic_cast<const CUniSequenceDataType*>(type);
                        if (uni->GetElementType()->IsContainer()) {
                            --indent;
                        }
                    }
                }
            }
            if (needTitle) {
                PrintASNNewLine(out, indent);
                out << tag << ',' <<
                       type->GetSourceLine() <<",";
                out << type->GetFullName() << ',' << type->GetSpecKeyword();
                if ( GetDefault() ) {
                    GetDefault()->PrintASN(out << ",DEFAULT,", indent + 1);
                }
                else if ( Optional() ) {
                    out << ",OPTIONAL";
                }
                type->Comments().PrintASN(out, indent,CComments::eNoEOL);
                m_Comments.PrintASN(out, indent,CComments::eNoEOL);
            }
        }
        type->PrintSpecDump(out, indent);
    }
}

void CDataMember::PrintXMLSchema(CNcbiOstream& out, int indent, bool contents_only) const
{
#if !_DATATOOL_USE_SCHEMA_STYLE_COMMENTS
    m_Comments.PrintDTD(out, CComments::eNoEOL); 
#endif
    GetType()->PrintXMLSchema(out, indent, contents_only);
}

void CDataMember::PrintDTD(CNcbiOstream& out) const
{
    GetType()->PrintDTD(out, m_Comments);
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
void CDataMember::SetNillable(void)
{
    m_Nillable = true;
}

END_NCBI_SCOPE
