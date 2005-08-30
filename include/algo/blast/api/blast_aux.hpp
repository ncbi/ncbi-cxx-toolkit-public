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
*/

/// @file blast_aux.hpp
/// Contains C++ wrapper classes to structures in algo/blast/core as well as
/// some auxiliary functions to convert CSeq_loc to/from BlastMask structures.

#ifndef ALGO_BLAST_API___BLAST_AUX__HPP
#define ALGO_BLAST_API___BLAST_AUX__HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ddumpable.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/metareg.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqloc/Seq_interval.hpp>

#include <algo/blast/api/blast_types.hpp>
// NewBlast includes
#include <algo/blast/core/blast_util.h>
#include <algo/blast/core/blast_options.h>
#include <algo/blast/core/blast_filter.h> // Needed for BlastMaskLoc & BlastSeqLoc
#include <algo/blast/core/blast_extend.h>
#include <algo/blast/core/blast_gapalign.h>
#include <algo/blast/core/blast_hits.h>
#include <algo/blast/core/blast_psi.h>
#include <algo/blast/core/blast_hspstream.h>

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)
    class CSeq_loc;
END_SCOPE(objects)

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_SCOPE(blast)

/* Convenience typedef's for common types used in BLAST to implement the RAII
 * idiom.
 */

/// Uses C Deleter (free) - used in functions that deal with CORE BLAST
#define TYPEDEF_AUTOPTR_CDELETER(type) \
typedef AutoPtr<type, CDeleter<type> > TAuto ## type ## Ptr

/// Uses delete [] operator - for C++ arrays
#define TYPEDEF_AUTOPTR_ARRAYDELETER(type) \
typedef AutoPtr<type, ArrayDeleter<type> > TAuto ## type ## ArrayPtr

#ifndef SKIP_DOXYGEN_PROCESSING
/// Declares TAutoUint1Ptr (for Uint1 arrays allocated with malloc/calloc)
TYPEDEF_AUTOPTR_CDELETER(Uint1);
/// Declares TAutoCharPtr (for Char arrays allocated with malloc/calloc)
TYPEDEF_AUTOPTR_CDELETER(Char);
/// Declares TAutoUint1ArrayPtr (for Uint1 arrays allocated with new[])
TYPEDEF_AUTOPTR_ARRAYDELETER(Uint1);
#endif

/// Map a string into an element of the ncbi::blast::EProgram enumeration 
/// (except eBlastProgramMax).
/// @param program_name [in]
/// @return an element of the ncbi::blast::EProgram enumeration, except
/// eBlastProgramMax
/// @throws CBlastException if the string does not map into any of the EProgram
/// elements
NCBI_XBLAST_EXPORT
EProgram ProgramNameToEnum(const std::string& program_name);

/// Returns a string program name, given a blast::EBlastProgramType enumeration.
/// @param program Enumerated program value [in]
/// @return String program name.
NCBI_XBLAST_EXPORT
string Blast_ProgramNameFromType(EBlastProgramType program);

/** Converts a CSeq_loc into a BlastSeqLoc structure used in NewBlast
 * @param slp CSeq_loc to convert [in]
 * @return Linked list of BlastSeqLoc structures
 */
NCBI_XBLAST_EXPORT
BlastSeqLoc*
CSeqLoc2BlastSeqLoc(const objects::CSeq_loc* slp);

/** Retrieves the requested genetic code in Ncbistdaa format. 
 * @param genetic_code numeric identifier for genetic code requested [in]
 * @return NULL if memory allocation failure, otherwise genetic code string.
 */
NCBI_XBLAST_EXPORT
TAutoUint1ArrayPtr
FindGeneticCode(int genetic_code);

///structure for seqloc info
class CSeqLocInfo : public CObject {
public:
    typedef enum ETranslationFrame {
        eFramePlus1  =  1,
        eFramePlus2  =  2,
        eFramePlus3  =  3,
        eFrameMinus1 = -1,
        eFrameMinus2 = -2,
        eFrameMinus3 = -3,
        eFrameNotSet = 0
    };
    CSeqLocInfo(objects::CSeq_interval* interval, int frame)
        : m_Interval(interval)
    { SetFrame(frame); }

    const objects::CSeq_interval& GetInterval() const { return *m_Interval; }
    void SetInterval(objects::CSeq_interval* interval) 
    { m_Interval.Reset(interval); }
    int GetFrame() const { return (int) m_Frame; }
    void SetFrame(int frame); // Throws exception on out-of-range input
private:
    CRef <objects::CSeq_interval> m_Interval; 
    ETranslationFrame m_Frame;         // For translated nucleotide sequence
};

inline void CSeqLocInfo::SetFrame(int frame)
{
    if (frame < -3 || frame > 3) {
        string msg = 
            "CSeqLocInfo::SetFrame: input " + NStr::IntToString(frame) + 
            " out of range";
        throw std::out_of_range(msg);
    }
    m_Frame = (ETranslationFrame) frame;
}

