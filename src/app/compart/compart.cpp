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
 * File Description: cDNA-to-Genomic local alignment (same species)
 *                   and compartmentization utility
*/

#include <ncbi_pch.hpp>

#include <math.h>
#include <algo/align/splign/compart_matching.hpp>
#include <algo/align/util/compartment_finder.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objmgr/util/seq_loc_util.hpp>
#include "compart.hpp"

BEGIN_NCBI_SCOPE


void CCompartApp::Init()
{
    HideStdArgs(fHideLogfile | fHideConffile | fHideVersion);

    auto_ptr<CArgDescriptions> argdescr(new CArgDescriptions);
    argdescr->SetUsageContext(GetArguments().GetProgramName(),
                              "Compart v.1.35. Unless -qdb and -sdb are specified, "
                              "the tool expects tabular blast hits at stdin collated "
                              "by query and subject, e.g. with 'sort -k 1,1 -k 2,2'");

    argdescr->AddOptionalKey ("qdb", "qdb", "cDNA BLAST database", 
                              CArgDescriptions::eString);

    argdescr->AddOptionalKey ("sdb", "sdb", "Genomic BLAST database", 
                              CArgDescriptions::eString);

    argdescr->AddFlag ("ho", "Print raw hits only - no compartments");

    argdescr->AddDefaultKey("penalty", "penalty", "Per-compartment penalty",
                            CArgDescriptions::eDouble, "0.55");
    
    argdescr->AddDefaultKey("min_idty", "min_idty", "Minimal overall identity. Note: in current implementation  there is no sense to set different 'min_idty' and 'min_singleton_idty' (minimum is used anyway).",
                            CArgDescriptions::eDouble, "0.70");
    
    argdescr->AddDefaultKey("min_singleton_idty", "min_singleton_idty", 
                            "Minimal identity for singleton compartments. "
                            "The actual parameter passed to the compartmentization "
                            "procedure is least of this parameter multipled "
                            "by the seq length, and min_singleton_idty_bps. Note: in current implementation  there is no sense to set different 'min_idty' and 'min_singleton_idty' (minimum is used anyway).",
                            CArgDescriptions::eDouble, "0.70");

    argdescr->AddDefaultKey("min_singleton_idty_bps", "min_singleton_idty_bps", 
                            "Minimal identity for singleton compartments "
                            "in base pairs. Default = parameter disabled.",
                            CArgDescriptions::eInteger, "9999999");

    argdescr->AddDefaultKey ("max_intron", "max_intron", 
                             "Maximum intron length (in base pairs)",
                             CArgDescriptions::eInteger,
                             NStr::IntToString
                             (CCompartmentFinder<THit>::s_GetDefaultMaxIntron()));

    argdescr->AddDefaultKey("dropoff", "dropoff", 
                            "Max score drop-off during hit extension.",
                            CArgDescriptions::eInteger,
                            NStr::IntToString(CElementaryMatching::
                                              s_GetDefaultDropOff()));

    argdescr->AddDefaultKey("min_query_len", "min_query_len", 
                            "Minimum length for individual cDNA sequences.",
                            CArgDescriptions::eInteger, "50");

    argdescr->AddDefaultKey("min_hit_len", "min_hit_len", 
                            "Minimum length for reported hits in hits-only mode. "
                            "No effect in compartments mode.",
                            CArgDescriptions::eInteger, "16");
   
    argdescr->AddDefaultKey ("maxvol", "maxvol", 
                             "Maximum index volume size in MB (approximate)",
                             CArgDescriptions::eInteger,
                             "512");

    argdescr->AddFlag("noxf", "[With external hits] Suppress overlap x-filtering: "
                      "print all compartment hits intact.");

    argdescr->AddOptionalKey("seqlens", "seqlens", 
                             "[With external hits] Two-column file with sequence IDs "
                             "and their lengths. If none supplied, the program will "
                             "attempt fetching the lengths from GenBank. "
                             "Cannot be used with -qdb.",
                             CArgDescriptions::eInputFile);

    argdescr->AddDefaultKey("N", "N", 
                            "[With external hits] Max number of compartments "
                            "per query (0 = All).",
                            CArgDescriptions::eInteger, "0");

    CArgAllow* constrain01 (new CArgAllow_Doubles(0.0, 1.0));
    argdescr->SetConstraint("penalty", constrain01);
    argdescr->SetConstraint("min_idty", constrain01);
    argdescr->SetConstraint("min_singleton_idty", constrain01);

    CArgAllow_Integers* constrain_maxvol (new CArgAllow_Integers(128,1024));
    argdescr->SetConstraint("maxvol", constrain_maxvol);

    CArgAllow_Integers* constrain_minqlen (new CArgAllow_Integers(21,99999));
    argdescr->SetConstraint("min_query_len", constrain_minqlen);

    CArgAllow_Integers* constrain_minhitlen (new CArgAllow_Integers(1,99999));
    argdescr->SetConstraint("min_hit_len", constrain_minhitlen);

    SetupArgDescriptions(argdescr.release());
}


