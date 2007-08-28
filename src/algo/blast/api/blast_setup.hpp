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

/** @file blast_setup.hpp
 * Internal auxiliary setup classes/functions for C++ BLAST APIs.
 * These facilities are free of any dependencies on the NCBI C++ object
 * manager.
 */

#ifndef ALGO_BLAST_API___BLAST_SETUP__HPP
#define ALGO_BLAST_API___BLAST_SETUP__HPP

#include <algo/blast/api/blast_aux.hpp>
#include <algo/blast/core/blast_options.h>
#include <algo/blast/api/blast_exception.hpp>
#include <algo/blast/api/blast_types.hpp>

// Object includes
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Na_strand.hpp>
#include <objects/seq/Seq_data.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(blast)
class CBlastOptions;

/// Structure to store sequence data and its length for use in the CORE
/// of BLAST (it's a malloc'ed array of Uint1 and its length)
/// FIXME: do not confuse with blast_seg.c's SSequence
struct NCBI_XBLAST_EXPORT SBlastSequence {
    // AutoPtr<Uint1, CDeleter<Uint1> > == TAutoUint1Ptr
    TAutoUint1Ptr   data;       /**< Sequence data */
    TSeqPos         length;     /**< Length of the buffer above (not
                                  necessarily sequence length!) */

    /** Default constructor */
    SBlastSequence()
        : data(NULL), length(0) {}

    /** Allocates a sequence buffer of the specified length
     * @param buf_len number of bytes to allocate [in]
     */
    SBlastSequence(TSeqPos buf_len)
        : data((Uint1*)calloc(buf_len, sizeof(Uint1))), length(buf_len)
    {
        if ( !data ) {
            NCBI_THROW(CBlastSystemException, eOutOfMemory, 
               "Failed to allocate " + NStr::IntToString(buf_len) + " bytes");
        }
    }

    /** Parametrized constructor 
     * @param d buffer containing sequence data [in]
     * @param l length of buffer above [in]
     */
    SBlastSequence(Uint1* d, TSeqPos l)
        : data(d), length(l) {}
};

/// Allows specification of whether sentinel bytes should be used or not
enum ESentinelType {
    eSentinels,         ///< Use sentinel bytes
    eNoSentinels        ///< Do not use sentinel bytes
};

/// Lightweight wrapper around an indexed sequence container. These sequences
/// are then used to set up internal BLAST data structures for sequence data
class NCBI_XBLAST_EXPORT IBlastQuerySource : public CObject 
{
public:
    /// Our no-op virtual destructor
    virtual ~IBlastQuerySource() {}
    
    /// Return strand for a sequence
    /// @param index index of the sequence in the sequence container [in]
    virtual objects::ENa_strand GetStrand(int index) const = 0;
    
    /// Return the number of elements in the sequence container
    virtual TSeqPos Size() const = 0;

    /// Returns true if the container is empty, else false
    bool Empty() const { return (Size() == 0); }
    
    /// Return the filtered (masked) regions for a sequence
    /// @param index index of the sequence in the sequence container [in]
    virtual CConstRef<objects::CSeq_loc> GetMask(int index) = 0;
    
    /// Return the filtered (masked) regions for a sequence
    /// @param index index of the sequence in the sequence container [in]
    virtual TMaskedQueryRegions GetMaskedRegions(int index) = 0;
    
    /// Return the CSeq_loc associated with a sequence
    /// @param index index of the sequence in the sequence container [in]
    virtual CConstRef<objects::CSeq_loc> GetSeqLoc(int index) const = 0;

    /// Return the sequence identifier associated with a sequence
    /// @param index index of the sequence in the sequence container [in]
    virtual const objects::CSeq_id* GetSeqId(int index) const = 0;

    /// Retrieve the genetic code associated with a sequence
    /// @param index index of the sequence in the sequence container [in]
    virtual Uint4 GetGeneticCodeId(int index) const = 0;
    
    /// Return the sequence data for a sequence
    /// @param index index of the sequence in the sequence container [in]
    /// @param encoding desired encoding [in]
    /// @param strand strand to fetch [in]
    /// @param sentinel specifies to use or not to use sentinel bytes around
    ///        sequence data. Note that this is ignored for proteins, as in the
    ///        CORE of BLAST, proteins always have sentinel bytes [in]
    /// @param warnings if not NULL, warnings will be returned in this string
    ///        [in|out]
    /// @return SBlastSequence structure containing sequence data requested
    virtual SBlastSequence
    GetBlastSequence(int index, EBlastEncoding encoding, 
                     objects::ENa_strand strand, ESentinelType sentinel, 
                     std::string* warnings = 0) const = 0;
    
