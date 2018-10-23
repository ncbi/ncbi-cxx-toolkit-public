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
 */

/** @file blast_types.hpp
 * Definitions of special type used in BLAST
 */

#ifndef ALGO_BLAST_API___BLAST_TYPE__HPP
#define ALGO_BLAST_API___BLAST_TYPE__HPP

#include <corelib/ncbistd.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <algo/blast/core/blast_export.h>
#include <algo/blast/core/blast_message.h>
#include <algo/blast/core/blast_def.h>
#include <algo/blast/core/blast_filter.h>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/// This enumeration is to evolve into a task/program specific list that 
/// specifies sets of default parameters to easily conduct searches using
/// BLAST.
/// @todo EProgram needs to be renamed to denote a task (similar to those
/// exposed by the BLAST web page) rather than a program type
/// N.B.: When making changes to this enumeration, please update 
/// blast::ProgramNameToEnum (blast_aux.[ch]pp), blast::GetNumberOfFrames
/// (blast_setup_cxx.cpp) and BlastNumber2Program and BlastProgram2Number
/// (blast_util.c)
enum EProgram {
    eBlastNotSet = 0,   ///< Not yet set.
    eBlastn,            ///< Nucl-Nucl (traditional blastn)
    eBlastp,            ///< Protein-Protein
    eBlastx,            ///< Translated nucl-Protein
    eTblastn,           ///< Protein-Translated nucl
    eTblastx,           ///< Translated nucl-Translated nucl
    eRPSBlast,          ///< protein-pssm (reverse-position-specific BLAST)
    eRPSTblastn,        ///< nucleotide-pssm (RPS blast with translated query)
    eMegablast,         ///< Nucl-Nucl (traditional megablast)
    eDiscMegablast,     ///< Nucl-Nucl using discontiguous megablast
    ePSIBlast,          ///< PSI Blast
    ePSITblastn,        ///< PSI Tblastn
    ePHIBlastp,         ///< Protein PHI BLAST
    ePHIBlastn,         ///< Nucleotide PHI BLAST
    eDeltaBlast,        ///< Delta Blast
    eVecScreen,         ///< Vector screening
    eMapper,            ///< Jumper alignment for mapping
    eKBlastp,            ///< KMER screening and BLASTP
    eBlastProgramMax    ///< Undefined program
};

/** Convert a EProgram enumeration value to a task name (as those used in the
 * BLAST command line binaries)
 * @param p EProgram enumeration value to convert [in]
 */
NCBI_XBLAST_EXPORT 
string EProgramToTaskName(EProgram p);

/// Map a string into an element of the ncbi::blast::EProgram enumeration 
/// (except eBlastProgramMax).
/// @param program_name [in]
/// @return an element of the ncbi::blast::EProgram enumeration, except
/// eBlastProgramMax
/// @throws CBlastException if the string does not map into any of the EProgram
/// elements
NCBI_XBLAST_EXPORT
EProgram ProgramNameToEnum(const std::string& program_name);

/// Validates that the task provided is indeed a valid task, otherwise throws a
/// CBlastException
/// @param task task name to validate [in]
NCBI_XBLAST_EXPORT
void ThrowIfInvalidTask(const string& task);

/// Convert EProgram to EBlastProgramType.
/// @param p Program expressed as an api layer EProgram.
/// @return Same program using the core enumeration.
NCBI_XBLAST_EXPORT
EBlastProgramType
EProgramToEBlastProgramType(EProgram p);

/// Error or Warning Message from search.
/// 
/// This class encapsulates a single error or warning message returned
/// from a search.  These include conditions detected by the algorithm
/// where no exception is thrown, but which impact the completeness or
/// accuracy of search results.  One example might be a completely
/// masked query.

class CSearchMessage : public CObject {
public:
    /// Construct a search message object.
    /// @param severity The severity of this message. [in]
    /// @param error_id A number unique to this error. [in]
    /// @param message A description of the error for the user. [in]
    CSearchMessage(EBlastSeverity   severity,
                   int              error_id,
                   const string   & message)
        : m_Severity(severity), m_ErrorId(error_id), m_Message(message)
    {
    }
    
    /// Construct an empty search message object.
    CSearchMessage()
        : m_Severity(EBlastSeverity(0)), m_ErrorId(0)
    {
    }
    
    /// Get the severity of this message.
    /// @return The severity of this message.
    EBlastSeverity GetSeverity() const
    {
        return m_Severity;
    }

    /// Adjust the severity of this message.
    /// @param sev The severity to assign. [in]
    void SetSeverity(EBlastSeverity sev) { m_Severity = sev; }
    
    /// Get the severity of this message as a string.
    /// @return A symbolic name for the severity level (such as "Warning").
    string GetSeverityString() const
    {
        return GetSeverityString(m_Severity);
    }
    
