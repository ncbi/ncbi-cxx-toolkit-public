#ifndef OBJECTS_BLASTDB___DEFLINE_EXTRA__HPP
#define OBJECTS_BLASTDB___DEFLINE_EXTRA__HPP

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
 * Authors:  Jian Ye
 *
 */

/// @file defline_extra.hpp
/// Blast defline related defines
#include <corelib/ncbistd.hpp>
#include <objects/blastdb/Blast_filter_program.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE

static const string kAsnDeflineObjLabel = "ASN1_BlastDefLine";
static const string kTaxDataObjLabel    = "TaxNamesData";

/** Bit field for the "links" element of Blast-def-line. 
See ASN.1 spec. in src/objects/blastdb/blastdb.asn. 
@note These link bits in the Blast-def-line will be moved to LinkoutDB (@see
CLinkoutDB) . Please keep these in sync (@see GetLinkoutTypes)
 */
enum LinkoutTypes {
  eLocuslink              = (1<<0),  ///< NCBI_DEPRECATED, replaced by eGene
  eUnigene                = (1<<1),  ///< Add Linkout for UniGene
  eStructure              = (1<<2),  ///< Add Linkout for structure.
  eGeo                    = (1<<3),  ///< Add Linkout for Geo
  eGene                   = (1<<4),  ///< Add Linkout for Gene
  eHitInMapviewer         = (1<<5),  ///< NCBI_DEPRECATED, use eMapviewer
  eMapviewer              = (1<<6),  ///< The main blast link goes to entrez but we put on a linkout icon that goes to mapviewer.
  eGenomicSeq             = (1<<7),  ///< Is a genomic sequence.  Used for the genome+transcript database to show the genomic 
                                     ///< and transcript sequences separately.
  eBioAssay               = (1<<8)   ///< Add Linkout for BioAssay (structure group resource)
};

/// Structure describing filtered regions created using a particular sequence
/// filtering algorithm
///
/// Sequence offsets are inclusive, one-based, positive values.  The
/// algorithm_id field can be one of the pre-defined ones or a user-defined
/// combination of algorithm and options.  See WriteDB and SeqDB for
/// more information about how to work with user defined types.

struct SBlastDbMaskData {

    /// Construct and clear this object.
    SBlastDbMaskData() : algorithm_id((int)eBlast_filter_program_not_set) {}
    
    /// Identifies the algorithm used.
    int algorithm_id;
    
    /// Start and end offsets of the filtered area.
    vector< pair<TSeqPos, TSeqPos> > offsets;

    /// Determines whether this object contains no offsets
    bool empty() const {
        return offsets.empty();
    }
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif  /* OBJECTS_BLASTDB___DEFLINE_EXTRA__HPP */
