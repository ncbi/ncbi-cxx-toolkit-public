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
 *   validation and cleanup of GeneOntology User-object
 *   .......
 *
 */
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbistr.hpp>

#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/Object_id.hpp>

#include <objtools/validator/go_term_validation_and_cleanup.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(validator)


static const string kGoTermText = "text string";
static const string kGoTermID = "go id";
static const string kGoTermPubMedID = "pubmed id";
static const string kGoTermRef = "go ref";
static const string kGoTermEvidence = "evidence";

static const string kGoTermProcess = "Process";
static const string kGoTermComponent = "Component";
static const string kGoTermFunction = "Function";

static const string kGeneOntology = "GeneOntology";

class CGoTermSortStruct
{
public:
    CGoTermSortStruct (const CUser_object::TData& sublist); // parse from field list


    static bool IsLegalGoTermType(const string& val);

    const string& GetTerm() const { return m_Term; }

    const string& GetGoid() const { return m_Goid; }

    int GetPmid() const { return m_Pmid; }

    const set<string>& GetEvidence() const { return m_Evidence; }

    const vector<string>& GetErrors() const { return m_Errors; }

protected:
    string m_Term;
    string m_Goid;
    int m_Pmid;
    set<string> m_Evidence;
    vector<string> m_Errors;

};



CGoTermSortStruct::CGoTermSortStruct(const CUser_object::TData& sublist) :
    m_Term(kEmptyStr), m_Goid(kEmptyStr), m_Pmid(0)
{
    m_Evidence.clear();
    m_Errors.clear();
    for (auto sub_it : sublist) {
        string label;
        if (sub_it->IsSetLabel() && sub_it->GetLabel().IsStr()) {
            label = sub_it->GetLabel().GetStr();
        }
        if (NStr::IsBlank(label)) {
            label = "[blank]";
        }
        if (NStr::Equal(label, kGoTermText)) {
            if (sub_it->GetData().IsStr()) {
                m_Term = sub_it->GetData().GetStr();
            } else {
                m_Errors.push_back("Bad data format for GO term qualifier term");
            }
        } else if (NStr::Equal(label, kGoTermID)) {
            if (sub_it->GetData().IsInt()) {
                m_Goid = NStr::IntToString(sub_it->GetData().GetInt());
            } else if (sub_it->GetData().IsStr()) {
                m_Goid = sub_it->GetData().GetStr();
            } else {
                m_Errors.push_back("Bad data format for GO term qualifier GO ID");
            }
        } else if (NStr::Equal(label, kGoTermPubMedID)) {
            if (sub_it->GetData().IsInt()) {
                m_Pmid = sub_it->GetData().GetInt();
            } else {
                m_Errors.push_back("Bad data format for GO term qualifier PMID");
            }
        } else if (NStr::Equal(label, kGoTermEvidence)) {
            if (sub_it->GetData().IsStr()) {
                m_Evidence.insert(sub_it->GetData().GetStr());
            } else {
                m_Errors.push_back("Bad data format for GO term qualifier evidence");
            }
        } else if (NStr::Equal(label, kGoTermRef)) {
            // recognized term

        } else {
            m_Errors.push_back("Unrecognized label on GO term qualifier field " + label);
        }
    }
}


bool operator<(const CGoTermSortStruct& l, const CGoTermSortStruct& r)
{
    int compare = NStr::Compare (l.GetTerm(), r.GetTerm());
    if (compare == 0) {
        compare = NStr::Compare (l.GetGoid(), r.GetGoid());
    }
    if (compare > 0) return false;
    if (compare < 0) return true;

    if (l.GetPmid() > r.GetPmid()) {
        return false;
    } else if (l.GetPmid() < r.GetPmid()) {
        return true;
    }

    auto ev1 = l.GetEvidence();
    auto ev2 = r.GetEvidence();
    if (ev1.size() > ev2.size()) {
        return false;
    } else if (ev1.size() < ev2.size()) {
        return true;
    }
    auto it1 = ev1.begin();
    auto it2 = ev2.begin();
    while (it1 != ev1.end() && it2 != ev2.end() && compare == 0) {
        compare = NStr::Compare(*it1, *it2);
        it1++;
        it2++;
    }

    return (compare < 0);
}