    /// Return the length of a sequence
    /// @param index index of the sequence in the sequence container [in]
    virtual TSeqPos GetLength(int index) const = 0;
};

/// Choose between a Seq-loc specified query strand and the strand obtained
/// from the CBlastOptions
/// @param query_seqloc Seq-loc corresponding to a given query sequence [in]
/// @param program program type from the CORE's point of view [in]
/// @param strand_option strand as specified by the BLAST options [in]
NCBI_XBLAST_EXPORT
objects::ENa_strand
BlastSetup_GetStrand(const objects::CSeq_loc& query_seqloc,
                     EBlastProgramType program,
                     objects::ENa_strand strand_option);

/// Lightweight wrapper around sequence data which provides a CSeqVector-like
/// interface to the data
class NCBI_XBLAST_EXPORT IBlastSeqVector {
public:
    /// Our no-op virtual destructor
    virtual ~IBlastSeqVector() {}

    /// Sets the encoding for the sequence data.
    /// Two encodings are really necessary: ncbistdaa and ncbi4na, both use 1
    /// byte per residue/base
    virtual void SetCoding(objects::CSeq_data::E_Choice coding) = 0;
    /// Returns the length of the sequence data (in the case of nucleotides,
    /// only one strand)
    /// @throws CBlastException if the size returned is 0
    TSeqPos size() const {
        TSeqPos retval = x_Size();
        if (retval == 0) {
            NCBI_THROW(CBlastException, eInvalidArgument, 
                       "Sequence with 0 length");
        }
        return retval;
    }
    /// Allows index-based access to the sequence data
    virtual Uint1 operator[] (TSeqPos pos) const = 0;

    /// For nucleotide sequences this instructs the implementation to convert
    /// its representation to be that of the plus strand
    void SetPlusStrand() {
        x_SetPlusStrand();
        m_Strand = objects::eNa_strand_plus;
    }
    /// For nucleotide sequences this instructs the implementation to convert
    /// its representation to be that of the minus strand
    void SetMinusStrand() {
        x_SetMinusStrand();
        m_Strand = objects::eNa_strand_minus;
    }
    /// Accessor for the strand currently set
    objects::ENa_strand GetStrand() const {
        return m_Strand;
    }
    /// Returns the compressed nucleotide data for the plus strand, still
    /// occupying one base per byte.
    virtual SBlastSequence GetCompressedPlusStrand() = 0;

protected:
    /// Method which retrieves the size of the sequence vector, as described in
    /// the size() method above
    virtual TSeqPos x_Size() const = 0;
    /// Method which does the work for setting the plus strand of the 
    /// nucleotide sequence data
    virtual void x_SetPlusStrand() = 0;
    /// Method which does the work for setting the minus strand of the 
    /// nucleotide sequence data
    virtual void x_SetMinusStrand() = 0;

    /// Maintains the state of the strand currently saved by the implementation
    /// of this class
    objects::ENa_strand m_Strand;
};

/** ObjMgr Free version of SetupQueryInfo.
 * NB: effective length will be assigned inside the engine.
 * @param queries Vector of query locations [in]
 * @param prog program type from the CORE's point of view [in]
 * @param strand_opt Unless the strand option is set to single strand, the 
 * actual CSeq_locs in the TSeqLocVector dictacte which strand to use
 * during the search [in]
 * @param qinfo Allocated query info structure [out]
 */
NCBI_XBLAST_EXPORT
void
SetupQueryInfo_OMF(const IBlastQuerySource& queries,
                   EBlastProgramType prog,
                   objects::ENa_strand strand_opt,
                   BlastQueryInfo** qinfo);

/// ObjMgr Free version of SetupQueries.
/// @param queries vector of blast::SSeqLoc structures [in]
/// @param qinfo BlastQueryInfo structure to obtain context information [in]
/// @param seqblk Structure to save sequence data, allocated in this 
/// function [out]
/// @param messages object to save warnings/errors for all queries [out]
/// @param prog program type from the CORE's point of view [in]
/// @param strand_opt Unless the strand option is set to single strand, the 
/// actual CSeq_locs in the TSeqLocVector dictacte which strand to use
/// during the search [in]

NCBI_XBLAST_EXPORT
void
SetupQueries_OMF(IBlastQuerySource& queries,
                 BlastQueryInfo* qinfo, 
                 BLAST_SequenceBlk** seqblk,
                 EBlastProgramType prog, 
                 objects::ENa_strand strand_opt,
                 TSearchMessages& messages);

