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
 * Auxiliary setup functions for Blast objects interface
 */

#ifndef ALGO_BLAST_API___BLAST_SETUP__HPP
#define ALGO_BLAST_API___BLAST_SETUP__HPP

#include <algo/blast/api/blast_types.hpp>
#include <algo/blast/api/blast_aux.hpp>
#include <algo/blast/core/blast_options.h>
#include <algo/blast/api/blast_exception.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)
    class CSeq_loc;
    class CScope;
END_SCOPE(objects)

BEGIN_SCOPE(blast)
class CBlastOptions;

/** Allocates the query information structure and fills the context 
 * offsets, in case of multiple queries, frames or strands. If query seqids
 * cannot be resolved, they will be ignored as warnings will be issued in
 * blast::SetupQueries.
 * NB: effective length will be assigned inside the engine.
 * @param queries Vector of query locations [in]
 * @param options BLAST search options [in]
 * @param qinfo Allocated query info structure [out]
 */
void
SetupQueryInfo(const TSeqLocVector& queries, const CBlastOptions& options, 
               BlastQueryInfo** qinfo); // out

/// Populates BLAST_SequenceBlk with sequence data for use in CORE BLAST
/// @param queries vector of blast::SSeqLoc structures [in]
/// @param options BLAST options object [in]
/// @param qinfo BlastQueryInfo structure to obtain context information [in]
/// @param seqblk Structure to save sequence data, allocated in this 
/// function [out]
/// @param blast_msg Structure to save warnings/errors, allocated in this
/// function [out]
void
SetupQueries(const TSeqLocVector& queries, const CBlastOptions& options,
             const CBlastQueryInfo& qinfo, BLAST_SequenceBlk** seqblk,
             Blast_Message** blast_msg);

/** Sets up internal subject data structure for the BLAST search.
 * @param subjects Vector of subject locations [in]
 * @param program BLAST program [in]
 * @param seqblk_vec Vector of subject sequence data structures [out]
 * @param max_subjlen Maximal length of the subject sequences [out]
 */
void
SetupSubjects(const TSeqLocVector& subjects, 
              EProgram program,
              vector<BLAST_SequenceBlk*>* seqblk_vec, 
              unsigned int* max_subjlen);

/// Structure to store sequence data and its length for use in the CORE
/// of BLAST (it's a malloc'ed array of Uint1 and its length)
/// FIXME: do not confuse with blast_seg.c's SSequence
struct SBlastSequence {
    TAutoUint1Ptr   data;       /**< Sequence data */
    TSeqPos         length;     /**< Length of the buffer above (not
                                  necessarily sequence length!) */

    /** Default constructor */
    SBlastSequence()
        : data(NULL), length(0) {}

    /** Parametrized constructor 
     * @param d buffer containing sequence data [in]
     * @param l length of buffer above [in]
     */
    SBlastSequence(Uint1* d, TSeqPos l)
        : data(d), length(l) {}
};

/// Allows specification of whether sentinel bytes should be used or not
enum ESentinelType {
    eSentinels,
    eNoSentinels
};

/** Retrieves a sequence using the object manager.
 * @param sl seqloc of the sequence to obtain [in]
 * @param encoding encoding for the sequence retrieved.
 *        Supported encodings include: NCBI2NA_ENCODING, NCBI4NA_ENCODING,
 *        BLASTNA_ENCODING, and BLASTP_ENCODING. [in]
 * @param scope Scope from which the sequences are retrieved [in]
 * @param strand strand to retrieve (applies to nucleotide only).
 *        N.B.: When requesting the NCBI2NA_ENCODING, only the plus strand
 *        is retrieved, because BLAST only requires one strand on the subject
 *        sequences (as in BLAST databases). [in]
 * @param sentinel Use eSentinels to guard nucleotide sequence with sentinel 
 *        bytes (ignored for protein sequences, which always have sentinels) 
 *        When using NCBI2NA_ENCODING, this argument should be set to
 *        eNoSentinels as a sentinel byte cannot be represented in this 
 *        encoding. [in]
 * @param warnings Used to emit warnings when fetching sequence (e.g.:
 *        replacement of invalid characters). Parameter must be allocated by
 *        caller of this function and warnings will be appended. [out]
 * @throws CBlastException, CSeqVectorException, CException
 * @return pair containing the buffer and its length. 
 */
