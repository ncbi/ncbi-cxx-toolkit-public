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
 * Author: Anatoliy Kuznetsov
 *
 * File Description: Test application BDB volume merge
 *
 */


#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbitime.hpp>
#include <stdio.h>

#include <db/bdb/bdb_expt.hpp>
#include <db/bdb/bdb_types.hpp>
#include <db/bdb/bdb_file.hpp>
#include <db/bdb/bdb_env.hpp>
#include <db/bdb/bdb_cursor.hpp>
#include <db/bdb/bdb_merge.hpp>
#include <db/bdb/bdb_bv_store.hpp>
#include <db/bdb/bdb_split_blob.hpp>

#include <util/bitset/ncbi_bitset.hpp>
#include <algo/volume_merge/bv_merger.hpp>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;




////////////////////////////////
// Test functions, classes, etc.
//




////////////////////////////////
// Test application
//

/// @internal
class CBDB_MergeTest : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);
};


void CBDB_MergeTest::Init(void)
{
    SetDiagTrace(eDT_Enable);

    SetDiagPostLevel(eDiag_Warning);
    SetDiagPostFlag(eDPF_File);
    SetDiagPostFlag(eDPF_Line);
    SetDiagPostFlag(eDPF_Trace);


    auto_ptr<CArgDescriptions> d(new CArgDescriptions);
    d->SetUsageContext("test_bdb_vmerge",
                       "test BDB vmerge");
    SetupArgDescriptions(d.release());
}


int CBDB_MergeTest::Run(void)
{
    cout << "Run BDB merge test" << endl << endl;

    CMergeVolumes merger;


    // Init. src. bv stores

    typedef SBDB_BvStore_Id<bm::bvector<> > TBvStore;

    SBDB_BvStore_Id<bm::bvector<> > vol1;
    vol1.Open("bv_vol1.db", CBDB_RawFile::eCreate);

    SBDB_BvStore_Id<bm::bvector<> > vol2;
    vol2.Open("bv_vol2.db", CBDB_RawFile::eCreate);

    SBDB_BvStore_Id<bm::bvector<> > vol3;
    vol3.Open("bv_vol3.db", CBDB_RawFile::eCreate);

    // Load BV store data
    { // vol1 keys: 1, 3, 4, 15
        {
        bm::bvector<>* bv1 = new bm::bvector<>();
        (*bv1)[10] = true; (*bv1)[100] = true; (*bv1)[200] = true;
        vol1.id = 1;
        vol1.WriteVector(*bv1, CBDB_BvStore<bm::bvector<> >::eNoCompact);
        delete bv1;
        }
        {
        bm::bvector<>* bv1 = new bm::bvector<>();
        (*bv1)[12] = true; (*bv1)[102] = true; (*bv1)[204] = true;
        vol1.id = 3;
        vol1.WriteVector(*bv1, CBDB_BvStore<bm::bvector<> >::eNoCompact);
        delete bv1;
        }
        {
        bm::bvector<>* bv1 = new bm::bvector<>();
        (*bv1)[10] = true; (*bv1)[102] = true; (*bv1)[201] = true;
        vol1.id = 4;
        vol1.WriteVector(*bv1, CBDB_BvStore<bm::bvector<> >::eNoCompact);
        delete bv1;
        }
        {
        bm::bvector<>* bv1 = new bm::bvector<>();
        (*bv1)[1] = true; (*bv1)[2] = true; (*bv1)[3] = true;
        vol1.id = 15;
        vol1.WriteVector(*bv1, CBDB_BvStore<bm::bvector<> >::eNoCompact);
        delete bv1;
        }
    }

    { // vol2 keys: 2, 3, 4, 5
        {
        bm::bvector<>* bv1 = new bm::bvector<>();
        (*bv1)[10] = true; (*bv1)[100] = true; (*bv1)[300] = true;
        vol2.id = 2;
        vol2.WriteVector(*bv1, CBDB_BvStore<bm::bvector<> >::eNoCompact);
        delete bv1;
        }
        {
        bm::bvector<>* bv1 = new bm::bvector<>();
        (*bv1)[12] = true; (*bv1)[102] = true; (*bv1)[202] = true;
        vol2.id = 3;
        vol2.WriteVector(*bv1, CBDB_BvStore<bm::bvector<> >::eNoCompact);
        delete bv1;
        }
        {
        bm::bvector<>* bv1 = new bm::bvector<>();
        (*bv1)[10] = true; (*bv1)[102] = true; (*bv1)[301] = true;
        vol2.id = 4;
        vol2.WriteVector(*bv1, CBDB_BvStore<bm::bvector<> >::eNoCompact);
        delete bv1;
        }
        {
        bm::bvector<>* bv1 = new bm::bvector<>();
        (*bv1)[15] = true;
        vol2.id = 5;
        vol2.WriteVector(*bv1, CBDB_BvStore<bm::bvector<> >::eNoCompact);
        delete bv1;
        }
    }

    { // vol3 keys: 4, 10, 11
        {
        bm::bvector<>* bv1 = new bm::bvector<>();
        (*bv1)[10] = true; (*bv1)[100] = true; (*bv1)[300] = true;
        vol3.id = 4;
        vol3.WriteVector(*bv1, CBDB_BvStore<bm::bvector<> >::eNoCompact);
        delete bv1;
        }
        {
        bm::bvector<>* bv1 = new bm::bvector<>();
        (*bv1)[12] = true; (*bv1)[102] = true; (*bv1)[202] = true;
        vol3.id = 10;
        vol3.WriteVector(*bv1, CBDB_BvStore<bm::bvector<> >::eNoCompact);
        delete bv1;
        }
        {
        bm::bvector<>* bv1 = new bm::bvector<>();
        (*bv1)[10] = true; (*bv1)[102] = true; (*bv1)[301] = true;
        vol3.id = 11;
        vol3.WriteVector(*bv1, CBDB_BvStore<bm::bvector<> >::eNoCompact);
        delete bv1;
        }
    }

    // Create storage
    typedef CBDB_BlobSplitStore<bm::bvector<> > TBlobSplit;
    TBlobSplit split_db(new CBDB_BlobDeMux());
    split_db.Open("vol_store", CBDB_RawFile::eCreate);
    CBDB_MergeStoreAsync<TBlobSplit> mstore(&split_db, eNoOwnership);

    vector<IMergeVolumeWalker*> vol_vector;
    vol_vector.push_back(new CBDB_MergeBlobWalkerAsync<TBvStore>(&vol1, eNoOwnership));
    vol_vector.push_back(new CBDB_MergeBlobWalkerAsync<TBvStore>(&vol2, eNoOwnership));
    vol_vector.push_back(new CBDB_MergeBlobWalkerAsync<TBvStore>(&vol3, eNoOwnership));

    merger.SetMergeAccumulator(new CMergeBitsetBlob<bm::bvector<> >());
    merger.SetMergeStore(&mstore, eNoOwnership);
    merger.SetVolumes(vol_vector, eNoOwnership);


    merger.Run();

    return 0;
}


///////////////////////////////////
// APPLICATION OBJECT  and  MAIN
//

int main(int argc, const char* argv[])
{
    // Execute main application function
    return CBDB_MergeTest().AppMain(argc, argv);
}
