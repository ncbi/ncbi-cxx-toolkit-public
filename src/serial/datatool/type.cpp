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
*   Base class for type description
*/

#include <ncbi_pch.hpp>
#include "type.hpp"
#include <serial/impl/autoptrinfo.hpp>
#include <serial/impl/aliasinfo.hpp>
#include "value.hpp"
#include "blocktype.hpp"
#include "module.hpp"
#include "classstr.hpp"
#include "aliasstr.hpp"
#include "exceptions.hpp"
#include "reftype.hpp"
#include "unitype.hpp"
#include "choicetype.hpp"
#include "statictype.hpp"
#include "enumtype.hpp"
#include "fileutil.hpp"
#include "srcutil.hpp"
#include <serial/error_codes.hpp>
#include <algorithm>


#define NCBI_USE_ERRCODE_X   Serial_DTType

BEGIN_NCBI_SCOPE


bool CDataType::sm_EnableDTDEntities = false;
bool CDataType::sm_EnforcedStdXml = false;
bool CDataType::sm_XmlSourceSpec = false;
set<string> CDataType::sm_SavedNames;
map<string,string> CDataType::sm_ClassToMember;

class CAnyTypeSource : public CTypeInfoSource
{
public:
    CAnyTypeSource(CDataType* type)
        : m_Type(type)
        {
        }

    TTypeInfo GetTypeInfo(void);

private:
    CDataType* m_Type;
};

TTypeInfo CAnyTypeSource::GetTypeInfo(void)
{
    return m_Type->GetAnyTypeInfo();
}

CDataType::CDataType(void)
    : m_ParentType(0), m_Module(0), m_SourceLine(0),
      m_DataMember(0), m_TypeStr(0), m_Set(0), m_Choice(0), m_Checked(false),
      m_Tag(eNoExplicitTag), m_IsAlias(false), m_NsQualified(eNSQNotSet)
{
}

CDataType::~CDataType()
{
// NOTE:  This compiler bug was fixed by Jan 24 2002, test passed with:
//           CC: Sun WorkShop 6 update 2 C++ 5.3 Patch 111685-03 2001/10/19
//        We leave the workaround here for maybe half a year (for other guys).
#if defined(NCBI_COMPILER_WORKSHOP)
// We have to use two #if's here because KAI C++ cannot handle #if foo == bar
#  if (NCBI_COMPILER_VERSION == 530)
    // BW_010::  to workaround (already reported to SUN, CASE ID 62563729)
    //           internal bug of the SUN Forte 6 Update 1 and Update 2 compiler
    (void) atoi("5");
#  endif
#endif
}

void CDataType::SetSourceLine(int line)
{
    m_SourceLine = line;
}

void CDataType::Warning(const string& mess, int err_subcode) const
{
    CNcbiDiag() << ErrCode(NCBI_ERRCODE_X, err_subcode)
                << LocationString() << ": " << mess;
}

void CDataType::PrintASNTypeComments(CNcbiOstream& out,
    int indent, int flags) const
{
    m_Comments.PrintASN(out, indent, flags);
}

void CDataType::PrintDTDTypeComments(CNcbiOstream& out, int /*indent*/) const
{
    m_Comments.PrintDTD(out, CComments::eNoEOL);
}

const char* CDataType::GetASNKeyword(void) const
{
    return "";
}

void CDataType::PrintSpecDump(CNcbiOstream& out, int indent) const
{
    if (!GetParentType()) {
        PrintASNNewLine(out, indent);
        out << 'T' << ',' <<
            GetSourceLine() <<",";
        out << GetFullName() << ',' << GetSpecKeyword();
        m_Comments.PrintASN(out, indent, CComments::eNoEOL);
    }
    PrintSpecDumpExtra(out, indent);
    if (!GetParentType()) {
        PrintASNNewLine(out, indent);
    }
}

void CDataType::PrintSpecDumpExtra(CNcbiOstream& out, int indent) const
{
}

string CDataType::GetSpecKeyword(void) const
{
    return GetASNKeyword();
}

string CDataType::GetSchemaTypeString(void) const
{
    return kEmptyStr;
}

