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
* Authors:  Eugene Vasilchenko
*
* File Description:
*   Test the functionality of CSeqVector
*
*/
#define NCBI_TEST_APPLICATION
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbitime.hpp>

#include <util/checksum.hpp>
#include <util/random_gen.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_vector.hpp>

#include <objects/seq/seq__.hpp>
#include <objects/seqloc/seqloc__.hpp>

#include <vector>
#include <utility>
#include <algorithm>

#include <common/test_assert.h>  /* This header must go last */


BEGIN_NCBI_SCOPE
using namespace objects;


/////////////////////////////////////////////////////////////////////////////
//
//  Test application
//

class CTestApp : public CNcbiApplication
{
public:
    virtual void Init(void);
    virtual int  Run (void);

    typedef vector< pair<TGi, TSeqPos> > TLevel;
    CConstRef<CBioseq> CreateBioseq(TGi gi, bool protein,
                                    int segments, const TLevel& pll);
    
protected:
    CRandom m_Random;
};


void CTestApp::Init(void)
{
    // Prepare command line descriptions
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->AddDefaultKey("seed", "RandomSeed",
                            "Force random seed",
                            CArgDescriptions::eInteger, "0");
    arg_desc->AddDefaultKey("count", "PassCount",
                            "Number of test passes to run",
                            CArgDescriptions::eInteger, "20");
    arg_desc->AddDefaultKey("levels", "DeltaLevels",
                            "Maximum number of seq delta levels",
                            CArgDescriptions::eInteger, "3");
    arg_desc->AddDefaultKey("sequences", "LevelSequences",
                            "Number of sequences on each level",
                            CArgDescriptions::eInteger, "20");
    arg_desc->AddDefaultKey("segments", "SeqSegments",
                            "Number of segments in each sequence",
                            CArgDescriptions::eInteger, "20");
    arg_desc->AddDefaultKey("requests", "SeqVectorRequests",
                            "Number of seq vector operations in pass",
                            CArgDescriptions::eInteger, "200");
    arg_desc->AddFlag("verbose", "Dump verbose results");
    arg_desc->AddDefaultKey("checksum", "CheckSum",
                            "Check md5 sum of result",
                            CArgDescriptions::eString, "");

    string prog_description = "test_objmgr_sv";
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              prog_description, false);

    SetupArgDescriptions(arg_desc.release());
}


