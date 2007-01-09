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
 * Authors:  Anatoliy Kuznetsov
 *
 * File Description: Volume merge test
 *
 */

#include <ncbi_pch.hpp>
#include <stdio.h>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbireg.hpp>
#include <algo/volume_merge/volume_merge.hpp>
#include <algo/volume_merge/bv_merger.hpp>
#include <util/bitset/ncbi_bitset.hpp>

#include <map>


USING_NCBI_SCOPE;

/**
    Memory map walker for testing purposes
    @internal
*/
class CMergeMapWalker : public IMergeVolumeWalker
{
public:
    CMergeMapWalker() {}
    virtual ~CMergeMapWalker();
    void AddVector(unsigned key, bm::bvector<>* bv);

    virtual IAsyncInterface* QueryIAsync() { return 0; }
    virtual bool IsEof() const;
    virtual bool IsGood() const { return true; }
    virtual void FetchFirst();
    virtual void Fetch();
    virtual const unsigned char* GetKeyPtr() const;
    virtual Uint4 GetUint4Key() const;
    const unsigned char* GetBufferPtr(size_t* buf_size) const;
    void Close() {}
    void SetRecordMoved() {}
private:
    void x_SerializeCurrent();
private:
    typedef multimap<unsigned, bm::bvector<>* > TMap;
    typedef TMap::const_iterator                TMapIter;

    TMap      m_VolumeMap;
    TMapIter  m_Iter;
    TMapIter  m_IterEnd;

    vector<unsigned char> m_Buffer;
};




CMergeMapWalker::~CMergeMapWalker()
{
    NON_CONST_ITERATE(TMap, it, m_VolumeMap) {
        delete it->second;
    }
}

void CMergeMapWalker::AddVector(unsigned key, bm::bvector<>* bv)
{
    m_VolumeMap.insert( TMap::value_type(key, bv) );
}

bool CMergeMapWalker::IsEof() const
{
    return m_Iter == m_IterEnd;
}

void CMergeMapWalker::FetchFirst()
{
    m_Iter = m_VolumeMap.begin();
    m_IterEnd = m_VolumeMap.end();

    x_SerializeCurrent();
}

void CMergeMapWalker::Fetch()
{
    _ASSERT(m_Iter != m_IterEnd);
    ++m_Iter;
    if (m_Iter != m_IterEnd) {
        x_SerializeCurrent();
    }
}

const unsigned char* CMergeMapWalker::GetKeyPtr() const
{
    return (const unsigned char*)&(m_Iter->first);
}

Uint4 CMergeMapWalker::GetUint4Key() const
{
    return m_Iter->first;
}

const unsigned char* 
CMergeMapWalker::GetBufferPtr(size_t* buf_size) const
{
    *buf_size = m_Buffer.size();
    return &(m_Buffer[0]);
}

void CMergeMapWalker::x_SerializeCurrent()
{
    bm::bvector<>::statistics st1;
    bm::bvector<> &bv = *(m_Iter->second);
    bv.calc_stat(&st1);

    if (st1.max_serialize_mem > m_Buffer.size()) {
        m_Buffer.resize(st1.max_serialize_mem);
    }
    size_t size = bm::serialize(bv, &(m_Buffer)[0]);
    m_Buffer.resize(size);
}

/////////////////////////////////

/**
    Test merge store just prints the results for visual control
    @internal
*/
class CMergePrintStore : public IMergeStore
{
public:
    virtual IAsyncInterface* QueryIAsync() { return 0; }
    virtual bool IsGood() const { return true; }
    virtual void Store(Uint4 blob_id, CMergeVolumes::TRawBuffer* buffer);
    virtual void Close() {}
    virtual CMergeVolumes::TRawBuffer* ReadBlob(Uint4 blob_id) 
    { 
        return 0; // write-only store 
    }
};

void CMergePrintStore::Store(Uint4                      blob_id, 
                             CMergeVolumes::TRawBuffer* buffer)
{
    bm::bvector<> bv;
    bm::deserialize(bv, &((*buffer)[0]));
    m_BufResourcePool->Put(buffer);
    NcbiCout << blob_id << ": ";
    bm::bvector<>::enumerator en(bv.first());
    for (;en.valid(); ++en) {
        NcbiCout << *en << ", ";
    }
    NcbiCout << endl;
}


/////////////////////////////////
// Test application
//

class CTestApplication : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);
};


void CTestApplication::Init(void)
{
    // Set err.-posting and tracing to maximum
    SetDiagTrace(eDT_Enable);
    SetDiagPostFlag(eDPF_All);
    SetDiagPostLevel(eDiag_Info);
}



