// $Id$
#ifndef OLIGORAR_CAPP_HPP
#define OLIGOFAR_CAPP_HPP

#include "defs.hpp"
#include "getopt.h"

BEGIN_OLIGOFAR_SCOPES

// General application class - frontend to getopt and POSIX/GNU commandline processing behavior
class CApp
{
public:
	CApp( int argc, char ** argv );
	virtual ~CApp();
	virtual int Run();
protected:
	virtual int  ParseArgs();

	// normally following virtual functions should be reimplemented in subclasses
	virtual int  Execute() = 0;
	virtual void Version();
	virtual void Help();

	virtual const option * GetLongOptions() const;
	virtual const char * GetOptString() const;

	// following function normally should be called as fallback in subclass::ParseArgs()
	virtual int ParseArg( int opt, const char * arg, int longindex );
	
	const char * GetProgramName() const;
	const char * GetProgramBasename() const;
	
	int GetArgCount() const;
	int GetArgIndex() const;
	char ** GetArgVector() const;
	char *  GetArg(int i) const;
protected:
	int m_argc;
	char ** m_argv;
	bool m_done;
};

END_OLIGOFAR_SCOPES

#endif