void CDataType::PrintDTD(CNcbiOstream& out) const
{
    if (x_IsSavedName(XmlTagName())) {
        return;
    }
    out << "\n\n";
    m_Comments.PrintDTD(out, CComments::eOneLine);
    PrintDTDElement(out);
    x_AddSavedName(XmlTagName());
    PrintDTDExtra(out);
}

void CDataType::PrintDTD(CNcbiOstream& out,
                         const CComments& extra) const
{
    if (m_DataMember && (m_DataMember->Attlist() || m_DataMember->Notag())) {
        return;
    }
    if (x_IsSavedName(XmlTagName())) {
        return;
    }
    out << "\n";
    if (IsReference() && extra.Empty()) {
        const CDataType* realType = Resolve();
        if (realType && realType != this) {
            realType->m_Comments.PrintDTD(out, CComments::eOneLine);
        }
    }
    m_Comments.PrintDTD(out, CComments::eOneLine);
    extra.PrintDTD(out, CComments::eNoEOL);
    PrintDTDElement(out);
    x_AddSavedName(XmlTagName());
    PrintDTDExtra(out);
}

void CDataType::PrintDTDExtra(CNcbiOstream& /*out*/) const
{
}

void CDataType::SetInSet(const CUniSequenceDataType* sequence)
{
    _ASSERT(GetParentType() == sequence);
    m_Set = sequence;
}

void CDataType::SetInChoice(const CChoiceDataType* choice)
{
    _ASSERT(GetParentType() == choice);
    m_Choice = choice;
}

void CDataType::AddReference(const CReferenceDataType* reference)
{
    _ASSERT(GetParentType() == 0 || reference->IsRefToParent());
    if ( !m_References )
        m_References = new TReferences;
    m_References->push_back(reference);
}

bool CDataType::IsInUniSeq(void) const
{
    return dynamic_cast<const CUniSequenceDataType*>(GetParentType()) != 0;
}

bool CDataType::IsUniSeq(void) const
{
    return dynamic_cast<const CUniSequenceDataType*>(this) != 0;
}

bool CDataType::IsContainer(void) const
{
    return dynamic_cast<const CDataMemberContainerType*>(this) != 0;
}

bool CDataType::IsEnumType(void) const
{
    return dynamic_cast<const CEnumDataType*>(this) != 0;
}

void CDataType::SetParent(const CDataType* parent, const string& memberName,
                          string xmlName)
{
    _ASSERT(parent != 0);
    _ASSERT(m_ParentType == 0 && m_Module == 0 && m_MemberName.empty());
    m_ParentType = parent;
    m_Module = parent->GetModule();
    x_SetMemberAndClassName( memberName);
    m_XmlName = xmlName;
    _ASSERT(m_Module != 0);
    if (m_DataMember && m_DataMember->GetDefault()) {
        m_DataMember->GetDefault()->SetModule(m_Module);
    }
    FixTypeTree();
}

void CDataType::SetParent(const CDataTypeModule* module,
                          const string& typeName)
{
    _ASSERT(module != 0);
    _ASSERT(m_ParentType == 0 && m_Module == 0 && m_MemberName.empty());
    m_Module = module;
    x_SetMemberAndClassName( typeName );
    FixTypeTree();
}

void CDataType::FixTypeTree(void) const
{
}

bool CDataType::Check(void)
{
    if ( m_Checked )
        return true;
    m_Checked = true;
    _ASSERT(m_Module != 0);
    return CheckType();
}

bool CDataType::CheckType(void) const
{
    return true;
}

const string CDataType::GetVar(const string& varName, int collect /*= 0*/) const
{
    const CDataType* parent = GetParentType();
    if (collect >=0 && GetDataMember()) {
#if 0
        collect = GetDataMember()->Notag() ? 1 : 0;
#else
        collect = GetDataMember()->GetType()->IsPrimitive() ? -1 : 1;
#endif
    }
    if ( !parent ) {
        return GetModule()->GetVar(m_MemberName, varName, collect > 0);
    }
    else {
        string s;
        if (IsUniSeq() && m_MemberName == parent->GetMemberName()) {
            s = parent->GetVar(varName, collect);
        } else {
            s = parent->GetVar(m_MemberName + '.' + varName, collect);
        }
        if (!s.empty()) {
            return s;
        }
        s = string(GetDEFKeyword()) + '.' + varName;
        return parent->GetVar(s, -1);
    }
}

