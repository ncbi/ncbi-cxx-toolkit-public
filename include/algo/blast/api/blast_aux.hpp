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
* Author:  Christiam Camacho
*
* File Description:
*   C++ Wrappers to NewBlast structures
*
*/

#ifndef BLAST_AUX__HPP
#define BLAST_AUX__HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ddumpable.hpp>

#include <objects/seqloc/Seq_loc.hpp>

// NewBlast includes
#include <blast_def.h>
#include <blast_options.h>
#include <blast_hits.h>
#include <blast_filter.h>       // Needed for BlastMask & BlastSeqLoc
#include <blast_util.h>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

BEGIN_NCBI_SCOPE

/** Converts a CSeq_loc into a BlastMaskPtr structure used in NewBlast
 * @param sl CSeq_loc to convert [in]
 * @param index Number of frame/query number? this CSeq_loc applies to [in]
 * @return Linked list of BlastMask structures
 */
BlastMask*
BLASTSeqLoc2BlastMask(const CSeq_loc& sl, int index);

CRef<CSeq_loc>
BLASTBlastMask2SeqLoc(BlastMask* mask);



/** Declares class to handle deallocating of the structure using the appropriate
 * function
 */

#define DECLARE_AUTO_CLASS_WRAPPER(struct_name, free_func) \
class C##struct_name##Ptr : public CDebugDumpable \
{ \
public: \
    C##struct_name##Ptr() : m_Ptr(NULL) {} \
    C##struct_name##Ptr(struct_name* p) : m_Ptr(p) {} \
    void Reset(struct_name* p) { if (m_Ptr) { free_func(m_Ptr); } m_Ptr = p; } \
    ~C##struct_name##Ptr() { if (m_Ptr) { free_func(m_Ptr); m_Ptr = NULL; } } \
    operator struct_name *() { return m_Ptr; } \
    operator struct_name *() const { return m_Ptr; } \
    struct_name* operator->() { return m_Ptr; } \
    struct_name* operator->() const { return m_Ptr; } \
    struct_name** operator&() { return &m_Ptr; } \
    void DebugDump(CDebugDumpContext ddc, unsigned int depth) const; \
private: \
    struct_name* m_Ptr; \
}

DECLARE_AUTO_CLASS_WRAPPER(BLAST_SequenceBlk, BlastSequenceBlkFree);

DECLARE_AUTO_CLASS_WRAPPER(BlastQueryInfo, BlastQueryInfoFree);
DECLARE_AUTO_CLASS_WRAPPER(QuerySetUpOptions, BlastQuerySetUpOptionsFree);

DECLARE_AUTO_CLASS_WRAPPER(LookupTableOptions, LookupTableOptionsFree);
DECLARE_AUTO_CLASS_WRAPPER(LookupTableWrap, BlastLookupTableDestruct);

DECLARE_AUTO_CLASS_WRAPPER(BlastInitialWordOptions,
        BlastInitialWordOptionsFree);
DECLARE_AUTO_CLASS_WRAPPER(BlastInitialWordParameters,
        BlastInitialWordParametersFree);

DECLARE_AUTO_CLASS_WRAPPER(BLAST_ExtendWord, BlastExtendWordFree);
DECLARE_AUTO_CLASS_WRAPPER(BlastExtensionOptions, BlastExtensionOptionsFree);
DECLARE_AUTO_CLASS_WRAPPER(BlastExtensionParameters,
        BlastExtensionParametersFree);

DECLARE_AUTO_CLASS_WRAPPER(BlastHitSavingOptions, BlastHitSavingOptionsFree);
DECLARE_AUTO_CLASS_WRAPPER(BlastHitSavingParameters,
        BlastHitSavingParametersFree);

DECLARE_AUTO_CLASS_WRAPPER(PSIBlastOptions, sfree);
DECLARE_AUTO_CLASS_WRAPPER(BlastDatabaseOptions, sfree);

DECLARE_AUTO_CLASS_WRAPPER(BlastScoreBlk, BlastScoreBlkFree);
DECLARE_AUTO_CLASS_WRAPPER(BlastScoringOptions, BlastScoringOptionsFree);

DECLARE_AUTO_CLASS_WRAPPER(BlastEffectiveLengthsOptions,
        BlastEffectiveLengthsOptionsFree);

DECLARE_AUTO_CLASS_WRAPPER(BlastGapAlignStruct, BLAST_GapAlignStructFree);
DECLARE_AUTO_CLASS_WRAPPER(BlastResults, BLAST_ResultsFree);

END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.5  2003/08/01 17:40:56  dondosha
* Use renamed functions and structures from local blastkar.h
*
* Revision 1.4  2003/07/31 19:45:33  camacho
* Eliminate Ptr notation
*
* Revision 1.3  2003/07/30 15:00:01  camacho
* Do not use Malloc/MemNew/MemFree
*
* Revision 1.2  2003/07/14 22:17:17  camacho
* Convert CSeq_loc to BlastMaskPtr
*
* Revision 1.1  2003/07/10 18:34:19  camacho
* Initial revision
*
*
* ===========================================================================
*/

#endif  /* BLAST_AUX__HPP */