bool CGoTermSortStruct::IsLegalGoTermType(const string& val)
{
    if (NStr::EqualNocase(val, kGoTermProcess)
        || NStr::EqualNocase(val, kGoTermComponent)
        || NStr::EqualNocase(val, kGoTermFunction)
        || NStr::IsBlank(val)) {
        return true;
    } else {
        return false;
    }
}


bool IsGeneOntology(const CUser_object& user_object)
{
    if (user_object.IsSetType() && user_object.GetType().IsStr() &&
        NStr::EqualCase(user_object.GetType().GetStr(), kGeneOntology)) {
        return true;
    } else {
        return false;
    }
}


void GetGoTermErrors(CUser_object::TData field_list, map<string, string>& id_terms, vector<TGoTermError>& errors)
{
    set<CGoTermSortStruct> terms;

    size_t num_terms = 0;
    for (auto  it : field_list) {
        if (!it->IsSetData() || !it->GetData().IsFields()) {
            errors.push_back(TGoTermError(eErr_SEQ_FEAT_BadGeneOntologyFormat, "Bad GO term format"));
            continue;
        }

        CUser_object::TData sublist = it->GetData().GetFields();
        // create sort structure and add to set
        CGoTermSortStruct a(it->GetData().GetFields());
        terms.insert(a);
        // report errors
        for (auto msg : a.GetErrors()) {
            errors.push_back(TGoTermError(eErr_SEQ_FEAT_BadGeneOntologyFormat, msg));
        }
        if (NStr::IsBlank(a.GetGoid())) {
            errors.push_back(TGoTermError(eErr_SEQ_FEAT_GeneOntologyTermMissingGOID, "GO term does not have GO identifier"));
        }

        // add id/term pair
        pair<string, string> p(a.GetGoid(), a.GetTerm());
        auto s = id_terms.find(a.GetGoid());
        if (s == id_terms.end()) {
            id_terms[a.GetGoid()] = a.GetTerm();
        } else if (!NStr::Equal(a.GetTerm(), s->second)) {
            errors.push_back(TGoTermError(eErr_SEQ_FEAT_InconsistentGeneOntologyTermAndId,
                    "Inconsistent GO terms for GO ID " + a.GetGoid()));

        }
        num_terms++;
    }
    if (num_terms > terms.size()) {
        errors.push_back(TGoTermError(eErr_SEQ_FEAT_DuplicateGeneOntologyTerm, "Duplicate GO term on feature"));
    }
}


vector<TGoTermError> GetGoTermErrors(const CSeq_feat& feat)
{
    vector<TGoTermError> rval;

    if (!feat.IsSetExt()) {
        return rval;
    }
    const CUser_object& user_object = feat.GetExt();
    if (!IsGeneOntology(user_object) ||
        !user_object.IsSetData()) {
        return rval;
    }

    map<string, string> id_terms;
    // iterate through fields
    for (auto it : user_object.GetData()) {
        // validate terms if match accepted type
        if (!it->GetData().IsFields()) {
            rval.push_back(TGoTermError(eErr_SEQ_FEAT_BadGeneOntologyFormat, "Bad data format for GO term"));
        } else if (!it->IsSetLabel() || !it->GetLabel().IsStr() || !it->IsSetData()) {
            rval.push_back(TGoTermError(eErr_SEQ_FEAT_BadGeneOntologyFormat, "Unrecognized GO term label [blank]"));
        } else {
            string qualtype = it->GetLabel().GetStr();
            if (CGoTermSortStruct::IsLegalGoTermType(qualtype)) {
                if (it->IsSetData()
                    && it->GetData().IsFields()) {
                    GetGoTermErrors(it->GetData().GetFields(), id_terms, rval);
                }
            } else {
                rval.push_back(TGoTermError(eErr_SEQ_FEAT_BadGeneOntologyFormat, "Unrecognized GO term label " + qualtype));
            }
        }
    }
    return rval;
}