/// Vector of per-query mask lists
typedef vector<list<CRef<CSeqLocInfo> > > TSeqLocInfoVector;

/// Converts a BlastMaskLoc internal structure into an object returned by the 
/// C++ API.
/// @param program Type of BLAST program [in]
/// @param seqid_v Vector of query Seq-ids [in]
/// @param mask All masking locations [in]
/// @param mask_v Vector of per-query lists of mask locations in CSeqLocInfo 
///               form. [out]
void 
Blast_GetSeqLocInfoVector(EBlastProgramType program, 
                          vector< CRef<objects::CSeq_id> >& seqid_v,
                          const BlastMaskLoc* mask, 
                          TSeqLocInfoVector& mask_v);


/** Declares class to handle deallocating of the structure using the appropriate
 * function
 */

#define DECLARE_AUTO_CLASS_WRAPPER(struct_name, free_func)                  \
/** Wrapper class for struct_name. */                                       \
                                                                            \
class NCBI_XBLAST_EXPORT C##struct_name : public CDebugDumpable             \
{                                                                           \
public:                                                                     \
    C##struct_name() : m_Ptr(NULL) {}                                       \
    C##struct_name(struct_name* p) : m_Ptr(p) {}                            \
    void Reset(struct_name* p = NULL) {                                     \
        if (m_Ptr) {                                                        \
            free_func(m_Ptr);                                               \
        }                                                                   \
        m_Ptr = p;                                                          \
    }                                                                       \
    ~C##struct_name() {                                                     \
        if (m_Ptr) {                                                        \
            free_func(m_Ptr);                                               \
            m_Ptr = NULL;                                                   \
        }                                                                   \
    }                                                                       \
    struct_name* Release() {                                                \
        struct_name* retval = m_Ptr;                                        \
        m_Ptr = NULL;                                                       \
        return retval;                                                      \
    }                                                                       \
    struct_name* Get() const { return m_Ptr; }                              \
    operator struct_name *() { return m_Ptr; }                              \
    operator struct_name *() const { return m_Ptr; }                        \
    struct_name* operator->() { return m_Ptr; }                             \
    struct_name* operator->() const { return m_Ptr; }                       \
    struct_name** operator&() { return &m_Ptr; }                            \
    void DebugDump(CDebugDumpContext ddc, unsigned int depth) const;        \
private:                                                                    \
    struct_name* m_Ptr;                                                     \
}

#ifndef SKIP_DOXYGEN_PROCESSING

DECLARE_AUTO_CLASS_WRAPPER(BLAST_SequenceBlk, BlastSequenceBlkFree);

DECLARE_AUTO_CLASS_WRAPPER(BlastQueryInfo, BlastQueryInfoFree);
DECLARE_AUTO_CLASS_WRAPPER(QuerySetUpOptions, BlastQuerySetUpOptionsFree);

DECLARE_AUTO_CLASS_WRAPPER(LookupTableOptions, LookupTableOptionsFree);
DECLARE_AUTO_CLASS_WRAPPER(LookupTableWrap, LookupTableWrapFree);

DECLARE_AUTO_CLASS_WRAPPER(BlastInitialWordOptions,
                           BlastInitialWordOptionsFree);
DECLARE_AUTO_CLASS_WRAPPER(BlastInitialWordParameters,
                           BlastInitialWordParametersFree);

DECLARE_AUTO_CLASS_WRAPPER(Blast_ExtendWord, BlastExtendWordFree);
DECLARE_AUTO_CLASS_WRAPPER(BlastExtensionOptions, BlastExtensionOptionsFree);
DECLARE_AUTO_CLASS_WRAPPER(BlastExtensionParameters, BlastExtensionParametersFree);

DECLARE_AUTO_CLASS_WRAPPER(BlastHitSavingOptions, BlastHitSavingOptionsFree);
DECLARE_AUTO_CLASS_WRAPPER(BlastHitSavingParameters,
                           BlastHitSavingParametersFree);

DECLARE_AUTO_CLASS_WRAPPER(PSIBlastOptions, PSIBlastOptionsFree);
DECLARE_AUTO_CLASS_WRAPPER(BlastDatabaseOptions, BlastDatabaseOptionsFree);

DECLARE_AUTO_CLASS_WRAPPER(BlastScoreBlk, BlastScoreBlkFree);
DECLARE_AUTO_CLASS_WRAPPER(BlastScoringOptions, BlastScoringOptionsFree);
DECLARE_AUTO_CLASS_WRAPPER(BlastScoringParameters, BlastScoringParametersFree);

DECLARE_AUTO_CLASS_WRAPPER(BlastEffectiveLengthsOptions,
                           BlastEffectiveLengthsOptionsFree);
