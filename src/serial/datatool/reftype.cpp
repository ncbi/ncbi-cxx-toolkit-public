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
*   Type reference defenition
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.14  2000/07/11 20:36:29  vasilche
* Removed unnecessary generation of namespace references for enum members.
* Removed obsolete methods.
*
* Revision 1.13  2000/07/10 17:32:00  vasilche
* Macro arguments made more clear.
* All old ASN stuff moved to serialasn.hpp.
* Changed prefix of enum info functions to GetTypeInfo_enum_.
*
* Revision 1.12  2000/06/16 16:31:40  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.11  2000/05/24 20:09:29  vasilche
* Implemented DTD generation.
*
* Revision 1.10  2000/04/07 19:26:33  vasilche
* Added namespace support to datatool.
* By default with argument -oR datatool will generate objects in namespace
* NCBI_NS_NCBI::objects (aka ncbi::objects).
* Datatool's classes also moved to NCBI namespace.
*
* Revision 1.9  2000/03/15 21:24:12  vasilche
* Error diagnostic about ambiguous types made more clear.
*
* Revision 1.8  2000/02/01 21:48:05  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
* Changed class generation.
* Moved datatool headers to include/internal/serial/tool.
*
* Revision 1.7  1999/12/29 16:01:51  vasilche
* Added explicit virtual destructors.
* Resolved overloading of InternalResolve.
*
* Revision 1.6  1999/12/03 21:42:12  vasilche
* Fixed conflict of enums in choices.
*
* Revision 1.5  1999/12/01 17:36:26  vasilche
* Fixed CHOICE processing.
*
* Revision 1.4  1999/11/16 15:41:16  vasilche
* Added plain pointer choice.
* By default we use C pointer instead of auto_ptr.
* Start adding initializers.
*
* Revision 1.3  1999/11/15 19:36:18  vasilche
* Fixed warnings on GCC
*
* ===========================================================================
*/

#include <serial/tool/reftype.hpp>
#include <serial/tool/typestr.hpp>
#include <serial/tool/value.hpp>
#include <serial/tool/module.hpp>
#include <serial/tool/exceptions.hpp>
#include <serial/tool/blocktype.hpp>
#include <serial/tool/enumtype.hpp>
#include <serial/classinfo.hpp>
#include <serial/serialimpl.hpp>

BEGIN_NCBI_SCOPE

CReferenceDataType::CReferenceDataType(const string& n)
    : m_UserTypeName(n)
{
}

void CReferenceDataType::PrintASN(CNcbiOstream& out, int ) const
{
    out << m_UserTypeName;
}

void CReferenceDataType::PrintDTD(CNcbiOstream& out) const
{
    out <<
        "<!ELEMENT "<<XmlTagName()<<" ( "<<UserTypeXmlTagName()<<" ) >\n";
}

void CReferenceDataType::FixTypeTree(void) const
{
    CParent::FixTypeTree();
    CDataType* resolved = ResolveOrNull();
    if ( resolved )
        resolved->AddReference(this);
}

bool CReferenceDataType::CheckType(void) const
{
    try {
        GetModule()->Resolve(m_UserTypeName);
        return true;
    }
    catch ( CTypeNotFound& exc ) {
        Warning("Unresolved type: " + m_UserTypeName + ": " + exc.what());
        return false;
    }
}

bool CReferenceDataType::CheckValue(const CDataValue& value) const
{
    CDataType* resolved = ResolveOrNull();
    if ( !resolved )
        return false;
    return resolved->CheckValue(value);
}

TTypeInfo CReferenceDataType::GetRealTypeInfo(void)
{
    CDataType* dataType = ResolveOrThrow();
    if ( dynamic_cast<CDataMemberContainerType*>(dataType) ||
         dynamic_cast<CEnumDataType*>(dataType) )
        return dataType->GetRealTypeInfo();
    return CParent::GetRealTypeInfo();
}

CTypeInfo* CReferenceDataType::CreateTypeInfo(void)
{
    CClassTypeInfo* info = CClassInfoHelper<AnyType>::CreateClassInfo(m_UserTypeName.c_str());
    info->SetImplicit(true);
    info->GetMembers().AddMember("", 0, ResolveOrThrow()->GetTypeInfo());
    return info;
}

TObjectPtr CReferenceDataType::CreateDefault(const CDataValue& value) const
{
    return ResolveOrThrow()->CreateDefault(value);
}

AutoPtr<CTypeStrings> CReferenceDataType::GenerateCode(void) const
{
    return CParent::GenerateCode();
}

AutoPtr<CTypeStrings> CReferenceDataType::GetFullCType(void) const
{
    const CDataType* resolved = ResolveOrThrow();
    if ( resolved->Skipped() )
        return resolved->GetFullCType();
    else
        return resolved->GetRefCType();
}

CDataType* CReferenceDataType::ResolveOrNull(void) const
{
    try {
        return GetModule()->Resolve(m_UserTypeName);
    }
    catch (CTypeNotFound& /* ignored */) {
        return 0;
    }
}

CDataType* CReferenceDataType::ResolveOrThrow(void) const
{
    try {
        return GetModule()->Resolve(m_UserTypeName);
    }
    catch (CTypeNotFound& exc) {
        throw CTypeNotFound(LocationString() + ": " + exc.what());
    }
}

CDataType* CReferenceDataType::Resolve(void)
{
    CDataType* resolved = ResolveOrNull();
    if ( !resolved )
        return this;
    return resolved;
}

const CDataType* CReferenceDataType::Resolve(void) const
{
    CDataType* resolved = ResolveOrNull();
    if ( !resolved )
        return this;
    return resolved;
}

END_NCBI_SCOPE
