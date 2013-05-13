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
 * Authors:  Josh Cherry
 *
 * File Description:  Simplified interface to Object Manager
 *
 */

#ifndef OBJTOOLS_SIMPLE___SIMPLE_OM__HPP
#define OBJTOOLS_SIMPLE___SIMPLE_OM__HPP

#include <objects/seq/seq_id_handle.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Na_strand.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/scope.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE

/// This class provides a simplified interface to the Object Manager.
/// It provides functions that return IUPAC sequences, sequence vectors,
/// biosequence handles, and Object Manager scopes in a single step,
/// without any need for explicit set-up of an object manager 
/// (so long as the Genbank loader is the only one required).
/// Sequence identifiers may be provided in a variety of forms
/// (CSeq_id's, strings, integer gi's, and CSeq_id_Handle's).

class NCBI_XOBJSIMPLE_EXPORT CSimpleOM
{
public:
    /// Return the IUPAC-format sequence for some kind of
    /// id or location
    static string GetIupac(const CSeq_id& id,
                           ENa_strand strand = eNa_strand_plus);
    static string GetIupac(const string& id_string,
                           ENa_strand strand = eNa_strand_plus);
    static string GetIupac(const CSeq_id_Handle& id,
                           ENa_strand strand = eNa_strand_plus);
    static string GetIupac(TGi gi,
                           ENa_strand strand = eNa_strand_plus);
    static string GetIupac(const CSeq_loc& loc,
                           ENa_strand strand = eNa_strand_unknown);

    /// Get the IUPAC-format sequence while avoiding returning
    /// a string by value (for efficiency)
    static void GetIupac(string& result, const CSeq_id& id,
                         ENa_strand strand = eNa_strand_plus);
    static void GetIupac(string& result, const string& id_string,
                         ENa_strand strand = eNa_strand_plus);
    static void GetIupac(string& result, const CSeq_id_Handle& id,
                         ENa_strand strand = eNa_strand_plus);
    static void GetIupac(string& result, TGi gi,
                         ENa_strand strand = eNa_strand_plus);
    static void GetIupac(string& result, const CSeq_loc& loc,
                         ENa_strand strand = eNa_strand_unknown);
    
    /// Return a sequence vector for some kind of id or location
    static CSeqVector GetSeqVector(const CSeq_id& id,
                                   ENa_strand strand = eNa_strand_plus);
    static CSeqVector GetSeqVector(const string& id_string,
                                   ENa_strand strand = eNa_strand_plus);
    static CSeqVector GetSeqVector(const CSeq_id_Handle& id,
                                   ENa_strand strand = eNa_strand_plus);
    static CSeqVector GetSeqVector(TGi gi,
                                   ENa_strand strand = eNa_strand_plus);
    static CSeqVector GetSeqVector(const CSeq_loc& loc,
                                   ENa_strand strand = eNa_strand_unknown);

    /// Return a biosequence handle for some kind of id
    static CBioseq_Handle GetBioseqHandle(const CSeq_id& id);
    static CBioseq_Handle GetBioseqHandle(const string& id_string);
    static CBioseq_Handle GetBioseqHandle(const CSeq_id_Handle& id);
    static CBioseq_Handle GetBioseqHandle(TGi gi);

    /// Return a new scope, possibly (by default) with default loaders,
    /// which will include the Genbank loader automatically
    static CRef<CScope> NewScope(bool with_defaults = true);

    /// Release the class's reference to the object manager;
    /// for use by certain shut-down procedures
    static void ReleaseOM(void);

private:
    /// Get our object manager, initializing if necessary
    static CRef<CObjectManager> x_GetOM(void);

    /// Reference to our object manager
    static CRef<CObjectManager> sm_OM;
};


END_objects_SCOPE
END_NCBI_SCOPE

#endif  // OBJTOOLS_SIMPLE___SIMPLE_OM__HPP
