#ifndef ALGO_ALIGN_NW_DEMO_HFILTER_HITFILTER_APP__HPP
#define ALGO_ALIGN_NW_DEMO_HFILTER_HITFILTER_APP__HPP

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
* File Description:  HitFilter application class definition
*                   
* ===========================================================================
*/

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>

#include <algo/align/util/blast_tabular.hpp>


BEGIN_NCBI_SCOPE


class CAppHitFilterException : public CException 
{
public:
    enum EErrCode {
        eInternal,
        eGeneral
    };
    virtual const char* GetErrCodeString(void) const {
        switch ( GetErrCode() ) {
        case eInternal:
            return "eInternal";
        case eGeneral:
            return "eGeneral";
        default:
            return CException::GetErrCodeString();
        }
    }
    NCBI_EXCEPTION_DEFAULT(CAppHitFilterException, CException);
};


BEGIN_SCOPE(objects)
    class CSeq_align;
END_SCOPE(objects)

class CAppHitFilter : public CNcbiApplication
{
public:

    virtual void Init();
    virtual int  Run();
    virtual void Exit();

    typedef CBlastTabular          THit;
    typedef CRef<THit>             THitRef;
    typedef vector<THitRef>        THitRefs;

private:

    typedef list<CRef<objects::CSeq_align> > TSeqAlignList;

    void x_ReadInputHits(THitRefs* phitrefs);
    void x_IterateSeqAlignList(const TSeqAlignList& sa_list, 
                               THitRefs* phitrefs,
                               bool parse_aln,
                               const THit::TCoord& min_len,
                               const double& min_idty) const;
    void x_DumpOutput(const THitRefs& hitrefs);
    void x_LoadConstraints(CNcbiIstream& istr, THitRefs& all);
    void x_LoadIDs(CNcbiIstream& istr);

    typedef map<string,string> TMapIds;
    TMapIds m_IDs;

    struct SBuildIDs {
        string m_id [2];
    };
    typedef map<string,SBuildIDs> TMapIdPairs;
    TMapIdPairs m_IDRevs;
};


END_NCBI_SCOPE


#endif /* ALGO_ALIGN_NW_DEMO_HFILTER_HITFILTER_APP__HPP */