void CCompartApp::x_ReadSeqLens(CNcbiIstream& istr)
{
    m_id2len.clear();
    while(istr) {
        string id;
        istr >> id;
        if(id.size() && id[0] != '#') {
            size_t len (0);
            istr >> len;
            if(len != 0) {
                m_id2len[id] = len;
            }
        }
    }
}


size_t CCompartApp::x_GetSeqLength(const string& id)
{
    TStrIdToLen::const_iterator ie (m_id2len.end()), im (m_id2len.find(id));
    if(im != ie) {
        return im->second;
    }
    else {
        USING_SCOPE(objects);

        CRef<CSeq_id> seqid;
        try { seqid.Reset(new CSeq_id(id)); }
        catch(CSeqIdException&) {
            return 0;
        }

        const size_t len (sequence::GetLength(*seqid, m_Scope.GetNonNullPointer()));
        
        m_id2len[id] = len;

        if(m_id2len.size() >= 1000) {
            m_Scope->ResetHistory();
        }

        return len;
    }
}


int CCompartApp::Run()
{   
    const CArgs& args (GetArgs());

    const bool is_qdb     (args["qdb"]);
    const bool is_seqlens (args["seqlens"]);


    /*
    const bool is_sdb     (args["sdb"]);
    const bool is_ho      (args["ho"]);
    const bool is_maxvol  (args["maxvol"]);
    const bool is_n       (args["N"]);
    
    bool invalid_args (false);
    if(is_qdb ^ is_sdb)        { invalid_args = true; }
    if(is_qdb  && is_seqlens)  { invalid_args = true; }
    if(is_qdb  && is_n)        { invalid_args = true; }
    if(!is_qdb && is_ho)       { invalid_args = true; }
    if(!is_qdb && is_maxvol)   { invalid_args = true; }
    */

    m_NoXF                     = args["noxf"];
    m_penalty                  = args["penalty"].AsDouble();
    m_min_idty                 = args["min_idty"].AsDouble();
    m_min_singleton_idty       = args["min_singleton_idty"].AsDouble();
    m_min_singleton_idty_bps   = args["min_singleton_idty_bps"].AsInteger();
    m_min_query_len            = args["min_query_len"].AsInteger();
    m_max_intron               = args["max_intron"].AsInteger();

    int rv (0);
    if(!is_qdb) {
        if(is_seqlens) {
            x_ReadSeqLens(args["seqlens"].AsInputFile());
        }
        else {
            USING_SCOPE(objects);    
            CRef<CObjectManager> objmgr (CObjectManager::GetInstance());
            CGBDataLoader::RegisterInObjectManager(*objmgr);
            m_Scope = new CScope(*objmgr);
            m_Scope->AddDefaults();
        }
        m_MaxCompsPerQuery         = args["N"].AsInteger();
        rv = x_DoWithExternalHits();
    }
    else {

        CBlastSequenceSource bss(args["qdb"].AsString());

        /*
        class CTestSequenceSource : public ISequenceSource {
        private:
            virtual const vector<CSeq_id_Handle>& GetIds(void) const { return m_sih; }
            virtual CBioseq_Handle GetSequence(const CSeq_id_Handle& sih) {
                return m_scope->GetBioseqHandle(*sih.GetSeqId());
            }
        public:
            CTestSequenceSource() {
                m_sih.push_back( CSeq_id_Handle::GetGiHandle(21637378) );
                m_sih.push_back( CSeq_id_Handle::GetGiHandle(47551258) );
                m_object_manager = CObjectManager::GetInstance();
                CGBDataLoader::RegisterInObjectManager(*m_object_manager);
                m_scope = new CScope(*m_object_manager);
                m_scope->AddDefaults();
            }
        protected:
            vector<CSeq_id_Handle> m_sih;
            CRef<CObjectManager> m_object_manager;
            CRef<CScope> m_scope;

        };
         CTestSequenceSource bss;

        cerr<<"number of seqs:  "<< bss.GetNumSeqs()<<endl;
        const char *seq;
        int len = ((ISequenceSource *)&bss)->GetSequence(1, &seq);
        cerr<<"sequence length: "<<len<<endl;
        //string sseq(seq, seq+len);
        ((ISequenceSource *)&bss)->RetSequence(&seq);
        */
    
        CRef<CElementaryMatching> matcher (
                     new CElementaryMatching(&bss,
                                             args["sdb"].AsString()));

        matcher->SetOutputMethod(true);

        matcher->SetMinQueryLength(m_min_query_len);

        matcher->SetPenalty(m_penalty);
        matcher->SetMinIdty(m_min_idty);
        matcher->SetMinSingletonIdty(m_min_singleton_idty);
        matcher->SetMaxIntron(m_max_intron);

        matcher->SetHitsOnly(args["ho"]);
        matcher->SetMinHitLength(args["min_hit_len"].AsInteger());
        matcher->SetMaxVolSize(1024 * 1024 * (args["maxvol"].AsInteger()));

        matcher->SetDropOff(args["dropoff"].AsInteger());

        try { matcher->Run(); }
        catch(std::bad_alloc&) {
            NCBI_THROW(CException, eUnknown, 
                       "Not enough memory available to run this program");
        }

        /* 
        //set SetOutputMethod to false before Run to get the results as a vector
        vector<vector<CRef<CBlastTabular> > > vec = matcher->GetResults();
        ITERATE(vector<vector<CRef<CBlastTabular> > >, itt, vec) {
            ITERATE(vector<CRef<CBlastTabular> >, ittt, *itt) {
                //cerr<<"Here"<<endl;
                cout<<**ittt<<endl;
            }
            cout<<endl;
        }
        */
    }

    return rv;
}


