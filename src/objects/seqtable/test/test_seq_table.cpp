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

#include <util/compress/zlib.hpp>
#include <util/compress/stream.hpp>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;
USING_SCOPE(objects);

/////////////////////////////////////////////////////////////////////////////
//

enum EVerboseLevel {
    eQuiet          = 0,
    eTestName       = 1,
    eTypeName       = 2,
    eDataSize       = 3,
    eVerification   = 4,
    eConversion     = 5,
    eDump           = 9
};

class CTestSeq_table : public CNcbiApplication
{
public:
    virtual void Init(void);
    virtual int  Run(void);

    void TestSparseIndex(void);
    void TestMultiDataBit(void);
    void TestMultiDataBitDelta(void);
    void TestMultiDataInt(void);
    void TestMultiDataIntDelta(void);
    void TestMultiDataIntScaled(void);
    void TestMultiDataReal(void);
    void TestMultiDataRealScaled(void);
    void TestMultiDataString(void);
    void TestMultiDataCommonStrings(void);
    void TestMultiDataBytes(void);
    void TestMultiDataCommonBytes(void);

    template<class C>
    void x_Serialize(string& s, const C& obj) const;
    template<class C>
    void x_Deserialize(C& obj, const string& s) const;

    bool m_UseGZip;
    int m_Verbose;
    int m_Seed;
    mutable CRandom m_Random;
    
    void VerifySetRows(const CSeqTable_sparse_index& index) const;
    void VerifyBool(const CSeqTable_multi_data& index) const;
    void VerifyInt(const CSeqTable_multi_data& index,
                   bool tail_same = false) const;
    void VerifyReal(const CSeqTable_multi_data& index) const;
    
    typedef vector< pair<size_t, size_t> > TSetRowsMap;
    TSetRowsMap m_SetRows;
    typedef vector<int> TInt;
    typedef vector<double> TReal;
    TInt m_Int;
    TReal m_Real;
};



/////////////////////////////////////////////////////////////////////////////
//


void CTestSeq_table::Init()
{
    auto_ptr<CArgDescriptions> d(new CArgDescriptions);

    d->SetUsageContext("test_seq_table",
                       "test various encodings of Seq-table data");

    d->AddDefaultKey("verbose", "Verbose",
                     "verbosity level of progress",
                     CArgDescriptions::eInteger, "0");
    d->AddFlag("gzip", "use gzip after serialization");
    d->AddOptionalKey("seed", "Seed",
                      "use specific random seed", CArgDescriptions::eInteger);
    SetupArgDescriptions(d.release());
}


int CTestSeq_table::Run()
{
    const CArgs& args = GetArgs();
    
    m_Verbose = args["verbose"].AsInteger();
    m_UseGZip = args["gzip"];
    if ( args["seed"] ) {
        m_Seed = args["seed"].AsInteger();
    }
    else {
        m_Random.Randomize();
        m_Seed = m_Random.GetSeed();
    }
    if ( m_Verbose >= eTestName ) {
        NcbiCout << "Using random seed " << m_Seed << NcbiEndl;
    }
    TestSparseIndex();
    TestMultiDataBit();
    TestMultiDataBitDelta();
    TestMultiDataInt();
    TestMultiDataIntDelta();
    /*
    TestMultiDataReal();
    TestMultiDataRealScaled();
    */
    /*
    TestMultiDataString();
    TestMultiDataCommonString();
    TestMultiDataBytes();
    TestMultiDataCommonBytes();
    */
    NcbiCout << "Passed." << NcbiEndl;
    return 0;
}