/** Object manager free version of SetupSubjects
 * @param subjects Vector of subject locations [in]
 * @param program BLAST program [in]
 * @param seqblk_vec Vector of subject sequence data structures [out]
 * @param max_subjlen Maximal length of the subject sequences [out]
 */
NCBI_XBLAST_EXPORT
void
SetupSubjects_OMF(IBlastQuerySource& subjects,
                  EBlastProgramType program,
                  vector<BLAST_SequenceBlk*>* seqblk_vec,
                  unsigned int* max_subjlen);

/** Object manager free version of GetSequence */
NCBI_XBLAST_EXPORT
SBlastSequence
GetSequence_OMF(IBlastSeqVector& sv, EBlastEncoding encoding, 
            objects::ENa_strand strand, 
            ESentinelType sentinel,
            std::string* warnings = 0);

/** Calculates the length of the buffer to allocate given the desired encoding,
 * strand (if applicable) and use of sentinel bytes around sequence.
 * @param sequence_length Length of the sequence [in]
 * @param encoding Desired encoding for calculation (supported encodings are
 *        listed in GetSequence()) [in]
 * @param strand Which strand to use for calculation [in]
 * @param sentinel Whether to include or not sentinels in calculation. Same
 *        criteria as GetSequence() applies [in]
 * @return Length of the buffer to allocate to contain original sequence of
 *        length sequence_length for given encoding and parameter constraints.
 *        If the sequence_length is 0, the return value will be 0 too
 * @throws CBlastException in case of unsupported encoding
 */
NCBI_XBLAST_EXPORT
TSeqPos
CalculateSeqBufferLength(TSeqPos sequence_length, EBlastEncoding encoding,
                         objects::ENa_strand strand =
                         objects::eNa_strand_unknown,
                         ESentinelType sentinel = eSentinels)
                         THROWS((CBlastException));

/// Compresses the sequence data passed in to the function from 1 base per byte
/// to 4 bases per byte
/// @param source input sequence data in ncbi2na format, with ambiguities
/// randomized [in]
/// @return compressed version of the input
/// @throws CBlastException in case of memory allocation failure
/// @todo use CSeqConvert::Pack?
NCBI_XBLAST_EXPORT
SBlastSequence CompressNcbi2na(const SBlastSequence& source);

/** Convenience function to centralize the knowledge of which sentinel bytes we
 * use for supported encodings. Note that only eBlastEncodingProtein,
 * eBlastEncodingNucleotide, and eBlastEncodingNcbi4na support sentinel bytes, 
 * any other values for encoding will cause an exception to be thrown.
 * @param encoding Encoding for which a sentinel byte is needed [in]
 * @return sentinel byte
 * @throws CBlastException in case of unsupported encoding
 */
NCBI_XBLAST_EXPORT
Uint1 GetSentinelByte(EBlastEncoding encoding) THROWS((CBlastException));

/** Returns the path (including a trailing path separator) to the location
 * where the BLAST database can be found.
 * @param dbname Database to search for
 * @param is_prot true if this is a protein matrix
 */
NCBI_XBLAST_EXPORT
string
FindBlastDbPath(const char* dbname, bool is_prot);

/** Returns the number of contexts for a given BLAST program
 * @sa BLAST_GetNumberOfContexts
 * @param p program 
 */
NCBI_XBLAST_EXPORT
unsigned int 
GetNumberOfContexts(EBlastProgramType p);


/// Returns the encoding for the sequence data used in BLAST for the query
/// @param program program type [in]
/// @throws CBlastException in case of unsupported program
NCBI_XBLAST_EXPORT
EBlastEncoding
GetQueryEncoding(EBlastProgramType program);

/// Returns the encoding for the sequence data used in BLAST2Sequences for 
/// the subject
/// @param program program type [in]
/// @throws CBlastException in case of unsupported program
NCBI_XBLAST_EXPORT
EBlastEncoding
GetSubjectEncoding(EBlastProgramType program);

/// Wrapper around SetupQueries
/// @param queries interface to obtain query data [in]
/// @param options BLAST algorithm options [in]
/// @param query_info BlastQueryInfo structure [in|out]
/// @param messages error/warning messages are returned here [in|out]
NCBI_XBLAST_EXPORT
BLAST_SequenceBlk*
SafeSetupQueries(IBlastQuerySource& queries,
                 const CBlastOptions* options,
                 BlastQueryInfo* query_info,
                 TSearchMessages& messages);

/// Wrapper around SetupQueryInfo
/// @param queries interface to obtain query data [in]
/// @param options BLAST algorithm options [in]
NCBI_XBLAST_EXPORT
BlastQueryInfo*
SafeSetupQueryInfo(const IBlastQuerySource& queries, 
                   const CBlastOptions* options);