bool CDataType::GetBoolVar(const string& varName, bool default_value) const
{
    string value = GetVar(varName);
    NStr::TruncateSpacesInPlace(value);
    if ( value.empty() ) {
        return default_value;
    }
    try {
        return NStr::StringToBool(value);
    }
    catch ( CException& /*ignored*/ ) {
    }
    try {
        return NStr::StringToInt(value) != 0;
    }
    catch ( CException& /*ignored*/ ) {
    }
    return default_value;
}

void  CDataType::ForbidVar(const string& var, const string& value)
{
    typedef multimap<string, string> TMultimap;
    if (!var.empty() && !value.empty()) {
        TMultimap::const_iterator it = m_ForbidVar.find(var);
        for ( ; it != m_ForbidVar.end() && it->first == var; ++it) {
            if (it->second == value) {
                return;
            }
        }
        m_ForbidVar.insert(TMultimap::value_type(var, value));
    }
}

void  CDataType::AllowVar(const string& var, const string& value)
{
    if (!var.empty() && !value.empty()) {
        multimap<string,string>::iterator it = m_ForbidVar.find(var);
        for ( ; it != m_ForbidVar.end() && it->first == var; ++it) {
            if (it->second == value) {
                m_ForbidVar.erase(it);
                return;
            }
        }
    }
}

const string CDataType::GetAndVerifyVar(const string& var) const
{
    const string tmp = GetVar(var);
    if (!tmp.empty()) {
        multimap<string,string>::const_iterator it = m_ForbidVar.find(var);
        for ( ; it != m_ForbidVar.end() && it->first == var; ++it) {
            if (it->second == tmp) {
                NCBI_THROW(CDatatoolException,eForbidden,
                    IdName()+": forbidden "+var+"="+tmp);
            }
        }
    }
    return tmp;
}

const string& CDataType::GetSourceFileName(void) const
{
    if (m_Module) {
        return GetModule()->GetSourceFileName();
    } else {
        return kEmptyStr;
    }
}

string CDataType::LocationString(void) const
{
    return GetSourceFileName() + ':' + NStr::IntToString(GetSourceLine()) +
        ": " + IdName(); 
}

string CDataType::IdName(void) const
{
    const CDataType* parent = GetParentType();
    if ( !parent ) {
        // root type
        return m_MemberName;
    }
    else {
        // member
        return parent->IdName() + '.' + m_MemberName;
    }
}

string CDataType::XmlTagName(void) const
{
    if (GetEnforcedStdXml()) {
        return m_MemberName;
    }
    const CDataType* parent = GetParentType();
    if ( !parent ) {
        // root type
        return m_MemberName;
    }
    else {
        // member
        return parent->XmlTagName() + '_' +
            (m_XmlName.empty() ? m_MemberName : m_XmlName);
    }
}

const string& CDataType::GlobalName(void) const
{
    if ( !GetParentType() ) {
        return m_MemberName;
    } else {
        const CDataMember* m = GetDataMember();
        if (!m) {
            const CUniSequenceDataType* seq = 
                dynamic_cast<const CUniSequenceDataType*>(GetParentType());
            if (seq) {
                m = seq->GetDataMember();
            }
        }
        if (m && m->NoPrefix() && !m->Attlist() && !m->Notag()) {
            m = GetParentType()->GetDataMember();
            if (!m) {
                return GetDataMember()->GetName();
            } else if (m->NoPrefix() && !m->Attlist()) {
                if (IsInUniSeq()) {
                    return m->GetName();
                } else {
                    return m_MemberName;
                }
            }
        }
        return NcbiEmptyString;
    }
}

string CDataType::GetKeyPrefix(void) const
{
    const CDataType* parent = GetParentType();
    if ( !parent ) {
        // root type
        return NcbiEmptyString;
    }
    else {
        // member
        string parentPrefix = parent->GetKeyPrefix();
        if ( parentPrefix.empty() )
            return m_MemberName;
        else
            return parentPrefix + '.' + m_MemberName;
    }
}

bool CDataType::Skipped(void) const
{
    return GetVar("_class") == "-";
}

