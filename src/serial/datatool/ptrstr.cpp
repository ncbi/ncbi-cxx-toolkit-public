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
* Revision 1.9  2004/05/17 21:03:14  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 1.8  2003/04/29 18:31:09  gouriano
* object data member initialization verification
*
* Revision 1.7  2000/08/25 15:59:23  vasilche
* Renamed directory tool -> datatool.
*
* Revision 1.6  2000/07/11 20:36:29  vasilche
* Removed unnecessary generation of namespace references for enum members.
* Removed obsolete methods.
*
* Revision 1.5  2000/06/16 16:31:39  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.4  2000/04/17 19:11:09  vasilche
* Fixed failed assertion.
* Removed redundant namespace specifications.
*
* Revision 1.3  2000/04/12 15:36:52  vasilche
* Added -on <namespace> argument to datatool.
* Removed unnecessary namespace specifications in generated files.
*
* Revision 1.2  2000/04/07 19:26:33  vasilche
* Added namespace support to datatool.
* By default with argument -oR datatool will generate objects in namespace
* NCBI_NS_NCBI::objects (aka ncbi::objects).
* Datatool's classes also moved to NCBI namespace.
*
* Revision 1.1  2000/03/07 14:06:32  vasilche
* Added generation of reference counted objects.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <serial/datatool/ptrstr.hpp>
#include <serial/datatool/classctx.hpp>
#include <serial/datatool/namespace.hpp>

BEGIN_NCBI_SCOPE

CPointerTypeStrings::CPointerTypeStrings(CTypeStrings* dataType)
    : m_DataType(dataType)
{
}

CPointerTypeStrings::CPointerTypeStrings(AutoPtr<CTypeStrings> dataType)
    : m_DataType(dataType)
{
}

CPointerTypeStrings::~CPointerTypeStrings(void)
{
}

CTypeStrings::EKind CPointerTypeStrings::GetKind(void) const
{
    return eKindPointer;
}

string CPointerTypeStrings::GetCType(const CNamespace& ns) const
{
    return GetDataType()->GetCType(ns)+'*';
}

string CPointerTypeStrings::GetPrefixedCType(const CNamespace& ns,
                                             const string& /*methodPrefix*/) const
{
    return GetCType(ns);
}

string CPointerTypeStrings::GetRef(const CNamespace& ns) const
{
    return "POINTER, ("+GetDataType()->GetRef(ns)+')';
}

string CPointerTypeStrings::GetInitializer(void) const
{
    return "0";
}

string CPointerTypeStrings::GetDestructionCode(const string& expr) const
{
    return
        GetDataType()->GetDestructionCode("*(" + expr + ')')+
        "delete ("+expr+");\n";
}

string CPointerTypeStrings::GetIsSetCode(const string& var) const
{
    return "("+var+") != 0";
}

string CPointerTypeStrings::GetResetCode(const string& var) const
{
    return var + " = 0;\n";
}

void CPointerTypeStrings::GenerateTypeCode(CClassContext& ctx) const
{
    GetDataType()->GeneratePointerTypeCode(ctx);
}



CRefTypeStrings::CRefTypeStrings(CTypeStrings* dataType)
    : CParent(dataType)
{
}

CRefTypeStrings::CRefTypeStrings(AutoPtr<CTypeStrings> dataType)
    : CParent(dataType)
{
}

CRefTypeStrings::~CRefTypeStrings(void)
{
}

CTypeStrings::EKind CRefTypeStrings::GetKind(void) const
{
    return eKindRef;
}

string CRefTypeStrings::GetCType(const CNamespace& ns) const
{
    return ns.GetNamespaceRef(CNamespace::KNCBINamespace)+"CRef< "+GetDataType()->GetCType(ns)+" >";
}

string CRefTypeStrings::GetPrefixedCType(const CNamespace& ns,
                                         const string& methodPrefix) const
{
    return ns.GetNamespaceRef(CNamespace::KNCBINamespace)+"CRef< "
        + GetDataType()->GetPrefixedCType(ns,methodPrefix)+" >";
}

string CRefTypeStrings::GetRef(const CNamespace& ns) const
{
    return "STL_CRef, ("+GetDataType()->GetRef(ns)+')';
}

string CRefTypeStrings::GetInitializer(void) const
{
    return NcbiEmptyString;
}

string CRefTypeStrings::GetDestructionCode(const string& expr) const
{
    return GetDataType()->GetDestructionCode("*(" + expr + ')');
}

string CRefTypeStrings::GetIsSetCode(const string& var) const
{
    return var;
}

string CRefTypeStrings::GetResetCode(const string& var) const
{
    return var + ".Reset();\n";
}

END_NCBI_SCOPE