void CTestSeq_table::VerifySetRows(const CSeqTable_sparse_index& index) const
{
    if ( m_Verbose >= eVerification ) {
        NcbiCout << "Size: " << index.GetSize() << NcbiEndl;
    }
    CSeqTable_sparse_index::const_iterator it = index.begin();
    ITERATE ( TSetRowsMap, it2, m_SetRows ) {
        _TRACE(it2->first<<" -> "<<it2->second);
        if ( !it ) {
            ERR_POST(Fatal<<"No index "<<it2->first);
        }
        if ( it.GetRow() != it2->first ) {
            ERR_POST(Fatal<<"Wrong row "<<it.GetRow()<<" vs "<<it2->first);
        }
        ++it;
    }
    if ( it ) {
        ERR_POST(Fatal<<"Extra row "<<it.GetRow());
    }

    for ( size_t i = 0; i < 10000; ++i ) {
        if ( m_Random.GetRand(0, 1) ) {
            size_t k = m_Random.GetRandIndex(m_SetRows.size());
            size_t row = m_SetRows[k].first;
            _ASSERT(index.HasValueAt(row));
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
                _ASSERT(index.HasValueAt(row));
                size_t value_index = index.GetIndexAt(row);
                _ASSERT(value_index != CSeqTable_sparse_index::kSkipped);
                _ASSERT(value_index == m_SetRows[k].second);
            }
            else {
                _ASSERT(!index.HasValueAt(row));
                size_t value_index = index.GetIndexAt(row);
                _ASSERT(value_index == CSeqTable_sparse_index::kSkipped);
            }
        }
    }
}


void CTestSeq_table::VerifyBool(const CSeqTable_multi_data& data) const
{
    size_t size = m_Int.size();
    if ( m_Verbose >= eVerification ) {
        NcbiCout << "Size: " << size << NcbiEndl;
    }
    bool v;
    for ( size_t row = 0; row < size; ++row ) {
        if ( !data.TryGetBool(row, v) ) {
            ERR_POST(Fatal<<"Cannot get bool at "<<row);
        }
        if ( v != m_Int[row] ) {
            ERR_POST(Fatal<<"Incorrect bool value at "<<row);
        }
    }
    for ( size_t row = size; row < size+32; ++row ) {
        if ( !data.TryGetBool(row, v) ) {
            break;
        }
        if ( v ) {
            ERR_POST(Fatal<<"Incorrect tail bool value at "<<row);
        }
    }
}


void CTestSeq_table::VerifyInt(const CSeqTable_multi_data& data,
                               bool tail_same) const
{
    size_t size = m_Int.size();
    if ( m_Verbose >= eVerification ) {
        NcbiCout << "Size: " << size << NcbiEndl;
    }
    int v;
    for ( size_t row = 0; row < size; ++row ) {
        if ( !data.TryGetInt(row, v) ) {
            ERR_POST(Fatal<<"Cannot get int at "<<row);
        }
        if ( v != m_Int[row] ) {
            ERR_POST(Fatal<<"Incorrect int value at "<<row);
        }
    }
    int tail_v = tail_same? v: 0;
    for ( size_t row = size; row < size+32; ++row ) {
        if ( !data.TryGetInt(row, v) ) {
            break;
        }
        if ( v != tail_v ) {
            ERR_POST(Fatal<<"Incorrect tail int value at "<<row);
        }
    }
}


template<class C>
void CTestSeq_table::x_Serialize(string& s, const C& obj) const
{
    if ( m_Verbose >= eDump ) {
        NcbiCout << MSerial_AsnText << obj;
    }
    CNcbiOstrstream stream;
    if ( m_UseGZip ) {
        CCompressionOStream zip_stream(stream,
                                       new CZipStreamCompressor,
                                       CCompressionOStream::fOwnProcessor);
        zip_stream << MSerial_AsnBinary << obj;
    }
    else {
        stream << MSerial_AsnBinary << obj;
    }
    s = CNcbiOstrstreamToString(stream);
    if ( m_Verbose >= eDataSize ) {
        NcbiCout << "Serialized size: " << s.size() << NcbiEndl;
    }
}


template<class C>
void CTestSeq_table::x_Deserialize(C& obj, const string& s) const
{
    CNcbiIstrstream stream(s.data(), s.size());
    if ( m_UseGZip ) {
        CCompressionIStream zip_stream(stream,
                                       new CZipStreamDecompressor,
                                       CCompressionIStream::fOwnProcessor);
        zip_stream >> MSerial_AsnBinary >> obj;
    }
    else {
        stream >> MSerial_AsnBinary >> obj;
    }
}


