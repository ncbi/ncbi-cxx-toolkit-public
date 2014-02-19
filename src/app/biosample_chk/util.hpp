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

CRef< CSeq_descr > GetBiosampleData(string accession, bool use_dev_server);

vector<string> GetBiosampleIDs(CBioseq_Handle bh);
vector<string> GetBioProjectIDs(CBioseq_Handle bh);

class CBiosampleFieldDiff : public CObject
{
public:
    CBiosampleFieldDiff() {};
    CBiosampleFieldDiff(string sequence_id, string biosample_id, string field_name, string src_val, string sample_val) :
        m_SequenceID(sequence_id), m_BiosampleID(biosample_id), m_FieldName(field_name), m_SrcVal(src_val), m_SampleVal(sample_val)
        {};

    ~CBiosampleFieldDiff(void) {};

    static void PrintHeader(ncbi::CNcbiOstream & stream, bool show_seq_id = true);
    void Print(ncbi::CNcbiOstream & stream, bool show_seq_id = true) const;
    void Print(ncbi::CNcbiOstream & stream, const CBiosampleFieldDiff& prev);
    const string& GetSequenceId() const { return m_SequenceID; };
    void SetSequenceId(string id) { m_SequenceID = id; };
    const string& GetFieldName() const { return m_FieldName; };
    const string& GetSrcVal() const { return m_SrcVal; };
    const string& GetSampleVal() const { return m_SampleVal; };
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


TBiosampleFieldDiffList GetFieldDiffs(string sequence_id, string biosample_id, const CBioSource& src, const CBioSource& sample);
TBiosampleFieldDiffList GetFieldDiffs(string sequence_id, string biosample_id, CConstRef<CUser_object> src, CConstRef<CUser_object> sample);

CRef<objects::CSeqTable_column> FindSeqTableColumnByName (CRef<objects::CSeq_table> values_table, string column_name);
void AddValueToColumn (CRef<CSeqTable_column> column, string value, size_t row);
void AddValueToTable (CRef<CSeq_table> table, string column_name, string value, size_t row);

string GetValueFromColumn(const CSeqTable_column& column, size_t row);
string GetValueFromTable(const CSeq_table& table, string column_name, size_t row);


bool AttributeNamesAreEquivalent (string name1, string name2);


END_SCOPE(objects)
END_NCBI_SCOPE

#endif //BIOSAMPLE_CHK__UTIL__HPP
