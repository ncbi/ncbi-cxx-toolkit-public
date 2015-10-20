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
 * Authors: Sema Kachalo
 *
 */

// See the notes on spell-checking algorithms by Peter Norvig: http://norvig.com/spell-correct.html

#include <ncbi_pch.hpp>
#include "discrepancy_core.hpp"
#include "utils.hpp"
#include <objmgr/util/feature.hpp>
#include <objmgr/util/sequence.hpp>

BEGIN_NCBI_SCOPE;
BEGIN_SCOPE(NDiscrepancy)
USING_SCOPE(objects);

DISCREPANCY_MODULE(spell_check);


static string Words[] = { "hypothetical", "protein", "hemoglobin", "Colleen", "Bollin" };
static size_t WordCount = sizeof(Words) / sizeof(Words[0]);

class CScrumble
{
public:
    CScrumble(const string&s) : S(s), X(0), Y(0) {}
    bool valid() { return X < S.length(); }
    void next() { Y++; if (Y > 53 || (Y == 53 && X == S.length() - 1)) { X++; Y = 0; } }
    string str()
    {
        if (Y < 26) {       // substitute
            string s = S;
            s[X] = 'A' + Y;
            return s;
        }
        else if (Y < 52) {  // insert
            string s = S.substr(0, X + 1) + S.substr(X);
            s[X] = 'A' + Y - 26;
            return s;
        }
        else if (Y == 52) { // delete
            string s = S.substr(0, X) + S.substr(X + 1);
            return s;
        }
        else { //(Y == 53)  // permute
            string s = S;
            char c = s[X];
            s[X] = s[X+1];
            s[X+1] = c;
            return s;
        }
    }
protected:
    const string& S;
    size_t X;
    size_t Y;
};

static map<string, string> Dictionary;
static map<string, vector<string> > Scrumbled;

static void InitSpellChecker()
{
    if (!Dictionary.empty()) {
        return;
    }
    for (size_t i = 0; i < WordCount; i++) {
        string s = Words[i];
        string u = s;
        NStr::ToUpper(u);
        Dictionary[u] = s;
        Scrumbled[u].push_back(s);
    }
    for (size_t i = 0; i < WordCount; i++) {
        string s = Words[i];
        string u = s;
        NStr::ToUpper(u);
        for (CScrumble Scr(u); Scr.valid(); Scr.next()) {
            string t = Scr.str();
            if (Dictionary.find(t) != Dictionary.end()) {
                continue;
            }
            vector<string>& v = Scrumbled[t];
            size_t n;
            for (n = 0; n < v.size(); n++) {
                if (v[n] == s) {
                    break;
                }
            }
            if (n == v.size()) {
                v.push_back(s);
            }
        }
    }
}


static void insert_unique(vector<string>& v, const string& s)
{
    for (size_t n = 0; n < v.size(); n++) {
        if (v[n] == s) {
            return;
        }
    }
    v.push_back(s);
}


static void insert_unique(vector<string>& v, const vector<string>& w)
{
    for (size_t n = 0; n < w.size(); n++) {
        insert_unique(v, w[n]);
    }
}


// SPELL_CHECK
DISCREPANCY_CASE(SPELL_CHECK, CSeqFeatData, 0, "Spell check")
{
    InitSpellChecker();
    if (obj.GetSubtype() != CSeqFeatData::eSubtype_prot) {
        return;
    }
    CConstRef<CSuspect_rule_set> rules = context.GetProductRules();
    const CProt_ref& prot = obj.GetProt();
    string str = *prot.GetName().begin();

    vector<string> words;
    NStr::Tokenize(str, " ", words);
    for (size_t i = 0; i < words.size(); i++) {
        string s = NStr::ToUpper(words[i]);
        if (Dictionary.find(s) != Dictionary.end()) {
            continue;
        }
        vector<string> w;
        if (Scrumbled.find(s) != Scrumbled.end()) {
            insert_unique(w, Scrumbled[s]);
        }
        for (CScrumble Scr(NStr::ToUpper(s)); Scr.valid(); Scr.next()) {
            string t = Scr.str();
            if (Scrumbled.find(t) != Scrumbled.end()) {
                insert_unique(w, Scrumbled[t]);
            }
        }
        if (w.size()) {
            string msg = s + " - should it be";
            for (size_t n = 0; n < w.size(); n++) {
                msg += " " + w[n] + "?";
            }
            m_Objs[msg].Add(*new CDiscrepancyObject(context.GetCurrentSeq_feat(), context.GetScope(), context.GetFile(), context.GetKeepRef(), false));
        }
    }
}


DISCREPANCY_SUMMARIZE(SPELL_CHECK)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE
