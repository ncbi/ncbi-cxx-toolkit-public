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

///
/// This version takes a scope.  This is useful when
/// the sequence being written has components that
/// are not in the database but have been added to
/// a scope known by the caller, or when the caller
/// knows of a scope that somehow goes along with a
/// sequence (e.g., a gbench plugin).
void NCBI_XOBJWRITE_EXPORT AgpWrite(CNcbiOstream& os,
                                    const objects::CSeqMap& seq_map,
                                    const string& object_id,
                                    const string& gap_type,
                                    bool linkage,
                                    objects::CScope& scope);


///
/// Write a bioseq in AGP format
void NCBI_XOBJWRITE_EXPORT AgpWrite(CNcbiOstream& os,
                                    const objects::CBioseq_Handle& handle,
                                    const string& object_id,
                                    const string& gap_type,
                                    bool linkage);

///
/// Write a location in AGP format
///
void NCBI_XOBJWRITE_EXPORT AgpWrite(CNcbiOstream& os,
                                    const objects::CBioseq_Handle& handle,
                                    TSeqPos from, TSeqPos to,
                                    const string& object_id,
                                    const string& gap_type,
                                    bool linkage);




END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.5  2005/05/02 16:07:30  dicuccio
 * Updated AgpWrite(): added additional constructors to write data from CSeqMap,
 * CBioseqHandle, and CBioseqHandle with sequence range.  Refactored internals.
 * Dump best seq-id instead of gi for component accession
 *
 * Revision 1.4  2004/07/09 11:54:52  dicuccio
 * Dropped version of AgpWrite() that takes the object manager - use only one API,
 * taking a CScope
 *
 * Revision 1.3  2004/07/07 21:45:07  jcherry
 * Removed form of AgpWrite that creates its own object manager
 *
 * Revision 1.2  2004/07/06 13:21:10  jcherry
 * Added export specifiers
 *
 * Revision 1.1  2004/06/29 13:29:29  jcherry
 * Initial version
 *
 * ===========================================================================
 */

