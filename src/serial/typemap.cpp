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
* Revision 1.3  2004/05/17 21:03:04  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 1.2  2000/11/07 17:25:42  vasilche
* Fixed encoding of XML:
*     removed unnecessary apostrophes in OCTET STRING
*     removed unnecessary content in NULL
* Added module names to CTypeInfo and CEnumeratedTypeValues
*
* Revision 1.1  2000/10/13 16:28:41  vasilche
* Reduced header dependency.
* Avoid use of templates with virtual methods.
* Reduced amount of different maps used.
* All this lead to smaller compiled code size (libraries and programs).
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <serial/typemap.hpp>
#include <serial/typemapimpl.hpp>

BEGIN_NCBI_SCOPE

CTypeInfoMap::CTypeInfoMap(void)
    : m_Data(0)
{
}

CTypeInfoMap::~CTypeInfoMap(void)
{
    delete m_Data;
}

TTypeInfo CTypeInfoMap::GetTypeInfo(TTypeInfo key, TTypeInfoGetter1 func)
{
    CTypeInfoMapData* data = m_Data;
    if ( !data )
        m_Data = data = new CTypeInfoMapData;
    return data->GetTypeInfo(key, func);
}

TTypeInfo CTypeInfoMap::GetTypeInfo(TTypeInfo key, TTypeInfoCreator1 func)
{
    CTypeInfoMapData* data = m_Data;
    if ( !data )
        m_Data = data = new CTypeInfoMapData;
    return data->GetTypeInfo(key, func);
}

TTypeInfo CTypeInfoMapData::GetTypeInfo(TTypeInfo key, TTypeInfoGetter1 func)
{
    TTypeInfo& slot = m_Map[key];
    TTypeInfo ret = slot;
    if ( !ret )
        slot = ret = func(key);
    return ret;
}

TTypeInfo CTypeInfoMapData::GetTypeInfo(TTypeInfo key, TTypeInfoCreator1 func)
{
    TTypeInfo& slot = m_Map[key];
    TTypeInfo ret = slot;
    if ( !ret )
        slot = ret = func(key);
    return ret;
}

CTypeInfoMap2::CTypeInfoMap2(void)
    : m_Data(0)
{
}

CTypeInfoMap2::~CTypeInfoMap2(void)
{
    delete m_Data;
}

TTypeInfo CTypeInfoMap2::GetTypeInfo(TTypeInfo arg1, TTypeInfo arg2,
                                     TTypeInfoGetter2 func)
{
    CTypeInfoMap2Data* data = m_Data;
    if ( !data )
        m_Data = data = new CTypeInfoMap2Data;
    return data->GetTypeInfo(arg1, arg2, func);
}

TTypeInfo CTypeInfoMap2::GetTypeInfo(TTypeInfo arg1, TTypeInfo arg2,
                                     TTypeInfoCreator2 func)
{
    CTypeInfoMap2Data* data = m_Data;
    if ( !data )
        m_Data = data = new CTypeInfoMap2Data;
    return data->GetTypeInfo(arg1, arg2, func);
}

TTypeInfo CTypeInfoMap2Data::GetTypeInfo(TTypeInfo arg1, TTypeInfo arg2,
                                         TTypeInfoGetter2 func)
{
    TTypeInfo& slot = m_Map[arg1][arg2];
    TTypeInfo ret = slot;
    if ( !ret )
        slot = ret = func(arg1, arg2);
    return ret;
}

TTypeInfo CTypeInfoMap2Data::GetTypeInfo(TTypeInfo arg1, TTypeInfo arg2,
                                         TTypeInfoCreator2 func)
{
    TTypeInfo& slot = m_Map[arg1][arg2];
    TTypeInfo ret = slot;
    if ( !ret )
        slot = ret = func(arg1, arg2);
    return ret;
}

END_NCBI_SCOPE
