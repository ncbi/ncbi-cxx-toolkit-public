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

/// This form uses its own object manager instance,
/// which has the GB loader as a default loader.
/// It is a simple way to write a sequence whose
/// components are in the database.
void AgpWrite(CNcbiOstream& os,
              const objects::CSeqMap& seq_map,
              const string& object_id,
              const string& gap_type,
              bool linkage);
/// This version takes an object manager instance.
/// This is useful to prevent another object manager
/// from being instantiated when the caller has access
/// to one that already exists.  It would also be useful
/// if the caller wanted to use an object manager with
/// a different set of default loaders than {CGBDataLoader}.
void AgpWrite(CNcbiOstream& os,
              const objects::CSeqMap& seq_map,
              const string& object_id,
              const string& gap_type,
              bool linkage,
              objects::CObjectManager& om);
/// This version takes a scope.  This is useful when
/// the sequence being written has components that
/// are not in the database but have been added to
/// a scope known by the caller, or when the caller
/// knows of a scope that somehow goes along with a
/// sequence (e.g., a gbench plugin).
void AgpWrite(CNcbiOstream& os,
              const objects::CSeqMap& seq_map,
              const string& object_id,
              const string& gap_type,
              bool linkage,
              objects::CScope& scope);

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2004/06/29 13:29:29  jcherry
 * Initial version
 *
 * ===========================================================================
 */

