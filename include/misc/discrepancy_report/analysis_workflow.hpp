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


#ifndef _ANALYSIS_WORKFLOW_H_
#define _ANALYSIS_WORKFLOW_H_

#include <corelib/ncbistd.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <misc/discrepancy_report/config.hpp>
#include <misc/discrepancy_report/analysis_process.hpp>
#include <misc/discrepancy_report/tag_policy.hpp>
#include <misc/discrepancy_report/group_policy.hpp>
#include <misc/discrepancy_report/order_policy.hpp>

BEGIN_NCBI_SCOPE

USING_SCOPE(objects);
BEGIN_SCOPE(DiscRepNmSpc);


// The CAnalysisWorkflow class is used to tie together various pieces of the 
// Discrepancy Report library. To generate a report, you would:
// 1)	Use the Enable method and/or Configure method to enable/disable tests
// 2)	Call the BuildTestList method to create a list of CAnalysisProcess objects
//      to use to evaluate the Seq-entry Handles.
// 3)	Call the CollectData method with as many different CSeq-entry Handles
//      as you wish to compare against each other for this report.
// 4)	Call the Collate method to collate data for all CAnalysisProcess objects 
//      after CollectData has been called for all of the CSeq-entry Handles
//      you wish to evaluate.
// 5)	Optionally specify a CTagPolicy, a CGroupPolicy, and/or a COrderPolicy.
//      This can be done at any time prior to calling the GetResults method,
//      and you could change the policies before calling GetResults again if
//       you wished.
// 6)	Call the GetResults method to produce a list of CReportItem objects. 
//      You can then display these with a GUI or use the CReportItem::Format
//      method to generate a text file. GetResults will perform the grouping,
//      tagging, and ordering actions from the policy objects specified before
//      GetResults is called.
// 7)	Optionally call ResetData to reuse this CAnalysisWorkflow object on another
//      record.
//
// Note that you could run each test individually by enabling just one test 
// each time to avoid keeping all results in memory, but then you would not 
// be able to use the grouping or ordering policies as effectively.

class NCBI_DISCREPANCY_REPORT_EXPORT CAnalysisWorkflow : public CObject
{
public:
    CAnalysisWorkflow() 
        : m_GroupPolicy(NULL),
        m_TagPolicy(NULL),
        m_OrderPolicy(new COrderPolicy())
        {};
    ~CAnalysisWorkflow() {};

    // The Configure method is a shorthand for creating a particular configuration
    // type, including the appropriate Tag Policy, Group Policy, Order Policy,
    // and list of tests to be enabled
    void Configure(CReportConfig::EConfigType config_type);

    // Set or replace an existing Tag Policy
    void SetTagPolicy(CRef<CTagPolicy> policy) { m_TagPolicy = policy; };

    // Set or replace an existing Group Policy
    void SetGroupPolicy(CRef<CGroupPolicy> policy) { m_GroupPolicy = policy; };

    // Set or replace an existing Order Policy
    void SetOrderPolicy(CRef<COrderPolicy> policy) { m_OrderPolicy = policy; };

    // access to CConfig object directly
    CReportConfig& SetConfig() { return m_Config; };

    // Pass-through to m_Config to enable/disable individual tests
    void Enable(string test_name, bool enable = true) { m_Config.Enable(test_name, enable); };

    // Pass-through to m_Config
    void DisableTRNATests() { m_Config.DisableTRNATests(); };

    // Call BuildTestList only after you are done enabling/disabling tests
    // This populates m_TestList with the list of CAnalysisProcess objects to handle
    // all enabled tests
    void BuildTestList();

    // Call CollectData to add data from this CSeq_entry_Handle to create the metadata
    // for the CSeq_entry_Handle and then call the CollectData method for each 
    // CAnalysisProcess object in m_TestList
    void CollectData(CSeq_entry_Handle seh, bool drop_references = false, const string& filename = kEmptyStr);

    // Calls the Collate method for each CAnalysisProcess object in m_TestList
    void Collate();

    // Calls the GetReportItems method for each CAnalysisProcess object in m_TestList,
    // then applies the group, tag, and order policy to the list of results
    TReportItemList GetResults();

    // Calls the ResetData method for each CAnalysisProcess object in m_TestList,
    // so that you could run this report maker on a new set of CSeq_entry_Handle
    // objects with the same test list and policies
    void ResetData();

    // runs Autofix for enabled tests, returns true if any changes were made
    // note: needs to re-run each test because autofixes from previous tests
    // may have affected results for following tests
    bool Autofix(CSeq_entry_Handle seh);

    
protected:
    CReportConfig m_Config;

    typedef vector<CRef<CAnalysisProcess> > TReportTestList;
    TReportTestList m_TestList;
    typedef map<string,bool> THandledTestMap;
    THandledTestMap m_Handled;

    CRef<CGroupPolicy> m_GroupPolicy;
    CRef<CTagPolicy> m_TagPolicy;
    CRef<COrderPolicy> m_OrderPolicy;

};

END_SCOPE(DiscRepNmSpc);
END_NCBI_SCOPE

#endif 
