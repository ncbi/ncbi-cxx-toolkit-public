#ifndef ALGO_BLAST_API__PSSM_INPUT__HPP
#define ALGO_BLAST_API__PSSM_INPUT__HPP

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

/** @file pssm_input.hpp
 * Defines interface for a sequence alignment processor that can populate a
 * multiple alignment data structure used by the PSSM engine.
 */

#include <corelib/ncbistl.hpp>
#include <algo/blast/core/blast_psi.h>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/// Abstract base class to encapsulate the sources and processing of PSSM input
/// data.
///
/// This interface represents the strategy to process PSSM input data and
/// to provide to the PSSM engine (context) the multiple sequence alignment 
/// structure and options that it can use to build the PSSM.
/// This class is meant to provide a uniform interface that the PSSM engine can
/// use to obtain its data to create a PSSM, allowing subclasses to provide
/// implementations to obtain this data from disparate sources (e.g.:
/// Seq-aligns, Cdd models, multiple sequence alignments, etc).
/// @note Might need to add the PSIDiagnosticsRequest structure
/// @sa CPsiBlastInputData
class IPssmInputData
{
public:
    /// Algorithm to produce multiple sequence alignment structure should be
    /// implemented in this method. This will be invoked by the CPssmEngine
    /// object before calling GetData()
    virtual void Process() = 0;

    /// Get the query sequence used as master for the multiple sequence
    /// alignment.
    virtual unsigned char* GetQuery() = 0;

    /// Get the query's length
    virtual unsigned int GetQueryLength() = 0;

    /// Obtain the multiple sequence alignment structure
    virtual PSIMsa* GetData() = 0;

    /// Obtain the options for the PSSM engine
    virtual const PSIBlastOptions* GetOptions() = 0;
};

END_SCOPE(blast)
END_NCBI_SCOPE

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.4  2004/08/04 20:52:37  camacho
 * Documentation changes
 *
 * Revision 1.3  2004/08/04 20:21:07  camacho
 * Renamed multiple sequence alignment data structure
 *
 * Revision 1.2  2004/08/02 13:27:01  camacho
 * + query sequence methods
 *
 * Revision 1.1  2004/07/29 17:53:47  camacho
 * Initial revision
 *
 *
 * ===========================================================================
 */
#endif  /* ALGO_BLAST_API__PSSM_INPUT_HPP */
