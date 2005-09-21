/* $Id$
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
 * Author:  Christiam Camacho, Kevin Bealer
 *
 */

/// @file blast_memento_priv.hpp
/// Class that allows the transfer of internal BLAST data structures from the
/// preliminary stage to the traceback stage.
/// NOTE: This file contains work in progress and the APIs are likely to change,
/// please do not rely on them until this notice is removed.

#ifndef ALGO_BLAST_API__BLAST_MEMENTO_PRIV__HPP
#define ALGO_BLAST_API__BLAST_MEMENTO_PRIV__HPP

#include <algo/blast/core/blast_gapalign.h>
#include <algo/blast/api/blast_aux.hpp>
#include "blast_options_local_priv.hpp"

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

// Forward declarations
class CBlastOptions;
class CBlastPrelimSearch;
class CBlastTracebackSearch;
class SPrelimTracebackMemento;

/// Class that allows the transfer of data structures from the
/// CBlastOptionsLocal class to either the BLAST preliminary or 
/// traceback search classes.
///
/// It is a modification of the memento design pattern in which the object is
/// not to restore state but rather to control access to private data
/// structures of classes enclosing BLAST CORE C-structures.
class CBlastOptionsMemento : public CObject
{
public:
    /// Note the no-op destructor, this class does not own any of its data
    /// members!
    ~CBlastOptionsMemento() {}

private:
    CBlastOptionsMemento(CBlastOptionsLocal* local_opts)
    {
        m_ProgramType   = local_opts->GetProgramType();
        m_QueryOpts     = local_opts->m_QueryOpts.Get();
        m_LutOpts       = local_opts->m_LutOpts.Get();
        m_InitWordOpts  = local_opts->m_InitWordOpts.Get();
        m_ExtnOpts      = local_opts->m_ExtnOpts.Get();
        m_HitSaveOpts   = local_opts->m_HitSaveOpts.Get();
        m_PSIBlastOpts  = local_opts->m_PSIBlastOpts.Get();
        m_DbOpts        = local_opts->m_DbOpts.Get();
        m_ScoringOpts   = local_opts->m_ScoringOpts.Get();
        m_EffLenOpts    = local_opts->m_EffLenOpts.Get();
    }

    // The originator
    friend class CBlastOptions;

    // Recipients of data 
    friend class CBlastPrelimSearch;
    friend class CPrelimSearchRunner;   // functor that calls CORE BLAST
    friend class CBlastTracebackSearch;

    // this should replace (CPrelimSearch, CBlastTracebackSearch,
    // CBlastPrelimSearchRunner);
    friend class CSetupFactory; 

    // The data that is being shared (not that this object doesn't own these)
    EBlastProgramType m_ProgramType;
    QuerySetUpOptions* m_QueryOpts;
    LookupTableOptions* m_LutOpts;
    BlastInitialWordOptions* m_InitWordOpts;
    BlastExtensionOptions* m_ExtnOpts;
    BlastHitSavingOptions* m_HitSaveOpts;
    PSIBlastOptions* m_PSIBlastOpts;
    BlastDatabaseOptions* m_DbOpts;
    BlastScoringOptions* m_ScoringOpts;
    BlastEffectiveLengthsOptions* m_EffLenOpts;
};


/// Class that allows the transfer of data structures from the preliminary 
/// stage to the traceback stage.
///
/// It is a modification of the memento design pattern in which the object is
/// not to restore state but rather to control access to private data
/// structures of classes enclosing BLAST CORE C-structures.
class CBlastMemento : public CObject
{
public:
    /// Note the no-op destructor, this class does not own any of its data
    /// members!
    ~CBlastMemento() {}

private:

    // The originator
    friend class CBlastPrelimSearch;

    // Recipients of data 
    friend class CBlastTracebackSearch;

    CBlastMemento();

    // The state is represented by the structures below

    // Internal data structures
    BlastGapAlignStruct*        m_GapAlignStruct;
    BlastScoreBlk*              m_ScoreBlk;

    // Options and parameter structures
    CBlastInitialWordParameters m_InitWordParams;
    CBlastHitSavingParameters   m_HitSavingParams;
    CBlastExtensionOptions      m_ExtendWord;
    CBlastExtensionParameters   m_ExtensionParams;
    CBlastDatabaseOptions       m_SubjectOptions;
};

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

#endif  /* ALGO_BLAST_API__BLAST_MEMENTO_PRIV__HPP */