string CDataType::DefClassMemberName(void) const
{
    string cls;
    // only for local types
    if ( GetParentType() ) {
        cls = GetVar("_class");
        if ( !cls.empty() ) {
            if (cls[0] == 'C') {
                cls.erase(0,1);
            }
            if (cls[0]=='_') {
                cls.erase(0,1);
            }
        }
    }
    return cls;
}

void CDataType::x_SetMemberAndClassName(const string& memberName)
{
    m_MemberName = memberName;

    if ( GetParentType() ) {
        // local type
        if ( m_MemberName == "E" ) {
            m_ClassName = "E";
            for ( const CDataType* type = GetParentType(); type; type = type->GetParentType() ) {
                const CDataType* parent = type->GetParentType();
                if ( !parent )
                    break;
                if ( dynamic_cast<const CDataMemberContainerType*>(parent) ) {
                    m_ClassName += "_";
                    m_ClassName += Identifier(type->m_MemberName);
                    break;
                }
            }
            m_ClassName = "C_" + m_ClassName;
        }
        else {
            m_ClassName = "C_"+Identifier(m_MemberName);
        }

        const CDataType* parent = GetParentType();
        if (parent->IsUniSeq()) {
            parent = parent->GetParentType();
        }
        if (parent && parent->m_ClassName == m_ClassName) {
            m_ClassName += '_';
        }
    }
}

string CDataType::ClassName(void) const
{
    const string cls = GetVar("_class");
    if ( !cls.empty() )
        return cls;
    if ( GetParentType() ) {
        // local type
//        return "C_"+Identifier(m_MemberName);
        return m_ClassName;
    }
    else {
        // global type
#if 0
        return 'C'+Identifier(m_MemberName);
#else
        string g;
        for (size_t i=0;;++i) {
            g.assign(1,'C').append(i,'_').append(Identifier(m_MemberName));
            if (sm_ClassToMember.find(g) == sm_ClassToMember.end()) {
                sm_ClassToMember[g] = m_MemberName;
            } else {
                if ( sm_ClassToMember[g] != m_MemberName) {
                    continue;
                }
            }
            return g;
        }
#endif
    }
}

string CDataType::FileName(void) const
{
    if ( GetParentType() ) {
        return GetParentType()->FileName();
    }
    if ( m_CachedFileName.empty() ) {
        const string file = GetVar("_file");
        if ( !file.empty() ) {
            m_CachedFileName = file;
        }
        else {
            string dir = GetVar("_dir");
            if ( dir.empty() ) {
                _ASSERT(!GetParentType()); // for non internal classes
                dir = GetModule()->GetFileNamePrefix();
            }
            m_CachedFileName =
                Path(dir,
                     MakeFileName(m_MemberName, 5 /* strlen("_.cpp") */ ));
        }
    }
    return m_CachedFileName;
}

const CNamespace& CDataType::Namespace(void) const
{
    if ( !m_CachedNamespace.get() ) {
        const string ns = GetVar("_namespace");
        if ( !ns.empty() ) {
            m_CachedNamespace.reset(new CNamespace(ns));
        }
        else {
            if ( GetParentType() ) {
                return GetParentType()->Namespace();
            }
            else {
                return GetModule()->GetNamespace();
            }
        }
    }
    return *m_CachedNamespace;
}

string CDataType::InheritFromClass(void) const
{
    return GetVar("_parent_class");
}

const CDataType* CDataType::InheritFromType(void) const
{
    const string parentName = GetVar("_parent_type");
    if ( !parentName.empty() )
        return ResolveGlobal(parentName);
    return 0;
}

CDataType* CDataType::Resolve(void)
{
    return this;
}

const CDataType* CDataType::Resolve(void) const
{
    return this;
}

CDataType* CDataType::ResolveLocal(const string& name) const
{
    return GetModule()->Resolve(name);
}

CDataType* CDataType::ResolveGlobal(const string& name) const
{
    SIZE_TYPE dot = name.find('.');
    if ( dot == NPOS ) {
        // no module specified
        return GetModule()->Resolve(name);
    }
    else {
        // resolve by module
        string moduleName = name.substr(0, dot);
        string typeName = name.substr(dot + 1);
        return GetModule()->GetModuleContainer().InternalResolve(moduleName,
                                                                 typeName);
    }
}

