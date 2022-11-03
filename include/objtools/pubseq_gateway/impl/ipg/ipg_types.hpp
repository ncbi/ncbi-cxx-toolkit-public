#ifndef OBJTOOLS__PUBSEQ_GATEWAY__IPG__IPG_TYPES_HPP_
#define OBJTOOLS__PUBSEQ_GATEWAY__IPG__IPG_TYPES_HPP_
/*****************************************************************************
*  $Id$
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
v*  Government do not and cannot warrant the performance or results that
*  may be obtained by using this software or data. The NLM and the U.S.
*  Government disclaim all warranties, express or implied, including
*  warranties of performance, merchantability or fitness for any particular
*  purpose.
*
*  Please cite the author in any work or product based on this material.
*
*  Common facilities to work with WGS VDB database.
*
*****************************************************************************/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbitime.hpp>

#include <objtools/pubseq_gateway/impl/ipg/ipg_exception.hpp>

#include <functional>
#include <tuple>
#include <vector>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(ipg)

using TIpg = Int8;
using TGbState = Int4;
using TCdsValue = Int4;
using TDigest = vector<unsigned char>;
using TIpgWeights = vector<double>;
using TPubMedIds = set<Int4>;

enum class EIpgProteinFlags : Int4
{
    ePartial = 1,
    eRemote = 2,
    eUnverified = 4,
    eMatPeptide = 8
};

enum class EIpgSubgroupHashType : Int4
{
    eUndefined = 0,
    eMurmur3Split16 = 1,
};

enum class EIpgSubgroupsStatus : Int4
{
    eUndefined = 0,
    eNew = 1,
    eActive = 2,
    eWriteProtected = 3,
    eDisabled = 4,
};

struct SIpgSubgroupsConfig
{
    using TCIterator = vector<int32_t>::const_iterator;

    TIpg ipg{TIpg()};
    EIpgSubgroupsStatus status{EIpgSubgroupsStatus::eUndefined};
    EIpgSubgroupHashType hash_type{EIpgSubgroupHashType::eUndefined};
    vector<int32_t> subgroups;

    bool IsReadable() const
    {
        return status == EIpgSubgroupsStatus::eActive
            || status == EIpgSubgroupsStatus::eWriteProtected;
    }

    bool IsWriteable() const
    {
        switch(status) {
        case EIpgSubgroupsStatus::eActive: return true;
        case EIpgSubgroupsStatus::eWriteProtected:
            NCBI_THROW(CIpgStorageException, eIpgUpdateReportWriteProtected,
                string("IPG report '") + to_string(ipg) + "' is write protected."
            );
        default: return false;
        }
    }
};

struct SIpgCds
{
    using TCdsTuple = tuple<TCdsValue, TCdsValue, TCdsValue>;

    SIpgCds() = default;
    SIpgCds(TCdsTuple const& cds)
        : start(get<0>(cds))
        , stop(get<1>(cds))
        , strand(get<2>(cds))
    {
    }

    SIpgCds(TCdsValue vstart, TCdsValue vstop, TCdsValue vstrand)
        : start(vstart)
        , stop(vstop)
        , strand(vstrand)
    {
    }

    TCdsTuple AsTuple() const
    {
        return TCdsTuple(start, stop, strand);
    }

    string AsString() const
    {
        return "(" + to_string(start) + "," + to_string(stop) + "," + to_string(strand) + ")";
    }

    bool IsEmpty() const
    {
        return start == 0 && stop == 0 && strand == 0;
    }

    TCdsValue start{0};
    TCdsValue stop{0};
    TCdsValue strand{0};
};

using TIpgCds = SIpgCds;

END_SCOPE(ipg)
END_NCBI_SCOPE

#endif  // OBJTOOLS__PUBSEQ_GATEWAY__IPG__IPG_TYPES_HPP_
