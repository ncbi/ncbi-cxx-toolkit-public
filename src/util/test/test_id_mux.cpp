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
 * Author: Vladimir Ivanov
 *
 * File Description:
 *   Test program for id multiplexer
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <util/id_mux.hpp>
#include <util/bitset/ncbi_bitset.hpp>
#include <stdio.h>

#include <test/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;

typedef CIdDeMux<bm::bvector<> > TIdDeMux;

/// @internal
class CTestObjDeMux : public IObjDeMux<int>
{
public:
    typedef IObjDeMux<int> TParent;
public:
    CTestObjDeMux(){}
    void GetCoordinates(const int& /*obj*/, unsigned* coord)
    {
        coord[0] = 1; coord[1] = 2;
    }
};


/// @internal
class CTestIdMux : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);
};


void CTestIdMux::Init(void)
{
    SetDiagPostLevel(eDiag_Warning);

    auto_ptr<CArgDescriptions> d(new CArgDescriptions);

    d->SetUsageContext("test_id_mux",
                       "test id mux-demux");
    SetupArgDescriptions(d.release());
}


/// <pre>
/// Example:
///              small(0)    medium(1)     large(2)
///          +-------------+-------------+-------------+ 
/// vol= 0   |  1,3,5      |  8          | 11, 9       |
/// vol= 1   |  6,10       |  15,16      | 2, 4, 17    |
///          +-------------+-------------+-------------+ 
/// </pre>
int CTestIdMux::Run(void)
{
    {{
    CTestObjDeMux obj_demux;
    unsigned coord[2]; 
    obj_demux.GetCoordinates(0, coord);
    assert(coord[0] == 1 && coord[1] == 2);
    }}

    TIdDeMux demux(2);

    demux.InitDim(0, 2);
    demux.InitDim(1, 3);

    TIdDeMux::TDimVector& dv0 = demux.PutDimVector(0);
    TIdDeMux::TDimVector& dv1 = demux.PutDimVector(1);

    {{
    TIdDeMux::TDimensionalPoint pt(2);
    pt[0] = 1; pt[1] = 2;
    demux.SetCoordinates(17, pt);
    }}

    dv0[0]->set(1);
    dv0[0]->set(3);
    dv0[0]->set(5);
    dv0[0]->set(8);
    dv0[0]->set(11);
    dv0[0]->set(9);

    dv0[1]->set(6);
    dv0[1]->set(10);
    dv0[1]->set(15);
    dv0[1]->set(16);
    dv0[1]->set(2);
    dv0[1]->set(4);

    dv1[0]->set(1);
    dv1[0]->set(3);
    dv1[0]->set(5);
    dv1[0]->set(6);
    dv1[0]->set(10);

    dv1[1]->set(8);
    dv1[1]->set(15);
    dv1[1]->set(16);

    dv1[2]->set(11);
    dv1[2]->set(9);
    dv1[2]->set(2);
    dv1[2]->set(4);


    bool found;

    TIdDeMux::TDimensionalPoint pt;
    found = demux.GetCoordinates(5, &pt);
    assert(found);
    assert(pt.size() == 2);
    assert(pt[0] == 0);
    assert(pt[1] == 0);

    
    found = demux.GetCoordinates(15, &pt);
    assert(found);
    assert(pt.size() == 2);
    assert(pt[0] == 1 && pt[1] == 1);

    found = demux.GetCoordinates(11, &pt);
    assert(found);
    assert(pt.size() == 2);
    assert(pt[0] == 0 && pt[1] == 2);

    found = demux.GetCoordinates(2, &pt);
    assert(found);
    assert(pt.size() == 2);
    assert(pt[0] == 1 && pt[1] == 2);

    found = demux.GetCoordinates(17, &pt);
    assert(found);
    assert(pt.size() == 2);
    assert(pt[0] == 1 && pt[1] == 2);


    found = demux.GetCoordinates(200, &pt);
    assert(!found);



    return 0;
}


int main(int argc, const char* argv[])
{
    CTestIdMux app; 
    return app.AppMain(argc, argv);
}
