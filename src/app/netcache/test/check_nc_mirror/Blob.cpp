#include "ncbi_pch.hpp"
#include <connect/services/remote_app.hpp>
#include "Blob.h"
/*
CBlobOperation::operator stringstream()
{
	m_ss << m_BlobOperation << m_Time;

	return m_ss;
}

CBlob::CBlob( CServer &server, CService &service ): 
	m_Server( server ), m_Service( service ), m_os( *(*server).CreateOStream( m_Key ) )
{ }

void CBlob::Serialise()
{
	stringstream ss;

	m_os << m_Key.size() << m_Key;

	ss << m_Server.m_Host << ":" << m_Server.m_Port;
	m_os << ss.str().size() << ss.str();

	m_os << m_Service.m_Name.size() << m_Service.m_Name;

	m_os << (stringstream)CBlobOperation( CBlobOperation::Write );
}

void CTestBlob::Serialise()
{
	CBlob::Serialise();
}
*/
