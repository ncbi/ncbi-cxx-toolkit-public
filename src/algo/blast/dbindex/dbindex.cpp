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
 * Author:  Aleksandr Morgulis
 *
 * File Description:
 *   Implementation file for part of CDbIndex and some related classes.
 *
 */

#include <ncbi_pch.hpp>

#include <iostream>
#include <sstream>
#include <string>
#include <limits>

#include <objmgr/object_manager.hpp>
#include <objmgr/seq_vector.hpp>
#include <objects/seqloc/Seq_interval.hpp>

#include <algo/blast/core/blast_gapalign.h>
#include <algo/blast/core/blast_hits.h>

#include "sequence_istream_fasta.hpp"
#include "dbindex.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE( blastdbindex )

//-------------------------------------------------------------------------
const char * CDbIndex_Exception::GetErrCodeString() const
{
    switch( GetErrCode() ) {
        case eBadOption:   return "bad index creation option";
        case eBadSequence: return "bad sequence data";
        case eBadVersion:  return "wrong versin";
        case eBadData:     return "corrupt index data";
        case eIO:          return "I/O error";
        default:           return CException::GetErrCodeString();
    }
}

//-------------------------------------------------------------------------
CDbIndex::SOptions CDbIndex::DefaultSOptions()
{
    SOptions result = {
        0,                      // contiguous index
        UNCOMPRESSED,           // no compression of offset lists
        WIDTH_32,               // 32-bit index
        FASTA,                  // FastA input
        CHUNKS,                 // allow sequences to be split between index volumes
        12,                     // default Nmer size
        MAX_DBSEQ_LEN,          // defined by BLAST
        DBSEQ_CHUNK_OVERLAP,    // defined by BLAST
        REPORT_NORMAL,          // normal level of progress reporting
        1536,                   // max index size if 1.5 Gb by default
        VERSION                 // default index version
    };

    return result;
}

END_SCOPE( blastdbindex )
END_NCBI_SCOPE