CTypeRef CDataType::GetTypeInfo(void)
{
    if ( !m_TypeRef )
        m_TypeRef = CTypeRef(new CAnyTypeSource(this));
    return m_TypeRef;
}

TTypeInfo CDataType::GetAnyTypeInfo(void)
{
    TTypeInfo typeInfo = m_AnyTypeInfo.get();
    if ( !typeInfo ) {
        typeInfo = GetRealTypeInfo();
        if ( NeedAutoPointer(typeInfo) ) {
            if (IsAlias() && !IsStdType()) {
                CTypeInfo *alias =
                    new CAliasTypeInfo(GlobalName(),typeInfo);
                alias->SetModuleName( typeInfo->GetModuleName());
                typeInfo = alias;
            }
            m_AnyTypeInfo.reset(new CAutoPointerTypeInfo(typeInfo));
            typeInfo = m_AnyTypeInfo.get();
        }
    }
    return typeInfo;
}

bool CDataType::NeedAutoPointer(TTypeInfo typeInfo) const
{
    return typeInfo->GetSize() > sizeof(AnyType);
}

TTypeInfo CDataType::GetRealTypeInfo(void)
{
    TTypeInfo typeInfo = m_RealTypeInfo.get();
    if ( !typeInfo ) {
        m_RealTypeInfo.reset(CreateTypeInfo());
        typeInfo = m_RealTypeInfo.get();
    }
    return typeInfo;
}

CTypeInfo* CDataType::CreateTypeInfo(void)
{
    NCBI_THROW(CDatatoolException,eIllegalCall,
        "cannot create type info of "+IdName());
}

CTypeInfo* CDataType::UpdateModuleName(CTypeInfo* typeInfo) const
{
    if ( HaveModuleName() )
        typeInfo->SetModuleName(GetModule()->GetName());
    return typeInfo;
}

AutoPtr<CTypeStrings> CDataType::GenerateCode(void) const
{
    if ( !IsAlias() ) {
        AutoPtr<CClassTypeStrings> code(new CClassTypeStrings(GlobalName(),
                                                              ClassName(),
                                                              GetNamespaceName(),
                                                              this,
                                                              Comments()));
        AutoPtr<CTypeStrings> dType = GetFullCType();
        bool nonempty = false, noprefix = false;
        const CUniSequenceDataType* uniseq =
            dynamic_cast<const CUniSequenceDataType*>(this);
        if (uniseq) {
            nonempty = uniseq->IsNonEmpty();
        }
        noprefix = GetXmlSourceSpec();
        code->AddMember(dType, GetTag(), nonempty, noprefix);
        SetParentClassTo(*code);
        return AutoPtr<CTypeStrings>(code.release());
    }
    else {
        AutoPtr<CTypeStrings> dType = GetFullCType();
        AutoPtr<CAliasTypeStrings> code(new CAliasTypeStrings(GlobalName(),
                                                              ClassName(),
                                                              *dType.release(),
                                                              Comments()));
        code->SetNamespaceName( GetNamespaceName());
        return AutoPtr<CTypeStrings>(code.release());
    }
}

void CDataType::SetParentClassTo(CClassTypeStrings& code) const
{
    const CDataType* parent = InheritFromType();
    if ( parent ) {
        code.SetParentClass(parent->ClassName(),
                            parent->Namespace(),
                            parent->FileName());
    }
    else {
        string parentClassName = InheritFromClass();
        if ( !parentClassName.empty() ) {
            SIZE_TYPE pos = parentClassName.rfind("::");
            if ( pos != NPOS ) {
                code.SetParentClass(parentClassName.substr(pos + 2),
                                    CNamespace(parentClassName.substr(0, pos)),
                                    NcbiEmptyString);
            }
            else {
                code.SetParentClass(parentClassName,
                                    CNamespace::KEmptyNamespace,
                                    NcbiEmptyString);
            }
        }
    }
}

