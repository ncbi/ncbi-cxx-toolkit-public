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
 * Author:  Mike DiCuccio
 *
 * File Description:
 *    Performance tests for various iterators
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbitime.hpp>

#include <connect/ncbi_core_cxx.hpp>

// Object Manager includes
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/object_manager.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>

#include <serial/serial.hpp>
#include <serial/objistrasn.hpp>

#include <util/md5.hpp>

#include <vector>

using namespace ncbi;
using namespace objects;


/////////////////////////////////////////////////////////////////////////////
//
//  Demo application
//


class CSeqVecBench : public CNcbiApplication
{
public:
    virtual void Init(void);
    virtual int  Run (void);

    void start(const char* name);
    void start_iter(void);
    void end_iter(void);
    void end(void);
    void end_all(void);

    CStopWatch sw;
    vector<string> names;
    vector<double> times;
    bool get_best;
    unsigned test_count;
    unsigned counter;

    static int result_mask;
};


void CSeqVecBench::Init(void)
{
    // Prepare command line descriptions
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // GI to fetch
    arg_desc->AddOptionalKey("id", "Accession",
                             "sequence id to load and test",
                             CArgDescriptions::eString);

    arg_desc->AddDefaultKey("iters", "Iterations",
                             "number of iterations to run",
                             CArgDescriptions::eInteger, "5");

    arg_desc->AddFlag("average", "collect average time instead of minumum");
    arg_desc->AddFlag("iupac", "test Iupac coding");
    arg_desc->AddFlag("minus", "test minus strand of sequence");

    // Pass argument descriptions to the application
    //

    SetupArgDescriptions(arg_desc.release());
}

void CSeqVecBench::start(const char* n)
{
    names.push_back(n);
    times.push_back(0);
    cout << "Running test: " << names.back() << "..." << endl;
    test_count = 0;
}

inline
void CSeqVecBench::start_iter(void)
{
    counter = 0;
    sw.Start();
}


inline
void CSeqVecBench::end_iter(void)
{
    double time = sw.Elapsed();
    if ( get_best ) {
        if ( test_count == 0 || time < times.back() ) {
            times.back() = time;
            test_count = 1;
        }
    }
    else {
        times.back() += time;
        test_count += 1;
    }
}


void CSeqVecBench::end(void)
{
    times.back() /= test_count;
    cout << setw(40) << names.back() << " : " << 
        counter << " bases in " << times.back() << " secs" << endl;
}


void CSeqVecBench::end_all(void)
{
    cout << endl << "Normalized to "<<names.front()<<":" << endl;
    for ( size_t i = 1; i < names.size(); ++i ) {
        cout << setw(40) << names[i] << " : " << (times[i] / times[0]) << endl;
    }
}


int CSeqVecBench::result_mask = 0;


