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

#include <ncbi_pch.hpp>

#include <corelib/ncbistd.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/Key.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/IdCassScope.hpp>

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

/** CBlobFullStatMap */

void CBlobFullStatMap::Copy(const CBlobFullStatMap &  src)
{
    clear();
    Append(src);
}


void CBlobFullStatMap::Append(const CBlobFullStatMap &  src)
{
    reserve(src.size());
    for (const auto &  it : src) {
        operator[](it.first) = it.second;
    }
}


void CBlobFullStatMap::Append(const CBlobFullStatVec &  src)
{
    reserve(src.size());
    for (const auto &  it : src) {
        operator[](it.first) = it.second;
    }
}


/** CBlobFullStatVec */

void CBlobFullStatVec::Copy(const CBlobFullStatMap &  src)
{
    clear();
    Append(src);
}

void CBlobFullStatVec::Append(const CBlobFullStatMap &  src)
{
    for (const auto &  it : src) {
        push_back(it);
    }
}

void CBlobFullStatVec::Append(const CBlobFullStatVec &  src)
{
    for (const auto &  it : src) {
        push_back(it);
    }
}

END_IDBLOB_SCOPE
