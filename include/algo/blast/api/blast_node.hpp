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
 * Authors: Amelia Fong
 *
 */

/** @file blast_node.hpp
 *  BLAST node api
 */

#ifndef ALGO_BLAST_API___BLAST_NODE__HPP
#define ALGO_BLAST_API___BLAST_NODE__HPP

#include <algo/blast/core/blast_export.h>
#include <algo/blast/api/blast_aux.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

class NCBI_XBLAST_EXPORT CBlastNodeMsg : public CObject
{
public:
	enum EMsgType {
		eRunRequest,
		ePostResult,
		eErrorExit,
		ePostLog
	};
	CBlastNodeMsg(EMsgType type, void * obj_ptr): m_MsgType(type), m_Obj(obj_ptr) {}
	EMsgType GetMsgType() { return m_MsgType; }
	void * GetMsgBody() { return m_Obj; }
private:
	EMsgType m_MsgType;
	void * m_Obj;
};

class NCBI_XBLAST_EXPORT CBlastNodeMailbox : public CObject
{
public:
	CBlastNodeMailbox(int node_num, CConditionVariable & notify): m_NodeNum(node_num), m_Notify(notify){}
	void SendMsg(CRef<CBlastNodeMsg> msg);
	CRef<CBlastNodeMsg> ReadMsg()
	{
		CFastMutexGuard guard(m_Mutex);
		CRef<CBlastNodeMsg> rv;
		if (! m_MsgQueue.empty()){
			rv.Reset(m_MsgQueue.front());
			m_MsgQueue.pop_front();
		}
		return rv;
	}
	void UnreadMsg(CRef<CBlastNodeMsg> msg) { CFastMutexGuard guard(m_Mutex); m_MsgQueue.push_front(msg);}
	int GetNumMsgs () { CFastMutexGuard guard(m_Mutex); return m_MsgQueue.size(); }
	int GetNodeNum() { return m_NodeNum; }
	~CBlastNodeMailbox() { m_MsgQueue.resize(0); }
private:
	int m_NodeNum;
	CConditionVariable & m_Notify;
	list <CRef<CBlastNodeMsg> > m_MsgQueue;
	CFastMutex m_Mutex;
};

class NCBI_XBLAST_EXPORT CBlastNode : public CThread
{
public :
	enum EState {
		eInitialized,
		eRunning,
		eError,
		eDone,
	};
	CBlastNode (int node_num, const CNcbiArguments & ncbi_args, const CArgs& args,
			    CBlastAppDiagHandler & bah, int query_index, int num_queries, CBlastNodeMailbox * mailbox);

	 virtual int GetBlastResults(string & results) = 0;
	 int GetNodeNum() { return m_NodeNum;}
	 EState GetState() { return m_State; }
	 int GetStatus() { return m_Status; }
	 const CArgs & GetArgs() { return m_Args; }
	 CBlastAppDiagHandler & GetDiagHandler() { return m_Bah; }
	 const CNcbiArguments & GetArguments() { return m_NcbiArgs; }
	 void SendMsg(CBlastNodeMsg::EMsgType msg_type, void* ptr = NULL);
	 string & GetNodeIdStr() { return m_NodeIdStr;}
	 int GetNumOfQueries() {return m_NumOfQueries;}
	 int GetQueriesLength() {return m_QueriesLength;}
protected:
   	virtual ~CBlastNode(void);
   	virtual void* Main(void) = 0;
	void SetState(EState state) { m_State = state; }
	void SetStatus(int status) { m_Status = status; }
	void SetQueriesLength(int l) { m_QueriesLength = l;}
	int m_NodeNum;
private:
	const CNcbiArguments & m_NcbiArgs;
	const CArgs & m_Args;
	 CBlastAppDiagHandler & m_Bah;
	int m_QueryIndex;
	int m_NumOfQueries;
	string m_NodeIdStr;
	CRef<CBlastNodeMailbox> m_Mailbox;
	EState m_State;
	int m_Status;
	int m_QueriesLength;
};


class NCBI_XBLAST_EXPORT CBlastMasterNode
{
public:
	CBlastMasterNode(CNcbiOstream & out_stream, int num_threads);
	typedef map<int, CRef<CBlastNodeMailbox> > TPostOffice;
	typedef map<int, CRef<CBlastNode> > TRegisteredNodes;
	typedef map<int, double> TActiveNodes;
	typedef map<int, CRef<CBlastNodeMsg> > TFormatQueue;
	void RegisterNode(CBlastNode * node, CBlastNodeMailbox * mailbox);
	int GetNumNodes() { return m_RegisteredNodes.size();}
	int IsFull();
	void Shutdown() { m_MaxNumNodes = -1; }
	bool Processing();
	int IsActive()
	{
		if ((m_MaxNumNodes < 0) && (m_RegisteredNodes.size() == 0)){
			return false;
		}
		return true;
	}
	void FormatResults();
	CConditionVariable & GetBuzzer() {return m_NewEvent;}
	~CBlastMasterNode() {}
	int GetNumOfQueries() { return m_NumQueries; }
	Int8 GetQueriesLength() { return m_QueriesLength; }
	int GetNumErrStatus() { return m_NumErrStatus; }
private:
	void x_WaitForNewEvent();

	CNcbiOstream & m_OutputStream;
	int m_MaxNumThreads;
	int m_MaxNumNodes;
	CFastMutex m_Mutex;
	CStopWatch m_StopWatch;
	TPostOffice m_PostOffice;
	TRegisteredNodes m_RegisteredNodes;
	TActiveNodes m_ActiveNodes;
	TFormatQueue m_FormatQueue;
	CConditionVariable m_NewEvent;
	int m_NumErrStatus;
	int m_NumQueries;
	Int8 m_QueriesLength;
};


class NCBI_XBLAST_EXPORT CBlastNodeInputReader : public CStreamLineReader
{
public:

	CBlastNodeInputReader(CNcbiIstream& is, int batch_size, int est_avg_len) :
		CStreamLineReader(is), m_QueryBatchSize(batch_size), m_EstAvgQueryLength(est_avg_len), m_QueryCount(0) {}

	int GetQueryBatch(string & queries, int & query_no);

private:
	const int m_QueryBatchSize;
	const int m_EstAvgQueryLength;
	int m_QueryCount;
};

END_SCOPE(blast)
END_NCBI_SCOPE

#endif /* ALGO_BLAST_API___BLAST_NODE__HPP */
