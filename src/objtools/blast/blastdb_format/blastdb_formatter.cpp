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
 * Author: Christiam Camacho
 *
 */

/** @file blastdb_formatter.cpp
 *  Implementation of the CBlastDbFormatter class
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = 
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <ncbi_pch.hpp>
#include <objtools/blast/blastdb_format/invalid_data_exception.hpp>
#include <objtools/blast/blastdb_format/blastdb_formatter.hpp>
#include <numeric>      // for std::accumulate

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

CBlastDbFormatter::CBlastDbFormatter(const string& fmt_spec)
    : m_FmtSpec(fmt_spec)
{
    // Record where the offsets where the replacements must occur
    for (SIZE_TYPE i = 0; i < m_FmtSpec.size(); i++) {
        if (m_FmtSpec[i] == '%' && m_FmtSpec[i+1] == '%') {
            // remove the escape character for '%'
            m_FmtSpec.erase(i++, 1);
            continue;
        }

        if (m_FmtSpec[i] == '%') {
            m_ReplOffsets.push_back(i);
            m_ReplacementTypes.push_back(m_FmtSpec[i+1]);
        }
    }

    if (m_ReplOffsets.empty() || 
        m_ReplacementTypes.size() != m_ReplOffsets.size()) {
        NCBI_THROW(CInvalidDataException, eInvalidInput,
                   "Invalid format specification");
    }
}

/// Proxy class for retrieving meta data from a BLAST DB
class CBlastDbMetadata {
public:
    CBlastDbMetadata(const SSeqDBInitInfo& db_init_info)
        : m_DbInitInfo(db_init_info)
    {}

    string GetFileName() const {
        return NStr::Replace(m_DbInitInfo.m_BlastDbName, "\"", kEmptyStr);
    }
    string GetMoleculeType() const {
        return CSeqDB::ESeqType2String(m_DbInitInfo.m_MoleculeType);
    }
    string GetTitle() {
        x_InitBlastDb();
        return m_BlastDb->GetTitle();
    }
    string GetDate() {
        x_InitBlastDb();
        return m_BlastDb->GetDate();
    }
    string GetNumberOfSequences() {
        x_InitBlastDb();
        // FIXME: should this use CSeqDB::GetTotals?
        return NStr::IntToString(m_BlastDb->GetNumSeqs());
    }
    string GetDbLength() {
        x_InitBlastDb();
        // FIXME: should this use CSeqDB::GetTotals?
        return NStr::UInt8ToString(m_BlastDb->GetTotalLength());
    }
    string GetDiskUsage() {
        x_InitBlastDb();
        return NStr::UInt8ToString((Uint8)m_BlastDb->GetDiskUsage());
    }

private:
    /// Information to initialize the BLAST DB handle
    SSeqDBInitInfo m_DbInitInfo;
    /// BLAST DB handle
    CRef<CSeqDB> m_BlastDb;

    /// Initialize and cache BLAST DB handle if necessary
    void x_InitBlastDb() {
        if (m_BlastDb.Empty()) {
            m_BlastDb = m_DbInitInfo.InitSeqDb();
        }
        _ASSERT(m_BlastDb.NotEmpty());
    }
};

string
CBlastDbFormatter::Write(const SSeqDBInitInfo& db_init_info)
{
    CBlastDbMetadata dbmeta(db_init_info);
    vector<string> data2write;
    data2write.reserve(m_ReplacementTypes.size());
    ITERATE(vector<char>, fmt, m_ReplacementTypes) {
        switch (*fmt) {
        case 'f':   // file name
            data2write.push_back(dbmeta.GetFileName());
            break;
        case 't':   // title
            data2write.push_back(dbmeta.GetTitle());
            break;
        case 'n':   // number of sequences
            data2write.push_back(dbmeta.GetNumberOfSequences());
            break;
        case 'l':   // DB length
            data2write.push_back(dbmeta.GetDbLength());
            break;
        case 'p':   // molecule type
            data2write.push_back(dbmeta.GetMoleculeType());
            break;
        case 'd':   // date of last update
            data2write.push_back(dbmeta.GetDate());
            break;
        case 'U':   // Disk usage
            data2write.push_back(dbmeta.GetDiskUsage());
            break;
        default:
            CNcbiOstrstream os;
            os << "Unrecognized format specification: '%" << *fmt << "'";
            NCBI_THROW(CInvalidDataException, eInvalidInput, 
                       CNcbiOstrstreamToString(os));
        }
    }
    return x_Replacer(data2write);
}

/// Auxiliary functor to compute the length of a string (shamlessly copied from
/// seq_writer.cpp)
struct StrLenAdd : public binary_function<SIZE_TYPE, const string&, SIZE_TYPE>
{
    SIZE_TYPE operator() (SIZE_TYPE a, const string& b) const {
        return a + b.size();
    }
};

// also inspired by seq_writer.cpp
string
CBlastDbFormatter::x_Replacer(const vector<string>& data2write) const
{
    SIZE_TYPE data2write_size = accumulate(data2write.begin(), data2write.end(),
                                           0, StrLenAdd());
    string retval;
    retval.reserve(m_FmtSpec.size() + data2write_size -
                   (data2write.size() * 2));

    SIZE_TYPE fmt_idx = 0;
    for (SIZE_TYPE i = 0, kSize = m_ReplOffsets.size(); i < kSize; i++) {
        retval.append(&m_FmtSpec[fmt_idx], &m_FmtSpec[m_ReplOffsets[i]]);
        retval.append(data2write[i]);
        fmt_idx = m_ReplOffsets[i] + 2;
    }
    if (fmt_idx <= m_FmtSpec.size()) {
        retval.append(&m_FmtSpec[fmt_idx], &m_FmtSpec[m_FmtSpec.size()]);
    }

    return retval;
}

END_NCBI_SCOPE
