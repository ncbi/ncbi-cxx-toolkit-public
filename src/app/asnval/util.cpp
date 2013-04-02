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
 *      Implementation of utility classes and functions for biosample_chk.
 *
 */
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbistr.hpp>

#include <serial/enumvalues.hpp>
#include <serial/serialimpl.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/misc/sequence_macros.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/object_manager.hpp>

#include <vector>
#include <algorithm>
#include <list>

// for biosample fetching
#include <objects/seq/Seq_descr.hpp>
#include <connect/ncbi_conn_stream.hpp>
#include <serial/objistrasn.hpp>

#include "util.hpp"
#include "src_table_column.hpp"

#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/Object_id.hpp>
#include <connect/ncbi_conn_stream.hpp>
#include <serial/objistrasn.hpp>
#include <serial/objistr.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CRef< CSeq_descr >
GetBiosampleData(string accession)
{
    string host = "intranet";
    string path = "/biosample/fetch.cgi";
    string args = "accession=" + accession + "&format=asn1";
    CConn_HttpStream http_stream(host, path, args);
    auto_ptr<CObjectIStream> in_stream;
    in_stream.reset(new CObjectIStreamAsn(http_stream));
 
		CRef< CSeq_descr > response(new CSeq_descr());
		*in_stream >> *response;

    return response;
}


vector<string> GetBiosampleIDs(const CUser_object& user)
{
    vector<string> ids;

    if (!user.IsSetType() || !user.GetType().IsStr() || !NStr::EqualNocase(user.GetType().GetStr(), "DBLink")) {
        // Not DBLink object
        return ids;
    }
    try {
        const CUser_field& field = user.GetField("BioSample");
        if (field.IsSetData() && field.GetData().IsStrs()) {
            ITERATE(CUser_field::TData::TStrs, it, field.GetData().GetStrs()) {
                ids.push_back(*it);
            }
        }
    } catch (...) {
        // no biosample ID field
    }        

    return ids;
}


vector<string> GetBiosampleIDs(const CSeqdesc& seqdesc)
{
    vector<string> ids;

    if (seqdesc.IsUser()) {
        ids = GetBiosampleIDs(seqdesc.GetUser());
    }
    return ids;
}


vector<string> GetBiosampleIDs(CBioseq_Handle bh)
{
    vector<string> ids;

    CSeqdesc_CI desc_ci(bh, CSeqdesc::e_User);
    while (desc_ci) {
        vector<string> new_ids = GetBiosampleIDs(*desc_ci);
        ITERATE(vector<string>, s, new_ids) {
            ids.push_back(*s);
        }
        ++desc_ci;
    }
    return ids;
}


void CBiosampleFieldDiff::Print(CNcbiOstream& stream)
{
    stream << m_BiosampleID << "\t";
    stream << m_FieldName << "\t";
    stream << m_SequenceID << "\t";
    stream << m_SrcVal << "\t";
    stream << m_SampleVal << "\t";
    stream << endl;
}


void CBiosampleFieldDiff::Print(ncbi::CNcbiOstream & stream, const CBiosampleFieldDiff& prev)
{
    if (!NStr::EqualNocase(m_BiosampleID, prev.m_BiosampleID)) {
        Print(stream);
    } else {
        stream << "\t";
        if (!NStr::EqualNocase(m_FieldName, prev.m_FieldName)) {
            stream << m_FieldName;
        }
        stream << "\t";
        stream << m_SequenceID << "\t";
        stream << m_SrcVal << "\t";
        stream << m_SampleVal << "\t";
        stream << endl;
    }
}


int CBiosampleFieldDiff::CompareAllButSequenceID(const CBiosampleFieldDiff& other)
{
    int cmp = NStr::CompareCase(m_BiosampleID, other.m_BiosampleID);
    if (cmp == 0) {
        cmp = NStr::CompareNocase(m_FieldName, other.m_FieldName);
        if (cmp == 0) {
            cmp = NStr::CompareNocase(m_SrcVal, other.m_SrcVal);
            // note - if BioSample ID is the same, sample_val should also be the same
        }
    }

    return cmp;
}


int CBiosampleFieldDiff::Compare(const CBiosampleFieldDiff& other)
{
    int cmp = CompareAllButSequenceID(other);
    if (cmp == 0) {
        cmp = NStr::CompareCase(m_SequenceID, other.m_SequenceID);
    }

    return cmp;
}


bool s_CompareSourceFields (CSrcTableColumnBase * f1, CSrcTableColumnBase * f2)
{ 
    if (!f1) {
        return true;
    } else if (!f2) {
        return false;
    } 
    string name1 = f1->GetLabel();
    string name2 = f2->GetLabel();
    int cmp = NStr::Compare (name1, name2);
    if (cmp < 0) {
        return true;
    } else {
        return false;
    }        
}


