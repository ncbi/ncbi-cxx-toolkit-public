#ifndef ALGO_BLAST_API_DEMO___ALIGN_PARMS__HPP
#define ALGO_BLAST_API_DEMO___ALIGN_PARMS__HPP

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

/// @file align_parms.hpp
/// This sets up formatting parametes for DisplaySeqalign.
///
/// This code controls formatting options for the new DisplaySeqalign
/// code.  Seperating it facilitates maintaining compatibility if and
/// when DisplaySeqalign changes.

#include "optional.hpp"
#include <objtools/blast_format/showalign.hpp>

USING_NCBI_SCOPE;

/// CAlignParms class sets formatting options for DisplaySeqalign.
///
/// During the setup and execution of the search, this object is
/// imbued with search details and formatting options necessary to
/// configure the display of the output.  When it is time to output,
/// adjust_display is called to set up the DisplaySeqalign object.
/// The DisplaySeqalign code is new and malleable; this code is
/// designed to isolate remote_blast from these changes.  It also
/// serves to collect knowledge of the formatting interfaces in one
/// part of the remote_blast application.

class CAlignParms
{
public:
    /// Constructor (no arguments required).
    ///
    /// Constructor (no arguments required).
    CAlignParms(void)
    {
    }
    
    /// Set the RID to provide to DisplaySeqalign.
    ///
    /// I do not believe DisplaySeqalign makes use of the RID (yet),
    /// but it has the option, so we set it in case this changes.
    /// @param v RID string to pass to DisplaySeqalign.
    void SetRID(const string & v) { m_RID = v; }
    
    /// Set the number of alignments to display.
    ///
    /// This number of alignments will be shown.  Currently, this
    /// will always equal the number of the alignments retrieved, but
    /// there are plans to change this.
    /// @param n Number of alignments to display.
    void SetNumAlgn(const TOptInteger & n)
    {
        m_NumAlgn = n;
    }
    
    /// Set up the DisplaySeqalign.
    ///
    /// Set the accumulated and calculated formatting and output
    /// options in the specified CDisplaySeqalign object.
    /// @param disp The object to adjust.
    void AdjustDisplay(objects::CDisplaySeqalign & disp);
    
private:
    /// RID of this search.
    string      m_RID;

    /// Number of alignments to display.
    ///
    /// If set, output will be limited to this number of alignments.
    TOptInteger m_NumAlgn;
};

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.3  2005/06/23 16:18:46  camacho
 * Doxygen fixes
 *
 * Revision 1.2  2005/02/22 14:25:33  camacho
 * Moved showalign.[hc]pp to objtools/blast_format
 *
 * Revision 1.1  2004/02/18 17:04:41  bealer
 * - Adapt blast_client code for Remote Blast API, merging code into the
 *   remote_blast demo application.
 *
 * ===========================================================================
 */

#endif // ALGO_BLAST_API_DEMO___ALIGN_PARMS__HPP