int CCompartApp::x_DoWithExternalHits(void)
{
    m_CompartmentsPermanent.resize(0);
    m_Allocated = 0;

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

                    const int rv (x_ProcessPair(query0, hitrefs));
                    if(rv != 0) return rv;

                    if(query != query0) {

                        x_RankAndStore();

                        if(m_Allocated > 128 * 1024 * 1024) {

                            stable_sort(m_CompartmentsPermanent.begin(),
                                        m_CompartmentsPermanent.end());

                            ITERATE(TCompartRefs, ii, m_CompartmentsPermanent) {
                                cout << **ii << endl;
                                m_Allocated -= (*ii)->GetHitCount()*sizeof(THit);
                            }
                            m_CompartmentsPermanent.clear();
                        }
                    }

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
        x_RankAndStore();
        hitrefs.clear();
    }

    stable_sort(m_CompartmentsPermanent.begin(), m_CompartmentsPermanent.end());

    ITERATE(TCompartRefs, ii, m_CompartmentsPermanent) {
        cout << **ii << endl;
    }

    m_CompartmentsPermanent.clear();

    return 0;
}


int CCompartApp::x_ProcessPair(const string& query0, THitRefs& hitrefs)
{
    const size_t qlen (x_GetSeqLength(query0));

    if(qlen == 0) {
        cerr << "Cannot retrieve sequence lengths for: " 
             << query0 << endl;
        return 1;
    }

    if(qlen < m_min_query_len) {
        return 0;
    }

    typedef CCompartmentAccessor<THit> TAccessor;
    typedef TAccessor::TCoord          TCoord;

    const TCoord penalty_bps (TCoord(m_penalty * qlen + 0.5));
    const TCoord min_matches (TCoord(m_min_idty * qlen + 0.5));
    const TCoord msm1        (TCoord(m_min_singleton_idty * qlen + 0.5));
    const TCoord msm2        (m_min_singleton_idty_bps);
    const TCoord min_singleton_matches (min(msm1, msm2));

    TAccessor ca (penalty_bps, min_matches, min_singleton_matches, !m_NoXF);
    ca.SetMaxIntron(m_max_intron);
    ca.Run(hitrefs.begin(), hitrefs.end());

    THitRefs comp;
    for(bool b0 (ca.GetFirst(comp)); b0 ; b0 = ca.GetNext(comp)) {

        TCompartRef cr (new CCompartment (comp, qlen));
        m_Compartments.push_back(cr);
    }

    return 0;
}


