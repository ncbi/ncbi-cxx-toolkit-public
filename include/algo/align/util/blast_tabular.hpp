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
    CBlastTabular(const char* m8, bool force_local_ids = false);

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

    virtual void   x_PartialSerialize(CNcbiOstream& os) const;
    virtual void   x_PartialDeserialize(const char* m8);
};


/////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.7  2006/09/13 16:29:06  kapustin
 * Add local id option when creating from a blast string
 *
 * Revision 1.6  2006/03/21 16:16:44  kapustin
 * Support edit transcript string
 *
 * Revision 1.5  2005/09/12 16:21:34  kapustin
 * Add compartmentization algorithm
 *
 * Revision 1.4  2005/07/28 15:17:02  kapustin
 * Add export specifiers
 *
 * Revision 1.3  2005/07/28 12:29:26  kapustin
 * Convert to non-templatized classes where causing compilation incompatibility
 *
 * Revision 1.2  2005/06/03 16:20:21  lavr
 * Explicit (unsigned char) casts in ctype routines
 *
 * Revision 1.1  2005/04/18 15:23:00  kapustin
 * Initial revision
 *
 * ===========================================================================
 */

#endif /* ALGO_ALIGN_UTIL_BLAST_TABULAR__HPP  */

