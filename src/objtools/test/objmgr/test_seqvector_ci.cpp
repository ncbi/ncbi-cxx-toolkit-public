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
* Author:  Aleksey Grichenko
*
* File Description:
*   Test for CSeqVector_CI
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/ncbi_limits.hpp>
#include <util/random_gen.hpp>

// Objects includes
#include <objects/seqloc/Seq_id.hpp>

// Object manager includes
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/seq_vector_ci.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>

#include <objmgr/bioseq_handle.hpp>

BEGIN_NCBI_SCOPE
using namespace objects;


class CTestApp : public CNcbiApplication
{
public:
    virtual void Init(void);
    virtual int  Run (void);

private:
    void x_TestIterate(CSeqVector_CI& vit,
                       TSeqPos start,
                       TSeqPos stop);
    void x_TestGetData(CSeqVector_CI& vit,
                       TSeqPos start,
                       TSeqPos stop);
    void x_TestVector(TSeqPos start,
                      TSeqPos stop);
    bool x_CheckBuf(const string& buf, size_t pos, size_t len) const;

    CRef<CObjectManager> m_OM;
    CSeqVector m_Vect;
    string m_RefBuf;
};


void CTestApp::Init(void)
{
    // Prepare command line descriptions
    //

    // Create
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // GI to fetch
    arg_desc->AddDefaultKey("gi", "SeqEntryID",
                            "GI id of the Seq-Entry to fetch",
                            CArgDescriptions::eInteger,
                            "29791621");
    arg_desc->AddDefaultKey("cycles", "RandomCycles",
                            "repeat random test 'cycles' times",
                            CArgDescriptions::eInteger, "100");
    arg_desc->AddDefaultKey("seed", "RandomSeed",
                            "Force random seed",
                            CArgDescriptions::eInteger, "0");
    arg_desc->AddFlag("no_scope", "Use seq vector with null scope");

    // Program description
    string prog_description = "Test for CSeqVector_CI\n";
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              prog_description, false);

    // Pass argument descriptions to the application
    //

    SetupArgDescriptions(arg_desc.release());
}


bool CTestApp::x_CheckBuf(const string& buf, size_t pos, size_t len) const
{
    if ( pos > m_RefBuf.size() || pos + len > m_RefBuf.size() ||
         buf.size() != len ) {
        return false;
    }
    return memcmp(buf.data(), m_RefBuf.data()+pos, len) == 0;
}


void CTestApp::x_TestIterate(CSeqVector_CI& vit,
                             TSeqPos start,
                             TSeqPos stop)
{
    if (stop > m_Vect.size()) {
        stop = m_Vect.size();
    }
    if (start != kInvalidSeqPos) {
        if (start > m_Vect.size())
            start = m_Vect.size();
        vit.SetPos(start);
    }
    else {
        start = vit.GetPos();
    }

    // cout << "Testing iterator, " << start << " - " << stop << "... ";

    if (start > stop) {
        // Moving down
        const char* data = m_RefBuf.data();
        for ( TSeqPos pos = start; pos > stop;  ) {
            --vit; --pos;
            if ( vit.GetPos() != pos ) {
                cout << endl << "ERROR: GetPos failed at position " << pos << endl;
                throw runtime_error("Test failed");
            }
            if (*vit != data[pos]) {
                cout << endl << "ERROR: Test failed at position " << pos << endl;
                throw runtime_error("Test failed");
            }
        }
    }
    else {
        // Moving up
        const char* data = m_RefBuf.data();
        for ( TSeqPos pos = start; pos < stop; ++vit, ++pos ) {
            if ( vit.GetPos() != pos ) {
                cout << endl << "ERROR: GetPos failed at position " << pos << endl;
                throw runtime_error("Test failed");
            }
            if (*vit != data[pos]) {
                cout << endl << "ERROR: Test failed at position " << pos << endl;
                throw runtime_error("Test failed");
            }
        }
    }

    // cout << "OK" << endl;
}


