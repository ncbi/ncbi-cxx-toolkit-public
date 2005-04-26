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
*   Type info for class generation: includes, used classes, C code etc.
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.25  2005/04/26 14:18:50  vasilche
* Allow allocation of objects in CObjectMemoryPool.
*
* Revision 1.24  2004/09/07 14:09:45  grichenk
* Fixed assignment of default value to aliased types
*
* Revision 1.23  2004/05/17 21:03:14  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 1.22  2003/10/21 13:48:51  grichenk
* Redesigned type aliases in serialization library.
* Fixed the code (removed CRef-s, added explicit
* initializers etc.)
*
* Revision 1.21  2003/04/29 18:31:09  gouriano
* object data member initialization verification
*
* Revision 1.20  2003/03/10 18:55:19  gouriano
* use new structured exceptions (based on CException)
*
* Revision 1.19  2000/11/07 17:26:26  vasilche
* Added module names to CTypeInfo and CEnumeratedTypeValues
* Added possibility to set include directory for whole module
*
* Revision 1.18  2000/08/25 15:59:25  vasilche
* Renamed directory tool -> datatool.
*
* Revision 1.17  2000/07/11 20:36:30  vasilche
* Removed unnecessary generation of namespace references for enum members.
* Removed obsolete methods.
*
* Revision 1.16  2000/06/16 16:31:41  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.15  2000/04/17 19:11:09  vasilche
* Fixed failed assertion.
* Removed redundant namespace specifications.
*
* Revision 1.14  2000/04/12 15:36:53  vasilche
* Added -on <namespace> argument to datatool.
* Removed unnecessary namespace specifications in generated files.
*
* Revision 1.13  2000/04/07 19:26:36  vasilche
* Added namespace support to datatool.
* By default with argument -oR datatool will generate objects in namespace
* NCBI_NS_NCBI::objects (aka ncbi::objects).
* Datatool's classes also moved to NCBI namespace.
*
* Revision 1.12  2000/03/07 20:05:00  vasilche
* Added NewInstance method to generated classes.
*
* Revision 1.11  2000/03/07 14:06:34  vasilche
* Added generation of reference counted objects.
*
* Revision 1.10  2000/02/02 14:57:07  vasilche
* Added missing NCBI_NS_NSBI and NSBI_NS_STD macros to generated code.
*
* Revision 1.9  2000/02/01 21:48:09  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
* Changed class generation.
* Moved datatool headers to include/internal/serial/tool.
*
* Revision 1.8  2000/01/10 19:46:47  vasilche
* Fixed encoding/decoding of REAL type.
* Fixed encoding/decoding of StringStore.
* Fixed encoding/decoding of NULL type.
* Fixed error reporting.
* Reduced object map (only classes).
*
* Revision 1.7  1999/12/28 18:56:00  vasilche
* Reduced size of compiled object files:
* 1. avoid inline or implicit virtual methods (especially destructors).
* 2. avoid std::string's methods usage in inline methods.
* 3. avoid string literals ("xxx") in inline methods.
*
* Revision 1.6  1999/12/17 19:05:19  vasilche
* Simplified generation of GetTypeInfo methods.
*
* Revision 1.5  1999/12/01 17:36:28  vasilche
* Fixed CHOICE processing.
*
* Revision 1.4  1999/11/18 17:13:07  vasilche
* Fixed generation of ENUMERATED CHOICE and VisibleString.
* Added generation of initializers to zero for primitive types and pointers.
*
* Revision 1.3  1999/11/16 15:41:17  vasilche
* Added plain pointer choice.
* By default we use C pointer instead of auto_ptr.
* Start adding initializers.
*
* Revision 1.2  1999/11/15 19:36:20  vasilche
* Fixed warnings on GCC
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <serial/datatool/exceptions.hpp>
#include <serial/datatool/typestr.hpp>
#include <serial/datatool/classctx.hpp>
#include <serial/datatool/ptrstr.hpp>
#include <serial/datatool/namespace.hpp>

BEGIN_NCBI_SCOPE

CTypeStrings::CTypeStrings(void)
{
}

CTypeStrings::~CTypeStrings(void)
{
}

void CTypeStrings::SetModuleName(const string& name)
{
    _ASSERT(m_ModuleName.empty());
    m_ModuleName = name;
}

const CNamespace& CTypeStrings::GetNamespace(void) const
{
    return CNamespace::KEmptyNamespace;
}

const string& CTypeStrings::GetEnumName(void) const
{
    NCBI_THROW(CDatatoolException,eIllegalCall,"illegal call");
}

string CTypeStrings::GetInitializer(void) const
{
    return NcbiEmptyString;
}

string CTypeStrings::GetDestructionCode(const string& /*expr*/) const
{
    return NcbiEmptyString;
}

string CTypeStrings::GetResetCode(const string& /*var*/) const
{
    return NcbiEmptyString;
}

string CTypeStrings::GetDefaultCode(const string& var) const
{
    return var;
}

bool CTypeStrings::HaveSpecialRef(void) const
{
    return false;
}

bool CTypeStrings::CanBeKey(void) const
{
    switch ( GetKind() ) {
    case eKindStd:
    case eKindEnum:
    case eKindString:
        return true;
    default:
        return false;
    }
}

bool CTypeStrings::CanBeCopied(void) const
{
    switch ( GetKind() ) {
    case eKindStd:
    case eKindEnum:
    case eKindString:
    case eKindPointer:
    case eKindRef:
        return true;
    default:
        return false;
    }
}

bool CTypeStrings::NeedSetFlag(void) const
{
    switch ( GetKind() ) {
    case eKindPointer:
    case eKindRef:
//    case eKindContainer:
        return false;
    default:
        return true;
    }
}

string CTypeStrings::NewInstance(const string& init,
                                 const string& place) const
{
    CNcbiOstrstream s;
    s << "new";
    if ( GetKind() == eKindObject ) {
        s << place;
    }
    s << ' ' << GetCType(CNamespace::KEmptyNamespace) << '(' << init << ')';
    return CNcbiOstrstreamToString(s);
}

string CTypeStrings::GetIsSetCode(const string& /*var*/) const
{
    NCBI_THROW(CDatatoolException,eIllegalCall, "illegal call");
}

void CTypeStrings::AdaptForSTL(AutoPtr<CTypeStrings>& type)
{
    switch ( type->GetKind() ) {
    case eKindStd:
    case eKindEnum:
    case eKindString:
    case eKindPointer:
    case eKindRef:
        // already suitable for STL
        break;
    case eKindObject:
        type.reset(new CRefTypeStrings(type.release()));
        break;
    default:
        if ( !type->CanBeCopied()  ||  !type->CanBeKey()) {
            type.reset(new CPointerTypeStrings(type.release()));
        }
        break;
    }
}

void CTypeStrings::GenerateCode(CClassContext& ctx) const
{
    GenerateTypeCode(ctx);
}

void CTypeStrings::GenerateTypeCode(CClassContext& /*ctx*/) const
{
}

void CTypeStrings::GeneratePointerTypeCode(CClassContext& ctx) const
{
    GenerateTypeCode(ctx);
}

void CTypeStrings::GenerateUserHPPCode(CNcbiOstream& /*out*/) const
{
}

void CTypeStrings::GenerateUserCPPCode(CNcbiOstream& /*out*/) const
{
}

END_NCBI_SCOPE