/// Returns the path to a specified matrix.  
/// This is the implementation of the GET_MATRIX_PATH callback. 
///
/// @param matrix_name matrix name (e.g., BLOSUM62) [in]
/// @param is_prot matrix is for proteins if TRUE [in]
/// @return path to matrix, should be deallocated by user.
NCBI_XBLAST_EXPORT
char*
BlastFindMatrixPath(const char* matrix_name, Boolean is_prot);

/// Collection of BlastSeqLoc lists for filtering processing.
///
/// This class acts as a container for frame values and collections of
/// BlastSeqLoc objects used by the blast filtering processing code.
/// The support for filtering of blastx searches adds complexity and
/// creates more opportunities for errors to occur.  This class was
/// designed to handle some of that complexity, and guard against some
/// of those possible errors.

class NCBI_XBLAST_EXPORT CBlastQueryFilteredFrames : public CObject {
public:
    /// Data type for frame value, however inputs to methods use "int"
    /// instead of this type for readability and brevity.
    typedef CSeqLocInfo::ETranslationFrame ETranslationFrame;
    
    /// Construct container for frame values and BlastSeqLocs for the
    /// specified search program.
    /// @param program The type of search being done.
    CBlastQueryFilteredFrames(EBlastProgramType program);
    
    /// Construct container for frame values and BlastSeqLocs from a
    /// TMaskedQueryRegions vector.
    /// @param program Search program value used [in]
    /// @param mqr MaskedQueryRegions to convert [in]
    CBlastQueryFilteredFrames(EBlastProgramType           program,
                              const TMaskedQueryRegions & mqr);
    
    /// Destructor; frees any BlastSeqLoc lists not released by the
    /// caller.
    ~CBlastQueryFilteredFrames();
    
    /// Add a masked interval to the specified frame.
    ///
    /// The specified interval of the specified frame is masked.  This
    /// creates a BlastSeqLoc object inside this container for that
    /// frame, which will be freed at destruction time unless the
    /// client code calls Release() for that frame.
    ///
    /// @param intv The interval to mask.
    /// @param frame The specific frame, expressed as a value from ETranslationFrame, on which this interval falls.
    void AddSeqLoc(const objects::CSeq_interval & intv, int frame);
    
    /// Access the BlastSeqLocs for a given frame.
    ///
    /// A pointer is returned to the list of BlastSeqLocs associated
    /// with a given frame.
    /// @param frame The specific frame, expressed as a value from ETranslationFrame, on which this interval falls.
    BlastSeqLoc ** operator[](int frame);
    
    /// Release the BlastSeqLocs for a given frame.
    ///
    /// The given frame is cleared (the data removed) without freeing
    /// the associated objects.  The calling code takes responsibility
    /// for freeing the associated list of objects.
    /// @param frame The specific frame, expressed as a value from ETranslationFrame, on which this interval falls.
    void Release(int frame);
    
    /// Check whether the query is multiframe for this type of search.
    bool QueryHasMultipleFrames() const;
    
    /// Returns the list of frame values for which this object
    /// contains masking information.
    vector<ETranslationFrame> ListFrames() const;
    
    /// Returns true if this object contains any masking information.
    bool Empty() const;
    
    /// Adjusts all stored masks from nucleotide to protein offsets.
    ///
    /// Values stored here must be converted to protein offsets after
    /// a certain stage of processing.  This method only has an effect
    /// for types of searches that need this service (which are those
    /// searches where the query sequence is translated.)  Additional
    /// calls to this method will have no effect.
    ///
    /// @param dna_length The query length in nucleotide bases.
    void UseProteinCoords(TSeqPos dna_length);
    
private:
    /// Prevent copy construction.
    CBlastQueryFilteredFrames(CBlastQueryFilteredFrames & f);

    /// Prevent assignment.
    CBlastQueryFilteredFrames & operator=(CBlastQueryFilteredFrames & f);
    
    /// Verify the specified frame value.
    void x_VerifyFrame(int frame);
    
    /// Returns true if this program needs coordinate translation.
    bool x_NeedsTrans();
    
    /// The type of search being done.
    EBlastProgramType m_Program;
    
    /// Frame and BlastSeqLoc* info type.
    typedef map<ETranslationFrame, BlastSeqLoc*> TFrameSet;
    
    /// Frame and BlastSeqLoc* data.
    TFrameSet m_Seqlocs;
    
    /// True if this object's masked regions store DNA coordinates
    /// that will later be translated into protein coordinates.
    bool m_TranslateCoords;
};


END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

#endif  /* ALGO_BLAST_API___BLAST_SETUP__HPP */
