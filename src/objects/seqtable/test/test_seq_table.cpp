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
 * Author:  Eugene Vasilchenk
 *
 * File Description:
 *   Make Seq-table from repeat masker features
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbireg.hpp>

#include <serial/serial.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>

#include <objects/general/general__.hpp>
#include <objects/seqloc/seqloc__.hpp>
#include <objects/seqfeat/seqfeat__.hpp>
#include <objects/seqtable/seqtable__.hpp>
#include <objects/seqtable/seq_table_exception.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/seq/Annot_id.hpp>

#include <util/random_gen.hpp>
#include <util/bitset/ncbi_bitset.hpp>


#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;
USING_SCOPE(objects);

/////////////////////////////////////////////////////////////////////////////
//

class CTestSeq_table : public CNcbiApplication
{
public:
    virtual void Init(void);
    virtual int  Run(void);

    void TestSparseIndex(void);

    bool m_Verbose;
    int m_Seed;
    CRandom m_Random;
    
    void VerifySetRows(const CSeqTable_sparse_index& index);
    
    typedef vector< pair<size_t, size_t> > TSetRowsMap;
    TSetRowsMap m_SetRows;
};



/////////////////////////////////////////////////////////////////////////////
//


void CTestSeq_table::Init()
{
    auto_ptr<CArgDescriptions> d(new CArgDescriptions);

    d->SetUsageContext("test_seq_table",
                       "test various encodings of Seq-table data");

    d->AddFlag("verbose", "display progress information");
    d->AddOptionalKey("seed", "Seed",
                      "use specific random seed", CArgDescriptions::eInteger);
    SetupArgDescriptions(d.release());
}


int CTestSeq_table::Run()
{
    const CArgs& args = GetArgs();
    m_Verbose = args["verbose"];
    if ( args["seed"] ) {
        m_Seed = args["seed"].AsInteger();
    }
    else {
        m_Random.Randomize();
        m_Seed = m_Random.GetSeed();
    }
    if ( m_Verbose ) {
        NcbiCout << "Using random seed " << m_Seed << NcbiEndl;
    }
    TestSparseIndex();
    NcbiCout << "Passed." << NcbiEndl;
    return 0;
}


void CTestSeq_table::VerifySetRows(const CSeqTable_sparse_index& index)
{
    CSeqTable_sparse_index::const_iterator it = index.begin();
    ITERATE ( TSetRowsMap, it2, m_SetRows ) {
        _TRACE(it2->first<<" -> "<<it2->second);
        if ( !it ) {
            ERR_POST(Fatal<<"No index "<<it2->first);
        }
        if ( it.GetIndex() != it2->first ) {
            ERR_POST(Fatal<<"Wrong index "<<it.GetIndex()<<" vs "<<it2->first);
        }
        ++it;
    }
    if ( it ) {
        ERR_POST(Fatal<<"Extra index "<<it.GetIndex());
    }

    for ( size_t i = 0; i < 10000; ++i ) {
        if ( m_Random.GetRand(0, 1) ) {
            size_t k = m_Random.GetRandIndex(m_SetRows.size());
            size_t row = m_SetRows[k].first;
            _ASSERT(index.IsSelectedAt(row));
            size_t value_index = index.GetIndexAt(row);
            _ASSERT(value_index != CSeqTable_sparse_index::kSkipped);
            _ASSERT(value_index == m_SetRows[k].second);
        }
        else {
            size_t row = m_Random.GetRandIndex(m_SetRows.back().first*1.01);
            size_t k =
                lower_bound(m_SetRows.begin(), m_SetRows.end(),
                            make_pair(row, size_t(0))) - m_SetRows.begin();
            if ( k < m_SetRows.size() && m_SetRows[k].first == row ) {
                _ASSERT(index.IsSelectedAt(row));
                size_t value_index = index.GetIndexAt(row);
                _ASSERT(value_index != CSeqTable_sparse_index::kSkipped);
                _ASSERT(value_index == m_SetRows[k].second);
            }
            else {
                _ASSERT(!index.IsSelectedAt(row));
                size_t value_index = index.GetIndexAt(row);
                _ASSERT(value_index == CSeqTable_sparse_index::kSkipped);
            }
        }
    }
}


template<class C>
string Serialize(const C& obj)
{
    CNcbiOstrstream s;
    s << MSerial_AsnBinary << obj;
    return CNcbiOstrstreamToString(s);
}