//LCOV_EXCL_START
//not used by validation, will be used by Genome Workbench menu item
bool RemoveDuplicateGoTerms(CUser_object::TData& field_list)
{
    bool rval = false;

    set<CGoTermSortStruct > terms;

    auto it = field_list.begin();
    while (it != field_list.end()) {
        if (!(*it)->IsSetData() || !(*it)->GetData().IsFields()) {
            ++it;
            continue;
        }

        // create sort structure and add to list if not already found
        CGoTermSortStruct a((*it)->GetData().GetFields());
        if (terms.find(a) != terms.end()) {
            it = field_list.erase(it);
            rval = true;
        } else {
            terms.insert(a);
            ++it;
        }
    }

    return rval;
}


bool RemoveDuplicateGoTerms(CSeq_feat& feat)
{
    bool rval = false;
    if (!feat.IsSetExt()) {
        return rval;
    }
    CUser_object& user_object = feat.SetExt();
    if (!IsGeneOntology(user_object) ||
        !user_object.IsSetData()) {
        return rval;
    }

    // iterate through fields
    for (auto it : user_object.SetData()) {
        // only remove duplicates from properly formmated fields with accepted type
        if (!it->GetData().IsFields()) {
            // skip it
        } else if (!it->IsSetLabel() || !it->GetLabel().IsStr() || !it->IsSetData()) {
            // skip it
        } else {
            string qualtype = it->GetLabel().GetStr();
            if (CGoTermSortStruct::IsLegalGoTermType(qualtype)) {
                if (it->IsSetData()
                    && it->GetData().IsFields()) {
                    rval |= RemoveDuplicateGoTerms(it->SetData().SetFields());
                }
            }
        }
    }
    return rval;
}


void SetGoTermValue(CUser_field& field, const string& val, const string& val_name)
{
    bool found_existing = false;
    if (field.IsSetData() && field.GetData().IsFields()) {
        auto it = field.SetData().SetFields().begin();
        while (it != field.SetData().SetFields().end()) {
            bool do_erase = false;
            if ((*it)->IsSetLabel() && (*it)->GetLabel().IsStr() &&
                NStr::Equal((*it)->GetLabel().GetStr(), val_name)) {
                if (found_existing) {
                    do_erase = true;
                } else {
                    (*it)->SetData().SetStr(val);
                    found_existing = true;
                }
            }
            if (do_erase) {
                it = field.SetData().SetFields().erase(it);
            } else {
                it++;
            }
        }
    }
    if (!found_existing) {
        CRef<CUser_field> go_id(new CUser_field());
        go_id->SetLabel().SetStr(val_name);
        go_id->SetData().SetStr(val);
        field.SetData().SetFields().push_back (go_id);
    }
}


void SetGoTermValue(CUser_field& field, int val, const string& val_name)
{
    bool found_existing = false;
    if (field.IsSetData() && field.GetData().IsFields()) {
        auto it = field.SetData().SetFields().begin();
        while (it != field.SetData().SetFields().end()) {
            bool do_erase = false;
            if ((*it)->IsSetLabel() && (*it)->GetLabel().IsStr() &&
                NStr::Equal((*it)->GetLabel().GetStr(), val_name)) {
                if (found_existing) {
                    do_erase = true;
                } else {
                    (*it)->SetData().SetInt(val);
                    found_existing = true;
                }
            }
            if (do_erase) {
                it = field.SetData().SetFields().erase(it);
            } else {
                it++;
            }
        }
    }
    if (!found_existing) {
        CRef<CUser_field> go_id(new CUser_field());
        go_id->SetLabel().SetStr(val_name);
        go_id->SetData().SetInt(val);
        field.SetData().SetFields().push_back (go_id);
    }
}


