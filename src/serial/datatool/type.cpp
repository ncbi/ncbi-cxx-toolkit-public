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
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.57  2000/11/29 17:42:45  vasilche
* Added CComment class for storing/printing ASN.1/XML module comments.
* Added srcutil.hpp file to reduce file dependancy.
*
* Revision 1.56  2000/11/20 17:26:33  vasilche
* Fixed warnings on 64 bit platforms.
* Updated names of config variables.
*
* Revision 1.55  2000/11/15 20:34:56  vasilche
* Added user comments to ENUMERATED types.
* Added storing of user comments to ASN.1 module definition.
*
* Revision 1.54  2000/11/14 21:41:26  vasilche
* Added preserving of ASN.1 definition comments.
*
* Revision 1.53  2000/11/07 17:26:26  vasilche
* Added module names to CTypeInfo and CEnumeratedTypeValues
* Added possibility to set include directory for whole module
*
* Revision 1.52  2000/09/26 17:38:27  vasilche
* Fixed incomplete choiceptr implementation.
* Removed temporary comments.
*
* Revision 1.51  2000/08/25 15:59:24  vasilche
* Renamed directory tool -> datatool.
*
* Revision 1.50  2000/07/03 20:47:28  vasilche
* Removed unused variables/functions.
*
* Revision 1.49  2000/06/27 16:34:47  vasilche
* Fixed generated comments.
* Fixed class names conflict. Now internal classes' names begin with "C_".
*
* Revision 1.48  2000/05/24 20:09:30  vasilche
* Implemented DTD generation.
*
* Revision 1.47  2000/05/03 14:38:18  vasilche
* SERIAL: added support for delayed reading to generated classes.
* DATATOOL: added code generation for delayed reading.
*
* Revision 1.46  2000/04/17 19:11:09  vasilche
* Fixed failed assertion.
* Removed redundant namespace specifications.
*
* Revision 1.45  2000/04/12 15:36:52  vasilche
* Added -on <namespace> argument to datatool.
* Removed unnecessary namespace specifications in generated files.
*
* Revision 1.44  2000/04/07 19:26:35  vasilche
* Added namespace support to datatool.
* By default with argument -oR datatool will generate objects in namespace
* NCBI_NS_NCBI::objects (aka ncbi::objects).
* Datatool's classes also moved to NCBI namespace.
*
* Revision 1.43  2000/03/29 15:52:27  vasilche
* Generated files names limited to 31 symbols due to limitations of Mac.
* Removed unions with only one member.
*
* Revision 1.42  2000/03/07 14:06:33  vasilche
* Added generation of reference counted objects.
*
* Revision 1.41  2000/02/17 20:05:07  vasilche
* Inline methods now will be generated in *_Base.inl files.
* Fixed processing of StringStore.
* Renamed in choices: Selected() -> Which(), E_choice -> E_Choice.
* Enumerated values now will preserve case as in ASN.1 definition.
*
* Revision 1.40  2000/02/01 21:48:07  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
* Changed class generation.
* Moved datatool headers to include/internal/serial/tool.
*
* Revision 1.39  2000/01/10 19:46:46  vasilche
* Fixed encoding/decoding of REAL type.
* Fixed encoding/decoding of StringStore.
* Fixed encoding/decoding of NULL type.
* Fixed error reporting.
* Reduced object map (only classes).
*
* Revision 1.38  1999/12/29 16:01:51  vasilche
* Added explicit virtual destructors.
* Resolved overloading of InternalResolve.
*
* Revision 1.37  1999/12/28 18:56:00  vasilche
* Reduced size of compiled object files:
* 1. avoid inline or implicit virtual methods (especially destructors).
* 2. avoid std::string's methods usage in inline methods.
* 3. avoid string literals ("xxx") in inline methods.
*
* Revision 1.36  1999/12/21 17:18:37  vasilche
* Added CDelayedFostream class which rewrites file only if contents is changed.
*
* Revision 1.35  1999/12/20 21:00:19  vasilche
* Added generation of sources in different directories.
*
* Revision 1.34  1999/12/08 19:06:24  vasilche
* Fixed uninitialized variable.
*
* Revision 1.33  1999/12/03 21:42:13  vasilche
* Fixed conflict of enums in choices.
*
* Revision 1.32  1999/12/01 17:36:27  vasilche
* Fixed CHOICE processing.
*
* Revision 1.31  1999/11/16 15:41:17  vasilche
* Added plain pointer choice.
* By default we use C pointer instead of auto_ptr.
* Start adding initializers.
*
* Revision 1.30  1999/11/15 19:36:19  vasilche
* Fixed warnings on GCC
*
* ===========================================================================
*/

