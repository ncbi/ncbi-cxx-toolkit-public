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

/// @file itax4blast.hpp
/// Declares an interface for getting taxonomic information for filtering BLASTDBs

#ifndef OBJTOOLS_BLAST_SEQDB_READER___ITAX4BLAST_HPP
#define OBJTOOLS_BLAST_SEQDB_READER___ITAX4BLAST_HPP

BEGIN_NCBI_SCOPE

/// Interface to retrieve taxonomic information for filtering BLASTDBs
class NCBI_XOBJREAD_EXPORT ITaxonomy4Blast : public CObject {
public:
    /// Retrieve the descendant taxids for an arbitrary taxID
    /// @param taxid NCBI taxonomy ID of interest [in]
    /// @param descendants list of taxonomy IDs that are descendants from
    ///        taxid. This parameter will be an empty list if the taxid is invalid, 
    ///        or if it cannot be resolved [out]
    virtual void GetLeafNodeTaxids(const int taxid, vector<int>& descendants) = 0;
};


END_NCBI_SCOPE

#endif /* OBJTOOLS_BLAST_SEQDB_READER___ITAX4BLAST_HPP */
