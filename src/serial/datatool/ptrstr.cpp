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
* Revision 1.1  2000/03/07 14:06:32  vasilche
* Added generation of reference counted objects.
*
* ===========================================================================
*/

#include <serial/tool/ptrstr.hpp>
#include <serial/tool/classctx.hpp>

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

bool CPointerTypeStrings::CanBeKey(void) const
{
    return false;
}

bool CPointerTypeStrings::CanBeInSTL(void) const
{
    return true;
}

bool CPointerTypeStrings::NeedSetFlag(void) const
{
    return false;
}

string CPointerTypeStrings::GetCType(void) const
{
    return GetDataType()->GetCType()+'*';
}

string CPointerTypeStrings::GetRef(void) const
{
    return "&NCBI_NS_NCBI::CPointerTypeInfo::GetTypeInfo, "+GetDataType()->GetRef();
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

string CRefTypeStrings::GetCType(void) const
{
    return "NCBI_NS_NCBI::CRef< "+GetDataType()->GetCType()+" >";
}

string CRefTypeStrings::GetRef(void) const
{
    return "&NCBI_NS_NCBI::CRefTypeInfo< "+GetDataType()->GetCType()+" >::GetTypeInfo, "+GetDataType()->GetRef();
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

