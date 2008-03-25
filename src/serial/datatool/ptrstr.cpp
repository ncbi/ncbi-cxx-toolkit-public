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
*/

#include <ncbi_pch.hpp>
#include "ptrstr.hpp"
#include "classctx.hpp"
#include "namespace.hpp"

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
