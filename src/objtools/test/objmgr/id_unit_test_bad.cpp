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
*   Unit test for data loading from ID.
*/

#define NCBI_TEST_APPLICATION

#include <ncbi_pch.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/align_ci.hpp>
#include <objmgr/graph_ci.hpp>
#include <objmgr/annot_ci.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objtools/data_loaders/genbank/readers.hpp>

#include <corelib/ncbi_system.hpp>
#include <dbapi/driver/drivers.hpp>
#include <connect/ncbi_core_cxx.hpp>
#include <connect/ncbi_util.h>
#include <algorithm>
#include <numeric>

#include <objects/general/general__.hpp>
#include <objects/seqfeat/seqfeat__.hpp>
#include <serial/iterator.hpp>
#include <util/random_gen.hpp>

#include <corelib/test_boost.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);


NCBI_PARAM_DECL(bool, TEST, TRACE);
NCBI_PARAM_DEF_EX(bool, TEST, TRACE, false,
                  eParam_NoThread, TEST_TRACE);


static
bool s_IsTraceEnabled()
{
    static bool trace_enabled = NCBI_PARAM_TYPE(TEST, TRACE)::GetDefault();
    return trace_enabled;
}


#define TRACE_POST(msg) if ( !s_IsTraceEnabled() ); else LOG_POST(msg)


static CRandom s_Random;


template<class I>
void s_RandomShuffle(I iter1, I iter2)
{
    for ( int s = int(distance(iter1, iter2)); s > 1; --s, ++iter1 ) {
        swap(*iter1, *next(iter1, s_Random.GetRandIndex(s)));
    }
}


static CRef<CScope> s_InitScope(bool reset_loader = true)
{
    CRef<CObjectManager> om = CObjectManager::GetInstance();
    if ( reset_loader ) {
        CDataLoader* loader =
            om->FindDataLoader(CGBDataLoader::GetLoaderNameFromArgs());
        if ( loader ) {
            BOOST_CHECK(om->RevokeDataLoader(*loader));
        }
    }
#ifdef HAVE_PUBSEQ_OS
    DBAPI_RegisterDriver_FTDS();
    GenBankReaders_Register_Pubseq();
#endif
    CGBDataLoader::RegisterInObjectManager(*om);
    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();
    return scope;
}


template<class E>
string s_AsString(const vector<E>& ids)
{
    CNcbiOstrstream out;
    out << '{';
    for ( auto& e : ids ) {
        out << ' ' << e;
    }
    out << " }";
    return CNcbiOstrstreamToString(out);
}


static
const char* s_GetGBReader()
{
    const char* env = getenv("GENBANK_LOADER_METHOD_BASE");
    if ( !env ) {
        env = getenv("GENBANK_LOADER_METHOD");
    }
    if ( !env ) {
        // assume default ID2
        TRACE_POST("Assuming default reader ID2");
        env = "ID2";
    }
    return env;
}


bool s_CalcHaveID2(void)
{
    const char* env = s_GetGBReader();
    if ( NStr::EndsWith(env, "id1", NStr::eNocase) ||
         NStr::EndsWith(env, "pubseqos", NStr::eNocase) ) {
        // non-ID2 based readers
        TRACE_POST("No ID2, env=\""<<env<<"\"");
        return false;
    }
    else {
        TRACE_POST("ID2, env=\""<<env<<"\"");
        return true;
    }
}


bool s_CalcHaveID1(void)
{
    const char* env = s_GetGBReader();
    if ( NStr::EndsWith(env, "id1", NStr::eNocase) ) {
        TRACE_POST("ID1, env=\""<<env<<"\"");
        return true;
    }
    else {
        TRACE_POST("No ID1, env=\""<<env<<"\"");
        return false;
    }
}


bool s_HaveID2(void)
{
    static bool ret = s_CalcHaveID2();
    return ret;
}


bool s_HaveID1(void)
{
    static bool ret = s_CalcHaveID1();
    return ret;
}


static const CScope::TGetFlags kThrowNoData =
           CScope::fForceLoad | CScope::fThrowOnMissingData;
static const CScope::TGetFlags kThrowNoSeq =
           CScope::fForceLoad | CScope::fThrowOnMissingSequence;

// for various orders of operations:
// get fresh scope -> iterate 2 times ->
// iterate over all operations -> iterate 2 times the operation