const bool test_sparse_indexes_delta = 1;
const bool test_sparse_bit_set = 1;
const bool test_sparse_bit_set_bvector = 1;


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
    if ( m_Verbose >= eTestName ) {
        NcbiCout << "Testing sparse index: "
                 << set_rows.size() << " rows of " << num_rows << " set"
                 << NcbiEndl;
    }
    m_SetRows.clear();
    size_t value_index = 0;
    NON_CONST_ITERATE ( set<size_t>, it, set_rows ) {
        m_SetRows.push_back(make_pair(*it, value_index++));
    }

    CRef<CSeqTable_sparse_index> obj(new CSeqTable_sparse_index);
    string ss[5];

    if ( m_Verbose >= eTypeName ) {
        NcbiCout << "Testing indexes." << NcbiEndl;
    }
    ITERATE ( TSetRowsMap, it, m_SetRows ) {
        obj->SetIndexes().push_back(it->first);
    }

    _ASSERT(obj->IsIndexes());
    x_Serialize(ss[obj->Which()], *obj);
    VerifySetRows(*obj);

    if ( test_sparse_indexes_delta ) {
        if ( m_Verbose >= eTypeName ) {
            NcbiCout << "Testing indexes-delta." << NcbiEndl;
        }
        obj->ChangeTo(CSeqTable_sparse_index::e_Indexes_delta);
        //obj->ForgetOriginalState();
        _ASSERT(obj->IsIndexes_delta());
        x_Serialize(ss[obj->Which()], *obj);
        VerifySetRows(*obj);
    }
    
    if ( test_sparse_bit_set ) {
        if ( m_Verbose >= eTypeName ) {
            NcbiCout << "Testing bit-set." << NcbiEndl;
        }
        obj->ChangeTo(CSeqTable_sparse_index::e_Bit_set);
        //obj->ForgetOriginalState();
        _ASSERT(obj->IsBit_set());
        x_Serialize(ss[obj->Which()], *obj);
        VerifySetRows(*obj);
    }
    
    if ( test_sparse_bit_set_bvector ) {
        if ( m_Verbose >= eTypeName ) {
            NcbiCout << "Testing bit-set-bvector." << NcbiEndl;
        }
        obj->ChangeTo(CSeqTable_sparse_index::e_Bit_set_bvector);
        //obj->ForgetOriginalState();
        _ASSERT(obj->IsBit_set_bvector());
        x_Serialize(ss[obj->Which()], *obj);
        VerifySetRows(*obj);
    }

    for ( size_t i = 0; i < 4; ++i ) {
        CSeqTable_sparse_index::E_Choice src_type =
            CSeqTable_sparse_index::E_Choice(i+1);
        if ( src_type == CSeqTable_sparse_index::e_Indexes_delta &&
             !test_sparse_indexes_delta ) {
            continue;
        }
        if ( src_type == CSeqTable_sparse_index::e_Bit_set &&
             !test_sparse_bit_set ) {
            continue;
        }
        if ( src_type == CSeqTable_sparse_index::e_Bit_set_bvector &&
             !test_sparse_bit_set_bvector ) {
            continue;
        }
        for ( size_t j = 0; j < 4; ++j ) {
            CSeqTable_sparse_index::E_Choice dst_type =
                CSeqTable_sparse_index::E_Choice(j+1);
            if ( dst_type == CSeqTable_sparse_index::e_Indexes_delta &&
                 !test_sparse_indexes_delta ) {
                continue;
            }
            if ( dst_type == CSeqTable_sparse_index::e_Bit_set &&
                 !test_sparse_bit_set ) {
                continue;
            }
            if ( dst_type == CSeqTable_sparse_index::e_Bit_set_bvector &&
                 !test_sparse_bit_set_bvector ) {
                continue;
            }
            if ( m_Verbose >= eConversion ) {
                NcbiCout << "Deserializing variant " << src_type << NcbiEndl;
            }
            x_Deserialize(*obj, ss[src_type]);
            _ASSERT(obj->Which() == src_type);
            VerifySetRows(*obj);

            if ( m_Verbose >= eConversion ) {
                NcbiCout << "Switching to variant " << dst_type << NcbiEndl;
            }
            obj->ChangeTo(dst_type);
            //obj->ForgetOriginalState();
            _ASSERT(obj->Which() == dst_type);
            VerifySetRows(*obj);
        }
    }
}


