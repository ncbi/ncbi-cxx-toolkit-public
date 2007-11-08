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
/// Classes that capture the state of the BLAST options (or subsets of options)
/// and restore them later (usually upon destruction) using the RAII idiom.

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
    friend class CSetupFactory; 
    friend class CPrelimSearchRunner;
    friend class CBlastPrelimSearch;
    friend class CBlastTracebackSearch;
    friend class CFilteringMemento;
    friend class CEffectiveSearchSpaceCalculator;

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

/// Memento class to save, replace out, and restore the effective search space
/// options of the CBlastOptions object passed to its constructor.
/// This is done because the SplitQuery_SetEffectiveSearchSpace function
/// modifies the search spaces in the CBlastOptions object, but this shouldn't
/// be modified, so a temporary object is created and the destroyed.
class CEffectiveSearchSpacesMemento
{
public:
    /// Parametrized constructor
    /// @param options the BLAST options [in]
    CEffectiveSearchSpacesMemento(CBlastOptions* options)
        : m_Options(options), m_EffLenOrig(0), m_EffLenReplace(0)
    {
        _ASSERT(options);
        if (options->m_Local) {
            m_EffLenOrig = options->m_Local->m_EffLenOpts.Release();
            BlastEffectiveLengthsOptionsNew(&m_EffLenReplace);
            memcpy((void*) m_EffLenReplace,
                   (void*) m_EffLenOrig,
                   sizeof(*m_EffLenOrig));
            m_EffLenReplace->searchsp_eff = 
                (Int8*)malloc(sizeof(Int8) * m_EffLenOrig->num_searchspaces);
            memcpy((void*) m_EffLenReplace->searchsp_eff,
                   (void*) m_EffLenOrig->searchsp_eff,
                   sizeof(Int8) * m_EffLenOrig->num_searchspaces);
            options->m_Local->m_EffLenOpts.Reset(m_EffLenReplace);
        }
    }

    /// Destructor
    ~CEffectiveSearchSpacesMemento()
    {
        _ASSERT(m_Options->m_Local);
        m_Options->m_Local->m_EffLenOpts.Reset(m_EffLenOrig);
        m_Options = NULL;
        m_EffLenOrig = m_EffLenReplace = NULL;
    }

private:
    /** Snapshopt of BLAST options */
    CBlastOptions* m_Options;    
    /** Original effective length options */
    BlastEffectiveLengthsOptions* m_EffLenOrig;
    /** Effective length that will be replaced in the BLAST options object */
    BlastEffectiveLengthsOptions* m_EffLenReplace;
};

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

#endif  /* ALGO_BLAST_API__BLAST_MEMENTO_PRIV__HPP */