CConstRef<CBioseq> CTestApp::CreateBioseq(TGi gi,
                                          bool protein,
                                          int segments,
                                          const TLevel& pll)
{
    CRef<CBioseq> seq(new CBioseq);
    CRef<CSeq_id> id(new CSeq_id);
    id->SetGi(gi);
    seq->SetId().push_back(id);
    CSeq_inst& inst = seq->SetInst();
    inst.SetRepr(CSeq_inst::eRepr_delta);
    inst.SetMol(protein? CSeq_inst::eMol_aa: CSeq_inst::eMol_na);
    CDelta_ext& delta = inst.SetExt().SetDelta();
    int length = 0;
    for ( int seg = 0; seg < segments; ++seg ) {
        CRef<CDelta_seq> s(new CDelta_seq);
        int len;
        if ( pll.empty() ) {
            len = m_Random.GetRand(0, 2000>>m_Random.GetRand(0, 8));
            CSeq_literal& lit = s->SetLiteral();
            lit.SetLength(len);
            CSeq_data& data = lit.SetSeq_data();
            if ( protein ) {
                switch ( m_Random.GetRand(0, 3) ) {
                default:
                {
                    string& s = data.SetIupacaa().Set();
                    s.resize(len);
                    const char* BASES = "ABCDEFGHIKLMNPQRSTVWXYZ";
                    int MAX_BASE = strlen(BASES)-1;
                    NON_CONST_ITERATE ( string, it, s ) {
                        *it = BASES[m_Random.GetRand(0, MAX_BASE)];
                    }
                    break;
                }
                case 1:
                {
                    // gap
                    if ( m_Random.GetRand(0, 1) ) {
                        data.SetGap().SetType(CSeq_gap::eType_unknown);
                    }
                    else {
                        lit.ResetSeq_data();
                    }
                    break;
                }
                case 2:
                {
                    string& s = data.SetNcbieaa().Set();
                    s.resize(len);
                    const char* BASES = "*-ABCDEFGHIKLMNPQRSTUVWXYZ";
                    int MAX_BASE = strlen(BASES)-1;
                    NON_CONST_ITERATE ( string, it, s ) {
                        *it = BASES[m_Random.GetRand(0, MAX_BASE)];
                    }
                    break;
                }
                case 3:
                {
                    vector<char>& s = data.SetNcbistdaa().Set();
                    s.resize(len);
                    NON_CONST_ITERATE ( vector<char>, it, s ) {
                        *it = m_Random.GetRand(0, 25);
                    }
                    break;
                }
                case 9:
                {
                    vector<char>& s = data.SetNcbi8aa().Set();
                    s.resize(len);
                    NON_CONST_ITERATE ( vector<char>, it, s ) {
                        *it = m_Random.GetRand(0, 249);
                    }
                    break;
                }
                }
            }
            else {
                switch ( m_Random.GetRand(0, 3) ) {
                default:
                {
                    string& s = data.SetIupacna().Set();
                    s.resize(len);
                    const char* BASES = "ABCDGHKMNRSTVWY";
                    int MAX_BASE = strlen(BASES)-1;
                    NON_CONST_ITERATE ( string, it, s ) {
                        *it = BASES[m_Random.GetRand(0, MAX_BASE)];
                    }
                    break;
                }
                case 1:
                {
                    // gap
                    if ( m_Random.GetRand(0, 1) ) {
                        data.SetGap().SetType(CSeq_gap::eType_unknown);
                    }
                    else {
                        lit.ResetSeq_data();
                    }
                    break;
                }
                case 2:
                {
                    vector<char>& s = data.SetNcbi2na().Set();
                    s.resize((len+3)/4);
                    NON_CONST_ITERATE ( vector<char>, it, s ) {
                        *it = m_Random.GetRand(0, 255);
                    }
                    break;
                }
                case 3:
                {
                    vector<char>& s = data.SetNcbi4na().Set();
                    s.resize((len+1)/2);
                    NON_CONST_ITERATE ( vector<char>, it, s ) {
                        *it = m_Random.GetRand(0, 255);
                    }
                    break;
                }
                case 9:
                {
                    vector<char>& s = data.SetNcbi8na().Set();
                    s.resize(len);
                    NON_CONST_ITERATE ( vector<char>, it, s ) {
                        *it = m_Random.GetRand(0, 15);
                    }
                    break;
                }
                }
            }
        }
        else {
            int index = m_Random.GetRand(0, pll.size()-1);
            TGi ref_gi = pll[index].first;
            int ref_len = pll[index].second;
            CSeq_interval& interval = s->SetLoc().SetInt();
            interval.SetId().SetGi(ref_gi);
            interval.SetStrand(m_Random.GetRand(0, 1)?
                               eNa_strand_plus: eNa_strand_minus);
            int a = m_Random.GetRand(0, ref_len);
            int b;
            do {
                b = m_Random.GetRand(0, ref_len);
            } while ( b == a );
            if ( a > b ) swap(a, b);
            interval.SetFrom(a);
            interval.SetTo(b-1);
            len = b-a;
        }
        delta.Set().push_back(s);
        length += len;
    }
    inst.SetLength(length);
    return seq;
}


static int GetKey(CBioseq_Handle::EVectorCoding sv_coding,
                  ENa_strand strand,
                  bool ncbi2na,
                  int ncbi2na_seed)
{
    if ( ncbi2na ) {
        sv_coding = CBioseq_Handle::eCoding_Ncbi;
    }
    else {
        ncbi2na_seed = 0;
    }
    return sv_coding*1000 + strand*100 + ncbi2na*10 + ncbi2na_seed;
}


static string Pack(const string& s, CSeq_data::E_Choice coding)
{
    string ret;
    if ( coding == CSeq_data::e_Ncbi2na ) {
        size_t count = s.size();
        ret.reserve((count+3)>>2);
        for ( size_t i = 0; i+4 <= count; i += 4 ) {
            ret += char((s[i]<<6)|(s[i+1]<<4)|(s[i+2]<<2)|s[i+3]);
        }
        switch ( count&3 ) {
        default:
            break;
        case 1:
            ret += char((s[count-1]<<6));
            break;
        case 2:
            ret += char((s[count-2]<<6)|(s[count-1]<<4));
            break;
        case 3:
            ret += char((s[count-3]<<6)|(s[count-2]<<4)|(s[count-1]<<2));
            break;
        }
    }
    else if ( coding == CSeq_data::e_Ncbi4na ) {
        size_t count = s.size();
        ret.reserve((count+1)>>1);
        for ( size_t i = 0; i+2 <= count; i += 2 ) {
            ret += char((s[i]<<4)|s[i+1]);
        }
        if ( count&1 ) {
            ret += char(s[count-1]<<4);
        }
    }
    else {
        ret = s;
    }
    return ret;
}