// all_orders<> - iterate all possible orders of operations
// random_orders<> - iterate several random orders of operations

struct scope_operation
{
    int op;
    bool last;
    CScope* scope;

    scope_operation(int op, bool last, const CRef<CScope>& scope)
        : op(op), last(last), scope(scope.GetNCPointerOrNull())
        {
        }

    CScope* operator->() const { return scope; }
};

template<int N_OPS>
struct all_orders {
    enum AtEnd {
        at_end
    };
    static const int kCount0 = 1;
    static const int kCount1 = 2;
    static const int kCount2 = 2;

    typedef scope_operation value_type;

    struct const_iterator {
        const_iterator()
            : t0(0), t1(0), t2(0), i(0)

            {
                iota(std::begin(ops),
                     std::end(ops),
                     0);
                start_scope();
            }
        explicit
        const_iterator(AtEnd)
            : t0(kCount0), t1(0), t2(0), i(0)
            {
            }
        
        void start_scope()
            {
                TRACE_POST("Start");
                scope = s_InitScope();
            }

        bool operator==(const_iterator& b) const
            {
                return (t0 == b.t0 &&
                        t1 == b.t1 &&
                        t2 == b.t2 &&
                        i == b.i);
            }
        bool operator!=(const_iterator& b) const
            {
                return !(*this == b);
            }

        value_type operator*() const
            {
                return value_type(ops[i], i == N_OPS-1, scope);
            }

        const_iterator& operator++()
            {
                if ( ++t2 < kCount2 ) {
                    return *this;
                }
                t2 = 0;
                if ( ++i < N_OPS ) {
                    return *this;
                }
                i = 0;
                if ( ++t1 < kCount1 ) {
                    return *this;
                }
                t1 = 0;
                scope = null;
                if ( next_permutation(std::begin(ops),
                                      std::end(ops)) ) {
                    start_scope();
                    return *this;
                }
                if ( ++t0 < kCount0 ) {
                    start_scope();
                    return *this;
                }
                return *this;
            }

    private:
        int t0, t1, t2, i;
        int ops[N_OPS];
        CRef<CScope> scope;
    };

    const_iterator begin() const
        {
            return const_iterator();
        }
    const_iterator end() const
        {
            return const_iterator(at_end);
        }
};


template<int N_OPS>
struct random_orders {
    enum AtEnd {
        at_end
    };
    static const int kCount1 = 2;
    static const int kCount2 = 2;

    typedef scope_operation value_type;
    
    struct const_iterator {
        explicit
        const_iterator(int count0)
            : t0(0), count0(count0), t1(0), t2(0), i(0)

            {
                iota(std::begin(ops),
                     std::end(ops),
                     0);
                start_scope();
            }
        const_iterator(AtEnd, int count0)
            : t0(count0), count0(count0), t1(0), t2(0), i(0)
            {
            }
        
        void start_scope()
            {
                TRACE_POST("Start");
                s_RandomShuffle (std::begin(ops), std::end(ops));
                scope = s_InitScope();
            }

        bool operator==(const_iterator& b) const
            {
                return (t0 == b.t0 &&
                        t1 == b.t1 &&
                        t2 == b.t2 &&
                        i == b.i);
            }
        bool operator!=(const_iterator& b) const
            {
                return !(*this == b);
            }

        value_type operator*() const
            {
                return value_type(ops[i], i == N_OPS-1, scope);
            }

        const_iterator& operator++()
            {
                if ( ++t2 < kCount2 ) {
                    return *this;
                }
                t2 = 0;
                if ( ++i < N_OPS ) {
                    return *this;
                }
                i = 0;
                if ( ++t1 < kCount1 ) {
                    return *this;
                }
                t1 = 0;
                scope = null;
                if ( ++t0 < count0 ) {
                    start_scope();
                }
                return *this;
            }

    private:
        int t0, count0, t1, t2, i;
        int ops[N_OPS];
        CRef<CScope> scope;
    };

    const_iterator begin() const
        {
            return const_iterator(count0);
        }
    const_iterator end() const
        {
            return const_iterator(at_end, count0);
        }

    explicit
    random_orders(int count0)
        : count0(count0)
        {
        }

private:
    int count0;
};


