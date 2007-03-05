/* $Id$
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
 * Author:  Yuri Kapustin
 *
 * File Description:  Compartmentization demo
 *                   
*/

#include <ncbi_pch.hpp>

#include "compart.hpp"

#include <algo/align/util/compartment_finder.hpp>

#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objmgr/util/seq_loc_util.hpp>

BEGIN_NCBI_SCOPE


void CCompartApp::Init()
{
    HideStdArgs(fHideLogfile | fHideConffile | fHideVersion);

    auto_ptr<CArgDescriptions> argdescr(new CArgDescriptions);
    argdescr->SetUsageContext(GetArguments().GetProgramName(),
                              "Compart v.1.0.0");

    argdescr->AddDefaultKey("penalty", "penalty", 
                            "Per-compartment penalty",
                            CArgDescriptions::eDouble, "0.4");
    
    argdescr->AddDefaultKey("min_idty", "min_idty", 
                            "Minimal overall identity",
                            CArgDescriptions::eDouble, "0.4");
    
    argdescr->AddDefaultKey("min_singleton_idty", "min_singleton_idty", 
                            "Minimal identity for singleton compartments",
                            CArgDescriptions::eDouble, "0.4");
    
    argdescr->AddOptionalKey("seqlens", "seqlens", 
                             "Two-column file with sequence IDs and their lengths. "
                             "If no supplied, the program will attempt fetching "
                             "sequence lengths from GenBank.",
                             CArgDescriptions::eInputFile);

    CArgAllow* constrain01 = new CArgAllow_Doubles(0.0, 1.0);
    argdescr->SetConstraint("penalty", constrain01);
    argdescr->SetConstraint("min_idty", constrain01);
    argdescr->SetConstraint("min_singleton_idty", constrain01);

    {{
        USING_SCOPE(objects);
        CRef<CObjectManager> om = CObjectManager::GetInstance();
        CGBDataLoader::RegisterInObjectManager(*om);
        m_Scope.Reset(new CScope (*om));
        m_Scope->AddDefaults();
    }}
    SetupArgDescriptions(argdescr.release());
}


void CCompartApp::x_ReadSeqLens(CNcbiIstream& istr)
{
    m_id2len.clear();

    while(istr) {
        string id;
        size_t len = 0;
        istr >> id >> len;
        if(len != 0) {
            m_id2len[id] = len;
        }
    }
}


size_t CCompartApp::x_GetSeqLength(const string& id)
{
    TStrIdToLen::const_iterator ie = m_id2len.end(), im = m_id2len.find(id);
    if(im != ie) {
        return im->second;
    }
    else {
        USING_SCOPE(objects);

        CRef<CSeq_id> seqid;
        try {seqid.Reset(new CSeq_id(id));}
        catch(CSeqIdException& e) {
            return 0;
        }

        const size_t len = sequence::GetLength(*seqid, m_Scope.GetNonNullPointer());
        m_id2len[id] = len;
        return len;
    }
}


int CCompartApp::Run()
{   
    const CArgs& args = GetArgs();

    if(args["seqlens"]) {
        x_ReadSeqLens(args["seqlens"].AsInputFile());
    }

    m_penalty                  = args["penalty"].AsDouble();
    m_min_idty                 = args["min_idty"].AsDouble();
    m_min_singleton_idty       = args["min_singleton_idty"].AsDouble();

    THitRefs hitrefs;

    typedef map<string,string> TIdToId;
    TIdToId id2id;

    char line [1024];
    string query0, subj0;
    while(cin) {

        cin.getline(line, sizeof line, '\n');
        string s (NStr::TruncateSpaces(line));
        if(s.size()) {

            THitRef hit (new THit(s.c_str()));

            const string query (hit->GetQueryId()->GetSeqIdString(true));
            const string subj  (hit->GetSubjId()->GetSeqIdString(true));
            
            if(query0.size() == 0 || subj0.size() == 0) {
                query0 = query;
                subj0 = subj;
                id2id[query0] = subj0;
            }
            else {
                if(query != query0 || subj != subj0) {
                    int rv = x_ProcessPair(query0, hitrefs);
                    if(rv != 0) return rv;
                    query0 = query;
                    subj0 = subj;
                    hitrefs.clear();

                    TIdToId::const_iterator im = id2id.find(query0);
                    if(im == id2id.end() || im->second != subj0) {
                        id2id[query0] = subj0;
                    }
                    else {
                        cerr << "Input hit stream not properly ordered" << endl;
                        return 2;
                    }
                }
            }

            hitrefs.push_back(hit);
        }
    }

    if(hitrefs.size()) {
        int rv = x_ProcessPair(query0, hitrefs);
        if(rv != 0) return rv;
    }

    return 0;
}


int CCompartApp::x_ProcessPair(const string& query0, THitRefs& hitrefs)
{
    const size_t qlen = x_GetSeqLength(query0);
    if(qlen == 0) {
        cerr << "Cannot retrieve sequence lengths for: " 
             << query0 << endl;
        return 1;
    }

    const size_t penalty_bps   = size_t(m_penalty * qlen);
    const size_t min_matches   = size_t(m_min_idty * qlen);
    const size_t min_singleton_matches 
        = size_t(m_min_singleton_idty * qlen);

    CCompartmentAccessor<THit> ca ( hitrefs.begin(), hitrefs.end(),
                                    penalty_bps,
                                    min_matches,
                                    min_singleton_matches);
    THitRefs comp;
    if(ca.GetFirst(comp)) {

        ITERATE(THitRefs, ii, comp) {
            cout <<  **ii << endl;
        }
        cout << endl;
        while(ca.GetNext(comp)) {
            ITERATE(THitRefs, ii, comp) {
                cout <<  **ii << endl;
            }
            cout << endl;
        }
    }
    return 0;
}


void CCompartApp::Exit()
{
    return;
}

END_NCBI_SCOPE


USING_NCBI_SCOPE;

int main(int argc, const char* argv[]) 
{
    return CCompartApp().AppMain(argc, argv, 0, eDS_Default, 0);
}
