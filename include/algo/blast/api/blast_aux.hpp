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
#include <util/range.hpp>       // For TSeqRange

// BLAST includes
#include <algo/blast/api/blast_types.hpp>
#include <algo/blast/api/blast_exception.hpp>
#include <algo/blast/core/blast_query_info.h>
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
    class CSeq_interval;
    class CPacked_seqint;
END_SCOPE(objects)

template <>
struct Deleter<BlastHSPStream>
{
    static void Delete(BlastHSPStream* p) 
    { BlastHSPStreamFree(p); }
};

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
 * @return NULL if genetic_code is invalid or in case of memory allocation 
 * failure, otherwise genetic code string.
 * @note the returned string has length GENCODE_STRLEN
 */
NCBI_XBLAST_EXPORT
TAutoUint1ArrayPtr
FindGeneticCode(int genetic_code);

///structure for seqloc info
class NCBI_XBLAST_EXPORT CSeqLocInfo : public CObject {
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
    const objects::CSeq_id& GetSeqId() const { return m_Interval->GetId(); }
    void SetInterval(objects::CSeq_interval* interval) 
    { m_Interval.Reset(interval); }
    int GetFrame() const { return (int) m_Frame; }
    void SetFrame(int frame); // Throws exception on out-of-range input
    operator TSeqRange() const {
        TSeqRange retval(m_Interval->GetFrom(), 0);
        retval.SetToOpen(m_Interval->GetTo());
        return retval;
    }
private:
    CRef<objects::CSeq_interval> m_Interval; 
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

/// Collection of masked regions for a single query sequence
class NCBI_XBLAST_EXPORT TMaskedQueryRegions : public list< CRef<CSeqLocInfo> >
{
public:
    /// Return a new instance of this object that is restricted to the location
    /// specified
    /// @param location location describing the range to restrict. Note that
    /// only the provided range is examined, the Seq-id and strand of the 
    /// location (if assigned and different from unknown) is copied verbatim 
    /// into the return value of this method [in]
    /// @return empty TMaskedQueryRegions if this object is empty, otherwise 
    /// the intersection of location and this object
    TMaskedQueryRegions 
    RestrictToSeqInt(const objects::CSeq_interval& location) const;
};


/// Collection of masked regions for all queries in a BLAST search
/// @note this supports tra
typedef vector< TMaskedQueryRegions > TSeqLocInfoVector;

/** Function object to assist in finding all CSeqLocInfo objects which
 * corresponds to a given frame.
 */
class CFrameFinder : public unary_function<CRef<CSeqLocInfo>, bool>
{
public:
    /// Convenience typedef
    typedef CSeqLocInfo::ETranslationFrame ETranslationFrame;

    /// ctor
    /// @param frame the translation frame [in]
    CFrameFinder(ETranslationFrame frame) : m_Frame(frame) {}

    /// Returns true if its argument's frame corresponds to the one used to
    /// create this object
    /// @param seqlocinfo the object to examine [in]
    bool operator() (const CRef<CSeqLocInfo>& seqlocinfo) const {
        if (seqlocinfo.Empty()) {
            NCBI_THROW(CBlastException, eInvalidArgument, 
                       "Empty CRef<CSeqLocInfo>!");
        }
        return (seqlocinfo->GetFrame() == m_Frame) ? true : false;
    }
private:
    ETranslationFrame m_Frame;  ///< Frame to look for
};

/// Auxiliary function to convert a Seq-loc describing masked query regions to a 
/// TMaskedQueryRegions object
/// @param sloc Seq-loc to use as source (must be Packed-int or Seq-int) [in]
/// @param program BLAST program type [in]
/// @param assume_both_strands ignores the strand of sloc_in and adds masking
/// locations to both strands in return value. This is irrelevant for protein
/// queries
/// @return List of masked query regions.
/// @throws CBlastException if Seq-loc type cannot be converted to Packed-int
NCBI_XBLAST_EXPORT
TMaskedQueryRegions
PackedSeqLocToMaskedQueryRegions(CConstRef<objects::CSeq_loc> sloc,
                                 EBlastProgramType program,
                                 bool assume_both_strands = false);

/// Interface to build a CSeq-loc from a TMaskedQueryRegion; this
/// method always throws an exception, because conversion in this
/// direction can be lossy.  The conversion could be supported for
/// cases where it can be done safely, but it might be better to
/// convert the calling code to use a CBlastQueryVector.
NCBI_XBLAST_EXPORT
CRef<objects::CSeq_loc>
MaskedQueryRegionsToPackedSeqLoc( const TMaskedQueryRegions & sloc);


/// Converts a BlastMaskLoc internal structure into an object returned by the 
/// C++ API.
/// @param program Type of BLAST program [in]
/// @param queries Container of query ids and start/stop locations [in]
/// @param mask All masking locations [in]
/// @param mask_v Vector of per-query lists of mask locations in CSeqLocInfo 
///               form. [out]
NCBI_XBLAST_EXPORT
void 
Blast_GetSeqLocInfoVector(EBlastProgramType program, 
                          const objects::CPacked_seqint& queries,
                          const BlastMaskLoc* mask, 
                          TSeqLocInfoVector& mask_v);

/** Initializes and uninitializes the genetic code singleton as if it was an
 * automatic variable. It also provides MT-safety.*/
class NCBI_XBLAST_EXPORT CAutomaticGenCodeSingleton {
public:
    /// Default constructor
    CAutomaticGenCodeSingleton();
    /// destructor
    ~CAutomaticGenCodeSingleton();
private:
    /// Reference counter for this object so that the genetic code singleton is
    //not deleted prematurely
    static Uint4 m_RefCounter;
};


/** Declares class to handle deallocating of the structure using the appropriate
 * function
 */

#define DECLARE_AUTO_CLASS_WRAPPER(struct_name, free_func)                  \
/** Wrapper class for struct_name. */                                       \
                                                                            \
class NCBI_XBLAST_EXPORT C##struct_name : public CObject                    \
{                                                                           \
public:                                                                     \
    C##struct_name() : m_Ptr(NULL) {}                                       \
    C##struct_name(struct_name* p) : m_Ptr(p) {}                            \
    ~C##struct_name() { Reset(); }                                          \
    void Reset(struct_name* p = NULL) {                                     \
        if (m_Ptr) {                                                        \
            free_func(m_Ptr);                                               \
        }                                                                   \
        m_Ptr = p;                                                          \
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
    virtual void DebugDump(CDebugDumpContext ddc, unsigned int depth) const;\
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
DECLARE_AUTO_CLASS_WRAPPER(PSIDiagnosticsRequest, PSIDiagnosticsRequestFree);
DECLARE_AUTO_CLASS_WRAPPER(PSIDiagnosticsResponse, PSIDiagnosticsResponseFree);

DECLARE_AUTO_CLASS_WRAPPER(BlastSeqSrc, BlastSeqSrcFree);
DECLARE_AUTO_CLASS_WRAPPER(BlastSeqSrcIterator, BlastSeqSrcIteratorFree);
DECLARE_AUTO_CLASS_WRAPPER(Blast_Message, Blast_MessageFree);

DECLARE_AUTO_CLASS_WRAPPER(BlastMaskLoc, BlastMaskLocFree);
DECLARE_AUTO_CLASS_WRAPPER(BlastSeqLoc, BlastSeqLocFree);

DECLARE_AUTO_CLASS_WRAPPER(SBlastProgress, SBlastProgressFree);

#endif /* SKIP_DOXYGEN_PROCESSING */

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

#endif  /* ALGO_BLAST_API___BLAST_AUX__HPP */
