#ifndef FORMATGUESS__HPP
#define FORMATGUESS__HPP

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
 * Author: Anatoliy Kuznetsov
 *
 * File Description:  Different "fuzzy-logic" methods to identify file formats.
 *
 */

#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE


//////////////////////////////////////////////////////////////////
//
// Class implements different ad-hoc unreliable file format 
// identifications.
//

class NCBI_XUTIL_EXPORT CFormatGuess
{
public:
    enum EFormat {
        //< unknown format
        eUnknown = 0,

        //< binary ASN.1
        eBinaryASN,

        //< text ASN.1
        eTextASN,

        // FASTA format sequence record
        eFasta,

        //< XML
        eXml,

        //< RepeatMasker Output
        eRmo,

        //< Glimmer3 predictions
        eGlimmer3,

        //< Phrap ACE assembly file
        ePhrapAce,

        //< GFF/GTF style annotations
        eGtf,

        //< AGP format assembly
        eAgp,

        //< Newick file
        eNewick,

        //< Distance matrix file
        eDistanceMatrix,

        //< Five-column feature table
        eFiveColFeatureTable,

        //< Taxplot file
        eTaxplot,

        //< Generic table
        eTable,

        //< Text alignment
        eAlignment,

        //< GenBank/GenPept/DDBJ/EMBL flat-file sequence portion
        eFlatFileSequence
    };

    enum ESequenceType {
        eUndefined,
        eNucleotide,
        eProtein
    };

    // Guess sequence type. Function calculates sequence alphabet and 
    // identifies if the source belongs to nucleotide or protein sequence
    static ESequenceType SequenceType(const char* str, unsigned length);

    /// Guess file format structure.
    EFormat Format(const string& path);
    /// Format prediction based on an input stream
    EFormat Format(CNcbiIstream& input);
    
    /// Format prediction based on memory buffer
    /// Recommended that buffer should contain at least 1K of data
    EFormat Format(const unsigned char* buffer, 
                   size_t               buffer_size);

};

END_NCBI_SCOPE

#endif
