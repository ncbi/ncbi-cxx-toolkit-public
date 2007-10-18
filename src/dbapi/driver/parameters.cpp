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


#define NCBI_USE_ERRCODE_X   Dbapi_DrvrUtil


BEGIN_NCBI_SCOPE


CDB_Params::CDB_Params(unsigned int nof_params)
{
    m_NofParams = nof_params;
    m_Params= 0;
    if (m_NofParams != 0) {
        m_Params = new SParam[m_NofParams];
        for (unsigned int i = 0;  i < m_NofParams;  m_Params[i++].status = 0)
            continue;
    }
}


bool CDB_Params::BindParam(unsigned int param_no, const string& param_name,
                           CDB_Object* param, bool is_out)
{
    if (param_no >= m_NofParams) {
        // try to find the place for this param
        param_no = m_NofParams;
        if ( !param_name.empty() ) {
            // try to find this name
            for (param_no = 0;  param_no < m_NofParams;  param_no++) {
                if (m_Params[param_no].status != 0  &&
                    param_name.compare(m_Params[param_no].name) == 0)
                    break;
            }
        }
        if (param_no >= m_NofParams) {
            for (param_no = 0;
                 param_no < m_NofParams  &&  m_Params[param_no].status != 0;
                 ++param_no)
                continue;
        }
    }

    if(param_no >= m_NofParams) { // we need more memory
        SParam* t= new SParam[param_no + 1];
        unsigned int i;
        if(m_Params) {
            for(i= 0; i < m_NofParams; i++) {
                t[i]= m_Params[i];
            }
            delete [] m_Params;
        }
        m_Params= t;
        for(i= m_NofParams; i <= param_no; m_Params[i++].status = 0);
        m_NofParams= param_no + 1;
    }

    if (param_no < m_NofParams) {
        // we do have a param number
        if ((m_Params[param_no].status & fSet) != 0) {
            // we need to free the old one
            delete m_Params[param_no].param;
            m_Params[param_no].status ^= fSet;
        }
        if ( !param_name.empty() ) {
            if (param_name.compare(m_Params[param_no].name) != 0) {
                m_Params[param_no].name = param_name;
            }
        } else {
            if(m_Params[param_no].status) {
                string n= m_Params[param_no].name;
                if(!n.empty()) m_Params[param_no].name.erase();
            }
        }
        m_Params[param_no].param = param;
        m_Params[param_no].status |= fBound | (is_out ? fOutput : 0) ;
        return true;
    }

    return false;
}


bool CDB_Params::SetParam(unsigned int param_no, const string& param_name,
                          CDB_Object* param, bool is_out)
{
    if (param_no >= m_NofParams) {
        // try to find the place for this param
        param_no = m_NofParams;
        if ( !param_name.empty() ) {
            // try to find this name
            for (param_no = 0;  param_no < m_NofParams;  param_no++) {
                if (m_Params[param_no].status != 0  &&
                    m_Params[param_no].name.compare(param_name) == 0)
                    break;
            }
        }
        if (param_no >= m_NofParams) {
            for (param_no = 0;
                 param_no < m_NofParams  &&  m_Params[param_no].status != 0;
                 ++param_no);
        }
    }

    if(param_no >= m_NofParams) { // we need more memory
        SParam* t= new SParam[param_no + 1];
        unsigned int i;
        if(m_Params) {
            for(i= 0; i < m_NofParams; i++) {
                t[i]= m_Params[i];
            }
            delete [] m_Params;
        }
        m_Params= t;
        for(i= m_NofParams; i <= param_no; m_Params[i++].status = 0);
        m_NofParams= param_no + 1;
    }

    if (param_no < m_NofParams) {
        // we do have a param number
        if ((m_Params[param_no].status & fSet) != 0) {
            if (m_Params[param_no].param->GetType() == param->GetType()) {
                // types are the same
                m_Params[param_no].param->AssignValue(*param);
            }
            else
                { // we need to delete the old one
                    delete m_Params[param_no].param;
                    m_Params[param_no].param = param->Clone();
                }
        }
        else {
            m_Params[param_no].param = param->Clone();
        }
        if ( !param_name.empty()) {
            if (m_Params[param_no].name.compare(param_name) != 0) {
                m_Params[param_no].name = param_name;
            }
        }
        else {
            if(!m_Params[param_no].name.empty()) m_Params[param_no].name.erase();
        }
        m_Params[param_no].status |= fSet | (is_out ? fOutput : 0);
        return true;
    }

    return false;
}


CDB_Params::~CDB_Params()
{
    try {
        if ( !m_NofParams  || !m_Params)
            return;

        for (unsigned int i = 0;  i < m_NofParams;  i++) {
            if ((m_Params[i].status & fSet) != 0)
                delete m_Params[i].param;
        }
        delete [] m_Params;
    }
    NCBI_CATCH_ALL_X( 2, NCBI_CURRENT_FUNCTION )
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
            && (pos == 0 || !isalnum((unsigned char) result[pos - 1]))
            && !isalnum((unsigned char) result[pos + name_len])
            && result[pos + name_len] != '_') {
            result.replace(pos, name_len, val);
            len = result.length();
            pos += val_len;
        }
    }

    return result;
}


END_NCBI_SCOPE