int CTestApplication::Run(void)
{
    
    //SetupDiag(eDS_ToStdout);

	try {
        CMergeVolumes merger;

        vector<IMergeVolumeWalker*> vol_vector;

        CMergeMapWalker* vol1 = new CMergeMapWalker();
        CMergeMapWalker* vol2 = new CMergeMapWalker();
        CMergeMapWalker* vol3 = new CMergeMapWalker();

        { // vol1 keys: 1, 3, 4, 15
            {
            bm::bvector<>* bv1 = new bm::bvector<>();
            (*bv1)[10] = true; (*bv1)[100] = true; (*bv1)[200] = true;
            vol1->AddVector(1, bv1);
            }
            {
            bm::bvector<>* bv1 = new bm::bvector<>();
            (*bv1)[12] = true; (*bv1)[102] = true; (*bv1)[204] = true;
            vol1->AddVector(3, bv1);
            }
            {
            bm::bvector<>* bv1 = new bm::bvector<>();
            (*bv1)[10] = true; (*bv1)[102] = true; (*bv1)[201] = true;
            vol1->AddVector(4, bv1);
            }
            {
            bm::bvector<>* bv1 = new bm::bvector<>();
            (*bv1)[1] = true; (*bv1)[2] = true; (*bv1)[3] = true;
            vol1->AddVector(15, bv1);
            }
        }
        { // vol2 keys: 2, 3, 4, 4
            {
            bm::bvector<>* bv1 = new bm::bvector<>();
            (*bv1)[10] = true; (*bv1)[100] = true; (*bv1)[300] = true;
            vol2->AddVector(2, bv1);
            }
            {
            bm::bvector<>* bv1 = new bm::bvector<>();
            (*bv1)[12] = true; (*bv1)[102] = true; (*bv1)[202] = true;
            vol2->AddVector(3, bv1);
            }
            {
            bm::bvector<>* bv1 = new bm::bvector<>();
            (*bv1)[10] = true; (*bv1)[102] = true; (*bv1)[301] = true;
            vol2->AddVector(4, bv1);
            }
            {
            bm::bvector<>* bv1 = new bm::bvector<>();
            (*bv1)[15] = true;
            vol2->AddVector(4, bv1);
            }
        }

        { // vol3 keys: 4, 10, 11, 11
            {
            bm::bvector<>* bv1 = new bm::bvector<>();
            (*bv1)[10] = true; (*bv1)[100] = true; (*bv1)[300] = true;
            vol3->AddVector(4, bv1);
            }
            {
            bm::bvector<>* bv1 = new bm::bvector<>();
            (*bv1)[12] = true; (*bv1)[102] = true; (*bv1)[202] = true;
            vol3->AddVector(10, bv1);
            }
            {
            bm::bvector<>* bv1 = new bm::bvector<>();
            (*bv1)[10] = true; (*bv1)[102] = true; (*bv1)[301] = true;
            vol3->AddVector(11, bv1);
            }
            {
            bm::bvector<>* bv1 = new bm::bvector<>();
            (*bv1)[11] = true; (*bv1)[102] = true; (*bv1)[302] = true;
            vol3->AddVector(11, bv1);
            }
        }

        vol_vector.push_back(vol1);
        vol_vector.push_back(vol2);
        vol_vector.push_back(vol3);

        merger.SetMergeAccumulator(new CMergeBitsetBlob<bm::bvector<> >());
        merger.SetMergeStore(new CMergePrintStore());
        merger.SetVolumes(vol_vector);

        merger.Run();

	}
	catch (exception& ex) 
	{
		cout << "Error! " << ex.what() << endl;
	}

    return 0;
}





int main(int argc, char** argv)
{

    CTestApplication theTestApplication;
    return theTestApplication.AppMain(argc, argv, 0 /*envp*/, eDS_ToMemory);

    return 0;
}

/*
 * ===========================================================================
 * $Log$
 * Revision 1.4  2007/01/09 22:12:04  vasilche
 * Use exact pair<> type for WorkShop.
 *
 * Revision 1.3  2006/11/30 11:07:17  kuznets
 * added BLOB read from the merge store (merge-update)
 *
 * Revision 1.2  2006/11/20 16:31:48  kuznets
 * Fixed buffer leak in the demo code
 *
 * Revision 1.1  2006/11/17 07:30:47  kuznets
 * initial revision
 *
 * ===========================================================================
 */
