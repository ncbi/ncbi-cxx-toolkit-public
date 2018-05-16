#ifndef KEY__HPP
#define KEY__HPP

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

#include <unordered_map>
#include <vector>
#include <corelib/ncbistd.hpp>

#include "cass_exception.hpp"
#include "IdCassScope.hpp"


BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

// if blob has large chunks, bfComplete flag indicates whether chunks
// are successfully inserted or updated
#define bfComplete      0x0000000001
#define bfPacked        0x0000000002
#define bfCheckFailed   0x0000000004


struct SBlobStat
{
    SBlobStat() :
        modified(0),
        flags(0)
    {}

    void Reset(void)
    {
        modified = 0;
        flags = 0;
    }

    int64_t     modified;
    int64_t     flags;
};

struct SBlobFullStat
{
    void Reset()
    {
        modified = 0;
        flags = 0;
        size = 0;
        seen = false;
    }

    int64_t     modified;
    int64_t     flags;
    int64_t     size;
    bool        seen;
};


class CBlobFullStatMap;

class CBlobFullStatVec: public deque<pair<int32_t, SBlobFullStat>>
{
public:
    void Copy(const CBlobFullStatMap &  src);
    void Append(const CBlobFullStatMap &  src);
    void Append(const CBlobFullStatVec &  src);
};

typedef vector<int32_t> TKeyList;

class CBlobFullStatMap: public unordered_map<int32_t, SBlobFullStat>
{
public:
    void Copy(const CBlobFullStatMap &  src);
    void Append(const CBlobFullStatMap &  src);
    void Append(const CBlobFullStatVec &  src);
};

END_IDBLOB_SCOPE
#endif
