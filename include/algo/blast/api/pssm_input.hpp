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
#include <util/math/matrix.hpp>
#include <objects/seq/Bioseq.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/// Base class for the IPssmInputData and IPssmInputFreqRatios interfaces,
/// provided to avoid duplicating the methods that are common to both
/// interfaces
struct IPssmInput_Base : public CObject
{
    /// virtual destructor
    virtual ~IPssmInput_Base() {}

    /// Get the query sequence used as master for the multiple sequence
    /// alignment in ncbistdaa encoding.
    virtual unsigned char* GetQuery() = 0;

    /// Get the query's length
    virtual unsigned int GetQueryLength() = 0;

    /// Obtain the name of the underlying matrix to use when building the PSSM
    virtual const char* GetMatrixName() {
        return BLAST_DEFAULT_MATRIX;
    }

    /// Obtain the gap existence value for the underlying matrix used to build the PSSM.
    virtual int GetGapExistence() {
        return BLAST_GAP_OPEN_PROT;
    }

    /// Obtain the gap extension value for the underlying matrix used to build the PSSM.
    virtual int GetGapExtension() {
        return BLAST_GAP_EXTN_PROT;
    }

    /// Get a CBioseq object for attachment into the CPssmWithParameters
    /// that CPssmEngine produces (only attached if it's not NULL). This is
    /// required for any PSSM which is intended to be used as a starting point
    /// for a PSI-BLAST iteration
    virtual CRef<objects::CBioseq> GetQueryForPssm() {
        return CRef<objects::CBioseq>(NULL);
    }
};

/// Abstract base class to encapsulate the source(s) and pre-processing of 
/// PSSM input data as well as options to the PSI-BLAST PSSM engine.
///
/// This interface represents the strategy to pre-process PSSM input data and
/// to provide to the PSSM engine (context) the multiple sequence alignment 
/// structure and options that it can use to build the PSSM.
/// This class is meant to provide a uniform interface that the PSSM engine can
/// use to obtain its data to create a PSSM, allowing subclasses to provide
/// implementations to obtain this data from disparate sources (e.g.:
/// Seq-aligns, Cdd models, multiple sequence alignments, etc).
/// @note Might need to add the PSIDiagnosticsRequest structure
/// @sa CPsiBlastInputData
struct IPssmInputData : public IPssmInput_Base
{
    /// virtual destructor
    virtual ~IPssmInputData() {}

    /// Algorithm to produce multiple sequence alignment structure should be
    /// implemented in this method. This will be invoked by the CPssmEngine
    /// object before calling GetData()
    virtual void Process() = 0;

    /// Obtain the multiple sequence alignment structure
    virtual PSIMsa* GetData() = 0;

    /// Obtain the options for the PSSM engine
    virtual const PSIBlastOptions* GetOptions() = 0;

    /// Obtain the diagnostics data that is requested from the PSSM engine
    /// Its results will be populated in the PssmWithParameters ASN.1 object
    virtual const PSIDiagnosticsRequest* GetDiagnosticsRequest() {
        return NULL;    // default is not requesting any diagnostics
    }
};

/// Interface used to retrieve the PSSM frequency ratios to allow for "restart"
/// processing in PSI-BLAST: Given a preliminary
struct IPssmInputFreqRatios : public IPssmInput_Base
{
    /// virtual destructor
    virtual ~IPssmInputFreqRatios() {}

    /// Algorithm to produce the PSSM's frequecy ratios should be
    /// implemented in this method. This will be invoked by the CPssmEngine
    /// object before calling GetData()
    virtual void Process() = 0;

    /// Obtain a matrix of frequency ratios with this->GetQueryLength() columns
    /// and BLASTAA_SIZE rows
    virtual const CNcbiMatrix<double>& GetData() = 0;
};

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

#endif  /* ALGO_BLAST_API__PSSM_INPUT_HPP */