vector<CSrcTableColumnBase * > GetAvailableFields(vector<CConstRef<CBioSource> > src)
{
    vector<CSrcTableColumnBase * > fields;

    ITERATE(vector<CConstRef<CBioSource> >, it, src) {
        vector<CSrcTableColumnBase * > src_fields = GetSourceFields(**it);
        fields.insert(fields.end(), src_fields.begin(), src_fields.end());
    }

    // no need to sort and unique if there are less than two fields
    if (fields.size() < 2) {
        return fields;
    }
    sort(fields.begin(), fields.end(), s_CompareSourceFields);

    vector<CSrcTableColumnBase * >::iterator f_prev = fields.begin();
    vector<CSrcTableColumnBase * >::iterator f_next = f_prev;
    f_next++;
    while (f_next != fields.end()) {
        if (NStr::Equal((*f_prev)->GetLabel(), (*f_next)->GetLabel())) {
            CSrcTableColumnBase *to_delete = *f_next;
            f_next = fields.erase(f_next);
            delete to_delete;
        } else {
            ++f_prev;
            ++f_next;
        }
    }

    return fields;
}


typedef enum {
  eConflictIgnoreAll = 0,
  eConflictIgnoreMissingInBioSource,
  eConflictIgnoreMissingInBioSample
} EConflictIgnoreType;


typedef struct ignoreconflict {
  string qual_name;
  EConflictIgnoreType ignore_type;
} IgnoreConflictData;


static IgnoreConflictData sIgnoreConflictList[] = {
  { "rev-primer-name", eConflictIgnoreMissingInBioSample } ,
  { "rev-primer-seq", eConflictIgnoreMissingInBioSample } ,
  { "fwd-primer-name", eConflictIgnoreMissingInBioSample } ,
  { "fwd-primer-seq", eConflictIgnoreMissingInBioSample } ,
  { "environmental-sample", eConflictIgnoreMissingInBioSample } ,
  { "germline", eConflictIgnoreMissingInBioSample } ,
  { "endogenous-virus-name", eConflictIgnoreMissingInBioSample } ,
  { "map", eConflictIgnoreMissingInBioSample } ,
  { "metagenomic", eConflictIgnoreMissingInBioSample } ,
  { "plasmid-name", eConflictIgnoreMissingInBioSample } ,
  { "plastid-name", eConflictIgnoreMissingInBioSample } ,
  { "chromosome", eConflictIgnoreMissingInBioSample } ,
  { "map", eConflictIgnoreMissingInBioSample } ,
  { "linkage-group", eConflictIgnoreMissingInBioSample } ,
  { "rearranged", eConflictIgnoreMissingInBioSample } ,
  { "segment", eConflictIgnoreMissingInBioSample } ,
  { "transgenic", eConflictIgnoreMissingInBioSample } ,
  { "old lineage", eConflictIgnoreMissingInBioSample } ,
  { "old name", eConflictIgnoreMissingInBioSample } ,
  { "lineage", eConflictIgnoreAll } ,
  { "biovar", eConflictIgnoreAll } ,
  { "chemovar", eConflictIgnoreAll } ,
  { "forma", eConflictIgnoreAll } ,
  { "forma-specialis", eConflictIgnoreAll } ,
  { "pathovar", eConflictIgnoreAll } ,
  { "serotype", eConflictIgnoreAll } ,
  { "serovar", eConflictIgnoreAll } ,
  { "subspecies", eConflictIgnoreAll } ,
  { "variety", eConflictIgnoreAll } };

static const int kNumIgnoreConflictList = sizeof (sIgnoreConflictList) / sizeof (IgnoreConflictData);

static bool s_ShouldIgnoreConflict(string label, string src_val, string sample_val)
{
    int i;
    bool rval = false;

    if (NStr::EqualNocase(src_val, sample_val)) {
        return true;
    }
    for (i = 0; i < kNumIgnoreConflictList; i++) {
        if (NStr::EqualNocase (label, sIgnoreConflictList[i].qual_name)) {
            switch (sIgnoreConflictList[i].ignore_type) {
                case eConflictIgnoreAll:
                    rval = true;
                    break;
                case eConflictIgnoreMissingInBioSource:
                    if (NStr::IsBlank(src_val)) {
                      rval = true;
                    }
                    break;
                case eConflictIgnoreMissingInBioSample:
                    if (NStr::IsBlank(sample_val)) {
                      rval = true;
                    }
                    break;
            }
            break;
        }
    }
    return rval;
}


vector<CBiosampleFieldDiff *> GetFieldDiffs(string sequence_id, string biosample_id, const CBioSource& src, const CBioSource& sample)
{
    vector<CBiosampleFieldDiff *> rval;

    vector<CConstRef<CBioSource> > src_list;
    CConstRef<CBioSource> s1(&src);
    src_list.push_back(s1);
    CConstRef<CBioSource> s2(&sample);
    src_list.push_back(s2);

    vector<CSrcTableColumnBase * > field_list = GetAvailableFields (src_list);

    ITERATE(vector<CSrcTableColumnBase * >, it, field_list) {
        string src_val = (*it)->GetFromBioSource(src);
        string sample_val = (*it)->GetFromBioSource(sample);
        if (!s_ShouldIgnoreConflict((*it)->GetLabel(), src_val, sample_val)) {
            CBiosampleFieldDiff *diff = new CBiosampleFieldDiff(sequence_id, biosample_id, (*it)->GetLabel(), src_val, sample_val);
            rval.push_back(diff);
        }
    }

    ITERATE(vector<CSrcTableColumnBase * >, it, field_list) {
        CSrcTableColumnBase * to_delete = *it;
        delete to_delete;
    }
    field_list.clear();

    return rval;
}


END_SCOPE(objects)
END_NCBI_SCOPE
