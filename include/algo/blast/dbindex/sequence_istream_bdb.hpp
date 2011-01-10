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
 *   Header file for CSequenceIStreamBlastDB class.
 *
 */

#ifndef C_SEQUENCE_I_STREAM_BDB_HPP
#define C_SEQUENCE_I_STREAM_BDB_HPP

#include <algo/blast/core/blast_export.h>
#include <objtools/blast/seqdb_reader/seqdb.hpp>

#include "sequence_istream.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE( blastdbindex )

/** Sequence stream that reads BLAST nucleotide databases.
*/
class NCBI_XBLAST_EXPORT CSequenceIStreamBlastDB : public CSequenceIStream
{
    public:

        /** Report on supported subject filter algorithms.
            @param dbname [I] name of the BLAST database
        */
        static string ShowSupportedFilters( const string & dbname )
        {
            CSeqDB db( dbname, CSeqDB::eNucleotide );
            return db.GetAvailableMaskAlgorithmDescriptions();
        }

        /** Object constructor.
            @param dbname         [I]     name of the BLAST database
            @param use_filter     [I]     use/not use subject masking
            @param filter_algo_id [I]     filtering algorithm
        */
        CSequenceIStreamBlastDB( 
                const string & dbname, 
                bool use_filter, 
                int filter_algo_id = 0 );

        /** Object destructor. */
        virtual ~CSequenceIStreamBlastDB() {}

        /** Get next sequence data.
            @return sequence data for the next sequence in the database
        */
        virtual CRef< TSeqData > next();

        /** Roll back to the start of the previous sequence. */
        virtual void putback();

    private:

        CRef< CSeqDB > seqdb_;  /**< BLAST database object. */
        CSeqDB::TOID oid_;      /**< Current OID (to be read). */
        int filter_algo_id_;    /**< Filtering algorithm id. */
        bool use_filter_;       /**< Whether to extract mask from database. */
};

END_SCOPE( blastdbindex )
END_NCBI_SCOPE

#endif
