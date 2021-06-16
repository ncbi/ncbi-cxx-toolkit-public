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
 * Authors: Sergey Satskiy
 *
 * File Description: PSG processor interface
 *
 */

#include <ncbi_pch.hpp>

#include "ipsgs_processor.hpp"
#include "pubseq_gateway.hpp"


string
IPSGS_Processor::StatusToString(IPSGS_Processor::EPSGS_Status  status)
{
    switch (status) {
        case IPSGS_Processor::ePSGS_InProgress:
            return "ePSGS_InProgress";
        case IPSGS_Processor::ePSGS_Found:
            return "ePSGS_Found";
        case IPSGS_Processor::ePSGS_NotFound:
            return "ePSGS_NotFound";
        case IPSGS_Processor::ePSGS_Error:
            return "ePSGS_Error";
        case IPSGS_Processor::ePSGS_Cancelled:
            return "ePSGS_Cancelled";
        default:
            break;
    }
    return "unknown (" + to_string(status) + ")";
}


IPSGS_Processor::EPSGS_StartProcessing
IPSGS_Processor::SignalStartProcessing(void)
{
    return CPubseqGatewayApp::GetInstance()->SignalStartProcessing(this);
}


void IPSGS_Processor::SignalFinishProcessing(void)
{
    if (!m_FinishSignalled) {
        CPubseqGatewayApp::GetInstance()->SignalFinishProcessing(
                                this, CPSGS_Dispatcher::ePSGS_Processor);
        m_FinishSignalled = true;
    }
}

