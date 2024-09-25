#ifndef SAMPLE__DBAPI_ADVANCED_FEATURES_COMMON__HPP
#define SAMPLE__DBAPI_ADVANCED_FEATURES_COMMON__HPP

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
 * Authors: David McElhany, Pavel Ivanov, Michael Kholodov
 *
 */

 /// @dbapi_simple_common.hpp
 /// CNcbiSample_Dbapi_Advanced_Features class definition, to be used by
 /// 'dbapi_advanced_features' test and the NEW_PROJECT-class utilities.


#include <sample/ncbi_sample_api.hpp>


/** @addtogroup Sample
 *
 * @{
 */

USING_NCBI_SCOPE;


/// CNcbiSample_Dbapi_Advanced_Features -- class describing 
/// 'dbapi_advanced_features' test business logic.
/// 
class CNcbiSample_Dbapi_Advanced_Features : public CNcbiSample
{
public:
    CNcbiSample_Dbapi_Advanced_Features() {}

    /// Sample description
    virtual string Description(void) { return "DBAPI Advanced Features sample"; }

    /// Initialize app parameters with hard-coded values,
    /// they will be redefined in the main application.
    /// 
    virtual void Init(void)
    {
        m_ServerName = "My_Server_Name";
        m_DriverName = "My_Driver_Name";
    }

    /// Sample's business logic
    virtual int Run(void)
    {
        int exit_code = RunSample();
        return exit_code;
    }

    /// Cleanup
    void Exit(void) {}

public:
    int RunSample(void);

public:
    // Run parameters
    string m_ServerName;
    string m_DriverName;
};


/* @} */

#endif // SAMPLE__DBAPI_ADVANCED_FEATURES_COMMON__HPP
