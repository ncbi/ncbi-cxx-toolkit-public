/* $Id$
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
 * Author:  Vladimir Soussov
 *
 * File Description:  Param container implementation
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbimisc.hpp>
#include <dbapi/driver/util/parameters.hpp>
#include <dbapi/error_codes.hpp>
#include <dbapi/driver/exception.hpp>


#define NCBI_USE_ERRCODE_X   Dbapi_DrvrUtil


BEGIN_NCBI_SCOPE

namespace impl {

////////////////////////////////////////////////////////////////////////////////
CDB_Params::SParam::SParam(void) : 
    m_Param(NULL), 
    m_Status(0)
{
}

CDB_Params::SParam::~SParam(void)
{
    DeleteParam();
}


void 
CDB_Params::SParam::Bind(const string& param_name, CDB_Object* param, bool is_out)
{
    DeleteParam();

    m_Param = param;
    m_Name = param_name;
    m_Status |= fBound | (is_out ? fOutput : 0) ;
}

void 
CDB_Params::SParam::Set(const string& param_name, CDB_Object* param, bool is_out)
{
    if ((m_Status & fSet) != 0) {
        if (m_Param->GetType() == param->GetType()) {
            // types are the same
            m_Param->AssignValue(*param);
        } else { 
            // we need to delete the old one
            DeleteParam();

            m_Param = param->Clone();
        }
    } else {
        m_Param = param->Clone();
    }

    m_Name = param_name;
    m_Status |= fSet | (is_out ? fOutput : 0);
}


////////////////////////////////////////////////////////////////////////////////
CDB_Params::CDB_Params(void)
: m_Locked(false)
{
}


bool 
CDB_Params::GetParamNumInternal(const string& param_name, unsigned int& param_num) const
{
    // try to find this name
    for (param_num = 0;  param_num < m_Params.size(); ++param_num) {
        const SParam& param = m_Params[param_num];
        if (param.m_Status != 0 && param_name == param.m_Name) {
            // We found it ...
            return true;
        }
    }

    return false;
}


unsigned int 
CDB_Params::GetParamNum(const string& param_name) const
{
    unsigned int param_no = 0;

    if (!GetParamNumInternal(param_name, param_no)) {
        // Parameter not found ...
        DATABASE_DRIVER_ERROR("Invalid parameter's name: " + param_name, 122510 );
    }

    return param_no;
}

unsigned int 
CDB_Params::GetParamNum(unsigned int param_no, const string& param_name)
{
    if (param_no == CDB_Params::kNoParamNumber) {
        if (!param_name.empty()) {
            // try to find this name
            if (!GetParamNumInternal(param_name, param_no)) {
                // Parameter not found ...
		CHECK_DRIVER_ERROR(IsLocked(), "Parameters are locked. New bindins are not allowed.", 20001);
                m_Params.resize(m_Params.size() + 1);
                return (unsigned int)(m_Params.size() - 1);
            }
        }
    } else {
        if (param_no >= m_Params.size()) {
	    CHECK_DRIVER_ERROR(IsLocked(), "Parameters are locked. New bindins are not allowed.", 20001);
            m_Params.resize(param_no + 1);
        }
    }

    return param_no;
}


bool CDB_Params::BindParam(unsigned int param_no, const string& param_name,
                           CDB_Object* param, bool is_out)
{
    param_no = GetParamNum(param_no, param_name);
    m_Params[param_no].Bind(param_name, param, is_out);
    return true;
}


bool CDB_Params::SetParam(unsigned int param_no, const string& param_name,
                          CDB_Object* param, bool is_out)
{
    param_no = GetParamNum(param_no, param_name);
    m_Params[param_no].Set(param_name, param, is_out);
    return true;
}


CDB_Params* CDB_Params::SemiShallowClone(void) const
{
    auto_ptr<CDB_Params> result(new CDB_Params);
    SParam empty_param;
    ITERATE (deque<SParam>, it, m_Params) {
        result->m_Params.push_back(empty_param);
        SParam& p = result->m_Params.back();

        p.m_Name   = it->m_Name;
        p.m_Param  = ((it->m_Param == NULL) ? NULL
                      : it->m_Param->ShallowClone());
        p.m_Status = (it->m_Status & ~fBound) | fSet;
    }
    return result.release();
}


CDB_Params::~CDB_Params()
{
}


string
g_SubstituteParam(const string& query, const string& name, const string& val)
{
    string result = query;
    size_t name_len = name.length();
    size_t val_len = val.length();
    size_t len = result.length();
    char q = 0;

    for (size_t pos = 0;  pos < len;  pos++) {
        if (q) {
            if (result[pos] == q)
                q = 0;
            continue;
        }
        if (result[pos] == '"' || result[pos] == '\'') {
            q = result[pos];
            continue;
        }
        if (NStr::Compare(result, pos, name_len, name) == 0
            &&  (pos == 0  ||  !isalnum((unsigned char) result[pos - 1]))
            &&  (pos + name_len >= result.size()
                 ||  (!isalnum((unsigned char) result[pos + name_len])
                      &&  result[pos + name_len] != '_')))
        {
            result.replace(pos, name_len, val);
            len = result.length();
            pos += val_len;
        }
    }

    return result;
}

}

END_NCBI_SCOPE