template<class C>
void Deserialize(C& obj, const string& str)
{
    CNcbiIstrstream s(str.data(), str.size());
    s >> MSerial_AsnBinary >> obj;
}


void CTestSeq_table::TestSparseIndex(void)
{
    m_Random.SetSeed(m_Seed);
    size_t num_rows = m_Random.GetRand(10000, 100000);
    size_t set_count = m_Random.GetRand(10, num_rows>>m_Random.GetRand(0, 9));
    set<size_t> set_rows;
    for ( size_t i = 0; i < set_count; ) {
        size_t start_row = m_Random.GetRandIndex(num_rows);
        size_t row_count = m_Random.GetRand(1, 1<<m_Random.GetRand(0, 10));
        row_count = min(row_count, num_rows-start_row);
        
        for ( size_t j = 0; j < row_count; ++j ) {
            set_rows.insert(start_row+j);
        }
        i += row_count;
    }
    if ( m_Verbose ) {
        NcbiCout << "Testing sparse index: "
                 << set_rows.size() << " rows of " << num_rows << " set"
                 << NcbiEndl;
    }
    size_t value_index = 0;
    NON_CONST_ITERATE ( set<size_t>, it, set_rows ) {
        m_SetRows.push_back(make_pair(*it, value_index++));
    }

    CRef<CSeqTable_sparse_index> obj(new CSeqTable_sparse_index);
    if ( m_Verbose ) {
        NcbiCout << "Testing indexes." << NcbiEndl;
    }
    ITERATE ( TSetRowsMap, it, m_SetRows ) {
        obj->SetIndexes().push_back(it->first);
    }

    string ss[5];
    _ASSERT(obj->IsIndexes());
    ss[obj->Which()] = Serialize(*obj);
    if ( m_Verbose ) {
        NcbiCout << "Serialized size: " << ss[obj->Which()].size() << NcbiEndl;
    }
    VerifySetRows(*obj);

    if ( m_Verbose ) {
        NcbiCout << "Testing indexes-delta." << NcbiEndl;
    }
    obj->ChangeTo(CSeqTable_sparse_index::e_Indexes_delta);
    _ASSERT(obj->IsIndexes_delta());
    ss[obj->Which()] = Serialize(*obj);
    if ( m_Verbose ) {
        NcbiCout << "Serialized size: " << ss[obj->Which()].size() << NcbiEndl;
    }
    VerifySetRows(*obj);
    
    if ( m_Verbose ) {
        NcbiCout << "Testing bit-set." << NcbiEndl;
    }
    obj->ChangeTo(CSeqTable_sparse_index::e_Bit_set);
    _ASSERT(obj->IsBit_set());
    ss[obj->Which()] = Serialize(*obj);
    if ( m_Verbose ) {
        NcbiCout << "Serialized size: " << ss[obj->Which()].size() << NcbiEndl;
    }
    VerifySetRows(*obj);
    
    if ( m_Verbose ) {
        NcbiCout << "Testing bit-set-bvector." << NcbiEndl;
    }
    obj->ChangeTo(CSeqTable_sparse_index::e_Bit_set_bvector);
    _ASSERT(obj->IsBit_set_bvector());
    ss[obj->Which()] = Serialize(*obj);
    if ( m_Verbose ) {
        NcbiCout << "Serialized size: " << ss[obj->Which()].size() << NcbiEndl;
    }
    VerifySetRows(*obj);

    for ( size_t i = 0; i < 4; ++i ) {
        CSeqTable_sparse_index::E_Choice src_type =
            CSeqTable_sparse_index::E_Choice(i+1);
        for ( size_t j = 0; j < 4; ++j ) {
            CSeqTable_sparse_index::E_Choice dst_type =
                CSeqTable_sparse_index::E_Choice(j+1);
            if ( m_Verbose ) {
                NcbiCout << "Deserializing variant " << src_type << NcbiEndl;
            }
            Deserialize(*obj, ss[src_type]);
            _ASSERT(obj->Which() == src_type);
            VerifySetRows(*obj);

            if ( m_Verbose ) {
                NcbiCout << "Switching to variant " << dst_type << NcbiEndl;
            }
            obj->ChangeTo(dst_type);
            _ASSERT(obj->Which() == dst_type);
            VerifySetRows(*obj);
        }
    }
}


int main(int argc, const char* argv[])
{
    return CTestSeq_table().AppMain(argc, argv);
}