bool PCompartmentRanker(const CCompartApp::TCompartRef& lhs,
                        const CCompartApp::TCompartRef& rhs)
{
    //#define PCOMPARTMENT_RANKER_M1

#ifdef PCOMPARTMENT_RANKER_M1

    const size_t exons_lhs (lhs->GetExonCount());
    const size_t exons_rhs (rhs->GetExonCount());
    if(exons_lhs == exons_rhs) {
        return lhs->GetMatchCount() > rhs->GetMatchCount();
    }
    else {
        return exons_lhs > exons_rhs;
    }

#else

    const size_t idtybin_lhs (lhs->GetIdentityBin());
    const size_t idtybin_rhs (rhs->GetIdentityBin());
    if(idtybin_lhs == idtybin_rhs) {
        const size_t exons_lhs (lhs->GetExonCount());
        const size_t exons_rhs (rhs->GetExonCount());
        if(exons_lhs == exons_rhs) {
            return lhs->GetMatchCount() > rhs->GetMatchCount();
        }
        else {
            return exons_lhs > exons_rhs;
        }
    }
    else {
        return idtybin_lhs > idtybin_rhs;
    }
#endif

#undef PCOMPARTMENT_RANKER_M1
}


void CCompartApp::x_RankAndStore(void)
{
    const size_t cdim (m_Compartments.size());
    if(cdim == 0) {
        return;
    }

    if(m_MaxCompsPerQuery > 0 && cdim > m_MaxCompsPerQuery) {
        stable_sort(m_Compartments.begin(), m_Compartments.end(), PCompartmentRanker);
        m_Compartments.resize(m_MaxCompsPerQuery);
    }

    for(size_t i (0), in (m_Compartments.size()); i < in; ++i) {
        TCompartRef cr (m_Compartments[i]);
        m_CompartmentsPermanent.push_back(cr);
        m_Allocated += cr->GetHitCount() * sizeof(THit);
    }
    
    m_Compartments.resize(0);
}


void CCompartApp::Exit()
{
    return;
}

 
CCompartApp::CCompartment::TRange CCompartApp::CCompartment::GetSpan(void) const
{
    if(m_HitRefs.size() == 0) {
        NCBI_THROW(CException, eUnknown, "Span requested for empty compartment");
    }
    THit::TCoord a (m_HitRefs.front()->GetSubjStart()),
        b (m_HitRefs.back()->GetSubjStop());
    if(a > b) {
        THit::TCoord c (a);
        a = b;
        b = c;
    }

    return CCompartApp::CCompartment::TRange(a, b);
}

CCompartApp::CCompartment::CCompartment(const THitRefs& hitrefs, size_t length):
    m_SeqLength(length), m_IdentityBin(0), m_ExonCount(0), m_MatchCount(0)
{
    if(hitrefs.size() == 0) {
        NCBI_THROW(CException, eUnknown,
                   "Cannot init compartment with empty hit list");
    }

    for(THitRefs::const_reverse_iterator ii(hitrefs.rbegin()), ie(hitrefs.rend());
        ii != ie; x_AddHit(*ii++));

    x_EvalExons();
}


void CCompartApp::CCompartment::x_AddHit(const THitRef& hitref)
{
    if(m_HitRefs.size() == 0) {
        m_HitRefs.push_back(hitref);
    }
    else {

        const THitRef& hb (m_HitRefs.back());
        const bool cs (hb->GetSubjStrand());
        if(cs != hitref->GetSubjStrand()) {
            NCBI_THROW(CException, eUnknown, "Hit being added has strand "
                       "different from that of the compartment.");
        }

        m_HitRefs.push_back(hitref);
    }
}


