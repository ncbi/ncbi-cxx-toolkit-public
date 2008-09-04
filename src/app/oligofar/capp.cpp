// $Id$
#include <ncbi_pch.hpp>
#include "capp.hpp"
#ifndef _WIN32
#include <sysexits.h>
#else
#define EX_USAGE 64
#endif
#include <iostream>

USING_OLIGOFAR_SCOPES;

CApp::CApp( int argc, char ** argv ) : 
	m_argc(argc), m_argv(argv), m_done(false)
{}

CApp::~CApp()
{}

int CApp::Run()
{
	int rc = ParseArgs();
	if( rc || m_done ) return rc;
	return Execute();
}

void CApp::Version( const char * )
{
	cout << GetProgramBasename() << " compiled on " << __DATE__ << endl;
}

void CApp::Help( const char * )
{
	cout << "usage: [-hV] [--help=[arg]] [--version=[arg]]\n";
}

const char * CApp::GetOptString() const 
{
	 return "";
}

int CApp::ParseArg( int optopt, const char * optarg, int )
{
	switch( optopt ) {
	case 'h': Help( optarg ); break;
	case 'V': Version( optarg ); break;
	default:  return EX_USAGE;
	}
	m_done = true;
	return 0;
}

const option * CApp::GetLongOptions() const 
{
	return 0;
}

int CApp::ParseArgs()
{
	int optopt;
	int longindex = -1;
	int rc = 0;
	string optstring("hV");
	optstring.append(GetOptString());
	while(( optopt = getopt_long( GetArgCount(), GetArgVector(), optstring.c_str(), GetLongOptions(), &longindex )) != EOF )
		rc |= ParseArg( optopt, optarg, longindex );
	return rc;
}

const char * CApp::GetProgramName() const
{
	return m_argv[0];
}

const char * CApp::GetProgramBasename() const 
{
	const char * backslash = strrchr(GetProgramName(), '/');
	return backslash? backslash+1 : GetProgramName();
}

int CApp::GetArgCount() const
{
	return m_argc;
}

int CApp::GetArgIndex() const
{
	return optind;
}

char ** CApp::GetArgVector() const 
{
	return m_argv;
}

char * CApp::GetArg( int i ) const 
{
	return m_argv[i];
}