    /// Get the symbolic name for a level of severity as a string.
    /// @param severity The severity as an enumeration.
    /// @return A symbolic name for the severity level (such as "Warning").
    static string GetSeverityString(EBlastSeverity severity)
    {
        switch(severity) {
        case eBlastSevInfo:    return "Informational Message";
        case eBlastSevWarning: return "Warning";
        case eBlastSevError:   return "Error";
        case eBlastSevFatal:   return "Fatal Error";
        }
        return "Message";
    }
    
    /// Get the error identifier.
    /// @return An identifier unique to this specific message.
    int GetErrorId() const
    {
        return m_ErrorId;
    }

    /// Set the error message.
    /// @return A reference allowing the user to set the error string.
    string& SetMessage(void) { return m_Message; }
    
    /// Get the error message.
    /// @return A message describing this error or warning.
    string GetMessage(bool withSeverity = true) const
    {
    	if (withSeverity) {
    		return GetSeverityString() + ": " + m_Message;
    	}
   		return m_Message;
    }

    /// Compare two error messages for equality.
    /// @return True if the messages are the same.
    bool operator==(const CSearchMessage& rhs) const;

    /// Compare two error messages for inequality.
    /// @return True if the messages are not the same.
    bool operator!=(const CSearchMessage& rhs) const;

    /// Compare two error messages for order.
    /// @return True if the first message is less than the second.
    bool operator<(const CSearchMessage& rhs) const;
    
private:
    /// The severity of this error or warning message.
    EBlastSeverity m_Severity;
    
    /// A unique identifier specifying what kind of error this is.
    int            m_ErrorId;
    
    /// A message describing the error to the application user.
    string         m_Message;
};

/// Class for the messages for an individual query sequence.
class NCBI_XBLAST_EXPORT TQueryMessages : public vector< CRef<CSearchMessage> >
{
public:
    /// Set the query id as a string.
    /// @param id The query id.
    void SetQueryId(const string& id);

    /// Get the query id as a string.
    /// @return The query id.
    string GetQueryId() const;

    /// Combine other messages with these.
    /// @param other The second list of messages.
    void Combine(const TQueryMessages& other);

private:
    /// The query identifier.
    string m_IdString;
};

/// typedef for the messages for an entire BLAST search, which could be
/// comprised of multiple query sequences
class NCBI_XBLAST_EXPORT TSearchMessages : public vector<TQueryMessages>
{
public:
    /// Add a message for all queries.
    /// @param severity The severity of this message. [in]
    /// @param error_id A number unique to this error. [in]
    /// @param message A description of the error for the user. [in]
    void AddMessageAllQueries(EBlastSeverity   severity,
                              int              error_id,
                              const string   & message);
    
    /// @return true if messages exist.
    bool HasMessages() const;
    
    /// Converts messages to a string, which is returned.
    /// @return A string containing all such messages.
    string ToString() const;
    
    /// Combine another set of search messages with this one.
    ///
    /// Another set of messages is combined with these; each element
    /// of the other set is combined with the element of this set
    /// having the same index.  The size of both sets must match.
    ///
    /// @param other_msgs Other messages to add to these.
    void Combine(const TSearchMessages& other_msgs);
    
    /// Find and remove redundant messages.
    void RemoveDuplicates();
};

/// Specifies the style of Seq-aligns that should be built from the
/// internal BLAST data structures
enum EResultType {
    eDatabaseSearch,    ///< Seq-aligns in the style of a database search
    eSequenceComparison /**< Seq-aligns in the BLAST 2 Sequence style (one
                         alignment per query-subject pair) */
};

/// Vector of Seq-align-sets
typedef vector< CRef<objects::CSeq_align_set> > TSeqAlignVector;

inline bool
CSearchMessage::operator==(const CSearchMessage& rhs) const
{
    if (m_Severity == rhs.m_Severity &&
        m_ErrorId  == rhs.m_ErrorId &&
        m_Message  == rhs.m_Message) {
        return true;
    } else {
        return false;
    }
}

inline bool
CSearchMessage::operator!=(const CSearchMessage& rhs) const
{
    return !(*this == rhs);
}

inline bool
CSearchMessage::operator<(const CSearchMessage& rhs) const
{
    if (m_ErrorId < rhs.m_ErrorId ||
        m_Severity < rhs.m_Severity ||
        m_Message < rhs.m_Message) {
        return true;
    } else {
        return false;
    }
}

/// Wrapper for BlastSeqLoc structure.
class CBlastSeqLocWrap : public CObject
{
    public:

        /// Instance constructor.
        /// @param locs pointer to the object to hold
        CBlastSeqLocWrap( BlastSeqLoc * locs ) : locs_( locs ) {}

        /// Instance destructor.
        virtual ~CBlastSeqLocWrap() { BlastSeqLocFree( locs_ ); }

        /// Get access to the held object.
        /// @return pointer storred by the wrapping object
        BlastSeqLoc * getLocs() const { return locs_; }

    private:

        BlastSeqLoc * locs_;    ///< Wrapped pointer.
};

END_SCOPE(blast)
END_NCBI_SCOPE

#endif  /* ALGO_BLAST_API___BLAST_TYPE__HPP */