bool CCompartApp::CCompartment::GetStrand(void) const
{
    if(m_HitRefs.size()) {
        return m_HitRefs.front()->GetSubjStrand();
    }
    NCBI_THROW(CException, eUnknown, "Cannot determine compartment strand");
}


// compares by subject, query, strand, then order on the subject
bool CCompartApp::CCompartment::operator < (const CCompartApp::CCompartment& rhs)
const
{
    const THit::TId& subjid_lhs (m_HitRefs.front()->GetSubjId());
    const THit::TId& subjid_rhs (rhs.m_HitRefs.front()->GetSubjId());
    const int co (subjid_lhs->CompareOrdered(*subjid_rhs));
    if(co == 0) {

        const THit::TId& queryid_lhs (m_HitRefs.front()->GetQueryId());
        const THit::TId& queryid_rhs (rhs.m_HitRefs.front()->GetQueryId());
        const int co (queryid_lhs->CompareOrdered(*queryid_rhs));

        if(co == 0) {

            const bool strand_lhs (GetStrand());
            const bool strand_rhs (rhs.GetStrand());
            if(strand_lhs == strand_rhs) {
                if(strand_lhs) {
                    return GetSpan().first < rhs.GetSpan().first;
                }
                else {
                    return GetSpan().first > rhs.GetSpan().first;
                }
            }
            else {
                return strand_lhs < strand_rhs;
            }
        }
        else {
            return co < 0;
        }
    }
    else {
        return co < 0;
    }
}


bool operator < (const CCompartApp::TCompartRef& lhs,
                 const CCompartApp::TCompartRef& rhs)
{
    return *lhs < *rhs;
}


// Evaluate all variables used in comaprtment ranking. These are:
// - m_IdentityBin
// - m_ExonCount
// - m_MatchCount
void CCompartApp::CCompartment::x_EvalExons(void)
{
    const size_t kMinIntronLength (25);
    const size_t kMinExonLength   (10);

    size_t exons (1);
    THitRef& h (m_HitRefs.front());
    double matches ( h->GetLength() * h->GetIdentity() );

    if(m_HitRefs.size() > 1) {

        if(GetStrand()) {

            THitRef prev;
            ITERATE(THitRefs, ii, m_HitRefs) {

                const THitRef& h (*ii);
                if(prev.NotEmpty()) {

                    const THit::TCoord q0 (prev->GetQueryStop());
                    if(q0 + kMinExonLength <= h->GetQueryStop()) {

                        const THit::TCoord s0 (h->GetSubjStart() 
                                               - (h->GetQueryStart() - q0));
                        if(prev->GetSubjStop() + kMinIntronLength <= s0) {
                            ++exons;
                        }
                        const THit::TCoord q0max (max(q0,h->GetQueryStart()));
                        matches += (h->GetQueryStop() - q0max) * h->GetIdentity();
                    }
                }
                prev = h;
            }
        }
        else {

            THitRef prev;
            ITERATE(THitRefs, ii, m_HitRefs) {

                const THitRef& h (*ii);
                if(prev.NotEmpty()) {

                    const THit::TCoord q0 (prev->GetQueryStop());
                    if(q0 + kMinExonLength <= h->GetQueryStop()) {

                        const THit::TCoord s0 (h->GetSubjStart() 
                                               + h->GetQueryStart() - q0);
                        if(s0 + kMinIntronLength <= prev->GetSubjStop()) {
                            ++exons;
                        }
                        const THit::TCoord q0max (max(q0,h->GetQueryStart()));
                        matches += (h->GetQueryStop() - q0max) * h->GetIdentity();
                    }
                }
                prev = h;
            }
        }
    }
    
    m_ExonCount = exons;
    m_MatchCount = size_t(round(matches));
    m_IdentityBin = size_t(floor(double(m_MatchCount) / m_SeqLength / 0.1));
}


ostream& operator << (ostream& ostr, const CCompartApp::CCompartment& rhs)
{
    ITERATE(CCompartApp::THitRefs, ii, rhs.m_HitRefs) {
        ostr << **ii << endl;
    }
    return ostr;
}


END_NCBI_SCOPE


USING_NCBI_SCOPE;

int main(int argc, const char* argv[]) 
{
    return CCompartApp().AppMain(argc, argv, 0, eDS_Default, 0);
}
