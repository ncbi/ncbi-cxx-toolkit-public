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
    for ( size_t s = distance(iter1, iter2); s > 1; --s, ++iter1 ) {
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


string s_AsString(const vector<CSeq_id_Handle>& ids)
{
    CNcbiOstrstream out;
    out << '{';
    ITERATE ( vector<CSeq_id_Handle>, it, ids ) {
        out << ' ' <<  *it;
    }
    out << " }";
    return CNcbiOstrstreamToString(out);
}


bool s_HaveID2(void)
{
    const char* env = getenv("GENBANK_LOADER_METHOD_BASE");
    if ( !env ) {
        env = getenv("GENBANK_LOADER_METHOD");
    }
    if ( !env ) {
        // assume default ID2
        TRACE_POST("Assuming ID2, env=null");
        return true;
    }
    if ( NStr::EndsWith(env, "id1", NStr::eNocase) ||
         NStr::EndsWith(env, "pubseqos", NStr::eNocase) ) {
        // non-ID2 based readers
        TRACE_POST("No ID2, env=\""<<env<<"\"");
        return false;
    }
    TRACE_POST("ID2, env=\""<<env<<"\"");
    return true;
}


bool s_HaveID1(void)
{
    const char* env = getenv("GENBANK_LOADER_METHOD_BASE");
    if ( !env ) {
        env = getenv("GENBANK_LOADER_METHOD");
    }
    if ( !env ) {
        // assume default ID2
        TRACE_POST("Assuming no ID1, env=null");
        return false;
    }
    if ( NStr::EndsWith(env, "id1", NStr::eNocase) ) {
        TRACE_POST("ID1, env=\""<<env<<"\"");
        return true;
    }
    TRACE_POST("No ID1, env=\""<<env<<"\"");
    return false;
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

struct scope_operation : pair<int, CScope*>
{
    scope_operation(int op, const CRef<CScope>& scope)
        : pair(op, scope.GetNCPointerOrNull())
        {
        }

    int op() const { return first; }
    CScope* operator->() const { return second; }
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
                return value_type(ops[i], scope);
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
                return value_type(ops[i], scope);
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
    if ( !s_HaveID2() ) {
        return;
    }
    CSeq_id_Handle id = CSeq_id_Handle::GetGiHandle(GI_CONST(1));
    vector<CSeq_id_Handle> idvec(1, id);
    LOG_POST("CheckNoSeqGi: "<<id);
    for ( auto op : all_orders<5>() ) {
        switch ( op.op() ) {
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
        case 4:
            TRACE_POST("GetIds");
            BOOST_CHECK(op->GetIds(id).empty());
            break;
        }
    }
}


BOOST_AUTO_TEST_CASE(CheckNoSeqAcc)
{
    CSeq_id_Handle id = CSeq_id_Handle::GetGiHandle(GI_CONST(1));
    vector<CSeq_id_Handle> idvec(1, id);
    LOG_POST("CheckNoSeqAcc: "<<id);
    for ( auto op : all_orders<5>() ) {
        switch ( op.op() ) {
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
        case 4:
            TRACE_POST("GetIds");
            BOOST_CHECK(op->GetIds(id).empty());
            break;
        }
    }
}


BOOST_AUTO_TEST_CASE(CheckNoSeqAll)
{
    if ( !s_HaveID2() ) {
        return;
    }
    CSeq_id_Handle id = CSeq_id_Handle::GetGiHandle(GI_CONST(1));
    vector<CSeq_id_Handle> idvec(1, id);
    LOG_POST("CheckNoSeq: "<<id);
    for ( auto op : random_orders<8>(100) ) {
        switch ( op.op() ) {
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
    CSeq_id_Handle id = CSeq_id_Handle::GetGiHandle(GI_CONST(156205));
    vector<CSeq_id_Handle> idvec(1, id);
    LOG_POST("CheckNoAcc: "<<id);
    for ( auto op : all_orders<5>() ) {
        switch ( op.op() ) {
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
        case 4:
            TRACE_POST("GetGi");
            BOOST_CHECK(op->GetGi(id, kThrowNoSeq));
            break;
        case 5:
            TRACE_POST("GetIds");
            BOOST_CHECK(!op->GetIds(id).empty());
            break;
        case 6:
            TRACE_POST("GetBioseqHandle");
            BOOST_CHECK(op->GetBioseqHandle(id));
            break;
        }
    }
}


BOOST_AUTO_TEST_CASE(CheckNoGi)
{
    if ( !s_HaveID2() ) {
        return;
    }
    CSeq_id_Handle id = CSeq_id_Handle::GetHandle("gnl|Annot:SNP|568802115");
    //CSeq_id_Handle id = CSeq_id_Handle::GetHandle("gnl|SRA|SRR000010.1.2");
    vector<CSeq_id_Handle> idvec(1, id);
    LOG_POST("CheckNoGi: "<<id);
    for ( auto op : all_orders<5>() ) {
        switch ( op.op() ) {
        case 0:
            TRACE_POST("GetGi");
            BOOST_CHECK(!op->GetGi(id, kThrowNoSeq));
            break;
        case 1:
            TRACE_POST("GetGi");
            BOOST_CHECK_THROW(op->GetGi(id, kThrowNoData), CObjMgrException);
            break;
        case 2:
            TRACE_POST("GetGiBulk");
            BOOST_CHECK(!op->GetGis(idvec, kThrowNoSeq)[0]);
            break;
        case 3:
            TRACE_POST("GetGiBulkThrow");
            BOOST_CHECK_THROW(op->GetGis(idvec, kThrowNoData), CObjMgrException);
            break;
        case 4:
            TRACE_POST("GetAcc");
            BOOST_CHECK(!op->GetAccVer(id));
            break;
        case 5:
            TRACE_POST("GetIds");
            BOOST_CHECK(!op->GetIds(id).empty());
            break;
        case 6:
            TRACE_POST("GetBioseqHandle");
            BOOST_CHECK(op->GetBioseqHandle(id));
            break;
        }
    }
}


NCBITEST_INIT_TREE()
{
}
