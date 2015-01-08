/*  $Id$
 * =========================================================================
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
 * =========================================================================
 *
 * Author:  Colleen Bollin
 *
 * File Description:
 *   class for storing tests for Discrepancy Report
 *
 * $Log:$ 
 */

#include <ncbi_pch.hpp>

#include <misc/discrepancy_report/analysis_workflow.hpp>

BEGIN_NCBI_SCOPE;

USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(DiscRepNmSpc);


void CAnalysisWorkflow::Configure(CReportConfig::EConfigType config_type)
{
    m_Config.Configure(config_type); 
    switch (config_type) {
        case CReportConfig::eConfigTypeOnCaller:
            m_GroupPolicy.Reset(new COncallerGroupPolicy());
            m_OrderPolicy.Reset(new COncallerOrderPolicy());
            break;
        case CReportConfig::eConfigTypeBigSequences:
        case CReportConfig::eConfigTypeGenomes:
        case CReportConfig::eConfigTypeTSA:
        case CReportConfig::eConfigTypeDefault:
            m_OrderPolicy.Reset(new CCommandLineOrderPolicy());
            break;
        default:
            break;
    }
}


void CAnalysisWorkflow::BuildTestList()
{
    ResetData();
    m_Handled.clear();
    m_TestList.clear();
    vector<string> enabled = m_Config.GetEnabledTestNames();
    ITERATE(vector<string>, test_name, enabled) {
        if (!m_Handled[*test_name]) {
            CRef<CAnalysisProcess> test = CAnalysisProcessFactory::Create(*test_name);
            if (test) {
                test->SetDefaultLineage(m_Config.GetDefaultLineage());
                m_TestList.push_back(test);
                vector<string> all_tests = test->Handles();
                ITERATE(vector<string>, h, all_tests) {
                    m_Handled[*h] = true;
                }
            }
        }
    }
}


void CAnalysisWorkflow::CollectData(CSeq_entry_Handle seh, bool drop_references, const string& filename)
{
    if (m_TestList.empty()) {
        BuildTestList();
    }
    CReportMetadata metadata;
    metadata.SetFilename(filename);

    NON_CONST_ITERATE(TReportTestList, it, m_TestList) {
        (*it)->CollectData(seh, metadata);
        if (drop_references) {
            (*it)->DropReferences(seh.GetScope());
        }
    }
}


void CAnalysisWorkflow::Collate()
{
    NON_CONST_ITERATE(TReportTestList, it, m_TestList) {
        (*it)->CollateData();
    }
}


TReportItemList CAnalysisWorkflow::GetResults()
{
    TReportItemList list;

    NON_CONST_ITERATE(TReportTestList, it, m_TestList) {
        TReportItemList items = (*it)->GetReportItems(m_Config);
        list.insert(list.end(), items.begin(), items.end());
    }

    if (m_GroupPolicy) {
        m_GroupPolicy->Group(list);
    }
    if (m_TagPolicy) {
        NON_CONST_ITERATE(TReportItemList, it, list) {
            m_TagPolicy->AddTag(**it);
        }
    }
    if (m_OrderPolicy) {
        m_OrderPolicy->Order(list);
    }

    return list;
}


void CAnalysisWorkflow::ResetData()
{
    NON_CONST_ITERATE(TReportTestList, it, m_TestList) {
        (*it)->ResetData();
    }
}


bool CAnalysisWorkflow::Autofix(CSeq_entry_Handle seh)
{
    bool rval = false;
    ResetData();
    if (m_TestList.empty()) {
        BuildTestList();
    }
    CReportMetadata metadata;
    NON_CONST_ITERATE(TReportTestList, it, m_TestList) {
        (*it)->CollectData(seh, metadata);
        (*it)->CollateData();
        if ((*it)->Autofix(seh.GetScope())) {
            rval = true;
        }
    }
    ResetData();
    return rval;
}


END_NCBI_SCOPE