SBlastSequence
GetSequence(const objects::CSeq_loc& sl, Uint1 encoding, 
            objects::CScope* scope,
            objects::ENa_strand strand = objects::eNa_strand_plus, 
            ESentinelType sentinel = eSentinels,
            std::string* warnings = NULL);

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
 */
TSeqPos
CalculateSeqBufferLength(TSeqPos sequence_length, Uint1 encoding,
                         objects::ENa_strand strand =
                         objects::eNa_strand_unknown,
                         ESentinelType sentinel = eSentinels)
                         THROWS((CBlastException));

/** Convenience function to centralize the knowledge of which sentinel bytes we
 * use for supported encodings. Note that only BLASTP_ENCODING,
 * BLASTNA_ENCODING, and NCBI4NA_ENCODING support sentinel bytes, any other
 * values for encoding will cause an exception to be thrown.
 * @param encoding Encoding for which a sentinel byte is needed [in]
 * @return sentinel byte
 */
Uint1 GetSentinelByte(Uint1 encoding) THROWS((CBlastException));

#if 0
// not used right now
/** Translates nucleotide query sequences to protein in the requested frame
 * @param nucl_seq forward (plus) strand of the nucleotide sequence [in]
 * @param nucl_seq_rev reverse (minus) strand of the nucleotide sequence [in]
 * @param nucl_length length of a single strand of the nucleotide sequence [in]
 * @param frame frame to translate, allowed values: 1,2,3,-1,-2,-3 [in]
 * @param translation buffer to hold the translation, should be allocated
 * outside this function [out]
 */
void
BLASTGetTranslation(const Uint1* nucl_seq, const Uint1* nucl_seq_rev, 
                    const int nucl_length, const short frame, Uint1* translation);
#endif

/** Returns the path (including a trailing path separator) to the location
 * where the matrix can be found.
 * @param matrix_name matrix to search for
 * @param is_prot true if this is a protein matrix
 */
string
FindMatrixPath(const char* matrix_name, bool is_prot);

/** Returns the path (including a trailing path separator) to the location
 * where the BLAST database can be found.
 * @param dbname Database to search for
 * @param is_prot true if this is a protein matrix
 */
string
FindBlastDbPath(const char* dbname, bool is_prot);

/** Returns the number of frames for a given BLAST program
 * @param p program 
 */
unsigned int 
GetNumberOfFrames(EProgram p);


/// Returns the encoding for the sequence data used in BLAST for the query
/// @param program program type [in]
/// @throws CBlastException in case of unsupported program
Uint1
GetQueryEncoding(EProgram program);

/// Returns the encoding for the sequence data used in BLAST2Sequences for 
/// the subject
/// @param program program type [in]
/// @throws CBlastException in case of unsupported program
Uint1
GetSubjectEncoding(EProgram program);

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

