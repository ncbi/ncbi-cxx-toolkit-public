#ifndef CHECK_NC_MIRROR_BLOB_HPP
#define CHECK_NC_MIRROR_BLOB_HPP

#include <time.h>
#include <sstream>

USING_NCBI_SCOPE;

#include "Service.h"
#include "Server.h"
/*
class CBlobOperation
{
private:
	stringstream m_ss;

public:
	enum EBlopOperation{ 
		None, Write, Edit, Delete 
	} m_BlobOperation;

	time_t m_Time;

	CBlobOperation( EBlopOperation blob_operation, time_t m_Time=time(0) );

	operator stringstream();
};

class CBlob
{
private:
	string m_Key;

protected:
	CServer m_Server;
	CService m_Service;
	
	CNcbiOstream m_os;

public:
	//enum { IC, NC } m_Type;

	//string m_OriginalScript;
	//time_t m_ScriptStart;

	CBlob( CServer &server, CService &service );

	virtual void Serialise();
	virtual void Deserealise();
};

class CTestBlob: public CBlob
{
public:
	//string m_Server;
	//string m_Service;
	
	//string m_ClientHost;

	CTestBlob( CServer &server, CService &service ): CBlob( server, service ) {}

	virtual void Serialise();
	virtual void Deserealise();
};

class CMasterBlob: public CBlob
{
public:
};
*/
#endif /* CHECK_NC_MIRROR_BLOB_HPP */

