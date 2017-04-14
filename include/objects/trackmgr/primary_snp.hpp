#ifndef OBJECTS_TRACKMGR__PRIMARY_SNP_HPP
#define OBJECTS_TRACKMGR__PRIMARY_SNP_HPP

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
 * Authors: Peter Meric
 *
 * File Description:  TMS API for lookup of primary SNP track
 *
 */


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
///
/// CPrimarySNPTrack --
///
/// Collection of static functions to look up primary SNP track

class CPrimarySNPTrack
{
public:
    using TTrackId = string;

public:
    /// Find the track-id for a specific sequence.
    ///
    /// @param gi
    ///   Sequence GI to search.
    /// @return
    ///   TMS track-id
    static TTrackId GetTrackId(TGi gi);

    /// Find the track-id for a specific sequence.
    ///
    /// @param seq_accver
    ///   Sequence accession.version to search.
    /// @return
    ///   TMS track-id
    static TTrackId GetTrackId(const string& seq_accver);

private:
    CPrimarySNPTrack() = delete;
    ~CPrimarySNPTrack() = delete;
};


END_NCBI_SCOPE

#endif // OBJECTS_TRACKMGR__PRIMARY_SNP_HPP

