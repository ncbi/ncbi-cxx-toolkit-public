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
 * File Description:
 *   Queueing and Polling code for blast_client.
 *
 */

// Local
#include <ncbi_pch.hpp>
#include "align_parms.hpp"

void CAlignParms::AdjustDisplay(CDisplaySeqalign & disp)
{
    if ( !m_RID.empty() ) {
        disp.SetRid(m_RID);
    }
    
    if (m_NumAlgn.Exists()) {
        disp.SetNumAlignToShow(m_NumAlgn.GetValue());
    }
    
    typedef CDisplaySeqalign   TDS;
    typedef TDS::DisplayOption TDSdo;
                
    disp.SetAlignOption(TDSdo(0
                              | TDS::eShowBlastStyleId
                              | TDS::eShowBlastInfo
                              | TDS::eShowMiddleLine
                              ));
    
    disp.SetAlignType(CDisplaySeqalign::eProt);
    disp.SetMiddleLineStyle(CDisplaySeqalign::eChar);
}

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.2  2004/05/21 21:41:38  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.1  2003/09/26 16:53:49  bealer
 * - Add blast_client project for netblast protocol, initial code commit.
 *
 * ===========================================================================
 */
