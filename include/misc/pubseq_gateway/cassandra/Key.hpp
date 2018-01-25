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
 *  List and map of entities
 *
 */

#ifndef _ENT_HPP_
#define _ENT_HPP_
 
#include <unordered_map>
#include <vector>
#include <corelib/ncbistd.hpp>

#include "IdLogUtil/AutoBuf.hpp"
#include "IdLogUtil/IdLogUtl.hpp"
#include "IdCassScope.hpp"
#include "IdCassScope.hpp"


BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

// if blob has large chunks, bfComplete flag indicates whether chunks are successfully inserted or updated
#define bfComplete		0x0000000001
#define bfPacked		0x0000000002
#define bfCheckFailed	0x0000000004

struct SBlobStat {
	int64_t modified;
	int64_t flags;
	SBlobStat() :
		modified(0),
		flags(0)
	{}
    void Reset() {
        modified = 0;
        flags = 0;
    }
};

struct CBlob {
	IdLogUtil::CAutoBuf blob;
	SBlobStat stat;
	int restart_from;
	bool no_data;
	bool error;
    string error_str;
	CBlob() : restart_from(0), no_data(true), error(false) {}
	void Reset(bool is_restart, uint64_t MaxBlobSize = 1L * 1024 * 1024) {
		no_data = true;
		error = false;
        error_str.clear();
		if (is_restart) {
			if (blob.Size() > 0 && restart_from == 0) {
				blob.Reset(blob.Limit());
			}
		}
		else {
			stat.modified = 0;
			stat.flags = 0;
			restart_from = 0;
			blob.Reset(MaxBlobSize);
		}
	}
};

struct SBlobFullStat {
	int64_t modified;
	int64_t flags;
	int64_t size;
	bool seen;
    void Reset() {
        modified = 0;
        flags = 0;
        size = 0;
        seen = false;
    }
};

class CBlobFullStatMap;

class CBlobFullStatVec: public deque<pair<int32_t, SBlobFullStat>> {
public:
	void Copy(const CBlobFullStatMap& src);
	void Append(const CBlobFullStatMap& src);
	void Append(const CBlobFullStatVec& src);
};

typedef vector<int32_t> keylist_t;

class CBlobFullStatMap: public unordered_map<int32_t, SBlobFullStat> {
public:
	void Copy(const CBlobFullStatMap& src);
	void Append(const CBlobFullStatMap& src);
	void Append(const CBlobFullStatVec& src);
};

template<typename M>
void SaveKeysToFile(const M& keymap, const string& file_name) {
    const int LEN = 128 * 1024;
    char buffer[LEN];
    ofstream kfile;
    kfile.rdbuf()->pubsetbuf(buffer, LEN);
    kfile.open(file_name, ios::out);
    if (kfile.is_open()) {
        for (const auto& it : keymap) {
            char buf[1024];
            int len = snprintf(buf, sizeof(buf), "%d %ld %ld %ld\n", it.first, it.second.modified, it.second.size, it.second.flags);
            kfile.write(buf, len);
        }
        kfile.close();
    }
    else
        RAISE_ERROR(eGeneric, string("failed to open key file: \"") + file_name + "\"");
}


END_IDBLOB_SCOPE

#endif
