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
*   !!! PUT YOUR DESCRIPTION HERE !!!
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.3  2004/05/17 21:03:02  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 1.2  2000/10/13 20:22:55  vasilche
* Fixed warnings on 64 bit compilers.
* Fixed missing typename in templates.
*
* Revision 1.1  2000/09/18 20:00:22  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <serial/item.hpp>

BEGIN_NCBI_SCOPE

CItemInfo::CItemInfo(const CMemberId& id,
                     TPointerOffsetType offset, TTypeInfo type)
    : m_Id(id), m_Index(kInvalidMember), m_Offset(offset), m_Type(type)
{
}

CItemInfo::CItemInfo(const CMemberId& id,
                     TPointerOffsetType offset, const CTypeRef& type)
    : m_Id(id), m_Index(kInvalidMember), m_Offset(offset), m_Type(type)
{
}

CItemInfo::CItemInfo(const char* id,
                     TPointerOffsetType offset, TTypeInfo type)
    : m_Id(id), m_Index(kInvalidMember), m_Offset(offset), m_Type(type)
{
}

CItemInfo::CItemInfo(const char* id,
                     TPointerOffsetType offset, const CTypeRef& type)
    : m_Id(id), m_Index(kInvalidMember), m_Offset(offset), m_Type(type)
{
}

CItemInfo::~CItemInfo(void)
{
}

END_NCBI_SCOPE
