/*  $Id:
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
 * Authors:  Amelia Fong
 *
 */

/** @file blast_node.cpp
 *  BLAST node api
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <algo/blast/api/remote_blast.hpp>
#include <algo/blast/blastinput/blast_fasta_input.hpp>
#include <algo/blast/api/blast_node.hpp>

#if defined(NCBI_OS_UNIX)
#include <unistd.h>
#endif

#ifndef SKIP_DOXYGEN_PROCESSING
USING_NCBI_SCOPE;
USING_SCOPE(blast);
USING_SCOPE(objects);
#endif
static void
s_UnregisterDataLoader(const string & dbloader_name)
{
	try {
		if (dbloader_name != kEmptyStr ) {
 	   	CRef<CObjectManager> om = CObjectManager::GetInstance();
  	 		if(om->RevokeDataLoader(dbloader_name)){
   			 	_TRACE("Unregistered Data Loader:  " + dbloader_name);
   			}
   			else {
   				_TRACE("Failed to Unregistered Data Loader:  " + dbloader_name);
   			}
   		}
	}
	catch(CException &) {
		_TRACE("Failed to unregister data loader: " + dbloader_name);
	}
}

void CBlastNodeMailbox::SendMsg(CRef<CBlastNodeMsg> msg)
{
       CFastMutexGuard guard(m_Mutex);
       m_MsgQueue.push_back(msg);
       m_Notify.SignalSome();
}

CBlastNode::CBlastNode (int node_num, const CNcbiArguments & ncbi_args, const CArgs& args,
		                CBlastAppDiagHandler & bah, int query_index, int num_queries, CBlastNodeMailbox * mailbox):
                        m_NodeNum(node_num), m_NcbiArgs(ncbi_args), m_Args(args),
                        m_Bah(bah), m_QueryIndex(query_index), m_NumOfQueries(num_queries),
                        m_QueriesLength(0), m_DataLoaderName(kEmptyStr)
{
	if(mailbox != NULL) {
		m_Mailbox.Reset(mailbox);
	}
	string p("Query ");
	p+=NStr::IntToString(query_index) + "-" + NStr::IntToString(query_index + num_queries -1);
	m_NodeIdStr = p;
}

CBlastNode::~CBlastNode () {


	s_UnregisterDataLoader(m_DataLoaderName);
	if(m_Mailbox.NotEmpty()) {
		m_Mailbox.Reset();
	}
}

void CBlastNode::SendMsg(CBlastNodeMsg::EMsgType msg_type, void* ptr)
{
	if (m_Mailbox.NotEmpty()) {
		CRef<CBlastNodeMsg>  m( new CBlastNodeMsg(msg_type, ptr));
		m_Mailbox->SendMsg(m);
	}
}

CBlastMasterNode::CBlastMasterNode(CNcbiOstream & out_stream, int num_threads):
		m_OutputStream(out_stream), m_MaxNumThreads(num_threads), m_MaxNumNodes(num_threads + 2),
		m_NumErrStatus(0), m_NumQueries(0), m_QueriesLength(0)
{
	m_StopWatch.Start();
}

void
CBlastMasterNode::x_WaitForNewEvent()
{
	CFastMutexGuard guard(m_Mutex);
	m_NewEvent.WaitForSignal(m_Mutex);
}

void
CBlastMasterNode::RegisterNode(CBlastNode * node, CBlastNodeMailbox * mailbox)
{
	if(node == NULL) {
		 NCBI_THROW(CBlastException, eInvalidArgument, "Empty Node" );
	}
	if(mailbox == NULL) {
		 NCBI_THROW(CBlastException, eInvalidArgument, "Empty mailbox" );
	}
	if(mailbox->GetNodeNum() != node->GetNodeNum()) {
		 NCBI_THROW(CBlastException, eCoreBlastError, "Invalid mailbox node number" );
	}
	{
		CFastMutexGuard guard(m_Mutex);
		int node_num = node->GetNodeNum();
		if ((m_PostOffice.find(node_num) != m_PostOffice.end()) ||
	    	(m_RegisteredNodes.find(node_num) != m_RegisteredNodes.end())){
		 	NCBI_THROW(CBlastException, eInvalidArgument, "Duplicate chunk num" );
		}
		m_PostOffice[node_num]= mailbox;
		m_RegisteredNodes[node_num] = node;
	}
}

bool CBlastMasterNode::Processing()
{
	NON_CONST_ITERATE(TPostOffice, itr, m_PostOffice) {
		if(itr->second->GetNumMsgs() > 0) {
			CRef<CBlastNodeMsg> msg = itr->second->ReadMsg();
			int chunk_num = itr->first;
			if (msg.NotEmpty()) {
				switch (msg->GetMsgType()) {
					case CBlastNodeMsg::eRunRequest:
					{
						if ((int) m_ActiveNodes.size() < m_MaxNumThreads) {
							CBlastNode * n = (CBlastNode *) msg->GetMsgBody();
							if(n != NULL) {
								double start_time = m_StopWatch.Elapsed();
								n->Run();
								pair< int, double > p(chunk_num, start_time);
								m_ActiveNodes.insert(p);
								CRef<CBlastNodeMsg> empty_msg;
								pair<int,CRef<CBlastNodeMsg> > m(chunk_num, empty_msg);
								m_FormatQueue.insert(m);
								INFO_POST("Starting Chunk # " << chunk_num) ;
							}
							else {
		 						NCBI_THROW(CBlastException, eCoreBlastError, "Invalid mailbox node number" );
							}
						}
						else {
							itr->second->UnreadMsg(msg);
							FormatResults();
							if (IsFull()) {
								x_WaitForNewEvent();
							}
							return true;
						}
						break;
					}
					case CBlastNodeMsg::ePostResult:
					case CBlastNodeMsg::eErrorExit:
					{
						m_FormatQueue[itr->first] = msg;
						double diff = m_StopWatch.Elapsed() - m_ActiveNodes[itr->first];
						m_ActiveNodes.erase(chunk_num);
						CTimeSpan s(diff);
						INFO_POST("Chunk #" << chunk_num << " completed in " << s.AsSmartString());
						break;
					}
					case CBlastNodeMsg::ePostLog:
					{
						break;
					}
					default:
					{
		 				NCBI_THROW(CBlastException, eCoreBlastError, "Invalid node message type");
						break;
					}
				}
			}
		}
	}
	FormatResults();
	return IsActive();
}

void CBlastMasterNode::FormatResults()
{
	TFormatQueue::iterator itr= m_FormatQueue.begin();

	while (itr != m_FormatQueue.end()){
		CRef<CBlastNodeMsg> msg(itr->second);
		if(msg.Empty()) {
			break;
		}
		CBlastNode * n = (CBlastNode *) msg->GetMsgBody();
		if(n == NULL) {
			string err_msg = "Empty formatting msg for chunk num # " + NStr::IntToString(itr->first);
 			NCBI_THROW(CBlastException, eCoreBlastError, err_msg);
		}
		int node_num = n->GetNodeNum();
		if (msg->GetMsgType() == CBlastNodeMsg::ePostResult) {
			string results;
			n->GetBlastResults(results);
			if (results != kEmptyStr) {
				m_OutputStream << results;
			}
		}
		else if (msg->GetMsgType() == CBlastNodeMsg::eErrorExit) {
			m_NumErrStatus++;
			ERR_POST("Chunk # " << node_num << " exit with error (" << n->GetStatus() << ")");
		}
		else {
 			NCBI_THROW(CBlastException, eCoreBlastError, "Invalid msg type");
		}
		m_NumQueries += n->GetNumOfQueries();
		m_QueriesLength += n->GetQueriesLength();
		n->Detach();
		m_PostOffice.erase(node_num);
		m_RegisteredNodes.erase(node_num);

		itr++;
	}

	if (itr != m_FormatQueue.begin()) {
		m_FormatQueue.erase(m_FormatQueue.begin(), itr);
	}
}

int CBlastMasterNode::IsFull()
{
	TRegisteredNodes::reverse_iterator rr = m_RegisteredNodes.rbegin();
	TActiveNodes::reverse_iterator ra = m_ActiveNodes.rbegin();
	unsigned int in_buffer = m_MaxNumThreads;
	if ((!m_RegisteredNodes.empty()) && (!m_ActiveNodes.empty())) {
		in_buffer = rr->first - ra->first;
	}
	return ((int) (m_ActiveNodes.size() + in_buffer) >=   m_MaxNumNodes);
}


bool s_IsSeqID(string & line)
{
    static const int kMainAccSize = 32;
    size_t digit_pos = line.find_last_of("0123456789|", kMainAccSize);
    if (digit_pos != NPOS) {
    	return true;
    }

    return false;
}

int
CBlastNodeInputReader::GetQueryBatch(string & queries, int & query_no)
{
	CNcbiOstrstream ss;
	int q_size = 0;
	int q_count = 0;
	queries.clear();
	query_no = -1;

    while ( !AtEOF()) {
       	string line = NStr::TruncateSpaces_Unsafe(*++(*this), NStr::eTrunc_Begin);
	    if (line.empty()) {
	    	continue;
	    }
	    char c =line[0];
	    if (c == '!'  ||  c == '#' || c == ';') {
	    	continue;
	    }
	    bool isId = s_IsSeqID(line);
	    if ( isId || ( c == '>' )) {
	    	if (q_size >= m_QueryBatchSize) {
	    		UngetLine();
	    		break;
	    	}
	    	q_count ++;
	    }
	    if (c != '>') {
	    	q_size += isId? m_EstAvgQueryLength : line.size();
	    }
     	ss << line << endl;
    }
    ss << std::ends;
    ss.flush();
    if (q_count > 0){
    	queries = ss.str();
    	query_no = m_QueryCount +1;
    	m_QueryCount +=q_count;
    }
    return q_count;
}
