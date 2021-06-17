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


#ifndef ALIGNINFO_H
#define ALIGNINFO_H


#include <corelib/ncbistl.hpp>
#include <vector>
#include "IntronChain.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(prosplign)

class CAlignInfo
{
public:
    CAlignInfo(int length, CIgapIntronPool& pool);
    ~CAlignInfo();
    size_t size() const { return m_length; }
    void ClearIIC(void);
public:
    vector<int> w, h, v, fh, fv;
    CIgapIntronChain *wis, *his, *vis, *fhis, *fvis;
private:
    CAlignInfo(const CAlignInfo& ori);
    CAlignInfo& operator=(const CAlignInfo& ori);

    size_t m_length;
};

class CFrAlignRow
{
public:
    inline CFrAlignRow(int length) {
        w.resize(length);
        v.resize(length);
    }
    vector<int> w, v;
};

class CProSplignScaledScoring;

class CAlignRow
{
public:
    CAlignRow(int length, const CProSplignScaledScoring& scoring);
    vector<int> m_w, m_v, m_h1, m_h2, m_h3;
    int *w, *v, *h1, *h2, *h3;
private:
    CAlignRow(const CAlignRow& ori);
    CAlignRow& operator=(const CAlignRow& ori);
};

class CFindGapIntronRow : public CAlignRow
{
public:
    CFindGapIntronRow(int length, const CProSplignScaledScoring& scoring, CIgapIntronPool& pool);
    ~CFindGapIntronRow();
    size_t size() const { return m_length; }
    void ClearIIC(void);
public:
    CIgapIntronChain *wis, *vis, *h1is, *h2is, *h3is;
private:
    CFindGapIntronRow(const CFindGapIntronRow& ori);
    CFindGapIntronRow& operator=(const CFindGapIntronRow& ori);

    size_t m_length;
};

END_SCOPE(prosplign)
END_NCBI_SCOPE

#endif//ALIGNINFO_H
