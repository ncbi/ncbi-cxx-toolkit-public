#ifndef ALGO_ALIGN_UTIL_DEMO_COMPART_COMPART__HPP
#define ALGO_ALIGN_UTIL_DEMO_COMPART_COMPART__HPP

/* $Id$
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
* Author:  Yuri Kapustin
*
* File Description:  Compartmentization demo
*                   
* ===========================================================================
*/

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>

#include <algo/align/util/blast_tabular.hpp>
#include <objmgr/scope.hpp>

BEGIN_NCBI_SCOPE


class CCompartApp : public CNcbiApplication
{
public:

    virtual void Init();
    virtual int  Run();
    virtual void Exit();

    typedef CBlastTabular          THit;
    typedef CRef<THit>             THitRef;
    typedef vector<THitRef>        THitRefs;

    class CCompartment: public CObject {
    public:

        CCompartment(const THitRefs& hitrefs, size_t length);

        typedef pair<THit::TCoord, THit::TCoord> TRange;
        TRange GetSpan    (void) const;

        bool   GetStrand  (void) const;
        bool   operator < (const CCompartment& rhs) const;

        size_t GetIdentityBin(void) const {
            return m_IdentityBin;
        }

        size_t GetExonCount(void) const {
            return m_ExonCount;
        }

        size_t GetMatchCount(void) const {
            return m_MatchCount;
        }

        size_t GetHitCount(void) const {
            return m_HitRefs.size();
        }

        friend ostream& operator << (ostream& ostr, const CCompartment& rhs);

    protected:

        THitRefs m_HitRefs;

        size_t   m_SeqLength;

        size_t   m_IdentityBin;
        size_t   m_ExonCount;
        size_t   m_MatchCount;

        void     x_AddHit    (const THitRef& hitref);
        void     x_EvalExons (void);
    };

    typedef CRef<CCompartment>  TCompartRef;
    typedef vector<TCompartRef> TCompartRefs;


private:

    typedef map<string,size_t> TStrIdToLen;

    TStrIdToLen            m_id2len;
    CRef<objects::CScope>  m_Scope;

    double                 m_penalty;
    double                 m_min_idty;
    double                 m_min_singleton_idty;
    size_t                 m_min_singleton_idty_bps;

    size_t                 m_MaxCompsPerQuery;

    bool                   m_NoXF;

    size_t                 m_Allocated;

    int     x_ProcessPair  (const string& query0, THitRefs& hitrefs);
    void    x_RankAndStore (void);
    void    x_ReadSeqLens  (CNcbiIstream& istr);
    size_t  x_GetSeqLength (const string& id);

    int     x_DoWithExternalHits(void);

    size_t  GetExonCont(void);
    size_t  GetMatchCount(void);

    TCompartRefs           m_Compartments;
    TCompartRefs           m_CompartmentsPermanent;
};


END_NCBI_SCOPE

#endif