void CTestSeq_table::TestMultiDataBit(void)
{
    m_Random.SetSeed(m_Seed+1);

    // bit delta
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
    if ( m_Verbose >= eTestName ) {
        NcbiCout << "Testing multi-data bit: "
                 << set_rows.size() << " rows of " << num_rows << " set"
                 << NcbiEndl;
    }
    m_Int.assign(num_rows, 0);
    NON_CONST_ITERATE ( set<size_t>, it, set_rows ) {
        m_Int[*it] = 1;
    }

    CRef<CSeqTable_multi_data> obj(new CSeqTable_multi_data);
    if ( m_Verbose >= eTypeName ) {
        NcbiCout << "Testing int." << NcbiEndl;
    }
    obj->SetInt() = m_Int;

    const size_t N = 3;
    CSeqTable_multi_data::E_Choice tt[N] = {
        CSeqTable_multi_data::e_Int,
        CSeqTable_multi_data::e_Bit,
        CSeqTable_multi_data::e_Bit_bvector
    };
    string ss[N];
    _ASSERT(obj->IsInt());
    _ASSERT(obj->Which() == tt[0]);
    x_Serialize(ss[0], *obj);
    VerifyBool(*obj);
    VerifyInt(*obj);
    
    if ( m_Verbose >= eTypeName ) {
        NcbiCout << "Testing bit." << NcbiEndl;
    }
    obj->ChangeTo(CSeqTable_multi_data::e_Bit);
    //obj->ForgetOriginalState();
    _ASSERT(obj->IsBit());
    _ASSERT(obj->Which() == tt[1]);
    x_Serialize(ss[1], *obj);
    VerifyBool(*obj);
    VerifyInt(*obj);
    
    if ( m_Verbose >= eTypeName ) {
        NcbiCout << "Testing bit-bvector." << NcbiEndl;
    }
    obj->ChangeTo(CSeqTable_multi_data::e_Bit_bvector);
    //obj->ForgetOriginalState();
    _ASSERT(obj->IsBit_bvector());
    x_Serialize(ss[2], *obj);
    VerifyBool(*obj);
    VerifyInt(*obj);
    
    for ( size_t i = 0; i < N; ++i ) {
        CSeqTable_multi_data::E_Choice src_type = tt[i];
        for ( size_t j = 0; j < N; ++j ) {
            CSeqTable_multi_data::E_Choice dst_type = tt[j];
            if ( m_Verbose >= eConversion ) {
                NcbiCout << "Deserializing variant " << src_type << NcbiEndl;
            }
            x_Deserialize(*obj, ss[i]);
            _ASSERT(obj->Which() == src_type);
            VerifyBool(*obj);
            VerifyInt(*obj);
            
            if ( m_Verbose >= eConversion ) {
                NcbiCout << "Switching to variant " << dst_type << NcbiEndl;
            }
            obj->ChangeTo(dst_type);
            //obj->ForgetOriginalState();
            _ASSERT(obj->Which() == dst_type);
            VerifyBool(*obj);
            VerifyInt(*obj);
        }
    }
}


