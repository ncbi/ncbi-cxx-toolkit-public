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


#ifndef _SRC_QUAL_PROBLEM_H_
#define _SRC_QUAL_PROBLEM_H_

#include <corelib/ncbistd.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <misc/discrepancy_report/analysis_process.hpp>

#include <objects/seqfeat/BioSource.hpp>

BEGIN_NCBI_SCOPE

USING_SCOPE(objects);
BEGIN_SCOPE(DiscRepNmSpc);

class CSrcQualItem : public CObject
{
public:
    CSrcQualItem() {};
    CSrcQualItem(const string& qual_name) 
        : m_QualName(qual_name), 
          m_AllSame(true), 
          m_AllUnique(true),
          m_AllPresent(true),
          m_SomeMulti(false)
          {};
    ~CSrcQualItem() {};

    void AddValue(const string& val, size_t obj_num);
    void CheckForMissing(size_t obj_num);
    void AddMissing(size_t obj_num) { m_Missing.push_back(obj_num); m_AllPresent = false; };
    bool AllUnique();

    CRef<CReportItem> MakeReportItem(const string& setting_name, const TReportObjectList& objs) const;

protected:
    string m_QualName;
    vector<size_t> m_Missing;
    vector<size_t> m_Multi;
    typedef map<string, vector<size_t> > TValToObjMap;
    TValToObjMap m_ObjsPerValue;
    typedef map<size_t, vector<string> > TObjToValMap;
    TObjToValMap m_ValuesPerObj;
    bool m_AllSame;
    bool m_AllUnique;
    bool m_AllPresent;
    bool m_SomeMulti;

    typedef map<size_t, bool> TReportedObj;

    void x_AddItemsFromList(CReportItem& item, const vector<size_t>& list, const TReportObjectList& objs) const;
    static bool x_QualifierProblemMayBeFatal(const string& qual_name);
};


class CSrcQualProblem : public CAnalysisProcess
{
public:
    CSrcQualProblem();
    ~CSrcQualProblem() {};

    virtual void CollectData(CSeq_entry_Handle seh, const CReportMetadata& metadata);
    virtual void CollateData();
    virtual void ResetData();
    virtual TReportItemList GetReportItems(CReportConfig& cfg) const;
    virtual vector<string> Handles() const;
    virtual void DropReferences(CScope& scope);

protected:
    TReportObjectList m_Objs;

    typedef map<string, CRef<CSrcQualItem> > TQualReportList;
    TQualReportList m_QualReports;

    // for values seen in more than one qualifier on the same BioSource
    typedef pair<string, string> TMultiQualDesc;
    typedef pair<TMultiQualDesc, size_t> TMultiQualItem;
    typedef vector<TMultiQualItem> TMultiQualItemList;
    TMultiQualItemList m_MultiQualItems;

    typedef map<string, vector<string> > TSameValDiffQualList;

    void x_AddToMaps(const string& qual_name, const string& qual_val, 
                     TSameValDiffQualList& vals_seen, 
                     size_t idx);

    void x_AddValuesFromSrc(const CBioSource& src, size_t index);
};


END_SCOPE(DiscRepNmSpc);
END_NCBI_SCOPE

#endif 
