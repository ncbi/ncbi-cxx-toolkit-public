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
 * Author: Christiam Camacho
 *
 */

/** @file blastdb_aux.cpp
 * Auxiliary functions for BLAST database application
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = 
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <ncbi_pch.hpp>
#include "blastdb_aux.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

CSeqDB::ESeqType 
ParseTypeString(const string& s)
{
    CSeqDB::ESeqType retval = CSeqDB::eUnknown;
    if (s == "prot") {
        retval = CSeqDB::eProtein;
    } else if (s == "nucl") {
        retval = CSeqDB::eNucleotide;
    } else if (s == "guess") {
        retval = CSeqDB::eUnknown;
    } else {
        _ASSERT("Unknown molecule for BLAST DB" != 0);
    }
    return retval;
}

END_SCOPE(blast)
END_NCBI_SCOPE
