#ifndef ALGO_ALIGN_UTIL_BLAST_TABULAR__HPP
#define ALGO_ALIGN_UTIL_BLAST_TABULAR__HPP

/* $Id$
* ===========================================================================
*
*                            public DOMAIN NOTICE                          
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
* File Description:
*   CBlastTabular (a.k.a "m8") format representation
*
*/

#include <corelib/ncbistd.hpp>
#include <algo/align/util/align_shadow.hpp>

BEGIN_NCBI_SCOPE


class NCBI_XALGOALIGN_EXPORT CBlastTabular: public CAlignShadow
{
public:

    typedef CAlignShadow TParent;
    typedef TParent::TCoord TCoord;

    // c'tors
    CBlastTabular(void) {};

    CBlastTabular(const objects::CSeq_align& seq_align, bool save_xcript = false);

    CBlastTabular(const TId& idquery, TCoord qstart, bool qstrand,
                  const TId& idsubj, TCoord sstart, bool sstrand,
                  const string& xcript);

    CBlastTabular(const char* m8, bool force_local_ids = false);

    /// Construct CBlastTabular from m8 line,
    /// use score_func to select seq-id from FASTA-style ids  
    typedef int (*SCORE_FUNC)(const CRef<objects::CSeq_id>& id);
    CBlastTabular(const char* m8, SCORE_FUNC score_func);

    // getters / setters
    void   SetLength(TCoord length);
    TCoord GetLength(void) const;

    void   SetMismatches(TCoord mismatches);
    TCoord GetMismatches(void) const;

    void   SetGaps(TCoord gaps);
    TCoord GetGaps(void) const;

    void   SetEValue(double evalue);
    double GetEValue(void) const;

    void   SetIdentity(float identity);
    float  GetIdentity(void) const;

    void   SetScore(float evalue);
    float  GetScore(void) const;

    virtual void Modify(Uint1 where, TCoord new_pos);

protected:
    
    TCoord  m_Length;     // length of the alignment           
    TCoord  m_Mismatches; // number of mismatches              
    TCoord  m_Gaps;       // number of gap openings            
    double  m_EValue;     // e-Value                           
    float   m_Identity;   // Percent identity (ranging from 0 to 1)
    float   m_Score;      // bit score

    template <class F>
    void   x_Deserialize(const char* m8, F seq_id_extractor);

    virtual void   x_PartialSerialize(CNcbiOstream& os) const;
    virtual void   x_PartialDeserialize(const char* m8);
};


/////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

END_NCBI_SCOPE

#endif /* ALGO_ALIGN_UTIL_BLAST_TABULAR__HPP  */