int CSeqVecBench::Run(void)
{
    const CArgs& args = GetArgs();

    CRef<CSeq_id> id;
    try {
        id.Reset(new CSeq_id(args["id"].AsString()));
    } catch (CSeqIdException& e) {
        LOG_POST(Fatal << "seq id " << args["id"].AsString()
                 << " not recognized: " << e.what());
    }

    // Setup application registry, error log, and MT-lock for CONNECT library
    CONNECT_Init(&GetConfig());

    CRef<CObjectManager> object_manager = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager(*object_manager);
    CScope scope(*object_manager);
    scope.AddDefaults();

    CBioseq_Handle handle = scope.GetBioseqHandle(*id);
    if ( !handle ) {
        LOG_POST(Fatal << "failed to retrieve sequence "
                 << args["id"].AsString());
    }

    // number of iterations we perform
    int iters = args["iters"].AsInteger();
    CBioseq_Handle::EVectorCoding coding = args["iupac"]?
        CBioseq_Handle::eCoding_Iupac: CBioseq_Handle::eCoding_Ncbi;
    CBioseq_Handle::EVectorStrand strand = args["minus"]?
        CBioseq_Handle::eStrand_Minus: CBioseq_Handle::eStrand_Plus;

    get_best = !args["average"];

    //
    // we ignore the first iteration, as additional fetching may occur here
    //
    {{
        cout << "forcing retrieval of whole sequence..." << endl;
        sw.Start();
        string sequence;
        CSeqVector vec = handle.GetSeqVector(coding, strand);
        vec.GetSeqData(0, vec.size(), sequence);
        cout << "  sequence is " << sequence.length() << " bases" << endl;
        CMD5 sum;
        sum.Update(sequence.data(), sequence.size());
        cout << "  md5 sum is: " << sum.GetHexSum() << endl;
        double load_time = sw.Elapsed();
        cout << "  loaded in " << load_time << " seconds" << endl;
    }}

    unsigned char mask = 0;
    int i;

    //
    // test 4: bulk retrieval
    //
    start("only string::const_iterator");
    {{
        CSeqVector vec = handle.GetSeqVector(coding, strand);
        string str;
        vec.GetSeqData(0, vec.size(), str);
        const int mul1 = 1;
        const int mul2 = 3;
        for (i = iters*mul1;  i;  --i) {
            start_iter();
            for ( int j = 0; j < mul2; ++j ) {
                ITERATE(string, iter, str) {
                    mask ^= *iter;
                    ++counter;
                }
            }
            end_iter();
        }
        counter /= mul2;
        times.back() /= mul2;
    }}
    end();

    //
    // test 1: iterate CSeqVector using operator[]
    //
    start("CSeqVector::operator[]");
    for (i = 0;  i < iters;  ++i) {
        start_iter();
        CSeqVector vec = handle.GetSeqVector(coding, strand);
        for (size_t j = 0;  j < vec.size();  ++j, ++counter) {
            mask ^= vec[j];
        }
        end_iter();
    }
    end();

    //
    // test 1a: iterate CSeqVector using operator[]
    //
    start("double CSeqVector::operator[]");
    for (i = 0;  i < iters;  ++i) {
        start_iter();
        CSeqVector vec = handle.GetSeqVector(coding, strand);
        for (size_t j = 0;  j < vec.size();  ++j, ++counter) {
            mask ^= vec[j];
            mask ^= vec[j];
        }
        end_iter();
    }
    end();

    //
    // test 2: bulk retrieval
    //
    start("GetSeqData() + string::operator[]");
    for (i = 0;  i < iters;  ++i) {
        start_iter();
        CSeqVector vec = handle.GetSeqVector(coding, strand);
        string str;
        vec.GetSeqData(0, vec.size(), str);
        for (size_t j = 0;  j < str.size();  ++j, ++counter) {
            mask ^= str[j];
        }
        end_iter();
    }
    end();

    //
    // test 3: bulk retrieval
    //
    start("GetSeqData() + string::const_iterator");
    for (i = 0;  i < iters;  ++i) {
        start_iter();
        CSeqVector vec = handle.GetSeqVector(coding, strand);
        string str;
        vec.GetSeqData(0, vec.size(), str);
        ITERATE(string, iter, str) {
            mask ^= *iter;
            ++counter;
        }
        end_iter();
    }
    end();

    //
    // test 4: bulk retrieval
    //
    start("only string::const_iterator");
    {{
        CSeqVector vec = handle.GetSeqVector(coding, strand);
        string str;
        vec.GetSeqData(0, vec.size(), str);
        const int mul1 = 1;
        const int mul2 = 1;
        for (i = iters*mul1;  i;  --i) {
            start_iter();
            for ( int j = 0; j < mul2; ++j ) {
                ITERATE(string, iter, str) {
                    mask ^= *iter;
                    ++counter;
                }
            }
            end_iter();
        }
        counter /= mul2;
        times.back() /= mul2;
    }}
    end();

    //
    // test 5: CSeqVector_CI
    //
    start("CSeqVector_CI");
    for (i = 0;  i < iters;  ++i) {
        start_iter();
        CSeqVector vec = handle.GetSeqVector(coding, strand);
        for ( CSeqVector_CI iter(vec); iter; ++iter, ++counter ) {
            mask ^= *iter;
        }
        end_iter();
    }
    end();

    //
    // test 6: ITERATE
    //
    start("ITERATE");
    for (i = 0;  i < iters;  ++i) {
        start_iter();
        CSeqVector vec = handle.GetSeqVector(coding, strand);
        ITERATE ( CSeqVector, iter, vec ) {
            mask ^= *iter;
            ++counter;
        }
        end_iter();
    }
    end();

    //
    // test 7: chunks
    start("GetSeqData()/ch + string::operator[]");
    for (i = 0;  i < iters;  ++i) {
        start_iter();
        CSeqVector vec = handle.GetSeqVector(coding, strand);
        static const TSeqPos chunk_size = 4096;
        string buf;
        for ( TSeqPos start = 0, size = vec.size();
              start < size;  start += chunk_size ) {
            TSeqPos count = size - start;
            if ( chunk_size < count )
                count = chunk_size;
            vec.GetSeqData(start, start + count, buf);
            for (size_t j = 0;  j < buf.size();  ++j, ++counter) {
                mask ^= buf[j];
            }
        }
        end_iter();
    }
    end();

    //
    // test 8: chunks
    start("GetSeqData()/ch + string::const_iterator");
    for (i = 0;  i < iters;  ++i) {
        start_iter();
        CSeqVector vec = handle.GetSeqVector(coding, strand);
        static const TSeqPos chunk_size = 4096;
        string buf;
        for ( TSeqPos start = 0, size = vec.size();
              start < size;  start += chunk_size ) {
            TSeqPos count = size - start;
            if ( chunk_size < count )
                count = chunk_size;
            vec.GetSeqData(start, start + count, buf);
            ITERATE(string, iter, buf) {
                mask ^= *iter;
                ++counter;
            }
        }
        end_iter();
    }
    end();

    end_all();

    result_mask ^= mask;
    
    return 0;
}

/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    return CSeqVecBench().AppMain(argc, argv, 0, eDS_Default, 0);
}