void CTestApp::x_TestGetData(CSeqVector_CI& vit,
                             TSeqPos start,
                             TSeqPos stop)
{
    if (start == kInvalidSeqPos) {
        start = vit.GetPos();
    }

    if (start > stop)
        swap(start, stop);

    // cout << "Testing GetSeqData(" << start << ", " << stop << ")... " << endl;

    string buf;
    vit.GetSeqData(start, stop, buf);
    if (start >= stop  &&  buf.size() > 0) {
        cout << endl << "ERROR: Test failed -- invalid data length" << endl;
        throw runtime_error("Test failed");
    }

    if (stop > m_Vect.size())
        stop = m_Vect.size();

    if ( !x_CheckBuf(buf, start, stop - start) ) {
        cout << endl << "ERROR: Test failed -- invalid data" << endl;
        throw runtime_error("Test failed");
    }
    // cout << "OK" << endl;
}


void CTestApp::x_TestVector(TSeqPos start,
                            TSeqPos stop)
{
    if (start > m_Vect.size()) {
        start = m_Vect.size();
    }
    if (stop > m_Vect.size()) {
        stop = m_Vect.size();
    }
    // cout << "Testing vector[], " << start << " - " << stop << "... ";

    if (start > stop) {
        // Moving down
        const char* data = m_RefBuf.data();
        for (TSeqPos i = start; i > stop; ) {
            --i;
            if (m_Vect[i] != data[i]) {
                cout << endl << "ERROR: Test failed at position " << i << endl;
                throw runtime_error("Test failed");
            }
        }
    }
    else {
        // Moving up
        const char* data = m_RefBuf.data();
        for (TSeqPos i = start; i < stop; ++i) {
            if (m_Vect[i] != data[i]) {
                cout << endl << "ERROR: Test failed at position " << i << endl;
                throw runtime_error("Test failed");
            }
        }
    }

    // cout << "OK" << endl;
}


