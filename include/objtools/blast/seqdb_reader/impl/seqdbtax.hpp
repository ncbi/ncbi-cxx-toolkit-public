#ifndef OBJTOOLS_READERS_SEQDB__SEQDBTAX_HPP
#define OBJTOOLS_READERS_SEQDB__SEQDBTAX_HPP

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
 * Author:  Kevin Bealer
 *
 */

/// @file seqdbtax.hpp
/// Defines database volume access classes.
/// 
/// Defines classes:
///     CSeqDBTaxNames, CSeqDBTaxId, CSeqDBTaxInfo
/// 
/// Implemented for: UNIX, MS-Windows

#include <objtools/blast/seqdb_reader/seqdb.hpp>
#include <objtools/blast/seqdb_reader/impl/seqdbfile.hpp>
#include <objtools/blast/seqdb_reader/impl/seqdbisam.hpp>

BEGIN_NCBI_SCOPE

/// Import definitions from the objects namespace.
USING_SCOPE(objects);



/// CSeqDBTaxInfo class
/// 
/// This manages access to the taxonomy database.

class NCBI_XOBJREAD_EXPORT CSeqDBTaxInfo  {
public:
    
    /// Get the taxonomy names for a given tax id
    ///
    /// The tax id is looked up in the taxonomy database and the
    /// corresponding strings indicating the taxonomy names are
    /// returned in the provided structure.
    ///
    /// @param tax_id
    ///   The taxonomic identiifer.
    /// @param info
    ///   A container structure in which to return the names.
    /// @param locked
    ///   The lock holder object for this thread.
    /// @return true if the taxonomic id was found
    static bool GetTaxNames(TTaxId tax_id, SSeqDBTaxInfo  & info);
    

};

END_NCBI_SCOPE

#endif // OBJTOOLS_READERS_SEQDB__SEQDBTAX_HPP


