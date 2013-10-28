/*  $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *   This software/database is a "United States Government Work" under the
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
 * Authors:
 *   Dmitry Kazimirov
 *
 * File Description:
 *   Multithreaded CCompoundID test.
 *
 */

#include <ncbi_pch.hpp>

#include <connect/services/compound_id.hpp>

#include <util/random_gen.hpp>
#include <util/thread_pool_old.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>

#include <common/test_assert.h>  /* This header must go last */

#define NUMBER_OF_THREADS 24
#define NUMBER_OF_REQUESTS 10000

#define MAX_NUMBER_OF_FIELDS 100
#define MAX_RANDOM_STRING_LEN 5

USING_NCBI_SCOPE;

class CCompoundIDTestRequest : public CStdRequest
{
public:
    CCompoundIDTestRequest(CCompoundIDPool::TInstance compound_id_pool,
            CRandom::TValue random_seed);

protected:
    virtual void Process();

private:
    string x_GenerateRandomString();

    CCompoundIDPool m_CompoundIDPool;
    CRandom m_Random;
};

inline CCompoundIDTestRequest::CCompoundIDTestRequest(
        CCompoundIDPool::TInstance compound_id_pool,
        CRandom::TValue random_seed) :
    m_CompoundIDPool(compound_id_pool),
    m_Random(random_seed)
{
}

#define RANDOM_ROOT (roots[m_Random.GetRandIndex(roots.size())])

void CCompoundIDTestRequest::Process()
{
    // Generate a random compound ID.
    size_t number_of_fields = m_Random.GetRand(1, MAX_NUMBER_OF_FIELDS);

    vector<CCompoundID> roots;

    roots.push_back(m_CompoundIDPool.NewID((ECompoundIDClass)
            m_Random.GetRandIndex(eCIC_NumberOfClasses)));

    while (--number_of_fields > 0)
        switch (m_Random.GetRandIndex(eCIT_NumberOfTypes)) {
        case eCIT_ID:
            RANDOM_ROOT.AppendID(m_Random.GetRand());
            break;
        case eCIT_Integer:
            RANDOM_ROOT.AppendInteger(
                    -(Int8) (m_Random.GetMax() >> 1) + m_Random.GetRand());
            break;
        case eCIT_ServiceName:
            RANDOM_ROOT.AppendServiceName(x_GenerateRandomString());
            break;
        case eCIT_DatabaseName:
            RANDOM_ROOT.AppendDatabaseName(x_GenerateRandomString());
            break;
        case eCIT_Timestamp:
            RANDOM_ROOT.AppendCurrentTime();
            break;
        case eCIT_Random:
            RANDOM_ROOT.AppendRandom(m_Random.GetRand());
            break;
        case eCIT_IPv4Address:
            RANDOM_ROOT.AppendIPv4Address((Uint4) m_Random.GetRand());
            break;
        case eCIT_Host:
            RANDOM_ROOT.AppendHost(x_GenerateRandomString());
            break;
        case eCIT_Port:
            RANDOM_ROOT.AppendPort((Uint2) m_Random.GetRand());
            break;
        case eCIT_IPv4SockAddr:
            RANDOM_ROOT.AppendIPv4SockAddr(
                    (Uint4) m_Random.GetRand(), (Uint2) m_Random.GetRand());
            break;
        case eCIT_Path:
            RANDOM_ROOT.AppendPath(x_GenerateRandomString());
            break;
        case eCIT_String:
            RANDOM_ROOT.AppendString(x_GenerateRandomString());
            break;
        case eCIT_Boolean:
            RANDOM_ROOT.AppendBoolean((m_Random.GetRand() & 1) != 0);
            break;
        case eCIT_Flags:
            RANDOM_ROOT.AppendFlags(m_Random.GetRand());
            break;
        case eCIT_Label:
            RANDOM_ROOT.AppendLabel(x_GenerateRandomString());
            break;
        case eCIT_Cue:
            RANDOM_ROOT.AppendCue(m_Random.GetRand());
            break;
        case eCIT_SeqID:
            RANDOM_ROOT.AppendSeqID(x_GenerateRandomString());
            break;
        case eCIT_TaxID:
            RANDOM_ROOT.AppendTaxID(m_Random.GetRand());
            break;
        case eCIT_NestedCID:
            roots.push_back(m_CompoundIDPool.NewID((ECompoundIDClass)
                    m_Random.GetRandIndex(eCIC_NumberOfClasses)));
            break;
        case eCIT_NumberOfTypes:
            _ASSERT(0);
            break;
        }

    while (roots.size() > 1) {
        size_t container_idx = m_Random.GetRandIndex(roots.size());

        size_t nested_idx = (container_idx +
                m_Random.GetRand(1, roots.size() - 1)) % roots.size();

        roots[container_idx].AppendNestedCID(roots[nested_idx]);

        roots.erase(roots.begin() + nested_idx);
    }

    string packed_id = roots[0].ToString();

    roots.erase(roots.begin());

    CCompoundID unpacked_id = m_CompoundIDPool.FromString(packed_id);

    string dump = unpacked_id.Dump();

    _ASSERT(m_CompoundIDPool.FromDump(dump).ToString() == packed_id);

    if (!unpacked_id.IsEmpty()) {
        unpacked_id.GetFirstField().Remove();
        if (!unpacked_id.IsEmpty())
            unpacked_id.GetLastField().Remove();
        _ASSERT(unpacked_id.ToString() != packed_id);
    }
}

string CCompoundIDTestRequest::x_GenerateRandomString()
{
    int len = m_Random.GetRand(1, MAX_RANDOM_STRING_LEN);

    string char_array;

    do
        char_array += (char) m_Random.GetRandIndex(256);
    while (--len > 0);

    return char_array;
}

class CCompoundIDTestApp : public CNcbiApplication
{
public:
    virtual void Init();
    virtual int Run();
};

void CCompoundIDTestApp::Init()
{
    CNcbiApplication::Init();

    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
        "CCompoundID test application");

    arg_desc->AddOptionalKey("nthreads", "nthreads",
        "Number of worker threads", CArgDescriptions::eInteger);

    arg_desc->AddOptionalKey("nrequests", "nrequests",
        "Number of operations to perform", CArgDescriptions::eInteger);

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}

int CCompoundIDTestApp::Run(void)
{
    CCompoundIDPool compound_id_pool;

    CRandom::TValue random_seed = (CRandom::TValue) time(NULL);

    // Even though the seed is printed out, the test will not
    // be easily reproducible because each thread has its own
    // random generator to avoid having an artificial point of
    // synchronization between threads.
    // The results of this test heavily depend on timing.
    LOG_POST("Using random seed " << random_seed);

    CRandom random_seed_generator(random_seed);

    const CArgs& args = GetArgs();

    int nthreads = NUMBER_OF_THREADS;
    if (args["nthreads"])
        nthreads = args["nthreads"].AsInteger();

    int nrequests = NUMBER_OF_REQUESTS;
    if (args["nrequests"])
        nrequests = args["nrequests"].AsInteger();

    CStdPoolOfThreads pool(nthreads, nrequests);

    pool.Spawn(nthreads);

    for (int i = NUMBER_OF_REQUESTS;  i > 0;  i--)
        pool.AcceptRequest(CRef<ncbi::CStdRequest>(new CCompoundIDTestRequest(
                compound_id_pool, random_seed_generator.GetRand())));

    pool.KillAllThreads(true);

    return 0;
}

int main(int argc, const char* argv[])
{
    return CCompoundIDTestApp().AppMain(argc, argv);
} 