void ClearGoTermValue(CUser_field& field, const string& val_name)
{
    if (field.IsSetData() && field.GetData().IsFields()) {
        auto it = field.SetData().SetFields().begin();
        while (it != field.SetData().SetFields().end()) {
            if ((*it)->IsSetLabel() && (*it)->GetLabel().IsStr() &&
                NStr::Equal((*it)->GetLabel().GetStr(), val_name)) {
                it = field.SetData().SetFields().erase(it);
            } else {
                it++;
            }
        }
    }
}


void SetGoTermId(CUser_field& field, const string& val)
{
    SetGoTermValue(field, val, kGoTermID);
}


void SetGoTermText(CUser_field& field, const string& val)
{
    SetGoTermValue(field, val, kGoTermText);
}


void SetGoTermPMID(CUser_field& field, int pmid)
{
    SetGoTermValue(field, pmid, kGoTermPubMedID);
}


void AddGoTermEvidence(CUser_field& field, const string& val)
{
    CRef<CUser_field> go_id(new CUser_field());
    go_id->SetLabel().SetStr(kGoTermEvidence);
    go_id->SetData().SetStr(val);
    field.SetData().SetFields().push_back (go_id);
}


void ClearGoTermEvidence(CUser_field& field)
{
    ClearGoTermValue(field, kGoTermEvidence);
}


void ClearGoTermPMID(CUser_field& field)
{
    ClearGoTermValue(field, kGoTermPubMedID);
}


void AddGoTermToList(CSeq_feat& feat, CRef<CUser_field> field, const string& val_name)
{
    if (feat.IsSetExt() && !IsGeneOntology(feat.GetExt())) {
        return;
    } else if (!feat.IsSetExt()) {
        feat.SetExt().SetType().SetStr(kGeneOntology);
    }

    bool found_existing = false;
    if (feat.GetExt().IsSetData()) {
        for (auto it : feat.SetExt().SetData()) {
            if (it->IsSetLabel() &&
                it->GetLabel().IsStr() &&
                NStr::Equal(it->GetLabel().GetStr(), val_name) &&
                (!it->IsSetData() || it->GetData().IsFields())) {
                it->SetData().SetFields().push_back(field);
                found_existing = true;
            }
        }
    }
    if (!found_existing) {
        CRef<CUser_field> new_list(new CUser_field());
        new_list->SetLabel().SetStr(val_name);
        new_list->SetData().SetFields().push_back(field);
        feat.SetExt().SetData().push_back(new_list);
    }
}


void AddProcessGoTerm(CSeq_feat& feat, CRef<CUser_field> field)
{
    AddGoTermToList(feat, field, kGoTermProcess);
}


void AddComponentGoTerm(CSeq_feat& feat, CRef<CUser_field> field)
{
    AddGoTermToList(feat, field, kGoTermComponent);
}


void AddFunctionGoTerm(CSeq_feat& feat, CRef<CUser_field> field)
{
    AddGoTermToList(feat, field, kGoTermFunction);
}


size_t CountGoTerms(const CSeq_feat& feat, const string& list_name)
{
    if (!feat.IsSetExt() || !IsGeneOntology(feat.GetExt()) ||
        !feat.GetExt().IsSetData()) {
        return 0;
    }
    for (auto it : feat.GetExt().GetData()) {
        if (it->IsSetLabel() && it->GetLabel().IsStr() &&
            NStr::Equal(it->GetLabel().GetStr(), list_name) &&
            it->IsSetData() &&
            it->GetData().IsFields()) {
            return it->GetData().GetFields().size();
        }
    }
    return 0;
}


size_t CountProcessGoTerms(const CSeq_feat& feat)
{
    return CountGoTerms(feat, kGoTermProcess);
}


size_t CountComponentGoTerms(const CSeq_feat& feat)
{
    return CountGoTerms(feat, kGoTermComponent);
}


size_t CountFunctionGoTerms(const CSeq_feat& feat)
{
    return CountGoTerms(feat, kGoTermFunction);
}
//LCOV_EXCL_STOP


END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE
