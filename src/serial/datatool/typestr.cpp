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

#include <serial/tool/typestr.hpp>
#include <serial/tool/classctx.hpp>
#include <serial/tool/stlstr.hpp>

CTypeStrings::~CTypeStrings(void)
{
}

string CTypeStrings::GetInitializer(void) const
{
    return string();
}

string CTypeStrings::GetDestructionCode(const string& /*expr*/) const
{
    return string();
}

string CTypeStrings::GetResetCode(const string& /*var*/) const
{
    return string();
}

bool CTypeStrings::CanBeKey(void) const
{
    return true;
}

bool CTypeStrings::CanBeInSTL(void) const
{
    return true;
}

bool CTypeStrings::NeedSetFlag(void) const
{
    return true;
}

string CTypeStrings::GetIsSetCode(const string& /*var*/) const
{
    return "...";
}

CTypeStrings* CTypeStrings::ToPointer(void)
{
    return new CPointerTypeStrings(this);
}

void CTypeStrings::GenerateCode(CClassContext& ctx) const
{
    //    AddForwardDeclarations(ctx);
    //    AddIncludes(ctx.HPPIncludes(), ctx.CPPIncludes());
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

string CTypeStrings::GetTypeInfoCode(const string& externalName,
                                     const string& memberName) const
{
    return "NCBI_NS_NCBI::AddMember("
        "info->GetMembers(), "
        "\""+externalName+"\", "
        "NCBI_NS_NCBI::Check< "+GetCType()+" >::Ptr(MEMBER_PTR("+memberName+")), "
        +GetRef()+')';
}
