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

#include <serial/stltypes.hpp>
#include <serial/autoptrinfo.hpp>
#include <serial/datatool/unitype.hpp>
#include <serial/datatool/blocktype.hpp>
#include <serial/datatool/stlstr.hpp>
#include <serial/datatool/value.hpp>
#include <serial/datatool/reftype.hpp>

BEGIN_NCBI_SCOPE

CUniSequenceDataType::CUniSequenceDataType(const AutoPtr<CDataType>& element)
{
    SetElementType(element);
}

const char* CUniSequenceDataType::GetASNKeyword(void) const
{
    return "SEQUENCE";
}

void CUniSequenceDataType::SetElementType(const AutoPtr<CDataType>& type)
{
    if ( GetElementType() )
        THROW1_TRACE(runtime_error, "double element type " + LocationString());
    m_ElementType = type;
}

void CUniSequenceDataType::PrintASN(CNcbiOstream& out, int indent) const
{
    out << GetASNKeyword() << " OF ";
    GetElementType()->PrintASN(out, indent);
}

void CUniSequenceDataType::PrintDTD(CNcbiOstream& out) const
{
    const CDataType* data = GetElementType();
    const CReferenceDataType* ref =
        dynamic_cast<const CReferenceDataType*>(data);
    out <<
        "<!ELEMENT "<<XmlTagName()<<" ( ";
    if ( ref )
        out << ref->UserTypeXmlTagName();
    else
        out << data->XmlTagName();
    out << "* ) >\n";
    if ( !ref ) {
        // array of internal type, we should generate tag for element type
        if ( GetParentType() == 0 )
            out << '\n';
        data->PrintDTD(out);
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
    iterate ( CBlockDataValue::TValues, i, block->GetValues() ) {
        if ( !m_ElementType->CheckValue(**i) )
            ok = false;
    }
    return ok;
}

TObjectPtr CUniSequenceDataType::CreateDefault(const CDataValue& ) const
{
    THROW1_TRACE(runtime_error, "SET/SEQUENCE OF default not implemented");
}

CTypeInfo* CUniSequenceDataType::CreateTypeInfo(void)
{
    return CStlClassInfo_list<AnyType>::CreateTypeInfo(m_ElementType->GetTypeInfo().Get());
}

bool CUniSequenceDataType::NeedAutoPointer(TTypeInfo /*typeInfo*/) const
{
    return true;
}

AutoPtr<CTypeStrings> CUniSequenceDataType::GetFullCType(void) const
{
    AutoPtr<CTypeStrings> tData = GetElementType()->GetFullCType();
    CTypeStrings::AdaptForSTL(tData);
    string templ = GetVar("_type");
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
    return CStlClassInfo_list<AnyType>::CreateSetTypeInfo(GetElementType()->GetTypeInfo().Get());
}

AutoPtr<CTypeStrings> CUniSetDataType::GetFullCType(void) const
{
    string templ = GetVar("_type");
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
            templ = "multiset";
        }
        else {
            return AutoPtr<CTypeStrings>(new CListTypeStrings("list", tData, true));
        }
    }
    return AutoPtr<CTypeStrings>(new CSetTypeStrings(templ, tData));
}

END_NCBI_SCOPE