DECLARE_AUTO_CLASS_WRAPPER(BlastEffectiveLengthsParameters,
                           BlastEffectiveLengthsParametersFree);

DECLARE_AUTO_CLASS_WRAPPER(BlastGapAlignStruct, BLAST_GapAlignStructFree);
DECLARE_AUTO_CLASS_WRAPPER(BlastHSPResults, Blast_HSPResultsFree);

DECLARE_AUTO_CLASS_WRAPPER(PSIMatrix, PSIMatrixFree);
DECLARE_AUTO_CLASS_WRAPPER(PSIDiagnosticsResponse, PSIDiagnosticsResponseFree);

DECLARE_AUTO_CLASS_WRAPPER(BlastSeqSrc, BlastSeqSrcFree);
DECLARE_AUTO_CLASS_WRAPPER(BlastSeqSrcIterator, BlastSeqSrcIteratorFree);
DECLARE_AUTO_CLASS_WRAPPER(Blast_Message, Blast_MessageFree);

DECLARE_AUTO_CLASS_WRAPPER(BlastMaskLoc, BlastMaskLocFree);
DECLARE_AUTO_CLASS_WRAPPER(BlastSeqLoc, BlastSeqLocFree);

#endif /* SKIP_DOXYGEN_PROCESSING */

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

