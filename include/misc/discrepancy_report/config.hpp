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
 *  and reliability of the software and data,  the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties,  express or implied,  including
 *  warranties of performance,  merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Authors:  Colleen Bollin
 * Created:  17/09/2013 15:38:53
 */


#ifndef _CONFIG_H_
#define _CONFIG_H_

#include <corelib/ncbistd.hpp>
#include <serial/serialbase.hpp>

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(DiscRepNmSpc)

class CReportConfig : public CObject
{
public:
    CReportConfig() {}
    ~CReportConfig() {}

    bool IsEnabled(string test_name) { return m_TestList[NStr::ToUpper(test_name)]; }
    void Enable(string test_name, bool enable = true) { m_TestList[NStr::ToUpper(test_name)] = enable; }

    typedef enum {
        eConfigTypeDefault = 0,
        eConfigTypeBigSequences,
        eConfigTypeGenomes,
        eConfigTypeOnCaller,
        eConfigTypeMegaReport,
        eConfigTypeTSA
    } EConfigType;

    void Configure(EConfigType config_type);

    // short cut for disabling a specific group of tests
    void DisableTRNATests();

    vector<string> GetEnabledTestNames();

    // for setting a default lineage to be used by all tests
    string GetDefaultLineage() { return m_DefaultLineage; };
    void SetDefaultLineage(const string& lineage) { m_DefaultLineage = lineage; };


protected:

    typedef map <string, bool> TEnabledTestList;

    TEnabledTestList m_TestList;
    
    string m_DefaultLineage;
};

END_SCOPE(DiscRepNmSpc)
END_NCBI_SCOPE

#endif 