void CTestSeq_table::TestMultiDataBitDelta(void)
{
    m_Random.SetSeed(m_Seed+2);

    // bit delta
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
    if ( m_Verbose >= eTestName ) {
        NcbiCout << "Testing multi-data bit delta: "
                 << set_rows.size() << " rows of " << num_rows << " set"
                 << NcbiEndl;
    }
    m_Int.clear();
    m_Int.reserve(num_rows);
    int v = 0;
    NON_CONST_ITERATE ( set<size_t>, it, set_rows ) {
        size_t row = *it;
        m_Int.resize(row, v);
        ++v;
        m_Int.push_back(v);
    }
    m_Int.resize(num_rows, v);

    CRef<CSeqTable_multi_data> obj(new CSeqTable_multi_data);
    if ( m_Verbose >= eTypeName ) {
        NcbiCout << "Testing int." << NcbiEndl;
    }
    obj->SetInt() = m_Int;

    const size_t N = 4;
    CSeqTable_multi_data::E_Choice tt1[N] = {
        CSeqTable_multi_data::e_Int,
        CSeqTable_multi_data::e_Int_delta,
        CSeqTable_multi_data::e_Int_delta,
        CSeqTable_multi_data::e_Int_delta
    };
    CSeqTable_multi_data::E_Choice tt2[N] = {
        CSeqTable_multi_data::e_not_set,
        CSeqTable_multi_data::e_Int,
        CSeqTable_multi_data::e_Bit,
        CSeqTable_multi_data::e_Bit_bvector
    };
    string ss[N];
    _ASSERT(obj->IsInt());
    _ASSERT(obj->Which() == tt1[0]);
    if ( obj->IsInt_delta() ) _ASSERT(obj->GetInt_delta().Which() == tt2[0]);
    x_Serialize(ss[0], *obj);
    VerifyInt(*obj, true);

    if ( m_Verbose >= eTypeName ) {
        NcbiCout << "Testing int delta." << NcbiEndl;
    }
    obj->ChangeTo(tt1[1]);
    //obj->ForgetOriginalState();
    _ASSERT(obj->Which() == tt1[1]);
    if ( obj->IsInt_delta() ) {
        obj->SetInt_delta().ChangeTo(tt2[1]);
        //obj->ForgetOriginalState();
        _ASSERT(obj->GetInt_delta().Which() == tt2[1]);
    }
    x_Serialize(ss[1], *obj);
    VerifyInt(*obj, true);
    
    if ( m_Verbose >= eTypeName ) {
        NcbiCout << "Testing int delta bit." << NcbiEndl;
    }
    obj->ChangeTo(tt1[2]);
    //obj->ForgetOriginalState();
    _ASSERT(obj->Which() == tt1[2]);
    if ( obj->IsInt_delta() ) {
        obj->SetInt_delta().ChangeTo(tt2[2]);
        //obj->ForgetOriginalState();
        _ASSERT(obj->GetInt_delta().Which() == tt2[2]);
    }
    x_Serialize(ss[2], *obj);
    VerifyInt(*obj, true);
    
    if ( m_Verbose >= eTypeName ) {
        NcbiCout << "Testing int delta bit-bvector." << NcbiEndl;
    }
    obj->ChangeTo(tt1[3]);
    //obj->ForgetOriginalState();
    _ASSERT(obj->Which() == tt1[3]);
    if ( obj->IsInt_delta() ) {
        obj->SetInt_delta().ChangeTo(tt2[3]);
        //obj->ForgetOriginalState();
        _ASSERT(obj->GetInt_delta().Which() == tt2[3]);
    }
    x_Serialize(ss[3], *obj);
    VerifyInt(*obj, true);
    
    for ( size_t i = 0; i < N; ++i ) {
        CSeqTable_multi_data::E_Choice src_type1 = tt1[i];
        CSeqTable_multi_data::E_Choice src_type2 = tt2[i];
        for ( size_t j = 0; j < N; ++j ) {
            CSeqTable_multi_data::E_Choice dst_type1 = tt1[j];
            CSeqTable_multi_data::E_Choice dst_type2 = tt2[j];
            if ( m_Verbose >= eConversion ) {
                NcbiCout << "Deserializing variant "
                         << src_type1 << "/" << src_type2 << NcbiEndl;
            }
            x_Deserialize(*obj, ss[i]);
            _ASSERT(obj->Which() == src_type1);
            if ( obj->IsInt_delta() ) {
                _ASSERT(obj->GetInt_delta().Which() == src_type2);
            }
            VerifyInt(*obj, true);
            
            if ( m_Verbose >= eConversion ) {
                NcbiCout << "Switching to variant "
                         << dst_type1 << "/" << dst_type2 << NcbiEndl;
            }
            obj->ChangeTo(dst_type1);
            //obj->ForgetOriginalState();
            _ASSERT(obj->Which() == dst_type1);
            if ( obj->IsInt_delta() ) {
                obj->SetInt_delta().ChangeTo(dst_type2);
                //obj->ForgetOriginalState();
                _ASSERT(obj->GetInt_delta().Which() == dst_type2);
            }
            VerifyInt(*obj, true);
        }
    }
}