/*
* ===========================================================================
*
* $Log$
* Revision 1.63  2005/08/30 20:31:37  camacho
* + parametrized constructor for CSeqLocInfo
*
* Revision 1.62  2005/08/29 14:37:51  camacho
* From Ilya Dondoshansky:
* Added class CSeqLocInfo, type TSeqLocInfoVector and method to construct
* it from the BlastMaskLoc core structure
*
* Revision 1.61  2005/06/23 16:18:45  camacho
* Doxygen fixes
*
* Revision 1.60  2005/06/20 17:30:50  camacho
* Remove unneeded header which brings dependency on the object manager
*
* Revision 1.59  2005/06/08 19:49:00  camacho
* Added Get() method to auto class wrapper
*
* Revision 1.58  2005/05/25 17:37:30  camacho
* + RAII wrapper for BlastSeqLoc
*
* Revision 1.57  2005/05/24 19:08:05  camacho
* Make auto class wrapper's Reset method take a default NULL argument
*
* Revision 1.56  2005/04/18 14:00:18  camacho
* + RAII class for BlastSeqSrcIterator
*
* Revision 1.55  2005/03/28 16:56:04  jcherry
* Added export specifier to DECLARE_AUTO_CLASS_WRAPPER macro
*
* Revision 1.54  2005/01/19 15:46:50  papadopo
* remove obsolete prototype
*
* Revision 1.53  2005/01/10 18:34:40  dondosha
* BlastMaskLocDNAToProtein and BlastMaskLocProteinToDNA moved to core with changed signatures
*
* Revision 1.52  2004/12/29 15:11:24  camacho
* Add Release method to C structure RAII wrapper classes
*
* Revision 1.51  2004/12/28 18:47:38  dondosha
* Added auto class wrapper for BlastMaskLoc
*
* Revision 1.50  2004/12/28 16:45:57  camacho
* Move typedefs to AutoPtr to public header so that they are used consistently
*
* Revision 1.49  2004/12/20 21:50:17  camacho
* + RAII BlastEffectiveLengthsParameters
*
* Revision 1.48  2004/12/20 16:11:24  camacho
* + RAII wrapper for BlastScoringParameters
*
* Revision 1.47  2004/12/03 22:24:08  camacho
* Updated documentation
*
* Revision 1.46  2004/11/23 23:00:46  camacho
* + RAII class for BlastSeqSrc
*
* Revision 1.45  2004/11/04 15:51:59  papadopo
* prepend 'Blast' to RPSInfo and related structures
*
* Revision 1.44  2004/10/26 15:29:03  dondosha
* Added function Blast_FillRPSInfo, previously static in demo/blast_app.cpp
*
* Revision 1.43  2004/09/13 15:55:20  madden
* Remove unused parameter from CSeqLoc2BlastSeqLoc
*
* Revision 1.42  2004/09/13 12:44:37  madden
* Changes for redefinition of BlastSeqLoc and BlastMaskLoc
*
* Revision 1.41  2004/09/08 14:14:11  camacho
* Doxygen fixes
*
* Revision 1.40  2004/08/18 18:13:42  camacho
* Remove GetProgramFromBlastProgramType, add ProgramNameToEnum
*
* Revision 1.39  2004/08/11 14:24:20  camacho
* Move FindGeneticCode
*
* Revision 1.38  2004/08/04 20:09:48  camacho
* + class wrappers for PSIMatrix and PSIDiagnosticsResponse
*
* Revision 1.37  2004/06/23 14:02:05  dondosha
* Changed CSeq_loc argument in CSeqLoc2BlastMaskLoc to pointer
*
* Revision 1.36  2004/06/21 15:23:01  camacho
* Fix PSIBlastOptions free function
*
* Revision 1.35  2004/05/14 16:02:56  madden
* Rename BLAST_ExtendWord to Blast_ExtendWord in order to fix conflicts with C toolkit
*
* Revision 1.34  2004/05/05 15:28:10  dondosha
* Renamed functions in blast_hits.h accordance with new convention Blast_[StructName][Task]
*
* Revision 1.33  2004/04/30 17:12:42  dondosha
* Changed prefix from BLAST_ to conventional Blast_
*
* Revision 1.32  2004/03/19 18:56:04  camacho
* Move to doxygen AlgoBlast group
*
* Revision 1.31  2004/03/18 13:50:38  camacho
* Remove unused CDeleter template specializations
*
* Revision 1.30  2004/03/16 14:48:01  dondosha
* Typo fix in doxygen comment
*
* Revision 1.29  2004/03/12 16:33:22  camacho
* Rename BLAST_ExtendWord functions to avoid collisions with C toolkit libraries
*
* Revision 1.28  2004/03/12 15:57:59  camacho
* Make consistent use of New/Free functions for BLAST_ExtendWord structure
*
* Revision 1.27  2003/12/03 16:36:07  dondosha
* Renamed BlastMask to BlastMaskLoc, BlastResults to BlastHSPResults
*
* Revision 1.26  2003/11/26 18:22:13  camacho
* +Blast Option Handle classes
*
* Revision 1.25  2003/10/07 17:27:37  dondosha
* Lower case mask removed from options, added to the SSeqLoc structure
*
* Revision 1.24  2003/09/11 17:44:39  camacho
* Changed CBlastOption -> CBlastOptions
*
* Revision 1.23  2003/09/10 20:00:49  dondosha
* BlastLookupTableDestruct renamed to LookupTableWrapFree
*
* Revision 1.22  2003/08/27 21:27:58  camacho
* Fix to previous commit
*
* Revision 1.21  2003/08/27 18:40:02  camacho
* Change free function for blast db options struct
*
* Revision 1.20  2003/08/20 15:23:47  ucko
* DECLARE_AUTO_CLASS_WRAPPER: Remove occurrences of ## adjacent to punctuation.
*
* Revision 1.19  2003/08/20 14:45:26  dondosha
* All references to CDisplaySeqalign moved to blast_format.hpp
*
* Revision 1.18  2003/08/19 22:11:49  dondosha
* Major types definitions moved to blast_types.h
*
* Revision 1.17  2003/08/19 20:22:05  dondosha
* EProgram definition moved from CBlastOptions clase to blast scope
*
* Revision 1.16  2003/08/19 13:45:21  dicuccio
* Removed 'USING_SCOPE(objects)'.  Changed #include guards to be standards
* compliant.  Added 'objects::' where necessary.
*
* Revision 1.15  2003/08/18 22:17:52  camacho
* Renaming of SSeqLoc members
*
* Revision 1.14  2003/08/18 20:58:56  camacho
* Added blast namespace, removed *__.hpp includes
*
* Revision 1.13  2003/08/18 17:07:41  camacho
* Introduce new SSeqLoc structure (replaces pair<CSeq_loc, CScope>).
* Change in function to read seqlocs from files.
*
* Revision 1.12  2003/08/14 19:08:45  dondosha
* Use CRef instead of pointer to CSeq_loc in the TSeqLoc type definition
*
* Revision 1.11  2003/08/12 19:17:58  dondosha
* Added TSeqLocVector typedef so it can be used from all sources; removed scope argument from functions
*
* Revision 1.10  2003/08/11 19:55:04  camacho
* Early commit to support query concatenation and the use of multiple scopes.
* Compiles, but still needs work.
*
* Revision 1.9  2003/08/11 17:12:10  dondosha
* Do not use CConstRef as argument to CSeqLoc2BlastMaskLoc
*
* Revision 1.8  2003/08/11 16:09:53  dondosha
* Pass CConstRef by value in CSeqLoc2BlastMaskLoc
*
* Revision 1.7  2003/08/11 15:23:23  dondosha
* Renamed conversion functions between BlastMaskLoc and CSeqLoc; added algo/blast/core to headers from core BLAST library
*
* Revision 1.6  2003/08/11 13:58:51  dicuccio
* Added export specifiers.  Fixed problem with unimplemented private copy ctor
* (truly make unimplemented)
*
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
* Convert CSeq_loc to BlastMaskLocPtr
*
* Revision 1.1  2003/07/10 18:34:19  camacho
* Initial revision
*
*
* ===========================================================================
*/

#endif  /* ALGO_BLAST_API___BLAST_AUX__HPP */
