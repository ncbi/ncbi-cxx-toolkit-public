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
* Author:  Ilya Dondoshansky
*
* File Description:
*   Class interface for performing BLAST traceback only, when preliminary 
*   BLAST results have already been found.
*
*/

#ifndef ALGO_BLAST_API___DBTRACEBACK__HPP
#define ALGO_BLAST_API___DBTRACEBACK__HPP

#include <algo/blast/api/db_blast.hpp>
#include <algo/blast/core/blast_gapalign.h>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

class NCBI_XBLAST_EXPORT CDbBlastTraceback : public CDbBlast
{
public:
    CDbBlastTraceback(const TSeqLocVector& queries, BlastSeqSrc* seq_src,
                      EProgram p, BlastHSPResults* results);
    CDbBlastTraceback(const TSeqLocVector& queries, BlastSeqSrc* seq_src,
                      CBlastOptionsHandle& opts, BlastHSPResults* results);
    void PartialRun(); 

protected:    
    int SetupSearch();
    void RunSearchEngine();
    void x_ResetQueryDs();
private:
    BlastExtensionParameters* mi_pExtParams;
    BlastHitSavingParameters* mi_pHitParams;
    BlastGapAlignStruct* mi_pGapAlign;
};

inline void CDbBlastTraceback::PartialRun()
{
    // Don't need this method!
}

END_SCOPE(blast)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.1  2004/02/24 18:20:40  dondosha
* Class derived from CDbBlast to do only traceback, given precomputed HSP results
*
* Revision 1.8  2004/02/19 21:10:25  dondosha
* Added vector of error messages to the CDbBlast class
*
* Revision 1.7  2004/02/18 23:48:45  dondosha
* Added TrimBlastHSPResults method to remove extra HSPs if limit on total number is provided
*
* Revision 1.6  2003/12/15 15:52:29  dondosha
* Added constructor with options handle argument
*
* Revision 1.5  2003/12/10 20:08:59  dondosha
* Added function to retrieve the query info structure
*
* Revision 1.4  2003/12/08 22:43:05  dondosha
* Added getters for score block and return stats structures
*
* Revision 1.3  2003/12/03 16:36:07  dondosha
* Renamed BlastMask to BlastMaskLoc, BlastResults to BlastHSPResults
*
* Revision 1.2  2003/11/26 18:36:44  camacho
* Renaming blast_option*pp -> blast_options*pp
*
* Revision 1.1  2003/10/29 22:37:21  dondosha
* Database BLAST search class
*
* Revision 1.21  2003/10/16 03:16:39  camacho
* Fix to setting queries/subjects
*
* Revision 1.20  2003/09/11 17:44:39  camacho
* Changed CBlastOption -> CBlastOptions
*
* Revision 1.19  2003/09/09 20:31:21  camacho
* Add const type qualifier
*
* Revision 1.18  2003/09/09 12:53:31  camacho
* Moved setup member functions to blast_setup_cxx.cpp
*
* Revision 1.17  2003/08/28 17:36:21  camacho
* Delete options before reassignment
*
* Revision 1.16  2003/08/25 17:15:33  camacho
* Removed redundant typedef
*
* Revision 1.15  2003/08/19 22:11:16  dondosha
* Cosmetic changes
*
* Revision 1.14  2003/08/19 20:24:17  dondosha
* Added TSeqAlignVector type as a return type for results-to-seqalign functions and input for formatting
*
* Revision 1.13  2003/08/19 13:45:21  dicuccio
* Removed 'USING_SCOPE(objects)'.  Changed #include guards to be standards
* compliant.  Added 'objects::' where necessary.
*
* Revision 1.12  2003/08/18 20:58:56  camacho
* Added blast namespace, removed *__.hpp includes
*
* Revision 1.11  2003/08/18 17:07:41  camacho
* Introduce new SSeqLoc structure (replaces pair<CSeq_loc, CScope>).
* Change in function to read seqlocs from files.
*
* Revision 1.10  2003/08/15 16:01:02  dondosha
* TSeqLoc and TSeqLocVector types definitions moved to blast_aux.hpp, so all applications can use them
*
* Revision 1.9  2003/08/11 19:55:04  camacho
* Early commit to support query concatenation and the use of multiple scopes.
* Compiles, but still needs work.
*
* Revision 1.8  2003/08/11 13:58:51  dicuccio
* Added export specifiers.  Fixed problem with unimplemented private copy ctor
* (truly make unimplemented)
*
* Revision 1.7  2003/08/08 19:42:14  dicuccio
* Compilation fixes: #include file relocation; fixed use of 'list' and 'vector'
* as variable names
*
* Revision 1.6  2003/08/01 17:40:56  dondosha
* Use renamed functions and structures from local blastkar.h
*
* Revision 1.5  2003/07/31 19:45:33  camacho
* Eliminate Ptr notation
*
* Revision 1.4  2003/07/30 19:58:02  coulouri
* use ListNode
*
* Revision 1.3  2003/07/30 15:00:01  camacho
* Do not use Malloc/MemNew/MemFree
*
* Revision 1.2  2003/07/14 22:16:37  camacho
* Added interface to retrieve masked regions
*
* Revision 1.1  2003/07/10 18:34:19  camacho
* Initial revision
*
*
* ===========================================================================
*/

#endif  /* ALGO_BLAST_API___DBBLAST__HPP */
