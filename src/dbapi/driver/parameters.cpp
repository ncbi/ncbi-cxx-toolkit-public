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
#include <dbapi/driver/util/parameters.hpp>
#include <ctype.h>


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
    if ( !m_NofParams  || !m_Params)
        return;

    for (unsigned int i = 0;  i < m_NofParams;  i++) {
        if ((m_Params[i].status & fSet) != 0)
            delete m_Params[i].param;
    }
    delete [] m_Params;
}


void g_SubstituteParam(string& query, const string& name, const string& val)
{
    size_t name_len = name.length();
    size_t val_len = val.length();
    size_t len = query.length();
    char q = 0;

    for (size_t pos = 0;  pos < len;  pos++) {
        if (q) {
            if (query[pos] == q)
                q = 0;
            continue;
        }
        if (query[pos] == '"' || query[pos] == '\'') {
            q = query[pos];
            continue;
        }
        if (NStr::Compare(query, pos, name_len, name) == 0
            && (pos == 0 || !isalnum(query[pos - 1]))
            && !isalnum(query[pos + name_len])
            && query[pos + name_len] != '_') {
            query.replace(pos, name_len, val);
            len = query.length();
            pos += val_len;
        }
    }
}


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.13  2004/05/17 21:11:38  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.12  2003/06/04 18:18:46  soussov
 * Sets m_Params to 0 if no params in constructor
 *
 * Revision 1.11  2002/12/02 17:00:07  soussov
 * replacing delete m_Params with delete [] m_Params
 *
 * Revision 1.10  2002/11/27 19:26:10  soussov
 * fixes bug with erasing the empty strings
 *
 * Revision 1.9  2002/11/26 17:49:03  soussov
 * reallocation if more than declared parameters added
 *
 * Revision 1.8  2002/05/16 21:33:00  soussov
 * Replaces the temp fix in SetParam with the permanent one
 *
 * Revision 1.7  2002/05/15 22:29:40  soussov
 * temporary fix for SetParam
 *
 * Revision 1.6  2001/11/06 17:59:53  lavr
 * Formatted uniformly as the rest of the library
 *
 * Revision 1.5  2001/10/25 00:22:10  vakatov
 * string::compare() with 4 args does not work with GCC ver. < 3, so
 * use NStr::Compare() instead.
 *
 * Revision 1.4  2001/10/24 19:05:29  lavr
 * g_SubstituteParam() fixed to correctly replace names with values
 *
 * Revision 1.3  2001/10/22 15:21:45  lavr
 * +g_SubstituteParam
 *
 * Revision 1.2  2001/10/04 18:41:24  soussov
 * fixes bug in CDB_Params::SetParam
 *
 * Revision 1.1  2001/09/21 23:40:00  vakatov
 * -----  Initial (draft) revision.  -----
 * This is a major revamp (by Denis Vakatov, with help from Vladimir Soussov)
 * of the DBAPI "driver" libs originally written by Vladimir Soussov.
 * The revamp involved massive code shuffling and grooming, numerous local
 * API redesigns, adding comments and incorporating DBAPI to the C++ Toolkit.
 *
 * ===========================================================================
 */