BOOST_AUTO_TEST_CASE(CheckNoSeqGi)
{
    // no sequence, check GI loading methods
    // should work with all readers
    CSeq_id_Handle id = CSeq_id_Handle::GetGiHandle(GI_CONST(1));
    vector<CSeq_id_Handle> idvec(1, id);
    LOG_POST("CheckNoSeqGi: "<<id);
    for ( auto op : all_orders<4>() ) {
        switch ( op.op ) {
        case 0:
            TRACE_POST("GetGi");
            BOOST_CHECK(!op->GetGi(id, kThrowNoData));
            break;
        case 1:
            TRACE_POST("GetGiThrow");
            BOOST_CHECK_THROW(op->GetGi(id, kThrowNoSeq), CObjMgrException);
            break;
        case 2:
            TRACE_POST("GetGiBulk");
            BOOST_CHECK(!op->GetGis(idvec, kThrowNoData)[0]);
            break;
        case 3:
            TRACE_POST("GetGiBulkThrow");
            BOOST_CHECK_THROW(op->GetGis(idvec, kThrowNoSeq), CObjMgrException);
            break;
        }
        if ( op.last ) {
            TRACE_POST("GetIds");
            BOOST_CHECK(op->GetIds(id).empty());
        }
    }
}


BOOST_AUTO_TEST_CASE(CheckNoSeqAcc)
{
    // no sequence, check acc loading methods
    // should work with all readers
    CSeq_id_Handle id = CSeq_id_Handle::GetGiHandle(GI_CONST(1));
    vector<CSeq_id_Handle> idvec(1, id);
    LOG_POST("CheckNoSeqAcc: "<<id);
    for ( auto op : all_orders<4>() ) {
        switch ( op.op ) {
        case 0:
            TRACE_POST("GetAccVer");
            BOOST_CHECK(!op->GetAccVer(id, kThrowNoData));
            break;
        case 1:
            TRACE_POST("GetAccVerThrow");
            BOOST_CHECK_THROW(op->GetAccVer(id, kThrowNoSeq), CObjMgrException);
            break;
        case 2:
            TRACE_POST("GetAccVerBulk");
            BOOST_CHECK(!op->GetAccVers(idvec, kThrowNoData)[0]);
            break;
        case 3:
            TRACE_POST("GetAccVerBulkThrow");
            BOOST_CHECK_THROW(op->GetAccVers(idvec, kThrowNoSeq), CObjMgrException);
            break;
        }
        if ( op.last ) {
            TRACE_POST("GetIds");
            BOOST_CHECK(op->GetIds(id).empty());
        }
    }
}


BOOST_AUTO_TEST_CASE(CheckNoSeqAll)
{
    // no sequence, check all loading methods
    // should work with all readers
    CSeq_id_Handle id = CSeq_id_Handle::GetGiHandle(GI_CONST(1));
    vector<CSeq_id_Handle> idvec(1, id);
    LOG_POST("CheckNoSeq: "<<id);
    for ( auto op : random_orders<8>(20) ) {
        switch ( op.op ) {
        case 0:
            TRACE_POST("GetAccVer");
            BOOST_CHECK(!op->GetAccVer(id));
            break;
        case 1:
            TRACE_POST("GetAccVerBulk");
            BOOST_CHECK(!op->GetAccVers(idvec, kThrowNoData)[0]);
            break;
        case 2:
            TRACE_POST("GetAccVerBulkThrow");
            BOOST_CHECK_THROW(op->GetAccVers(idvec, kThrowNoSeq), CObjMgrException);
            break;
        case 3:
            TRACE_POST("GetGi");
            BOOST_CHECK(!op->GetGi(id));
            break;
        case 4:
            TRACE_POST("GetGiBulk");
            BOOST_CHECK(!op->GetGis(idvec, kThrowNoData)[0]);
            break;
        case 5:
            TRACE_POST("GetGiBulkThrow");
            BOOST_CHECK_THROW(op->GetGis(idvec, kThrowNoSeq), CObjMgrException);
            break;
        case 6:
            TRACE_POST("GetIds");
            BOOST_CHECK(op->GetIds(id).empty());
            break;
        case 7:
            TRACE_POST("GetBioseqHandle");
            BOOST_CHECK(!op->GetBioseqHandle(id));
            break;
        }
    }
}


