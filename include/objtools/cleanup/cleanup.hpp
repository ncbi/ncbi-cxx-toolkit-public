#ifndef CLEANUP___CLEANUP__HPP
#define CLEANUP___CLEANUP__HPP

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
 * Author:  Robert Smith, Michael Kornbluh
 *
 * File Description:
 *   Basic Cleanup of CSeq_entries.
 *   .......
 *
 */
#include <objmgr/scope.hpp>
#include <objtools/cleanup/cleanup_change.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_entry;
class CBioseq;
class CBioseq_set;
class CSeq_annot;
class CSeq_feat;
class CSeq_submit;

class CSeq_entry_Handle;
class CBioseq_Handle;
class CBioseq_set_Handle;
class CSeq_annot_Handle;
class CSeq_feat_Handle;

class CCleanupChange;

class NCBI_CLEANUP_EXPORT CCleanup : public CObject 
{
public:

    enum EValidOptions {
        eClean_NoReporting       = 0x1,
        eClean_GpipeMode         = 0x2,
        eClean_NoNcbiUserObjects = 0x4,
        eClean_SyncGenCodes      = 0x8
    };

    // Construtor / Destructor
    CCleanup(CScope* scope = NULL);
    ~CCleanup();

    void SetScope(CScope* scope);
    
    // BASIC CLEANUP
    
    /// Cleanup a Seq-entry. 
    CConstRef<CCleanupChange> BasicCleanup(CSeq_entry& se,  Uint4 options = 0);
    /// Cleanup a Seq-submit. 
    CConstRef<CCleanupChange> BasicCleanup(CSeq_submit& ss,  Uint4 options = 0);
    /// Cleanup a Bioseq. 
    CConstRef<CCleanupChange> BasicCleanup(CBioseq& bs,     Uint4 ooptions = 0);
    /// Cleanup a Bioseq_set.
    CConstRef<CCleanupChange> BasicCleanup(CBioseq_set& bss, Uint4 options = 0);
    /// Cleanup a Seq-Annot.
    CConstRef<CCleanupChange> BasicCleanup(CSeq_annot& sa,  Uint4 options = 0);
    /// Cleanup a Seq-feat. 
    CConstRef<CCleanupChange> BasicCleanup(CSeq_feat& sf,   Uint4 options = 0);
    /// Cleanup a BioSource. 
    CConstRef<CCleanupChange> BasicCleanup(CBioSource& src,   Uint4 options = 0);

    // Handle versions.
    CConstRef<CCleanupChange> BasicCleanup(CSeq_entry_Handle& seh, Uint4 options = 0);
    CConstRef<CCleanupChange> BasicCleanup(CBioseq_Handle& bsh,    Uint4 options = 0);
    CConstRef<CCleanupChange> BasicCleanup(CBioseq_set_Handle& bssh, Uint4 options = 0);
    CConstRef<CCleanupChange> BasicCleanup(CSeq_annot_Handle& sak, Uint4 options = 0);
    CConstRef<CCleanupChange> BasicCleanup(CSeq_feat_Handle& sfh,  Uint4 options = 0);
    
    // Extended Cleanup
        /// Cleanup a Seq-entry. 
    CConstRef<CCleanupChange> ExtendedCleanup(CSeq_entry& se,  Uint4 options = 0);
    /// Cleanup a Seq-submit. 
    CConstRef<CCleanupChange> ExtendedCleanup(CSeq_submit& ss, Uint4 options = 0);
    /// Cleanup a Seq-Annot.
    CConstRef<CCleanupChange> ExtendedCleanup(CSeq_annot& sa,  Uint4 options = 0);

    // Handle versions
    CConstRef<CCleanupChange> ExtendedCleanup(CSeq_entry_Handle& seh, Uint4 options = 0);

    // Useful cleanup functions

/// Moves protein-specific features from nucleotide sequences in the Seq-entry to
/// the appropriate protein sequence.
/// @param seh Seq-entry Handle to edit [in]
/// @return Boolean return value indicates whether any changes were made
    static bool MoveProteinSpecificFeats(CSeq_entry_Handle seh);


private:
    // Prohibit copy constructor & assignment operator
    CCleanup(const CCleanup&);
    CCleanup& operator= (const CCleanup&);

    CScope*            m_Scope;
};



END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* CLEANUP___CLEANUP__HPP */
