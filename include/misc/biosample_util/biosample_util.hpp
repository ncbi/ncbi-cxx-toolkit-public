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
 * Author:  Colleen Bollin
 *
 * File Description:
 *   check biosource and structured comment descriptors against biosample database
 *
 */

#ifndef BIOSAMPLE_CHK__UTIL__HPP
#define BIOSAMPLE_CHK__UTIL__HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbistr.hpp>

#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqtable/Seq_table.hpp>
#include <objects/seqtable/SeqTable_column.hpp>

#include <objmgr/bioseq_handle.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(biosample_util)

CRef< CSeq_descr > GetBiosampleData(const string& accession, bool use_dev_server = false);

enum EStatus {
    eStatus_Unknown = 0,
    eStatus_Live,
    eStatus_Hup,
    eStatus_Withdrawn,
    eStatus_Suppressed,
    eStatus_ToBeCurated,
    eStatus_Replaced
};

typedef map<string, EStatus> TStatuses;
typedef pair<string, biosample_util::EStatus> TStatus;
EStatus GetBiosampleStatus(const string& accession, bool use_dev_server = false);
void GetBiosampleStatus(TStatuses& status, bool use_dev_server = false);
string GetBiosampleStatusName(EStatus status);


vector<string> GetBiosampleIDs(CBioseq_Handle bh);
vector<string> GetBioProjectIDs(CBioseq_Handle bh);


class CBiosampleFieldDiff : public CObject
{
public:
    CBiosampleFieldDiff() {};
    CBiosampleFieldDiff(const string& sequence_id, const string& biosample_id, const string& field_name, const string& src_val, const string& sample_val) :
        m_SequenceID(sequence_id), m_BiosampleID(biosample_id), m_FieldName(field_name), m_SrcVal(src_val), m_SampleVal(sample_val)
        {};
    CBiosampleFieldDiff(const string& sequence_id, const string& biosample_id, const CFieldDiff& diff) :
        m_SequenceID(sequence_id), m_BiosampleID(biosample_id),
        m_FieldName(diff.GetFieldName()),
        m_SrcVal(diff.GetSrcVal()),
        m_SampleVal(diff.GetSampleVal())
        {};

    ~CBiosampleFieldDiff(void) {};

    static void PrintHeader(ncbi::CNcbiOstream & stream, bool show_seq_id = true);
    void Print(ncbi::CNcbiOstream & stream, bool show_seq_id = true) const;
    void Print(ncbi::CNcbiOstream & stream, const CBiosampleFieldDiff& prev);
    const string& GetSequenceId() const { return m_SequenceID; };
    void SetSequenceId(const string& id) { m_SequenceID = id; };
    const string& GetFieldName() const { return m_FieldName; };
    string GetSrcVal() const { return CBioSource::IsStopWord(m_SrcVal) ? string("") : m_SrcVal; };
    string GetSampleVal() const { return CBioSource::IsStopWord(m_SampleVal) ? string("") : m_SampleVal; };
    const string& GetBioSample() const { return m_BiosampleID; };

    int CompareAllButSequenceID(const CBiosampleFieldDiff& other);
    int Compare(const CBiosampleFieldDiff& other);

private:
    string m_SequenceID;
    string m_BiosampleID;
    string m_FieldName;
    string m_SrcVal;
    string m_SampleVal;
};

typedef vector< CRef<CBiosampleFieldDiff> > TBiosampleFieldDiffList;

TBiosampleFieldDiffList
GetBioseqDiffs(CBioseq_Handle bh,
               const string& biosample_accession,
               size_t& num_processed,
               vector<string>& unprocessed_ids,
               bool use_dev_server = false,
               bool compare_structured_comments = false,
               const string& expected_prefix = "");

TBiosampleFieldDiffList GetFieldDiffs(const string& sequence_id, const string& biosample_id, const CBioSource& src, const CBioSource& sample);
TBiosampleFieldDiffList GetFieldDiffs(const string& sequence_id, const string& biosample_id, CConstRef<CUser_object> src, CConstRef<CUser_object> sample);

bool ResolveSuppliedBioSampleAccession(const string& biosample_accession, vector<string>& biosample_ids);

bool DoDiffsContainConflicts(const TBiosampleFieldDiffList& diffs, CNcbiOstream* log);

// This function is for generating a table of biosample values for a bioseq
// // that does not currently have a biosample ID
void AddBioseqToTable(CBioseq_Handle bh, CSeq_table& table, bool with_id,
                      bool include_comments = false, const string& expected_prefix = "");


string GetBestBioseqLabel(CBioseq_Handle bsh);
bool AttributeNamesAreEquivalent(string name1, string name2);

void PrintBioseqXML(CBioseq_Handle bh,
                    const string& id_prefix,
                    CNcbiOstream* report_stream,
                    const string& bioproject_accession,
                    const string& default_owner,
                    const string& hup_date,
                    const string& comment,
                    bool first_seq_only,
                    bool report_structured_comments,
                    const string& expected_prefix);

string OwnerFromAffil(const CAffil& affil);

END_SCOPE(biosample_util)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif //BIOSAMPLE_CHK__UTIL__HPP
