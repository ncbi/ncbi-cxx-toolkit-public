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
 * File Description:  Write agp file
 *
 */

#include <corelib/ncbistd.hpp>

#include <objmgr/seq_map.hpp>
#include <objmgr/scope.hpp>

BEGIN_NCBI_SCOPE

// Sometimes, users may of this API may wish to change how accessions
// are written.
class CAgpWriteComponentIdMapper {
public:
    virtual void do_map( string &in_out_component_id ) = 0;
};

///
/// Write a SeqMap in AGP format, using provided scope.
/// Gap-type and linkage evidence data must be present in sequence.
void NCBI_XOBJWRITE_EXPORT
AgpWrite(CNcbiOstream& os,
         const objects::CSeqMap& seq_map,
         const string& object_id,
         objects::CScope& scope,
         const vector<char>& component_types = vector<char>(),
         CAgpWriteComponentIdMapper * comp_id_mapper = NULL );

///
/// Write a bioseq in AGP format.
/// Gap-type and linkage evidence data must be present in sequence.
void NCBI_XOBJWRITE_EXPORT
AgpWrite(CNcbiOstream& os,
         const objects::CBioseq_Handle& handle,
         const string& object_id,
         const vector<char>& component_types = vector<char>(),
         CAgpWriteComponentIdMapper * comp_id_mapper = NULL );

///
/// Write a location in AGP format
/// Gap-type and linkage evidence data must be present in sequence.
void NCBI_XOBJWRITE_EXPORT
AgpWrite(CNcbiOstream& os,
         const objects::CBioseq_Handle& handle,
         TSeqPos from, TSeqPos to,
         const string& object_id,
         const vector<char>& component_types = vector<char>(),
         CAgpWriteComponentIdMapper * comp_id_mapper = NULL );

///
/// Write a SeqMap in AGP format, using provided scope.
/// Default gap type and linkage are used if this information
/// is missing from the sequence.
void NCBI_XOBJWRITE_EXPORT
AgpWrite(CNcbiOstream& os,
         const objects::CSeqMap& seq_map,
         const string& object_id,
         const string& default_gap_type,
         bool default_linkage,
         objects::CScope& scope,
         const vector<char>& component_types = vector<char>(),
         CAgpWriteComponentIdMapper * comp_id_mapper = NULL );

///
/// Write a bioseq in AGP format
/// Default gap type and linkage are used if this information
/// is missing from the sequence.
void NCBI_XOBJWRITE_EXPORT
AgpWrite(CNcbiOstream& os,
         const objects::CBioseq_Handle& handle,
         const string& object_id,
         const string& default_gap_type,
         bool default_linkage,
         const vector<char>& component_types = vector<char>(),
         CAgpWriteComponentIdMapper * comp_id_mapper = NULL );

///
/// Write a location in AGP format
/// Default gap type and linkage are used if this information
/// is missing from the sequence.
void NCBI_XOBJWRITE_EXPORT
AgpWrite(CNcbiOstream& os,
         const objects::CBioseq_Handle& handle,
         TSeqPos from, TSeqPos to,
         const string& object_id,
         const string& default_gap_type,
         bool default_linkage,
         const vector<char>& component_types = vector<char>(),
         CAgpWriteComponentIdMapper * comp_id_mapper = NULL );


END_NCBI_SCOPE
