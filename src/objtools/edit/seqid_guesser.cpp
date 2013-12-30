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
 */


#include <ncbi_pch.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/scope.hpp>
#include <objtools/edit/seqid_guesser.hpp>


BEGIN_NCBI_SCOPE

USING_SCOPE(ncbi::objects);
BEGIN_SCOPE(objects)
BEGIN_SCOPE(edit)


CSeqIdGuesser::CSeqIdGuesser(CSeq_entry_Handle entry) : m_SeqEntry(entry) 
{
    // to do: build up table of strings and seq-ids
    CBioseq_CI bi(m_SeqEntry);
    while (bi) {
        string local = "";
        ITERATE(CBioseq::TId, id, bi->GetCompleteBioseq()->GetId()) {
            if ((*id)->IsLocal()) {
                if ((*id)->GetLocal().IsId()) {
                    local = NStr::NumericToString((*id)->GetLocal().GetId());
                } else {
                    local = (*id)->GetLocal().GetStr();
                }
            }
        }
        ITERATE(CBioseq::TId, id, bi->GetCompleteBioseq()->GetId()) {
            string label = "";
            (*id)->GetLabel(&label);
            m_StringIdHash.insert(TStringIdHash::value_type(label, *id));
            label = "";
            (*id)->GetLabel(&label, CSeq_id::eContent);
            m_StringIdHash.insert(TStringIdHash::value_type(label, *id));
            if ((*id)->IsGenbank()) {
                size_t pos = NStr::Find(label, ".");
                if (pos != string::npos) {
                    m_StringIdHash.insert(TStringIdHash::value_type(label.substr(0, pos), *id));
                }
            } else if ((*id)->IsGeneral()) {
                const CDbtag& dbtag = (*id)->GetGeneral();
                if (dbtag.IsSetDb() && dbtag.IsSetTag()) {
                    string tag_str = "";
                    if (dbtag.GetTag().IsId()) {
                        tag_str = NStr::NumericToString(dbtag.GetTag().GetId());
                    } else {
                        tag_str = dbtag.GetTag().GetStr();
                    }
                    m_StringIdHash.insert(TStringIdHash::value_type(tag_str, *id));
                    // BankIt IDs can be specified with "BankIt" prefix
                    if (NStr::EqualNocase(dbtag.GetDb(), "BankIt")) {
                        m_StringIdHash.insert(TStringIdHash::value_type("BankIt" + tag_str, *id));
                    } else if (NStr::EqualNocase(dbtag.GetDb(), "NCBIFILE")) {
                        // File ID are usually a filename plus a slash plus the original local ID
                        // add the filename as its own identifier, but only add the local ID if it hasn't 
                        // already been encountered
                        size_t pos = NStr::Find(tag_str, "/", 0, string::npos, NStr::eLast);
                        if (pos != string::npos) {
                            string file = tag_str.substr(0, pos);
                            m_StringIdHash.insert(TStringIdHash::value_type(file, *id));
                            string file_id = tag_str.substr(pos + 1);
                            if (!NStr::EqualNocase(file_id, local)) {
                                m_StringIdHash.insert(TStringIdHash::value_type(file_id, *id));
                            }
                        } 
                    }
                }                 
            }
        }
        ++bi;
    }
}


