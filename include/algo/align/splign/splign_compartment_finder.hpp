#ifndef ALGO_ALIGN_SPLIGN_COMPARTMENT_FINDER__HPP
#define ALGO_ALIGN_SPLIGN_COMPARTMENT_FINDER__HPP

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
* Author:  Yuri Kapustin, Alexander Souvorov
*
* File Description:  Compartment Finder
*                   
* ===========================================================================
*/


#include <algo/align/splign/splign_hit.hpp>
#include <corelib/ncbi_limits.hpp>
#include <vector>


BEGIN_NCBI_SCOPE


// Detect all compartments over a hit set
class NCBI_XALGOALIGN_EXPORT CCompartmentFinder {

public:

    typedef vector<CHit> THits;
    typedef vector<const CHit*> THitConstPtrs;

    // hits must be in plus strand
    CCompartmentFinder(THits::const_iterator start,
                       THits::const_iterator finish);

    // setters and getters
    void SetMaxIntron (size_t intr_max) {
        m_intron_max = intr_max;
    }

    static size_t GetDefaultMaxIntron(void) {
        return 750000;
    }
    
    void SetPenalty(size_t penalty) {
        m_penalty = penalty;
    }
    
    void SetMinCoverage(size_t mincov) {
        m_min_coverage = mincov;
    }
    
    static size_t GetDefaultPenalty(void) {
        return 1000;
    }
    
    static size_t GetDefaultMinCoverage(void) {
        return 500;
    }


    size_t Run(void); // do the compartment search

    // order compartments by lower subj coordinate
    void OrderCompartments(void);

    // single compartment representation
    class NCBI_XALGOALIGN_EXPORT CCompartment {

    public:
        
        CCompartment(void) {
            m_coverage = 0;
            m_box[0] = m_box[2] = kMax_UInt;
            m_box[1] = m_box[3] = 0;
        }

        const THitConstPtrs& GetMembers(void) {
            return m_members;
        }
    
        void AddMember(const CHit* hit) {
            m_members.push_back(hit);
        }
    
        void UpdateMinMax(void);

        bool GetStrand(void) const;

        const CHit* GetFirst(void) const {
            m_iter = 0;
            return GetNext();
        }
        
        const CHit* GetNext(void) const {
            if(m_iter < m_members.size()) {
                return m_members[m_iter++];
            }
            return 0;
        }
        
        const size_t* GetBox(void) const {
            return m_box;
        }
        
        bool operator < (const CCompartment& rhs) {
            return m_coverage < rhs.m_coverage;
        }
        
        friend bool PLowerSubj(const CCompartment& c1,
                               const CCompartment& c2) {

            return c1.m_box[2] < c2.m_box[2];
        }
        
    protected:
        
        size_t              m_coverage;
        THitConstPtrs       m_members;
        size_t              m_box[4];
        mutable size_t      m_iter;
    };

    // iterate through compartments
    CCompartment* GetFirst(void);
    CCompartment* GetNext(void);


private:

    size_t              m_intron_max;    // max intron size
    size_t              m_penalty;       // penalty per compartment
    size_t              m_min_coverage;
    
    THitConstPtrs         m_hits;         // input hits
    vector<CCompartment>  m_compartments; // final compartments
    int                   m_iter;         // GetFirst/Next index

    // this structure describes the best target reached 
    // at a given query coordinatea  
    struct SQueryMark {
        size_t       m_coord;
        int          m_score;
        int          m_hit;
        
        SQueryMark(size_t coord, int score, int hit):
            m_coord(coord), m_score(score), m_hit(hit) {}
        
        bool operator < (const SQueryMark& rhs) const {
            return m_coord < rhs.m_coord;
        }
    };
    
    struct SHitStatus {
        
        enum EType {
            eNone, 
            eExtension,
            eOpening
        };
        
        EType m_type;
        int   m_prev;
        
        SHitStatus(void): m_type(eNone), m_prev(-1) {}
        SHitStatus(EType type, int prev): m_type(type), m_prev(prev) {}
        
    };
    
};


// Facilitate access to compartments over a hit set
class NCBI_XALGOALIGN_EXPORT CCompartmentAccessor
{
public:

    typedef CCompartmentFinder::THits THits;

    // [start,finish) are assumed to share same query and subj
    CCompartmentAccessor(THits::iterator start, THits::iterator finish,
                         size_t comp_penalty_bps,
                         size_t min_coverage);
    
    bool GetFirst(THits& compartment);
    bool GetNext(THits& compartment);
    
    size_t GetCount(void) const {
        return m_pending.size();
    }
    
    void Get(size_t i, THits& compartment) const {
        compartment = m_pending[i];
    }
    
    const size_t* GetBox(size_t i) const {
        return &m_ranges.front() + i*4;
    }
    
    bool GetStrand(size_t i) const {
        return m_strands[i];
    }
    
    
private:
    
    vector<THits>         m_pending;
    vector<size_t>        m_ranges;
    vector<bool>          m_strands;
    size_t                m_iter;
    
    
    void x_Copy2Pending(CCompartmentFinder& finder);
};


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.4  2005/03/28 18:26:52  jcherry
 * Added export specifier
 *
 * Revision 1.3  2005/03/28 17:38:22  jcherry
 * Added export specifiers
 *
 * Revision 1.2  2004/12/06 22:11:24  kapustin
 * File header update
 *
 * Revision 1.1  2004/09/27 17:15:03  kapustin
 * Move from /src/algo/align/splign
 *
 * Revision 1.4  2004/05/04 15:23:45  ucko
 * Split splign code out of xalgoalign into new xalgosplign.
 *
 * Revision 1.3  2004/04/23 14:37:44  kapustin
 * *** empty log message ***
 *
 * ==========================================================================
 */

#endif