#include <serial/datatool/type.hpp>
#include <serial/autoptrinfo.hpp>
#include <serial/datatool/value.hpp>
#include <serial/datatool/module.hpp>
#include <serial/datatool/classstr.hpp>
#include <serial/datatool/exceptions.hpp>
#include <serial/datatool/reftype.hpp>
#include <serial/datatool/unitype.hpp>
#include <serial/datatool/choicetype.hpp>
#include <serial/datatool/fileutil.hpp>
#include <serial/datatool/srcutil.hpp>
#include <algorithm>

BEGIN_NCBI_SCOPE

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
      m_Set(0), m_Choice(0), m_Checked(false)
{
}

CDataType::~CDataType()
{
}

void CDataType::SetSourceLine(int line)
{
    m_SourceLine = line;
}

void CDataType::Warning(const string& mess) const
{
    CNcbiDiag() << LocationString() << ": " << mess;
}

void CDataType::PrintASNTypeComments(CNcbiOstream& out, int indent) const
{
    m_Comments.PrintASN(out, indent);
}

void CDataType::PrintDTD(CNcbiOstream& out) const
{
    m_Comments.PrintDTD(out);
    PrintDTDElement(out);
    out << '\n';
    PrintDTDExtra(out);
}

void CDataType::PrintDTD(CNcbiOstream& out,
                         const CComments& extra) const
{
    m_Comments.PrintDTD(out);
    bool oneLineComment = extra.OneLine();
    if ( !oneLineComment )
        extra.PrintDTD(out);
    PrintDTDElement(out);
    if ( oneLineComment ) {
        out << ' ';
        extra.PrintDTD(out, CComments::eOneLine);
    }
    out << '\n';
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
    _ASSERT(GetParentType() == 0);
    if ( !m_References )
        m_References = new TReferences;
    m_References->push_back(reference);
}

void CDataType::SetParent(const CDataType* parent, const string& memberName)
{
    _ASSERT(parent != 0);
    _ASSERT(m_ParentType == 0 && m_Module == 0 && m_MemberName.empty());
    m_ParentType = parent;
    m_Module = parent->GetModule();
    m_MemberName = memberName;
    _ASSERT(m_Module != 0);
    FixTypeTree();
}

void CDataType::SetParent(const CDataTypeModule* module,
                          const string& typeName)
{
    _ASSERT(module != 0);
    _ASSERT(m_ParentType == 0 && m_Module == 0 && m_MemberName.empty());
    m_Module = module;
    m_MemberName = typeName;
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

const string& CDataType::GetVar(const string& varName) const
{
    const CDataType* parent = GetParentType();
    if ( !parent ) {
        return GetModule()->GetVar(m_MemberName, varName);
    }
    else {
        return parent->GetVar(m_MemberName + '.' + varName);
    }
}

const string& CDataType::GetSourceFileName(void) const
{
    return GetModule()->GetSourceFileName();
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
    const CDataType* parent = GetParentType();
    if ( !parent ) {
        // root type
        return m_MemberName;
    }
    else {
        // member
        return parent->XmlTagName() + '_' + m_MemberName;
    }
}

const string& CDataType::GlobalName(void) const
{
    if ( !GetParentType() )
        return m_MemberName;
    else
        return NcbiEmptyString;
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

string CDataType::ClassName(void) const
{
    const string& cls = GetVar("_class");
    if ( !cls.empty() )
        return cls;
    if ( GetParentType() ) {
        // local type
        return "C_"+Identifier(m_MemberName);
    }
    else {
        // global type
        return 'C'+Identifier(m_MemberName);
    }
}

string CDataType::FileName(void) const
{
    if ( m_CachedFileName.empty() ) {
        const string& file = GetVar("_file");
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
        const string& ns = GetVar("_namespace");
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
    const string& parentName = GetVar("_parent_type");
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
    THROW1_TRACE(runtime_error, "cannot create type info of "+IdName());
}

CTypeInfo* CDataType::UpdateModuleName(CTypeInfo* typeInfo) const
{
    if ( HaveModuleName() )
        typeInfo->SetModuleName(GetModule()->GetName());
    return typeInfo;
}

AutoPtr<CTypeStrings> CDataType::GenerateCode(void) const
{
    AutoPtr<CClassTypeStrings> code(new CClassTypeStrings(GlobalName(),
                                                          ClassName()));
    AutoPtr<CTypeStrings> dType = GetFullCType();
    code->AddMember(dType);
    SetParentClassTo(*code);
    return AutoPtr<CTypeStrings>(code.release());
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
    return AutoPtr<CTypeStrings>(new CClassRefTypeStrings(ClassName(),
                                                          Namespace(),
                                                          FileName()));
}

AutoPtr<CTypeStrings> CDataType::GetFullCType(void) const
{
    THROW1_TRACE(runtime_error, LocationString() + ": C++ type undefined");
}

string CDataType::GetDefaultString(const CDataValue& ) const
{
    Warning("Default is not supported by this type");
    return "...";
}

END_NCBI_SCOPE
