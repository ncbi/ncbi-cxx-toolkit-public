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

#include <serial/tool/type.hpp>
#include <serial/autoptrinfo.hpp>
#include <serial/tool/value.hpp>
#include <serial/tool/module.hpp>
#include <serial/tool/classstr.hpp>
#include <serial/tool/exceptions.hpp>
#include <serial/tool/reftype.hpp>
#include <serial/tool/unitype.hpp>
#include <serial/tool/choicetype.hpp>
#include <serial/tool/fileutil.hpp>
#include <algorithm>

BEGIN_NCBI_SCOPE

TTypeInfo CAnyTypeSource::GetTypeInfo(void)
{
    TTypeInfo typeInfo = m_Type->GetTypeInfo();
    if ( typeInfo->GetSize() > sizeof(AnyType) )
        typeInfo = new CAutoPointerTypeInfo(typeInfo);
    return typeInfo;
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

CNcbiOstream& CDataType::NewLine(CNcbiOstream& out, int indent)
{
    out <<
        "\n";
    for ( int i = 0; i < indent; ++i )
        out << "  ";
    return out;
}

const string& CDataType::GetVar(const string& value) const
{
    const CDataType* parent = GetParentType();
    if ( !parent ) {
        return GetModule()->GetVar(m_MemberName, value);
    }
    else {
        return parent->GetVar(m_MemberName + '.' + value);
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
    return 'C'+Identifier(m_MemberName);
}

string CDataType::FileName(void) const
{
    if ( m_CachedFileName.empty() ) {
        const string& file = GetVar("_file");
        if ( !file.empty() ) {
            m_CachedFileName = file;
        }
        else {
            _ASSERT(!GetParentType()); // for non internal classes
            m_CachedFileName =
                Path(GetModule()->GetFileNamePrefix(),
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
    const string& parentName = GetVar("_parent");
    if ( !parentName.empty() )
        return GetModule()->Resolve(parentName);

    // try to detect implicit inheritance
    if ( IsInChoice() ) // directly in CHOICE
        return 0; //GetInChoice();

    if ( IsReferenced() ) {
        // have references to me
        const CChoiceDataType* namedParent = 0;
        const CChoiceDataType* unnamedParent = 0;
        int unnamedParentCount = 0;
        // try to find exactly one reference from named CHOICE
        iterate ( TReferences, ri, GetReferences() ) {
            const CReferenceDataType* ref = *ri;
            if ( ref->IsInChoice() ) {
                const CChoiceDataType* choice = ref->GetInChoice();
                if ( !choice->GetParentType() ) {
                    // named CHOICE
                    if ( !namedParent )
                        namedParent = choice;
                    else
                        return 0; // more then one named CHOICE
                }
                else {
                    // unnamed CHOICE
                    if ( unnamedParentCount++ == 0 )
                        unnamedParent = choice;
                }
            }
        }
        if ( namedParent ) {
            return 0; //namedParent;
        }
        if ( unnamedParentCount == 1 ) {
            return 0; // unnamedParent;
        }
    }
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

TTypeInfo CDataType::GetTypeInfo(void)
{
    if ( !m_CreatedTypeInfo ) {
        m_CreatedTypeInfo = CreateTypeInfo();
        if ( !m_CreatedTypeInfo )
            THROW1_TRACE(runtime_error, "type undefined");
    }
    return m_CreatedTypeInfo.get();
}

AutoPtr<CTypeStrings> CDataType::GenerateCode(void) const
{
    AutoPtr<CClassTypeStrings> code(new CClassTypeStrings(IdName(),
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

CTypeInfo* CDataType::CreateTypeInfo(void)
{
    return 0;
}

END_NCBI_SCOPE
