#ifndef DBAPI_DRIVER_UTIL___PARAMETERS__HPP
#define DBAPI_DRIVER_UTIL___PARAMETERS__HPP

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
 * File Description:  Param container
 *
 */

#include <dbapi/driver/types.hpp>


BEGIN_NCBI_SCOPE


void g_SubstituteParam(string& query, const string& name, const string& val);


class CDB_Params
{
public:
    CDB_Params(unsigned int nof_params = 0);

    // static const unsigned int kNoParamNumber = kMax_UInt; <<=not compatible with MS compiler
    enum { kNoParamNumber = kMax_UInt };

    bool BindParam(unsigned int param_no, const string& param_name,
                   CDB_Object* param, bool is_out = false);
    bool SetParam(unsigned int param_no, const string& param_name,
                  CDB_Object* param, bool is_out = false);

    unsigned int NofParams() const {
        return m_NofParams;
    }

    CDB_Object* GetParam(unsigned int param_no) const {
        return (param_no >= m_NofParams) ? 0 : m_Params[param_no].param;
    }

    const string& GetParamName(unsigned int param_no) const {
        return (param_no >= m_NofParams) ? kEmptyStr : m_Params[param_no].name;
    }

    enum EStatus {
        fBound  = 0x1,  // the parameter is bound to some pointer
        fSet    = 0x2,  // the parameter is set (value copied)
        fOutput = 0x4   // it is "output" parameter
    };
    typedef int TStatus;

    TStatus GetParamStatus(unsigned int param_no) const {
        return (param_no >= m_NofParams) ? 0 : m_Params[param_no].status;
    }

    ~CDB_Params();

private:
    unsigned int m_NofParams;

    struct SParam {
        string       name;
        CDB_Object*  param;
        TStatus      status;
    };
    SParam* m_Params;
};


END_NCBI_SCOPE


#endif  /* DBAPI_DRIVER_UTIL___PARAMETERS__HPP */



/*
 * ===========================================================================
 * $Log$
 * Revision 1.4  2001/12/28 21:22:39  sapojnik
 * Made compatible with MS compiler: long long to Int8, static const within class def to enum
 *
 * Revision 1.3  2001/11/06 17:58:07  lavr
 * Formatted uniformly as the rest of the library
 *
 * Revision 1.2  2001/10/22 15:18:29  lavr
 * + g_SubstituteParam
 *
 * Revision 1.1  2001/09/21 23:39:55  vakatov
 * -----  Initial (draft) revision.  -----
 * This is a major revamp (by Denis Vakatov, with help from Vladimir Soussov)
 * of the DBAPI "driver" libs originally written by Vladimir Soussov.
 * The revamp involved massive code shuffling and grooming, numerous local
 * API redesigns, adding comments and incorporating DBAPI to the C++ Toolkit.
 *
 * ===========================================================================
 */