int CTestApp::Run(void)
{
    // Process command line args: get GI to load
    const CArgs& args = GetArgs();

    // GI with many segments of different sizes.
    int gi = args["gi"].AsInteger(); // 29791621;

    m_OM = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager(*m_OM);

    CScope scope(*m_OM);
    scope.AddDefaults();

    CSeq_id id;
    id.SetGi(gi);
    CBioseq_Handle handle = scope.GetBioseqHandle(id);

    // Check if the handle is valid
    if ( !handle ) {
        cout << "Can not find gi " << gi << endl;
        return 0;
    }
    cout << "Testing gi " << gi << endl;

    // Create seq-vector
    if ( !args["no_scope"] ) {
        m_Vect = handle.GetSeqVector(CBioseq_Handle::eCoding_Iupac);
    }
    else {
        CScope* no_scope = 0;
        m_Vect = CSeqVector(handle.GetSeqMap(), *no_scope,
                            CBioseq_Handle::eCoding_Iupac);
    }
    // Prepare reference data
    m_Vect.GetSeqData(0, m_Vect.size(), m_RefBuf);

    CSeqVector_CI vit = CSeqVector_CI(m_Vect);

    string buf;

    // start > stop test
    cout << "Testing empty range (start > stop)... ";
    vit.GetSeqData(m_Vect.size(), 0, buf);
    if (buf.size() != 0) {
        cout << endl << "ERROR: Test failed -- got non-empty data" << endl;
        throw runtime_error("Empty range test failed");
    }
    cout << "OK" << endl;

    // stop > length test
    cout << "Testing past-end read (stop > size)... ";
    vit.GetSeqData(max<TSeqPos>(m_Vect.size(), 100) - 100,
                   m_Vect.size() + 1,
                   buf);
    if ( !x_CheckBuf(buf,
                     max<TSeqPos>(m_Vect.size(), 100) - 100,
                     min<TSeqPos>(m_Vect.size(), 100)) ) {
        cout << "ERROR: GetSeqData() failed -- invalid data" << endl;
        throw runtime_error("Past-end read test failed");
    }
    cout << "OK" << endl;

    buf = ""; // .erase();

    // Compare iterator with GetSeqData()
    // Not using x_TestIterate() to also check operator bool()
    cout << "Testing basic iterator... ";
    const char* data = m_RefBuf.data();
    const char* data_end = data + m_RefBuf.size();
    const char* c = data;
    for (vit.SetPos(0); vit; ++vit, ++c) {
        if (c == data_end ||  *vit != *c) {
            cout << "ERROR: Invalid data at " << vit.GetPos() << endl;
            throw runtime_error("Basic iterator test failed");
        }
    }
    if (c != data_end) {
        cout << "ERROR: Invalid data length" << endl;
        throw runtime_error("Basic iterator test failed");
    }
    for (vit.SetPos(0), c = data; vit; ) {
        if (c == data_end ||  *vit++ != *c++) {
            cout << "ERROR: Invalid data at " << vit.GetPos()-1 << endl;
            throw runtime_error("Basic iterator test failed");
        }
    }
    if (c != data_end) {
        cout << "ERROR: Invalid data length" << endl;
        throw runtime_error("Basic iterator test failed");
    }
    cout << "OK" << endl;

    // Partial retrieval with GetSeqData() test, limit to 2000 bases
    cout << "Testing partial retrieval... ";
    for (unsigned i = max<int>(1, m_Vect.size() / 2 - 2000);
         i <= m_Vect.size() / 2; ++i) {
        x_TestGetData(vit, i, m_Vect.size() - i);
    }
    cout << "OK" << endl;

    // Butterfly test
    cout << "Testing butterfly reading... ";
    for (unsigned i = 1; i < m_Vect.size() / 2; ++i) {
        if (m_Vect[i] != data[i]) {
            cout << "ERROR: Butterfly test failed at position " << i << endl;
            throw runtime_error("Butterfly read test failed");
        }
    }
    cout << "OK" << endl;

    TSeqPos pos1 = 0;
    TSeqPos pos2 = 0;
    TSeqPos pos3 = 0;
    TSeqPos pos4 = 0;

    {{
        SSeqMapSelector sel(CSeqMap::fDefaultFlags, kMax_UInt);
        CSeqMap_CI map_it(handle, sel);
        if ( map_it ) {
            // Should happen for any non-empty sequence
            pos2 = map_it.GetEndPosition();
            ++map_it;
            if ( map_it ) {
                // Should happen for most segmented sequences
                pos3 = map_it.GetEndPosition();
                ++map_it;
                if ( map_it ) {
                    // May happen for some segmented sequences
                    pos4 = map_it.GetEndPosition();
                }
            }
        }
    }}

    // ========= Iterator tests ==========
    cout << "Testing iterator - single segment... ";
    // Single segment test, start from the middle of the first segment
    vit = CSeqVector_CI(m_Vect, (pos1 + pos2) / 2);
    // Back to the first segment start
    x_TestIterate(vit, kInvalidSeqPos, pos1);
    // Forward to the first segment end
    x_TestIterate(vit, kInvalidSeqPos, pos2);
    // Back to the first segment start again
    x_TestIterate(vit, kInvalidSeqPos, pos1);
    cout << "OK" << endl;

    // Try to run multi-segment tests
    if ( pos3 ) {
        cout << "Testing iterator - multiple segments... ";
        // Start from the middle of the second segment
        vit = CSeqVector_CI(m_Vect, (pos2 + pos3) / 2);
        // Back to the first segment start
        x_TestIterate(vit, kInvalidSeqPos, pos1);
        // Forward to the second or third segment end
        x_TestIterate(vit, kInvalidSeqPos, pos4 ? pos4 : pos3);
        // Back to the first segment start again
        x_TestIterate(vit, kInvalidSeqPos, pos1);
        cout << "OK" << endl;
    }

    // ========= GetSeqData() tests ==========
    cout << "Testing GetSeqData() - single segment... ";
    // Single segment test, start from the middle of the first segment
    vit = CSeqVector_CI(m_Vect, (pos1 + pos2) / 2);
    // Back to the first segment start
    x_TestGetData(vit, kInvalidSeqPos, pos1);
    // Forward to the first segment end
    x_TestGetData(vit, kInvalidSeqPos, pos2);
    // Back to the first segment start again
    x_TestGetData(vit, kInvalidSeqPos, pos1);
    cout << "OK" << endl;

    // Try to run multi-segment tests
    if ( pos3 ) {
        cout << "Testing GetSeqData() - multiple segments... ";
        // Start from the middle of the second segment
        vit = CSeqVector_CI(m_Vect, (pos2 + pos3) / 2);
        // Back to the first segment start
        x_TestGetData(vit, kInvalidSeqPos, pos1);
        // Forward to the second or third segment end
        x_TestGetData(vit, kInvalidSeqPos, pos4 ? pos4 : pos3);
        // Back to the first segment start again
        x_TestGetData(vit, kInvalidSeqPos, pos1);
        cout << "OK" << endl;
    }

    // ========= CSeqVector[] tests ==========
    cout << "Testing operator[] - single segment... ";
    // Single segment test, start from the middle of the first segment
    // Back to the first segment start
    x_TestVector((pos1 + pos2) / 2, pos1);
    // Forward to the first segment end
    x_TestVector(pos1, pos2);
    // Back to the first segment start again
    x_TestVector(pos2, pos1);
    cout << "OK" << endl;

    // Try to run multi-segment tests
    if ( pos3 ) {
        cout << "Testing operator[] - multiple segments... ";
        // Start from the middle of the second segment
        // Back to the first segment start
        x_TestVector((pos2 + pos3) / 2, pos1);
        // Forward to the second or third segment end
        x_TestVector(pos1, pos4 ? pos4 : pos3);
        // Back to the first segment start again
        x_TestVector(pos4 ? pos4 : pos3, pos1);
        cout << "OK" << endl;
    }

    // Random access tests
    cout << "Testing random access" << endl;
    unsigned int rseed = args["seed"].AsInteger();
    int cycles = args["cycles"].AsInteger();
    if (rseed == 0) {
        rseed = (unsigned)time( NULL );
    }
    cout << "Testing random reading (seed: " << rseed
         << ", cycles: " << cycles << ")... " << endl;
    CRandom random(rseed);
    vit = CSeqVector_CI(m_Vect);
    for (int i = 0; i < cycles; ++i) {
        TSeqPos start = random.GetRand(0, m_Vect.size());
        TSeqPos stop = random.GetRand(0, m_Vect.size());
        switch (i % 3) {
        case 0:
            x_TestIterate(vit, start, stop);
            break;
        case 1:
            x_TestGetData(vit, start, stop);
            break;
        case 2:
            x_TestVector(start, stop);
            break;
        }
    }
    cout << "OK" << endl;

    NcbiCout << "All tests passed" << NcbiEndl;
    return 0;
}