AutoPtr<CTypeStrings> CDataType::GetRefCType(void) const
{
    if ( !IsAlias() ) {
        return AutoPtr<CTypeStrings>(new CClassRefTypeStrings(ClassName(),
                                                              Namespace(),
                                                              FileName(),
                                                              Comments()));
    }
    else {
        AutoPtr<CTypeStrings> dType = GetFullCType();
        AutoPtr<CAliasRefTypeStrings> code(new CAliasRefTypeStrings(ClassName(),
                                                                    Namespace(),
                                                                    FileName(),
                                                                    *dType.release(),
                                                                    Comments()));
        return AutoPtr<CTypeStrings>(code.release());
    }
}

AutoPtr<CTypeStrings> CDataType::GetFullCType(void) const
{
    NCBI_THROW(CDatatoolException,eInvalidData,
        LocationString() + ": C++ type undefined");
}

string CDataType::GetDefaultString(const CDataValue& ) const
{
    Warning("Default is not supported by this type", 3);
    return "...";
}

bool CDataType::IsPrimitive(void) const
{
    const CStaticDataType* st = dynamic_cast<const CStaticDataType*>(this);
    if (st) {
        const CBoolDataType* b = dynamic_cast<const CBoolDataType*>(this);
        if (b) {
            return true;
        }
        const CIntDataType* i = dynamic_cast<const CIntDataType*>(this);
        if (i) {
            return true;
        }
        const CRealDataType* r = dynamic_cast<const CRealDataType*>(this);
        if (r) {
            return true;
        }
    }
    const CEnumDataType* e = dynamic_cast<const CEnumDataType*>(this);
    if (e) {
        return true;
    }
    return false;
}

bool CDataType::IsStdType(void) const
{
    // Primitive (except enums) or string
    const CStaticDataType* st = dynamic_cast<const CStaticDataType*>(this);
    if (st) {
        const CBoolDataType* b = dynamic_cast<const CBoolDataType*>(this);
        if (b) {
            return true;
        }
        const CIntDataType* i = dynamic_cast<const CIntDataType*>(this);
        if (i) {
            return true;
        }
        const CRealDataType* r = dynamic_cast<const CRealDataType*>(this);
        if (r) {
            return true;
        }
        const COctetStringDataType* o =
            dynamic_cast<const COctetStringDataType*>(this);
        if (o) {
            return true;
        }
        const CStringDataType* s = dynamic_cast<const CStringDataType*>(this);
        if (s) {
            return true;
        }
    }
    return false;
}

bool CDataType::IsReference(void) const
{
    return dynamic_cast<const CReferenceDataType*>(this) != 0;
}

bool CDataType::x_IsSavedName(const string& name)
{
    return sm_EnforcedStdXml &&
        sm_SavedNames.find(name) != sm_SavedNames.end();
}

void CDataType::x_AddSavedName(const string& name)
{
    if (sm_EnforcedStdXml) {
        sm_SavedNames.insert(name);
    }
}

const char* CDataType::GetDEFKeyword(void) const
{
    return "-";
}

string CDataType::GetFullName(void) const
{
    string name;
    const CDataType* parent = GetParentType();
    if (parent) {
        name = parent->GetFullName();
        if (!name.empty() && name[name.size()-1] != ':') {
            name += ':';
        }
    }
    bool notag = false;
    if (GetDataMember()) {
        notag = GetDataMember()->Attlist() || GetDataMember()->Notag();
        if (GetDataMember()->Notag()) {
            bool special=false;
            const CDataMemberContainerType* cont =
                dynamic_cast<const CDataMemberContainerType*>(this);
            if (cont) {
                special=true;
                const CDataMemberContainerType::TMembers& members = cont->GetMembers();
                bool hasAttlist = !members.empty() && members.front()->Attlist();
                if (( hasAttlist && members.size() == 2) ||
                    (!hasAttlist && members.size() == 1)) {
                    const CDataMember* member = members.back().get();
                    special = !member->GetType()->IsUniSeq();
                }
            } else if (IsUniSeq()) {
                const CDataType* parent = GetParentType();
                special = parent->GetDataMember() != 0;
                if (!special && parent->IsContainer()) {
                    cont = dynamic_cast<const CDataMemberContainerType*>(parent);
                    special = cont->GetMembers().size() == 1;
                }
            }
            if (special) {
                name += '[' + GetMemberName() + ']';
            }
        }
    } else {
        notag = IsInUniSeq();
    }
    if (!notag) {
        name += GetMemberName();
    }
    return name;
}

END_NCBI_SCOPE
