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
 * Authors: Dmitri Dmitrienko
 *
 * File Description:
 *
 *  useful utilities
 *
 */

#include <ncbi_pch.hpp>

#include <sys/time.h>

#include <iostream>
#include <objtools/pubseq_gateway/impl/diag/IdLogUtl.hpp>

namespace IdLogUtil {

USING_NCBI_SCOPE;

void DumpBinBuffer(const unsigned char* buf, size_t len) {
	for (size_t i = 0; i < len; i++)
		NcbiCout << setfill('0') << setw(2) << hex << (int)buf[i];
	NcbiCout << dec << NcbiEndl;
}

void DumpBinBuffer(const unsigned char* buf, size_t len, const string& filename) {
	filebuf fb;
	if (fb.open( filename.c_str(), ios::out | ios::binary)) {
		fb.sputn(reinterpret_cast<const char*>(buf), len);
		fb.close();		
	}
}

void StringFromFile(const string& filename, string& str) {
    filebuf fb;
    if (fb.open( filename.c_str(), ios::in | ios::binary)) {
	    streampos sz = fb.pubseekoff(0, ios_base::end);
	    fb.pubseekoff(0, ios_base::beg);
	    str.resize((size_t)sz);
	    if (sz > 0) {
		    fb.sgetn(&(str.front()), sz);
		}
	    fb.close();
	}
}

void StringToFile(const string& filename, const string& str) {
    filebuf fb;
    if (fb.open( filename.c_str(), ios::out | ios::binary)) {
	    fb.sputn(&(str.front()), str.size());
	    fb.close();
	}
}

void StringReplace(string& where, const string& what, const string& replace) {
	size_t pos = 0;
	while (pos < where.size()) {
		size_t nx = where.find(what, pos);
		if (nx != string::npos)
			where.replace(nx, what.size(), replace);
		else
			break;
		pos = nx + what.size();
	}
}

string StringFetch(string& src, char delim) {
	auto pos = src.find(delim);
    string rv;
    if (pos == string::npos) {
        rv = src;
        src.clear();
    }
    else {
        rv = src.substr(0, pos);
        src = src.substr(pos + 1);
    }
    return rv;
}

int64_t gettime(void) {
	int cnt = 100;

	struct timeval tv = {0};
	while (gettimeofday(&tv, NULL) != 0 && cnt-- > 0);
	return (int64_t)tv.tv_usec + ((int64_t)tv.tv_sec) * 1000000L;
}

int64_t gettime_ns(void) {
	int cnt = 100;
	struct timeval tv = {0};
	while (gettimeofday(&tv, NULL) != 0 && cnt-- > 0);
	return (int64_t)tv.tv_usec * 1000L + ((int64_t)tv.tv_sec) * 1000000000L;
}

string Int64ToDt(int64_t dt, bool inUTC) {
	char buf[64], bufms[64];
	struct tm tm;
	time_t t = (time_t)(dt / 1000L);
	if (inUTC)
		localtime_r(&t, &tm);
	else {
		gmtime_r(&t, &tm);
	}
	strftime(buf, sizeof(buf), "%m/%d/%Y %H:%M:%S", &tm);
	snprintf(bufms, sizeof(bufms), "%s.%03u", buf, (unsigned int)(dt % 1000L));
	return string(bufms);
}

string TrimRight(const string& s, const string& delimiters) {
	return s.substr( 0, s.find_last_not_of( delimiters ) + 1 );
}

string TrimLeft(const string& s, const string& delimiters) {
	return s.substr( s.find_first_not_of( delimiters ) );
}

string Trim(const string& s, const string& delimiters) {
	return TrimLeft(TrimRight(s, delimiters), delimiters);
}


};

