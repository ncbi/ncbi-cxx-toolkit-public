/* $Id$
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
 */

/// @file Linkage_evidence.hpp
/// User-defined methods of the data storage class.
///
/// This file was originally generated by application DATATOOL
/// using the following specifications:
/// 'seq.asn'.
///
/// New methods or data members can be added to it if needed.
/// See also: Linkage_evidence_.hpp


#ifndef OBJECTS_SEQ_LINKAGE_EVIDENCE_HPP
#define OBJECTS_SEQ_LINKAGE_EVIDENCE_HPP


// generated includes
#include <objects/seq/Linkage_evidence_.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

/////////////////////////////////////////////////////////////////////////////
class NCBI_SEQ_EXPORT CLinkage_evidence : public CLinkage_evidence_Base
{
    typedef CLinkage_evidence_Base Tparent;
public:
    // constructor
    CLinkage_evidence(void);
    // destructor
    ~CLinkage_evidence(void);

    typedef list< CRef< CLinkage_evidence > > TLinkage_evidence;

    // Convert a raw string into a container of linkage-evidence.
    // The result is appended to output_result.
    // @param output_result The result is appended to this.
    // @param linkage_evidence string, semicolon-separated list of
    //                         linkage-evidences.
    // @return true if succeeded.  If false, output_result is
    // guaranteed to be restored to its original state.
    static bool GetLinkageEvidence (
        TLinkage_evidence& output_result, 
        const string& linkage_evidence );

    // Convert a vector of strings into a container of linkage-evidence.
    // The result is appended to output_result.
    // @param output_result The result is appended to this.
    // @param linkage_evidence each string in the vector is one linkage-evidence.
    // @return true if succeeded.  If false, output_result is
    // guaranteed to be restored to its original state.
    static bool GetLinkageEvidence(
        TLinkage_evidence& output_result, 
        const vector<string> &linkage_evidence );

    // Convert a container of CLinkage_evidence into a 
    // semicolon-delimited string.
    // @param output_result the result
    // @return False doesn't exactly imply failure, per se.
    //         True if every linkage_evidence could be converted to a string.
    //         If false, output_result will contain the string "UNKNOWN"
    //         in some places,
    //         representing a linkage_evidence(s) we couldn't convert, but
    //         the rest of the string will be fine.
    static bool VecToString( string & output_result,
        const TLinkage_evidence & linkage_evidence );

private:
    // Prohibit copy constructor and assignment operator
    CLinkage_evidence(const CLinkage_evidence& value);
    CLinkage_evidence& operator=(const CLinkage_evidence& value);

};

/////////////////// CLinkage_evidence inline methods

// constructor
inline
CLinkage_evidence::CLinkage_evidence(void)
{
}


/////////////////// end of CLinkage_evidence inline methods


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE


#endif // OBJECTS_SEQ_LINKAGE_EVIDENCE_HPP
/* Original file checksum: lines: 86, chars: 2525, CRC32: 5681c8d4 */
