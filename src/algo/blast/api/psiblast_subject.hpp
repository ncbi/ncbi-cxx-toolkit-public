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

/// @file psiblast_subject.hpp
/// Declares IPsiBlastSubject interface

#ifndef ALGO_BLAST_API___PSIBLAST_SUBJECT__HPP
#define ALGO_BLAST_API___PSIBLAST_SUBJECT__HPP

#include <algo/blast/api/uniform_search.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

/////////////////////////////////////////////////////////////////////////////
// Forward declarations

struct BlastSeqSrc;

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

class IBlastSeqInfoSrc;

/// Interface to obtain subject data for PSI-BLAST search: BlastSeqSrc to 
/// provide sequence data and IBlastSeqInfoSrc to provide sequence id 
/// information. This interface caches the return value of x_MakeSeqSrc and 
/// x_MakeSeqInfoSrc so that the appropriate resource is retrieved/created 
/// only once.
class NCBI_XBLAST_EXPORT IPsiBlastSubject : public CObject
{
public:
    /// Derived classes' constructors shouldn't throw exceptions either, these
    /// should come from the Validate method
    IPsiBlastSubject() THROWS_NONE;

    /// Our virtual destructor
    virtual ~IPsiBlastSubject();

    /// Retrieves or constructs the BlastSeqSrc
    virtual BlastSeqSrc* MakeSeqSrc();

    /// This method should be called so that if the implementation has an
    /// internal "bookmark" of the chunks of the database it has assigned to
    /// different threads, this can be reset at the start of a PSI-BLAST 
    /// iteration. This method should be called before calling MakeSeqSrc() to
    /// ensure proper action on retrieving the BlastSeqSrc (in some cases it
    /// might cause a re-construction of the underlying BlastSeqSrc
    /// implementation). Since this might not apply in some cases, an empty 
    /// default implementation is provided.
    virtual void ResetBlastSeqSrcIteration() {}

    /// Retrieves or constructs the IBlastSeqInfoSrc
    virtual IBlastSeqInfoSrc* MakeSeqInfoSrc();

    /// Method to validate the input data (subclasses should perform
    /// validataion by overriding this method)
    virtual void Validate() {};

protected:
    /// Pointer to the BlastSeqSrc
    BlastSeqSrc* m_SeqSrc;

    /// True if the above resource is owned by this object
    bool m_OwnSeqSrc;

    /// Pointer to the IBlastSeqInfoSrc
    IBlastSeqInfoSrc* m_SeqInfoSrc;

    /// True if the above resource is owned by this object
    bool m_OwnSeqInfoSrc;

    /// This method actually retrieves or constructs the BlastSeqSrc
    virtual void x_MakeSeqSrc() = 0;

    /// This method actually retrieves or constructs the IBlastSeqInfoSrc
    virtual void x_MakeSeqInfoSrc() = 0;
};

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

#endif  /* ALGO_BLAST_API___PSIBLAST_SUBJECT__HPP */
