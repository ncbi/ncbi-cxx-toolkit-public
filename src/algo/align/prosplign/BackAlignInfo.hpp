/*
*  $Id$
*
* =========================================================================
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
*  Government do not and cannt warrant the performance or results that
*  may be obtained by using this software or data. The NLM and the U.S.
*  Government disclaim all warranties, express or implied, including
*  warranties of performance, merchantability or fitness for any particular
*  purpose.
*
*  Please cite the author in any work or product based on this material.
*
* =========================================================================
*
*  Author: Boris Kiryutin
*
* =========================================================================
*/


#ifndef PROSPLIGN_BACKALIGNINFO_HPP
#define PROSPLIGN_BACKALIGNINFO_HPP

#include <corelib/ncbi_limits.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(prosplign)

template <class el>
class MATR
{
public:
  MATR() { pels = NULL; }
  ~MATR() {  delete pels; }
  void Free() {
    delete pels;
    pels = NULL;
  }
  void Init(int rownum, int colnum) {
    coln = colnum;    
    if(!pels) pels = new vector<el>;
    if(pels->max_size()/colnum +1 <= /*(vector<el>::size_type)  bug in Linux*/(unsigned)rownum) throw bad_alloc();
    if(numeric_limits<unsigned long>::max()/colnum +1 <= /*(vector<el>::size_type)  bug in Linux*/(unsigned long)rownum) throw bad_alloc();
    pels->resize(rownum*(unsigned long)colnum);
  }
  el *operator[](int row) { return &(*pels)[0] + row*(unsigned long)coln; }
private:
    vector<el> *pels;
    int coln;
};


template<class T>
class CTBackAlignInfo
{
public:
    MATR<T> b;
    int ilen; //sequence1 length = number of rows in B
    int jlen; //sequence2 length = number of columns in B
    int maxi, maxj; //indexes to start back alignment from
//    int maxsc; //maximum score at the last column

     void Init(int oilen, int ojlen)
    {
    //  maxsc = 0;
        ilen = oilen;
        jlen = ojlen;
        b.Init(ilen, jlen);
    }

};

typedef CTBackAlignInfo<char> CBackAlignInfo;

END_SCOPE(prosplign)
END_NCBI_SCOPE

#endif //PROSPLIGN_BACKALIGNINFO_HPP