void CTestSeq_table::TestMultiDataInt(void)
{
    m_Random.SetSeed(m_Seed+3);

    // bit delta
    size_t num_rows = m_Random.GetRand(10000, 100000);
    int min_value = m_Random.GetRand(-100000, 100000);
    int max_value = m_Random.GetRand(min_value, 100000);
    m_Int.resize(num_rows);
    for ( size_t i = 0; i < num_rows; ++i ) {
        m_Int[i] = m_Random.GetRand(min_value, max_value);
    }
    if ( m_Verbose >= eTestName ) {
        NcbiCout << "Testing multi-data int: "
                 << num_rows << " rows"
                 << NcbiEndl;
    }

    CRef<CSeqTable_multi_data> obj(new CSeqTable_multi_data);
    if ( m_Verbose >= eTypeName ) {
        NcbiCout << "Testing int." << NcbiEndl;
    }
    obj->SetInt() = m_Int;

    const size_t N = 2;
    CSeqTable_multi_data::E_Choice tt1[N] = {
        CSeqTable_multi_data::e_Int,
        CSeqTable_multi_data::e_Int_delta
    };
    string ss[N];
    _ASSERT(obj->IsInt());
    _ASSERT(obj->Which() == tt1[0]);
    x_Serialize(ss[0], *obj);
    VerifyInt(*obj);

    if ( m_Verbose >= eTypeName ) {
        NcbiCout << "Testing int delta." << NcbiEndl;
    }
    obj->ChangeTo(tt1[1]);
    //obj->ForgetOriginalState();
    _ASSERT(obj->Which() == tt1[1]);
    x_Serialize(ss[1], *obj);
    VerifyInt(*obj);
    
    for ( size_t i = 0; i < N; ++i ) {
        CSeqTable_multi_data::E_Choice src_type1 = tt1[i];
        for ( size_t j = 0; j < N; ++j ) {
            CSeqTable_multi_data::E_Choice dst_type1 = tt1[j];
            if ( m_Verbose >= eConversion ) {
                NcbiCout << "Deserializing variant "
                         << src_type1 << NcbiEndl;
            }
            x_Deserialize(*obj, ss[i]);
            _ASSERT(obj->Which() == src_type1);
            VerifyInt(*obj);
            
            if ( m_Verbose >= eConversion ) {
                NcbiCout << "Switching to variant "
                         << dst_type1 << NcbiEndl;
            }
            obj->ChangeTo(dst_type1);
            //obj->ForgetOriginalState();
            _ASSERT(obj->Which() == dst_type1);
            VerifyInt(*obj);
        }
    }
}


void CTestSeq_table::TestMultiDataIntDelta(void)
{
    m_Random.SetSeed(m_Seed+3);

    // bit delta
    size_t num_rows = m_Random.GetRand(10000, 100000);
    int max_step = m_Random.GetRand(2, 100);
    m_Int.resize(num_rows);
    int v = 0;
    for ( size_t i = 0; i < num_rows; ++i ) {
        v += m_Random.GetRand(0, max_step >> m_Random.GetRandIndex(10));
        m_Int[i] = v;
    }
    if ( m_Verbose >= eTestName ) {
        NcbiCout << "Testing multi-data int delta: "
                 << num_rows << " rows"
                 << NcbiEndl;
    }

    CRef<CSeqTable_multi_data> obj(new CSeqTable_multi_data);
    if ( m_Verbose >= eTypeName ) {
        NcbiCout << "Testing int." << NcbiEndl;
    }
    obj->SetInt() = m_Int;

    const size_t N = 2;
    CSeqTable_multi_data::E_Choice tt1[N] = {
        CSeqTable_multi_data::e_Int,
        CSeqTable_multi_data::e_Int_delta
    };
    string ss[N];
    _ASSERT(obj->IsInt());
    _ASSERT(obj->Which() == tt1[0]);
    x_Serialize(ss[0], *obj);
    VerifyInt(*obj);

    if ( m_Verbose >= eTypeName ) {
        NcbiCout << "Testing int delta." << NcbiEndl;
    }
    obj->ChangeTo(tt1[1]);
    //obj->ForgetOriginalState();
    _ASSERT(obj->Which() == tt1[1]);
    x_Serialize(ss[1], *obj);
    VerifyInt(*obj);
    
    for ( size_t i = 0; i < N; ++i ) {
        CSeqTable_multi_data::E_Choice src_type1 = tt1[i];
        for ( size_t j = 0; j < N; ++j ) {
            CSeqTable_multi_data::E_Choice dst_type1 = tt1[j];
            if ( m_Verbose >= eConversion ) {
                NcbiCout << "Deserializing variant "
                         << src_type1 << NcbiEndl;
            }
            x_Deserialize(*obj, ss[i]);
            _ASSERT(obj->Which() == src_type1);
            VerifyInt(*obj);
            
            if ( m_Verbose >= eConversion ) {
                NcbiCout << "Switching to variant "
                         << dst_type1 << NcbiEndl;
            }
            obj->ChangeTo(dst_type1);
            //obj->ForgetOriginalState();
            _ASSERT(obj->Which() == dst_type1);
            VerifyInt(*obj);
        }
    }
}


int main(int argc, const char* argv[])
{
    return CTestSeq_table().AppMain(argc, argv);
}