/*
* ===========================================================================
*
* $Log$
* Revision 1.34  2005/03/04 16:53:27  camacho
* more doxygen fixes
*
* Revision 1.33  2005/03/04 16:07:05  camacho
* doxygen fixes
*
* Revision 1.32  2005/03/02 14:25:58  camacho
* Removed unneeded NCBI_XBLAST_EXPORT
*
* Revision 1.31  2005/03/01 20:52:42  camacho
* Doxygen fixes
*
* Revision 1.30  2005/01/21 17:38:36  camacho
* Update documentation
*
* Revision 1.29  2005/01/06 16:07:55  camacho
* + warnings output parameter to blast::GetSequence
*
* Revision 1.28  2005/01/06 15:41:20  camacho
* Add Blast_Message output parameter to SetupQueries
*
* Revision 1.27  2004/12/28 16:47:43  camacho
* 1. Use typedefs to AutoPtr consistently
* 2. Remove exception specification from blast::SetupQueries
* 3. Use SBlastSequence structure instead of std::pair as return value to
*    blast::GetSequence
*
* Revision 1.26  2004/08/16 19:48:30  dondosha
* Removed duplicate declaration of GetProgramFromBlastProgramType
*
* Revision 1.25  2004/08/11 14:24:50  camacho
* Move FindGeneticCode
*
* Revision 1.24  2004/08/11 11:56:48  ivanov
* Added export specifier NCBI_XBLAST_EXPORT
*
* Revision 1.23  2004/07/08 20:19:29  gorelenk
* Temporary added export spec to GetProgramFromBlastProgramType
*
* Revision 1.22  2004/07/06 15:49:30  dondosha
* Added GetProgramFromBlastProgramType function
*
* Revision 1.21  2004/05/19 14:52:01  camacho
* 1. Added doxygen tags to enable doxygen processing of algo/blast/core
* 2. Standardized copyright, CVS $Id string, $Log and rcsid formatting and i
*    location
* 3. Added use of @todo doxygen keyword
*
* Revision 1.20  2004/04/06 20:45:28  dondosha
* Added function FindBlastDbPath: should be moved to seqdb library
*
* Revision 1.19  2004/03/15 19:57:52  dondosha
* SetupSubjects takes just program argument instead of CBlastOptions*
*
* Revision 1.18  2004/03/06 00:39:47  camacho
* Some refactorings, changed boolen parameter to enum in GetSequence
*
* Revision 1.17  2003/12/29 17:03:47  camacho
* Added documentation to GetSequence
*
* Revision 1.16  2003/10/29 04:45:58  camacho
* Use fixed AutoPtr for GetSequence return value
*
* Revision 1.15  2003/09/12 17:52:42  camacho
* Stop using pair<> as return value from GetSequence
*
* Revision 1.14  2003/09/11 20:55:01  camacho
* Temporary fix for AutoPtr return value
*
* Revision 1.13  2003/09/11 17:45:03  camacho
* Changed CBlastOption -> CBlastOptions
*
* Revision 1.12  2003/09/10 04:25:28  camacho
* Minor change to return type of GetSequence
*
* Revision 1.11  2003/09/09 15:57:23  camacho
* Fix indentation
*
* Revision 1.10  2003/09/09 12:57:15  camacho
* + internal setup functions, use smart pointers to handle memory mgmt
*
* Revision 1.9  2003/08/28 22:43:02  camacho
* Change BLASTGetSequence signature
*
* Revision 1.8  2003/08/19 13:45:21  dicuccio
* Removed 'USING_SCOPE(objects)'.  Changed #include guards to be standards
* compliant.  Added 'objects::' where necessary.
*
* Revision 1.7  2003/08/18 20:58:56  camacho
* Added blast namespace, removed *__.hpp includes
*
* Revision 1.6  2003/08/11 19:55:04  camacho
* Early commit to support query concatenation and the use of multiple scopes.
* Compiles, but still needs work.
*
* Revision 1.5  2003/08/11 13:58:51  dicuccio
* Added export specifiers.  Fixed problem with unimplemented private copy ctor
* (truly make unimplemented)
*
* Revision 1.4  2003/08/01 22:35:02  camacho
* Added function to get matrix path (fixme)
*
* Revision 1.3  2003/07/30 15:00:01  camacho
* Do not use Malloc/MemNew/MemFree
*
* Revision 1.2  2003/07/23 21:29:06  camacho
* Update BLASTFindGeneticCode to get genetic code string with C++ toolkit
*
* Revision 1.1  2003/07/10 18:34:19  camacho
* Initial revision
*
*
* ===========================================================================
*/

#endif  /* ALGO_BLAST_API___BLAST_SETUP__HPP */