int CTestApp::Run(void)
{
    SetDiagPostFlag(eDPF_All);
    NcbiCout << "Testing ObjectManager CSeqVector..." << NcbiEndl;

    const CArgs& args = GetArgs();
    int seed = args["seed"].AsInteger();
    if ( seed == 0 ) {
        seed = int(time(0));
    }
    NcbiCout << "Using random seed: " << seed << NcbiEndl;
    m_Random.SetSeed(seed);
    int passes = args["count"].AsInteger();
    int max_levels = args["levels"].AsInteger();
    int sequences = args["sequences"].AsInteger();
    int segments = args["segments"].AsInteger();
    int requests = args["requests"].AsInteger();
    bool verbose = args["verbose"];
    string ref_sum = args["checksum"].AsString();
    if ( !ref_sum.empty() ) ref_sum = "MD5: " + ref_sum;

    CRef<CObjectManager> om = CObjectManager::GetInstance();

    CChecksum sum(CChecksum::eMD5);

    for ( int pass = 0; pass < passes; ++pass ) {
        int levels = m_Random.GetRand(1, max_levels);
        if ( verbose ) {
            NcbiCout << "Pass " << pass << ", " << levels << " levels."
                     << NcbiEndl;
        }
        CScope scope(*om);
        TLevel ll, pll;
        CBioseq_Handle main;
        bool protein = m_Random.GetRand(0, 1) != 0;
        for ( int level = 0; level < levels; ++level ) {
            swap(ll, pll);
            ll.clear();
            for ( int seqnum = 0; seqnum < sequences; ++seqnum ) {
                TGi gi = level*1000 + seqnum;
                CConstRef<CBioseq> seq =
                    CreateBioseq(gi, protein, segments, pll);
                if ( verbose ) {
                    NcbiCout << "Level "<<level<<" "<<MSerial_AsnText<<*seq;
                }
                CBioseq_Handle bh = scope.AddBioseq(*seq);
                ll.push_back(pair<TGi, TSeqPos>(gi, bh.GetBioseqLength()));
                if ( level == levels - 1 ) {
                    main = bh;
                    break;
                }
            }
        }
        map<int, string> ss;
        for ( int iupac = 0; iupac < 2; ++iupac ) {
            CBioseq_Handle::EVectorCoding coding = iupac?
                CBioseq_Handle::eCoding_Iupac: CBioseq_Handle::eCoding_Ncbi;
            for ( int minus = 0; minus < 2; ++minus ) {
                ENa_strand strand = minus? eNa_strand_minus: eNa_strand_plus;
                CSeqVector sv(main, coding, strand);
                for ( int ncbi2na = 0;
                      ncbi2na < (protein||iupac? 1: 2);
                      ++ncbi2na ) {
                    if ( ncbi2na ) {
                        sv.SetCoding(CSeq_data::e_Ncbi2na);
                    }
                    for ( int seed = 0; seed <= (ncbi2na? 4: 0); ++seed ) {
                        if ( seed ) {
                            sv.SetRandomizeAmbiguities(seed);
                        }
                        string d;
                        sv.GetSeqData(0, sv.size(), d);
                        _ASSERT(d.size() == main.GetBioseqLength());
                        int key = GetKey(coding, strand, ncbi2na != 0, seed);
                        if ( verbose ) {
                            NcbiCerr << "Ref ("
                                     << iupac << " "
                                     << minus << " "
                                     << ncbi2na << " "
                                     << seed
                                     << ") = " << key << " "
                                     << '"'<<NStr::PrintableString(d)<<'"'
                                     << NcbiEndl;
                        }
                        _ASSERT(!ss.count(key));
                        ss[key] = d;
                        sum.AddLine(d);
                    }
                }
            }
        }
        
        TSeqPos size = main.GetBioseqLength();
        for ( int req = 0; req < requests; ++req ) {
            TSeqPos start = m_Random.GetRand(0, size);
            TSeqPos stop = m_Random.GetRand(0, size);
            if ( start > stop ) swap(start, stop);
            if ( verbose ) {
                NcbiCout << "["<<start<<"..."<<stop<<") ";
            }

            bool iupac = m_Random.GetRand(0, 1) != 0;
            CBioseq_Handle::EVectorCoding coding = iupac?
                CBioseq_Handle::eCoding_Iupac: CBioseq_Handle::eCoding_Ncbi;
            bool minus = m_Random.GetRand(0, 1) != 0;
            ENa_strand strand = minus? eNa_strand_minus: eNa_strand_plus;
            CSeqVector sv(main, coding, strand);
            bool ncbi2na = !protein && !iupac && m_Random.GetRand(0, 1);
            if ( ncbi2na ) {
                sv.SetCoding(CSeq_data::e_Ncbi2na);
            }
            if ( verbose ) {
                if ( minus ) {
                    NcbiCout << "minus ";
                }
                if ( ncbi2na ) {
                    NcbiCout << "NCBI2NA ";
                }
                else if ( iupac ) {
                    NcbiCout << "IUPAC ";
                }
                else {
                    NcbiCout << "NCBI ";
                }
            }
            int randomize_ncbi2na_seed = m_Random.GetRand(0, 4); // 0 - no rand
            if ( randomize_ncbi2na_seed ) {
                sv.SetRandomizeAmbiguities(randomize_ncbi2na_seed);
                if ( verbose ) {
                    NcbiCout << "rand("<<randomize_ncbi2na_seed<<") ";
                }
            }
            if ( verbose ) {
                NcbiCout << NcbiEndl;
            }
            string data;
            sv.GetSeqData(start, stop, data);
            if ( verbose ) {
                NcbiCout << NStr::PrintableString(data) << NcbiEndl;
            }
            sum.AddLine(data);
            int key = GetKey(coding, strand, ncbi2na, randomize_ncbi2na_seed);
            _ASSERT(ss.count(key));
            const string& ref = ss[key];
            _ASSERT(ref.size() == main.GetBioseqLength());
            _ASSERT(equal(data.begin(), data.end(), ref.begin()+start));
            if ( !ncbi2na || randomize_ncbi2na_seed ) {
                string packed;
                sv.GetPackedSeqData(packed, start, stop);
                string packed_ref =
                    Pack(ref.substr(start, stop-start), sv.GetCoding());
                sum.AddLine(packed_ref);
                if ( packed != packed_ref ) {
                    NcbiCout << "packed != packed_ref\n";
                    for ( size_t i = 0;
                          i < packed.size() && i < packed_ref.size();
                          ++i ) {
                        NcbiCout << i;
                        int c1 = -1, c2 = -1;
                        if ( i < packed.size() ) {
                            c1 = packed[i]&255;
                            NcbiCout << " " << hex << c1 << dec;
                        }
                        else {
                            NcbiCout << " ?";
                        }
                        if ( i < packed_ref.size() ) {
                            c2 = packed_ref[i]&255;
                            NcbiCout << " " << hex << c2 << dec;
                        }
                        else {
                            NcbiCout << " ?";
                        }
                        if ( c1 != c2 ) {
                            NcbiCout << " ERROR";
                        }
                        NcbiCout << "\n";
                    }
                    NcbiCout << NcbiEndl;
                }
                _ASSERT(packed == packed_ref);
            }
        }
    }

    CNcbiOstrstream str;
    sum.WriteChecksumData(str);
    string str_sum = CNcbiOstrstreamToString(str);
    NcbiCout << "Checksum: " << str_sum << NcbiEndl;
    if ( !ref_sum.empty() && str_sum != ref_sum ) {
        NcbiCerr << "Checksum failed:" << NcbiEndl <<
            "Current: " << str_sum << NcbiEndl <<
            "Must be: " << ref_sum << NcbiEndl;
        return 1;
    }
    NcbiCout << "Passed" << NcbiEndl << NcbiEndl;

    return 0;
}


END_NCBI_SCOPE



/////////////////////////////////////////////////////////////////////////////
//  MAIN


USING_NCBI_SCOPE;

int main(int argc, const char* argv[])
{
    return CTestApp().AppMain(argc, argv, 0, eDS_Default, 0);
}
