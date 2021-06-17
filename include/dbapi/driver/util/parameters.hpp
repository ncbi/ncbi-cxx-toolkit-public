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
#include <deque>

BEGIN_NCBI_SCOPE

namespace impl {

NCBI_DBAPIDRIVER_EXPORT
string
g_SubstituteParam(const string& query, const string& name, const string& val);

class NCBI_DBAPIDRIVER_EXPORT  CDB_Params
{
public:
    CDB_Params(void);
    ~CDB_Params();

    enum { kNoParamNumber = kMax_UInt };

    bool BindParam(unsigned int param_no, const string& param_name,
                   CDB_Object* param, bool is_out = false);
    bool SetParam(unsigned int param_no, const string& param_name,
                  CDB_Object* param, bool is_out = false);

    unsigned int NofParams() const {
        return static_cast<unsigned int>(m_Params.size());
    }

    CDB_Object* GetParam(unsigned int param_no) const {
        return (param_no >= NofParams()) ? 0 : m_Params[param_no].m_Param;
    }

    const string& GetParamName(unsigned int param_no) const {
        return (param_no >= NofParams()) ? kEmptyStr : m_Params[param_no].m_Name;
    }

    /// This method will throw an exception if parameter's name doesn't exist.
    unsigned int GetParamNum(const string& param_name) const;
    /// This method will create a parameter if it doesn't exist.
    unsigned int GetParamNum(unsigned int param_no, const string& param_name);
    
    enum EStatus {
        fBound  = 0x1,  //< the parameter is bound to some pointer
        fSet    = 0x2,  //< the parameter is set (value copied)
        fOutput = 0x4   //< it is "output" parameter
    };
    typedef int TStatus;

    TStatus GetParamStatus(unsigned int param_no) const {
        return (param_no >= NofParams()) ? 0 : m_Params[param_no].m_Status;
    }

    void LockBinding(void)
    {
	m_Locked = true;
    }
    bool IsLocked(void) const
    {
	return m_Locked;
    }

    /// Yield a lightweight copy of these parameters, sharing
    /// representations for variable-length datatypes.
    CDB_Params* SemiShallowClone(void) const;
private:
    // No exceptions are thrown ...
    bool GetParamNumInternal(const string& param_name, unsigned int& param_num) const;
    
    struct SParam {
        SParam(void);
        ~SParam(void);

        void Bind(const string& param_name, CDB_Object* param, bool is_out = false);
        void Set(const string& param_name, CDB_Object* param, bool is_out = false);
        void DeleteParam(void)
        {
            if ((m_Status & fSet) != 0) {
                delete m_Param;
                m_Status ^= fSet;
            }
        }

        string       m_Name;
        CDB_Object*  m_Param;
        TStatus      m_Status;
    };

    deque<SParam> m_Params;
    bool	  m_Locked;
};

}

END_NCBI_SCOPE


#endif  /* DBAPI_DRIVER_UTIL___PARAMETERS__HPP */


