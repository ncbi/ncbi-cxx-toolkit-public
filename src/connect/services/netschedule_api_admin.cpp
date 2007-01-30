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
 * Author:  Anatoliy Kuznetsov, Maxim Didenko
 *
 * File Description:
 *   Implementation of NetSchedule API.
 *
 */

#include <ncbi_pch.hpp>
#include <connect/services/netschedule_api.hpp>


BEGIN_NCBI_SCOPE

void CNetScheduleAdmin::DropJob(const string& job_key) const
{
    m_API->x_SendJobCmdWaitResponse("DROJ", job_key);
}

void CNetScheduleAdmin::ShutdownServer(CNetScheduleAdmin::EShutdownLevel level) const
{
    if (m_API->GetPoll().IsLoadBalanced()) {
        // TODO throw an exception.
        return;
    }

    string cmd = "SHUTDOWN ";
    switch( level ) {
    case eDie :
        cmd = "SHUTDOWN SUICIDE ";
        break;
    case eShutdownImmidiate :
        cmd = "SHUTDOWN IMMEDIATE ";
        break;
    default:
        break;
    }
     
    m_API->SendCmdWaitResponse(m_API->x_GetConnector(), cmd); 
}


void CNetScheduleAdmin::ReloadServerConfig() const
{
    if (m_API->GetPoll().IsLoadBalanced()) {
        // TODO throw an exception.
        return;
    }
    m_API->SendCmdWaitResponse(m_API->x_GetConnector(), "RECO"); 

}

void CNetScheduleAdmin::CreateQueue(const string& qname, const string& qclass,
                                    const string& comment) const
{
    string param = "QCRE " + qname + " " + qclass;
    if (!comment.empty()) {
        param.append(" \"");
        param.append(comment);
        param.append("\"");
    }
    for (CNetSrvConnectorPoll::iterator it = m_API->GetPoll().begin(); 
         it != m_API->GetPoll().end(); ++it) {
        m_API->SendCmdWaitResponse(*it, param); 
    }
}


void CNetScheduleAdmin::DeleteQueue(const string& qname) const
{
    string param = "QDEL " + qname;

    for (CNetSrvConnectorPoll::iterator it = m_API->GetPoll().begin(); 
         it != m_API->GetPoll().end(); ++it) {
        m_API->SendCmdWaitResponse(*it, param); 
    }
}

void CNetScheduleAdmin::DumpJob(CNcbiOstream& out, const string& job_key) const
{
    string cmd = "DUMP " + job_key;
    CNetSrvConnector& conn = m_API->x_GetConnector(job_key);
    m_API->PrintServerOut(conn, out);
}

void CNetScheduleAdmin::DropQueue() const
{
    string cmd = "DROPQ";

    for (CNetSrvConnectorPoll::iterator it = m_API->GetPoll().begin(); 
         it != m_API->GetPoll().end(); ++it) {
        m_API->SendCmdWaitResponse(*it, cmd); 
    }
}


///////////////////????????????????????????/////////////////////////
void CNetScheduleAdmin::GetServerVersion(ISink& sink) const
{ 
    string cmd = "VERSION";
    for (CNetSrvConnectorPoll::iterator it = m_API->GetPoll().begin(); 
         it != m_API->GetPoll().end(); ++it) {
        CNcbiOstream& os = sink.GetOstream(m_API->GetConnectionInfo(*it));
        os << m_API->SendCmdWaitResponse(*it, cmd);
    }        
}



void CNetScheduleAdmin::DumpQueue(ISink& sink) const
{
    string cmd = "DUMP";        
    for (CNetSrvConnectorPoll::iterator it = m_API->GetPoll().begin(); 
         it != m_API->GetPoll().end(); ++it) {
        CNcbiOstream& os = sink.GetOstream(m_API->GetConnectionInfo(*it));
        it->WriteStr(cmd);
        m_API->PrintServerOut(*it, os);
    }
}


void CNetScheduleAdmin::PrintQueue(ISink& sink, CNetScheduleAPI::EJobStatus status) const
{
    string cmd = "QPRT " + CNetScheduleAPI::StatusToString(status);;        
    for (CNetSrvConnectorPoll::iterator it = m_API->GetPoll().begin(); 
         it != m_API->GetPoll().end(); ++it) {
        CNcbiOstream& os = sink.GetOstream(m_API->GetConnectionInfo(*it));
        it->WriteStr(cmd);
        m_API->PrintServerOut(*it, os);
    }
}



void CNetScheduleAdmin::GetServerStatistics(ISink& sink, 
                                            EStatisticsOptions opt) const
{
    string cmd = "STAT";
    if (opt == eStatisticsAll) {
        cmd += " ALL";
    }
    for(CNetSrvConnectorPoll::iterator it = m_API->GetPoll().begin(); 
        it != m_API->GetPoll().end(); ++it) {
        CNcbiOstream& os = sink.GetOstream(m_API->GetConnectionInfo(*it));
        it->WriteStr(cmd);
        m_API->PrintServerOut(*it, os);
    }
}


void CNetScheduleAdmin::Monitor(CNcbiOstream & out) const
{
    /*
    CheckConnect(kEmptyStr);
    CSockGuard sg(*m_Sock);

    MakeCommandPacket(&m_Tmp, "MONI ");
    WriteStr(m_Tmp.c_str(), m_Tmp.length() + 1);
    m_Tmp = "QUIT";
    WriteStr(m_Tmp.c_str(), m_Tmp.length() + 1);

    STimeout rto;
    rto.sec = 1;
    rto.usec = 0;
    m_Sock->SetTimeout(eIO_Read, &rto);

    string line;
    while (1) {

        EIO_Status st = m_Sock->ReadLine(line);       
        if (st == eIO_Success) {
            if (m_Tmp == "END")
                break;
            out << line << "\n" << flush;
        } else {
            EIO_Status st = m_Sock->GetStatus(eIO_Open);
            if (st != eIO_Success) {
                break;
            }
        }
    }
    */
}


void CNetScheduleAdmin::GetQueueList(ISink& sink) const
{
    string cmd = "QLIST";
    for(CNetSrvConnectorPoll::iterator it = m_API->GetPoll().begin(); 
        it != m_API->GetPoll().end(); ++it) {
        CNcbiOstream& os = sink.GetOstream(m_API->GetConnectionInfo(*it));
        os << m_API->SendCmdWaitResponse(*it, cmd);
    }
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log: netschedule_api_admin.cpp,v $
 * Revision 6.1  2007/01/09 15:29:55  didenko
 * Added new API for NetSchedule service
 *
 * ===========================================================================
 */