END_NCBI_SCOPE



/////////////////////////////////////////////////////////////////////////////
//  MAIN


USING_NCBI_SCOPE;

int main(int argc, const char* argv[])
{
    return CTestApp().AppMain(argc, argv);
}

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.9  2005/03/28 19:37:12  vasilche
* Test post-increment operator.
*
* Revision 1.8  2004/12/22 15:56:44  vasilche
* Fix CSeqMap usage to allow used TSE linking.
*
* Revision 1.7  2004/08/25 15:03:56  grichenk
* Removed duplicate methods from CSeqMap
*
* Revision 1.6  2004/07/21 15:51:26  grichenk
* CObjectManager made singleton, GetInstance() added.
* CXXXXDataLoader constructors made private, added
* static RegisterInObjectManager() and GetLoaderNameFromArgs()
* methods.
*
* Revision 1.5  2004/05/21 21:42:56  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.4  2004/04/12 23:24:27  ucko
* Avoid directly "dereferencing" a null scope, to fix the MIPSpro build.
*
* Revision 1.3  2004/04/12 16:49:56  vasilche
* Added option to test CSeqVector with null scope.
*
* Revision 1.2  2004/04/09 20:35:32  vasilche
* Fixed test on short sequences (<100).
*
* Revision 1.1  2003/12/16 17:51:23  kuznets
* Code reorganization
*
* Revision 1.6  2003/11/12 20:16:15  vasilche
* Fixed error: Attempt to delete Object Manager with open scopes.
*
* Revision 1.5  2003/08/29 13:34:48  vasilche
* Rewrote CSeqVector/CSeqVector_CI code to allow better inlining.
* CSeqVector::operator[] made significantly faster.
* Added possibility to have platform dependent byte unpacking functions.
*
* Revision 1.4  2003/08/06 20:51:54  grichenk
* Use CRandom class
*
* Revision 1.3  2003/07/09 18:49:33  grichenk
* Added arguments (seed and cycles), default random cycles set to 20.
*
* Revision 1.2  2003/06/26 17:02:52  grichenk
* Simplified output, decreased number of cycles.
*
* Revision 1.1  2003/06/24 19:50:02  grichenk
* Added test for CSeqVector_CI
*
*
* ===========================================================================
*/