BOOST_AUTO_TEST_CASE(CheckNoAcc)
{
    // have GI, have no accession
    // should work with all readers
    CSeq_id_Handle id = CSeq_id_Handle::GetGiHandle(GI_CONST(156205));
    vector<CSeq_id_Handle> idvec(1, id);
    LOG_POST("CheckNoAcc: "<<id);
    for ( auto op : all_orders<4>() ) {
        switch ( op.op ) {
        case 0:
            TRACE_POST("GetAccVer");
            BOOST_CHECK(!op->GetAccVer(id, kThrowNoSeq));
            break;
        case 1:
            TRACE_POST("GetAccVer");
            BOOST_CHECK_THROW(op->GetAccVer(id, kThrowNoData), CObjMgrException);
            break;
        case 2:
            TRACE_POST("GetAccVerBulk");
            BOOST_CHECK(!op->GetAccVers(idvec, kThrowNoSeq)[0]);
            break;
        case 3:
            TRACE_POST("GetAccVerBulkThrow");
            BOOST_CHECK_THROW(op->GetAccVers(idvec, kThrowNoData), CObjMgrException);
            break;
        }
        if ( op.last ) {
            TRACE_POST("GetGi");
            BOOST_CHECK(op->GetGi(id, kThrowNoSeq));
            TRACE_POST("GetIds");
            BOOST_CHECK(!op->GetIds(id).empty());
            TRACE_POST("GetBioseqHandle");
            BOOST_CHECK(op->GetBioseqHandle(id));
        }
    }
}


BOOST_AUTO_TEST_CASE(CheckNoGi)
{
    // have accession, have no GI
    // shouldn't work with ID1 reader
    CSeq_id_Handle id = CSeq_id_Handle::GetHandle("gnl|Annot:SNP|568802115");
    //CSeq_id_Handle id = CSeq_id_Handle::GetHandle("gnl|SRA|SRR000010.1.2");
    vector<CSeq_id_Handle> idvec(1, id);
    LOG_POST("CheckNoGi: "<<id);
    for ( auto op : all_orders<4>() ) {
        switch ( op.op ) {
        case 0:
            TRACE_POST("GetGi");
            if ( s_HaveID1() ) {
                // id1 treat no-data as no-seq
                BOOST_CHECK_THROW(op->GetGi(id, kThrowNoSeq), CObjMgrException);
            }
            else {
                // non-id1 distinguish no-data/no-seq
                BOOST_CHECK(!op->GetGi(id, kThrowNoSeq));
            }
            break;
        case 1:
            TRACE_POST("GetGi");
            if ( s_HaveID1() ) {
                // id1 treat no-data as no-seq
                BOOST_CHECK(!op->GetGi(id, kThrowNoData));
            }
            else {
                // non-id1 distinguish no-data/no-seq
                BOOST_CHECK_THROW(op->GetGi(id, kThrowNoData), CObjMgrException);
            }
            break;
        case 2:
            TRACE_POST("GetGiBulk");
            if ( s_HaveID1() ) {
                // id1 treat no-data as no-seq
                BOOST_CHECK_THROW(op->GetGis(idvec, kThrowNoSeq), CObjMgrException);
            }
            else {
                // non-id1 distinguish no-data/no-seq
                BOOST_CHECK(!op->GetGis(idvec, kThrowNoSeq)[0]);
            }
            break;
        case 3:
            TRACE_POST("GetGiBulkThrow");
            if ( s_HaveID1() ) {
                // id1 treat no-data as no-seq
                BOOST_CHECK(!op->GetGis(idvec, kThrowNoData)[0]);
            }
            else {
                // non-id1 distinguish no-data/no-seq
                BOOST_CHECK_THROW(op->GetGis(idvec, kThrowNoData), CObjMgrException);
            }
            break;
        }
        if ( op.last ) {
            TRACE_POST("GetAcc");
            BOOST_CHECK(!op->GetAccVer(id));
            if ( s_HaveID2() ) { // doesn't work with pubseqos reader
                TRACE_POST("GetIds");
                BOOST_CHECK(!op->GetIds(id).empty());
                TRACE_POST("GetBioseqHandle");
                BOOST_CHECK(op->GetBioseqHandle(id));
            }
        }
    }
}


NCBITEST_INIT_TREE()
{
}
