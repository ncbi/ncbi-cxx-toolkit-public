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


#ifndef PROSPLIGN_ALI_HPP
#define PROSPLIGN_ALI_HPP

#include <vector>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(prosplign)
using namespace objects;

enum EAliPieceType {
  eMP, //match/mismatch
  eVP,//vertical gap/frameshift (i.e. in nuc, prot is presented)
  eHP,//horisontal gap/frameshift (i.e. in prot, nuc is presented)
  eSP//intron
};

class CAliPiece {
public:
    EAliPieceType m_type;
    int m_len;
    inline void Init(EAliPieceType t, int len = 0) { 
        m_type = t; 
        m_len = len; 
    }
};

class CAli {
public:
    vector<CAliPiece> m_ps;
    
    CAli();
    CAli(const vector<pair<int, int> >& igi, bool lgap, bool rgap, const CAli& frali);//adds introns/end gaps
};

class CAliCreator
{
public:
    CAli& m_ali;
    EAliPieceType m_CurType;
    int m_CurLen;

    CAliCreator(CAli& ali);
    ~CAliCreator(void);
    inline void Fini(void)
    {
        Clear();
        reverse(m_ali.m_ps.begin(), m_ali.m_ps.end());  
    }
    inline void Add(EAliPieceType type, int len)
    {
        if(m_CurType != type) {
            Clear();
            m_CurType = type;
        }
        m_CurLen+=len;
    }
private:
    inline void Clear(void) 
    { 
        if(m_CurLen) {
            CAliPiece pc;
            pc.Init(m_CurType);
            pc.m_len = m_CurLen;
            m_ali.m_ps.push_back(pc);
        }
        m_CurLen = 0; 
    }
};

END_SCOPE(prosplign)
END_NCBI_SCOPE

#endif //PROSPLIGN_ALI_HPP
