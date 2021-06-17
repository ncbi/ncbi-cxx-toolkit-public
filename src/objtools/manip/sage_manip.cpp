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
 * Author:  Mike DiCuccio
 *
 * File Description:
 *    CSageManip -- manipulator for SAGE data; encodes/decodes from
 *                  a CUser_object
 */

#include <ncbi_pch.hpp>
#include <objtools/manip/sage_manip.hpp>
#include <objects/general/User_field.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


static const char* sc_tag    = "tag";
static const char* sc_count  = "count";
static const char* sc_method = "method";



CConstSageData::CConstSageData(const CUser_object& obj)
    : m_Object(const_cast<CUser_object*>(&obj))
{
}


CConstSageData::~CConstSageData()
{
}


const string& CConstSageData::GetTag(void) const
{
    return m_Object->GetField(sc_tag).GetData().GetStr();
}


int CConstSageData::GetCount(void) const
{
    return m_Object->GetField(sc_count).GetData().GetInt();
}


const CUser_field& CConstSageData::GetField(const string& field,
                                            const string& delim)
{
    return m_Object->GetField(field, delim);
}



CSageData::CSageData(CUser_object& obj)
    : CConstSageData(obj)
{
}


string& CSageData::SetTag(void)
{
    return m_Object->SetField(sc_tag).SetData().SetStr();
}


void CSageData::SetTag(const string& str)
{
    m_Object->SetField(sc_tag).SetData().SetStr(str);
}


string& CSageData::SetMethod(void)
{
    return m_Object->SetField(sc_method).SetData().SetStr();
}


void CSageData::SetMethod(const string& str)
{
    m_Object->SetField(sc_method).SetData().SetStr(str);
}


void CSageData::SetCount(int count)
{
    m_Object->SetField(sc_count).SetData().SetInt(count);
}


CUser_field& CSageData::SetField(const string& field)
{
    return m_Object->SetField(field);
}


CUser_field& CSageData::SetField(const string& field, const string& value)
{
    CUser_field& f = m_Object->SetField(field);
    f.SetData().SetStr(value);
    return f;
}


END_SCOPE(objects)
END_NCBI_SCOPE
