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
 * Authors:  Frank Ludwig
 *
 * File Description:  Common file reader utility functions.
 *
 */

#ifndef OBJTOOLS_READERS___READ_UTIL__HPP
#define OBJTOOLS_READERS___READ_UTIL__HPP

#include <corelib/ncbistd.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqfeat/Seq_feat.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects) // namespace ncbi::objects::

//  ============================================================================
/// Common file reader utility functions.
///
class NCBI_XOBJREAD_EXPORT CReadUtil
//  ============================================================================
{
public:
    /// Tokenize a given string, respecting quoted substrings an atomic units.
    ///
    static void Tokenize(
        const string& instr,
        const string& delim,
        vector< string >& tokens);

    /// Convert a raw ID string to a Seq-id, based in given customization flags.
    /// Recognized flags are:
    ///   CReaderBase::fAllIdsAsLocal, CReaderBase::fNumericalIdsAsLocal
    /// By default, numerical IDs below 500 are recognized as local IDs, and 500
    /// and above are considered GI numbers.
    ///
    static CRef<CSeq_id> AsSeqId(
        const string& rawId,
        unsigned int flags =0,
        bool localInts = true);

    static const TGi kMinNumericGi;

    /// Extract track information that should be present if the data originated from
    /// a UCSC data file. "name" and "db" have their designated accessors to recognize
    /// their special importance. The third variant can be used to access arbitrary
    /// track values.
    /// @param annot
    //      the Seq-annot to inspect.
    //  @param key
    //      the specific track line parameter to look up.
    //  @param value
    //      the value the caller asked for.
    //  @return
    ///     true if the requested information os available and the value parameter is
    ///     good to use.
    ///     false otherwise.
    static bool GetTrackName(
        const CSeq_annot& annot,
        string& value);
    static bool GetTrackAssembly(
        const CSeq_annot& annot,
        string& value);
    static bool GetTrackOffset(
        const CSeq_annot& annot,
        int& value);

    static bool GetTrackValue(
        const CSeq_annot& annot,
        const string& key,
        string& value);

    static void AddGeneOntologyTerm(
        CSeq_feat& feature,
        const CTempString& qual,
        const CTempString& val);
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // OBJTOOLS_READERS___READ_UTIL__HPP