vector<string> CSeqIdGuesser::GetIdStrings(CBioseq_Handle bsh)
{
    vector<string> id_str;

    string local = "";
    ITERATE(CBioseq::TId, id, bsh.GetCompleteBioseq()->GetId()) {
        if ((*id)->IsLocal()) {
            if ((*id)->GetLocal().IsId()) {
                local = NStr::NumericToString((*id)->GetLocal().GetId());
            } else {
                local = (*id)->GetLocal().GetStr();
            }
        }
    }
    ITERATE(CBioseq::TId, id, bsh.GetCompleteBioseq()->GetId()) {
        string label = "";
        (*id)->GetLabel(&label);
        id_str.push_back(label);
        label = "";
        (*id)->GetLabel(&label, CSeq_id::eContent);
        id_str.push_back(label);
        if ((*id)->IsGenbank()) {
            size_t pos = NStr::Find(label, ".");
            if (pos != string::npos) {
                id_str.push_back(label.substr(0, pos));
            }
        } else if ((*id)->IsGeneral()) {
            const CDbtag& dbtag = (*id)->GetGeneral();
            if (dbtag.IsSetDb() && dbtag.IsSetTag()) {
                string tag_str = "";
                if (dbtag.GetTag().IsId()) {
                    tag_str = NStr::NumericToString(dbtag.GetTag().GetId());
                } else {
                    tag_str = dbtag.GetTag().GetStr();
                }
                id_str.push_back(tag_str);
                // BankIt IDs can be specified with "BankIt" prefix
                if (NStr::EqualNocase(dbtag.GetDb(), "BankIt")) {
                    id_str.push_back("BankIt" + tag_str);
                } else if (NStr::EqualNocase(dbtag.GetDb(), "NCBIFILE")) {
                    // File ID are usually a filename plus a slash plus the original local ID
                    // add the filename as its own identifier, but only add the local ID if it hasn't 
                    // already been encountered
                    size_t pos = NStr::Find(tag_str, "/", 0, string::npos, NStr::eLast);
                    if (pos != string::npos) {
                        string file = tag_str.substr(0, pos);
                        id_str.push_back(file);                       
                        string file_id = tag_str.substr(pos + 1);
                        if (!NStr::EqualNocase(file_id, local)) {
                            id_str.push_back(file_id);
                        }
                    } 
                }
            }                 
        }
    }
    return id_str;
}


CRef<CSeq_id> CSeqIdGuesser::Guess(const string& id_str)
{
    TStringIdHash::iterator it = m_StringIdHash.find(id_str);
    if (it == m_StringIdHash.end()) {
        CRef<CSeq_id> empty(NULL);
        return empty;
    } else {
        return it->second;
    }
}


vector<CBioseq_Handle> CSeqIdGuesser::FindMatches(CRef<CStringConstraint> string_constraint)
{
    vector<CBioseq_Handle> bsh_list;

    if (!string_constraint) {
        // return all bioseqs
    } else if (string_constraint->GetMatchType() == CStringConstraint::eMatchType_Equals
               && !string_constraint->GetIgnoreCase()
               && !string_constraint->GetIgnoreSpace()) {
        CRef<CSeq_id> id = Guess(string_constraint->GetMatchText());
        if (id) {
            CBioseq_Handle bsh = m_SeqEntry.GetScope().GetBioseqHandle(*id);
            if (bsh) {
                bsh_list.push_back(bsh);
            }
        }
    } else {
        TStringIdHash::iterator it = m_StringIdHash.begin();
        while (it != m_StringIdHash.end()) {
            // look for matches
            if (string_constraint->DoesTextMatch(it->first)) {
                CBioseq_Handle bsh = m_SeqEntry.GetScope().GetBioseqHandle(*(it->second));
                if (bsh) {
                    bsh_list.push_back(bsh);
                }
            }
            ++it;
        }
    }

    return bsh_list;
}


bool CSeqIdGuesser::DoesSeqMatchConstraint(CBioseq_Handle bsh, CRef<CStringConstraint> string_constraint)
{
    if (!bsh) {
        return false;
    } else if (!string_constraint) {
        return true;
    }

    vector<string> id_str = GetIdStrings(bsh);
    bool any_match = false;
    bool all_match = true;
    ITERATE(vector<string>, it, id_str) {
        bool this_match = string_constraint->DoesTextMatch(*it);
        any_match |= this_match;
        all_match &= this_match;
    }
    if (string_constraint->GetNegation()) {
        return all_match;
    } else {
        return any_match;
    }
}


END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE

