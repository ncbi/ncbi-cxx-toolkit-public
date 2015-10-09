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
 * Author:  Kevin Bealer
 *
 */

/// @file writedb_isam.cpp
/// Implementation for the CWriteDB_Isam and related classes.

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <ncbi_pch.hpp>
#include <corelib/tempstr.hpp>
#include <objtools/blast/seqdb_writer/writedb_error.hpp>
#include <objtools/blast/seqdb_writer/writedb_isam.hpp>
#include <objtools/blast/seqdb_writer/writedb_convert.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/serial.hpp>
#include <objects/seqloc/seqloc__.hpp>
#include <objects/general/general__.hpp>
#include <stdio.h>
#include <sstream>

BEGIN_NCBI_SCOPE

/// Import C++ std namespace.
USING_SCOPE(std);

/// Compute the file extension for an ISAM file.
///
/// @param itype The type of ID data stored in the file.
/// @param protein True if the database type is protein.
/// @param is_index True for the index file (i.e. pni).
/// @return The three letter extension as a string.
static string
s_IsamExtension(EWriteDBIsamType itype,
                bool             protein,
                bool             is_index)
{
    char type_ch = '?';

    switch(itype) {
    case ePig:
        type_ch = 'p';
        break;

    case eGi:
        type_ch = 'n';
        break;

    case eAcc:
        type_ch = 's';
        break;

    case eTrace:
        type_ch = 't';
        break;

    case eHash:
        type_ch = 'h';
        break;

    default:
        NCBI_THROW(CWriteDBException,
                   eArgErr,
                   "Not implemented.");
    }

    string extn("???");
    extn[0] = protein ? 'p' : 'n';
    extn[1] = type_ch;
    extn[2] = is_index ? 'i' : 'd';

    return extn;
}

CWriteDB_Isam::CWriteDB_Isam(EIsamType      itype,
                             const string & dbname,
                             bool           protein,
                             int            index,
                             Uint8          max_file_size,
                             bool           sparse)
{
    m_DFile.Reset(new CWriteDB_IsamData(itype,
                                        dbname,
                                        protein,
                                        index,
                                        max_file_size));

    m_IFile.Reset(new CWriteDB_IsamIndex(itype,
                                         dbname,
                                         protein,
                                         index,
                                         m_DFile,
                                         sparse));
}

CWriteDB_Isam::~CWriteDB_Isam()
{
}

bool CWriteDB_Isam::CanFit(int num)
{
    return m_IFile->CanFit(num);
}

void CWriteDB_Isam::AddIds(int oid, const TIdList & idlist)
{
    m_IFile->AddIds(oid, idlist);
}

void CWriteDB_Isam::AddPig(int oid, int pig)
{
    m_IFile->AddPig(oid, pig);
}

void CWriteDB_Isam::AddHash(int oid, int hash)
{
    m_IFile->AddHash(oid, hash);
}

void CWriteDB_Isam::Close()
{
    // Index must be closed first, because ISAM indices are built in
    // memory until the volume is closed, and only then is anything
    // written to disk.

    m_IFile->Close();
    m_DFile->Close();
}

void CWriteDB_Isam::RenameSingle()
{
    m_IFile->RenameSingle();
    m_DFile->RenameSingle();
}

CWriteDB_IsamIndex::CWriteDB_IsamIndex(EWriteDBIsamType        itype,
                                       const string          & dbname,
                                       bool                    protein,
                                       int                     index,
                                       CRef<CWriteDB_IsamData> datafile,
                                       bool                    sparse)
    : CWriteDB_File  (dbname,
                      s_IsamExtension(itype, protein, true),
                      index,
                      0,
                      false),
      m_Type         (itype),
      m_Sparse       (sparse),
      m_PageSize     (0),
      m_BytesPerElem (0),
      m_DataFileSize (0),
      m_UseInt8      (false),
      m_DataFile     (datafile),
      m_Oid          (-1)
{
    // This is the one case where I don't worry about file size; if
    // the data file can hold the relevant data, the index file can
    // too.  The index file can be larger than the data file, but only
    // if there are less than (about) 9 entries; at that size I'm not
    // concerned about the byte limits.

    if (itype == eAcc || itype == eHash) {
        // If there is a maximum string size it's something large like
        // 4k per table line.  In practice, string table rows tend to
        // be less than 50 characters, with the median probably around
        // 15-20.  There is probably little harm in overestimating
        // this number so long as the overestimate is small relative
        // to the max file size.
        //
        // String indices normally have several versions of each key;
        // the number below represents 16 versions * 64 bytes per row.
        // This is far more than any current sequence currently uses.
        // If this is not enough, the max size limit may be violated
        // (but see the last sentence of paragraph 1).

        m_BytesPerElem = 1024;
        m_PageSize = 64;
    } else {
        m_BytesPerElem = 8;
        m_PageSize = 256;
    }
}

CWriteDB_IsamIndex::~CWriteDB_IsamIndex()
{
    m_OidStringData.clear();
}

void CWriteDB_IsamIndex::x_WriteHeader()
{
    int isam_version  = 1;
    int isam_type     = 0;
    int num_terms     = 0;
    int max_line_size = 0;

    switch(m_Type) {
    case eGi:
    case ePig:
    case eTrace:
        // numeric w/ int4 data or numeric w/ int8 data.
        isam_type = m_UseInt8 ? eIsamNumericLong : eIsamNumericType;
        num_terms = (int) m_NumberTable.size();
        max_line_size = 0;
        break;

    case eAcc:
    case eHash:
        isam_type = eIsamStringType; // string w/ data
        max_line_size = eMaxStringLine;
        num_terms = m_StringSort.Size();
        break;

    default:
        NCBI_THROW(CWriteDBException,
                   eArgErr,
                   "Unknown id type specified.");
    }

    int samples = s_DivideRoundUp(num_terms, m_PageSize);

    // These should probably use be a WriteInt4 method which should be
    // added to the CWriteDB_File class.

    WriteInt4(isam_version);
    WriteInt4(isam_type);
    WriteInt4((int)m_DataFileSize);

    WriteInt4(num_terms);
    WriteInt4(samples);
    WriteInt4(m_PageSize);

    WriteInt4(max_line_size);
    WriteInt4(m_Sparse ? 1 : 0);
    WriteInt4(0);
}

void CWriteDB_IsamIndex::x_FlushStringIndex()
{
    _ASSERT(m_StringSort.Size());

    // Note: This function can take a noticeable portion of the
    // database dumping time.  For some databases, the length of the
    // data file for the string index competes with the sequence file
    // to determine volumes seperation points.

    // String ISAM files have four parts.  First, the standard
    // meta-data header.  Then (at address 36), we have a table of
    // page offsets.  After this is a table of key offsets in the
    // index file, then finally the list of keys.

    int data_pos = 0;
    unsigned count = m_StringSort.Size();

    unsigned nsamples = s_DivideRoundUp(count, m_PageSize);

    string key_buffer;
    vector<int> key_off;

    // Reserve enough room, throwing in some extra.  Since we excerpt
    // every 64th entry, dividing by 64 would be exactly balanced.  To
    // make reallocation rare, I divide by 63 instead, and throw in an
    // extra 16 bytes.

    key_buffer.reserve((int)(m_DataFileSize/63 + 16));
    key_off.reserve(nsamples);

    unsigned i(0);

    string NUL("x");
    NUL[0] = (char) 0;

    int output_count = 0;
    int index = 0;

    m_StringSort.Sort();

    CWriteDB_PackedSemiTree::Iterator iter = m_StringSort.Begin();
    CWriteDB_PackedSemiTree::Iterator end_iter = m_StringSort.End();

    string element, prev_elem;

    // A string containing a NUL cannot possibly be valid, so I'm
    // using one as the "not set yet" value.

    element.resize(1);
    element[0] = char(0);

    while(iter != end_iter) {
        prev_elem.swap(element);
        iter.Get(element);

        if (prev_elem == element) {
            ++iter;
            continue;
        }

        // For each element whose index is a multiple of m_PageSize
        // (starting with element zero), we add the record to the
        // index file.

        // The page offset table can be written as it comes in, but
        // the key offsets and keys are not written until all the page
        // offsets have been written, so they are accumulated in a
        // vector (key_off) and string (key_buffer) respectively.

        if ((output_count & (m_PageSize-1)) == 0) {
            // Write the data file position to the index file.

            WriteInt4(data_pos);

            // Store the overall index file position where the key
            // will be written.

            key_off.push_back((int) key_buffer.size());

            // Store the string record for the index file (but this
            // string is NUL terminated, whereas the data file rows
            // are line feed (aka newline) terminated.

            key_buffer.append(element.data(), element.length()-1);
            key_buffer.append(NUL);
        }
        output_count ++;

        data_pos = m_DataFile->Write(element);
        index ++;

        ++iter;
    }

    // Write the final data position.

    WriteInt4(data_pos);

    // Push back the final buffer offset.

    key_off.push_back((int) key_buffer.size());

    int key_off_start = eKeyOffset + (nsamples + 1) * 8;

    // Write index file offsets of keys.

    for(i = 0; i < key_off.size(); i++) {
        WriteInt4(key_off[i] + key_off_start);
    }

    // Write buffer of keys.

    Write(key_buffer);
}

void CWriteDB_IsamIndex::x_FlushNumericIndex()
{
    _ASSERT(m_NumberTable.size());

    int row_index = 0;

    sort(m_NumberTable.begin(), m_NumberTable.end());

    int count = (int) m_NumberTable.size();

    const SIdOid * prevp = 0;

    // Note: could strip out code for 8/4 detection; then reorder this
    // to sort the table first.  At that point, 8 byte detection could
    // be done simply by looking at the last (highest) element.  This
    // would have the effect of making ISAM file size overflow less
    // accurate (since 8 byte detection is what triggers recomputation
    // of ISAM data size for 12 byte entries rather than 8).  It is
    // not likely that this would have any impact since ISAM files (or
    // at least TIs) are probably guaranteed not to exceed the limit;
    // in any case a conservative estimate of 12 bytes per ID could be
    // used for numeric or just for TI indices.

    if (m_UseInt8) {
        for(int i = 0; i < count; i++) {
            const SIdOid & elem = m_NumberTable[i];

            if (prevp && (*prevp == elem)) {
                continue;
            } else {
                prevp = & elem;
            }

            if ((row_index & (m_PageSize-1)) == 0) {
                WriteInt8(elem.id());
                WriteInt4(elem.oid());
            }

            m_DataFile->WriteInt8(elem.id());
            m_DataFile->WriteInt4(elem.oid());
            row_index ++;
        }

        // 64 bit numeric files end in (max-uint8, 0).

        WriteInt8(-1);
        WriteInt4(0);
    } else {
        for(int i = 0; i < count; i++) {
            const SIdOid & elem = m_NumberTable[i];

            if (prevp && (*prevp == elem)) {
                continue;
            } else {
                prevp = & elem;
            }

            if ((row_index & (m_PageSize-1)) == 0) {
                WriteInt4(elem.id());
                WriteInt4(elem.oid());
            }

            m_DataFile->WriteInt4(elem.id());
            m_DataFile->WriteInt4(elem.oid());
            row_index ++;
        }

        // 32 bit numeric files end in (max-uint4, 0).

        WriteInt4(-1);
        WriteInt4(0);
    }
}

void CWriteDB_IsamIndex::x_Flush()
{
    if (m_NumberTable.size() || m_StringSort.Size()) {
        Create();
        m_DataFile->Create();

        // Step 1: Write header data.
        x_WriteHeader();

        // Step 2: Flush all data to the data file.
        //  A. Sort all entries up front with sort.
        //  B. Pick out periodic samples for the index, every 256 elements
        //     for numeric and every 64 for string.

        if (m_Type == eAcc || m_Type == eHash) {
            x_FlushStringIndex();
        } else {
            x_FlushNumericIndex();
        }
    }

    x_Free();
}

void CWriteDB_IsamIndex::AddIds(int oid, const TIdList & idlist)
{
    if (m_Type == eAcc) {
        x_AddStringIds(oid, idlist);
    } else if (m_Type == eGi) {
        x_AddGis(oid, idlist);
    } else if (m_Type == eTrace) {
        x_AddTraceIds(oid, idlist);
    } else {
        NCBI_THROW(CWriteDBException,
                   eArgErr,
                   "Cannot call AddIds() for this index type.");
    }
}

void CWriteDB_IsamIndex::AddPig(int oid, int pig)
{
    SIdOid row(pig, oid);
    m_NumberTable.push_back(row);
    m_DataFileSize += 8;
}

void CWriteDB_IsamIndex::AddHash(int oid, int hash)
{
    char buf[256];
    int sz = sprintf(buf, "%u", (unsigned)hash);

    x_AddStringData(oid, buf, sz);
}

void CWriteDB_IsamIndex::x_AddGis(int oid, const TIdList & idlist)
{
    ITERATE(TIdList, iter, idlist) {
        const CSeq_id & seqid = **iter;

        if (seqid.IsGi()) {
            SIdOid row(GI_TO(Int8, seqid.GetGi()), oid);
            m_NumberTable.push_back(row);
            m_DataFileSize += 8;
        }
    }
}

void CWriteDB_IsamIndex::x_AddTraceIds(int oid, const TIdList & idlist)
{
    ITERATE(TIdList, iter, idlist) {
        const CSeq_id & seqid = **iter;

        if (seqid.IsGeneral() && seqid.GetGeneral().GetDb() == "ti") {
            const CObject_id & obj = seqid.GetGeneral().GetTag();

            Int8 id = (obj.IsId()
                       ? obj.GetId()
                       : NStr::StringToInt8(obj.GetStr()));

            SIdOid row(id, oid);
            m_NumberTable.push_back(row);

            if (m_UseInt8) {
                m_DataFileSize += 12;
            } else if (id >= kMax_Int) {
                // Adjust the data file size to account for the
                // already-stored IDs.

                m_UseInt8 = true;
                m_DataFileSize /= 8;
                m_DataFileSize *= 12;
                m_DataFileSize += 12;
            } else {
                m_DataFileSize += 8;
            }
        }
    }
}

void CWriteDB_IsamIndex::x_AddStringIds(int oid, const TIdList & idlist)
{
    // Build all sub-string objects and add those.

    ITERATE(TIdList, iter, idlist) {
        const CSeq_id & seqid = **iter;

        switch(seqid.Which()) {

        case CSeq_id::e_Gi:
            break;

        case CSeq_id::e_Pdb:
            x_AddPdb(oid, seqid);
            break;

        case CSeq_id::e_Local:
            x_AddLocal(oid, seqid);
            break;

        case CSeq_id::e_Patent:
            x_AddPatent(oid, seqid);
            break;

        case CSeq_id::e_General:
            if (! m_Sparse) {
                x_AddStdString(oid, seqid.AsFastaString());
                const CDbtag & dbt = seqid.GetGeneral();
                if (dbt.CanGetTag() && dbt.GetTag().IsStr()) {
                    x_AddStdString(oid, dbt.GetTag().GetStr());
                }
            }
            break;

        // default processing:
        default:
            {
                const CTextseq_id * textid  = seqid.GetTextseq_Id();
                if (textid) {
                    x_AddTextId(oid, *textid);
                } else {
                    string acc = seqid.AsFastaString();
                    x_AddStringData(oid, acc.data(), acc.size());
                }
            }
        }
    }
}

void CWriteDB_IsamIndex::x_AddLocal(int             oid,
                                    const CSeq_id & seqid)
{
    const CObject_id & objid = seqid.GetLocal();

    if (! m_Sparse) {
        x_AddStdString(oid, seqid.AsFastaString());
    }
    if (objid.IsStr()) {
        x_AddStdString(oid, objid.GetStr());
    }
}

void CWriteDB_IsamIndex::x_AddPatent(int             oid,
                                     const CSeq_id & seqid)
{
    if (! m_Sparse) {
        x_AddStdString(oid, seqid.AsFastaString());
    }
}

void CWriteDB_IsamIndex::x_AddPdb(int             oid,
                                  const CSeq_id & seqid)
{
    const CPDB_seq_id & pdb = seqid.GetPdb();

    // Sparse mode:
    //
    // "102l"
    // "102l  "
    // "102l| "
    //
    // Non-sparse mode:
    // "102l"
    // "102l  "
    // "102l| "
    // "pdb|102l| "

    CTempString mol;
    if (pdb.CanGetMol()) {
        mol = pdb.GetMol().Get();
    }
    if (! mol.size()) {
        NCBI_THROW(CWriteDBException,
                   eArgErr,
                   "Empty molecule string in pdb Seq-id.");
    }
    x_AddStringData(oid, mol);

    string full_id = seqid.AsFastaString();
    _ASSERT(full_id.size() > 4);
    if (! m_Sparse) {
        x_AddStdString(oid, full_id);
    }

    string short_id(full_id, 4);
    x_AddStdString(oid, short_id);

    int len = short_id.size();
    if (short_id[len-2] == '|') {
        short_id[len-2] = ' ';
    } else {
        // This is lower case chain encoding, i.e., xxxx|a -> xxxx|AA
        _ASSERT(short_id[len-1] == short_id[len-2]);
        _ASSERT(short_id[len-3] == '|');
        short_id[len-3] = ' ';
    }
    x_AddStdString(oid, short_id);
}

/// Compare two strings, ignoring case.
/// @param a First string.
/// @param b Second string.
bool s_NoCaseEqual(CTempString & a, CTempString & b)
{
    if (a.size() != b.size())
        return false;

    return 0 == NStr::strncasecmp(a.data(), b.data(), a.size());
}

void CWriteDB_IsamIndex::x_AddTextId(int                 oid,
                                     const CTextseq_id & id)
{
    CTempString acc, nm;

    // Note: if there is no accession, the id will not be added to a
    // sparse databases (even if there is a name).

    if (id.CanGetAccession()) {
        acc = id.GetAccession();
    }

    if (id.CanGetName()) {
        nm = id.GetName();
    }

    if (! acc.empty()) {
        x_AddStringData(oid, acc);
    }

    if (! m_Sparse) {
        // Skip name if it is empty or if it is the same as 'acc' when
        // case is ignored.

        if (! (nm.empty() || s_NoCaseEqual(acc, nm))) {
            x_AddStringData(oid, nm);
        }

        int ver = id.CanGetVersion() ? id.GetVersion() : 0;

        if (ver && acc.size()) {
            x_AddString(oid, acc, ver);
        }
    }
}

// All string handling goes through this method.

void CWriteDB_IsamIndex::x_AddStringData(int oid, const char * sbuf, int ssize)
{
    // NOTE: all of the string finagling in this code could probably
    // benefit from some kind of pool-of-strings swap-allocator.
    //
    // It would follow these rules:
    //
    // 1. User asks for string of a certain size and gets string
    //    reserve()d to that size.
    // 2. If pool is empty, it would just use reserve().
    // 3. If pool is not empty, it would take the first entry, use reserve(),
    //    and swap that back to the user.
    //
    // 4. Space management could be done via a large vector or a list
    //    of vectors.  If the first of these, the large vector could
    //    be reallocated by creating a new one and swapping in all
    //    the strings.  Alternately a fixed size pool of strings could
    //    be used.
    //
    // 5. User would need to return strings they were done with.

    char buf[256];

    int sz = ssize;
    memcpy(buf, sbuf, sz);
    _ASSERT(sz);

    // lowercase the 'key' portion
    for(int i = 0; i < sz; i++) {
        buf[i] = tolower(buf[i]);
    }

    buf[sz++] = (char) eKeyDelim;
    sz += sprintf(buf + sz, "%d", oid);
    buf[sz++] = (char) eRecordDelim;

    // fix for SB-218, SB-819
    if (oid != m_Oid) {
        m_Oid = oid;
        m_OidStringData.clear();
    }

    string tmp(buf, sz);
    pair< set<string>::iterator, bool> rv = m_OidStringData.insert(tmp);
    if (rv.second) {
        m_StringSort.Insert(buf, sz);
        m_DataFileSize += sz;
    }
}

void CWriteDB_IsamIndex::x_AddString(int oid, const CTempString & acc, int ver)
{
    _ASSERT(! m_Sparse);

    if (acc.size() && ver) {
        char buf[256];
        memcpy(buf, acc.data(), acc.size());

        int sz = acc.size();
        sz += sprintf(buf + sz, ".%d", ver);

        x_AddStringData(oid, buf, sz);
    }
}

CWriteDB_IsamData::CWriteDB_IsamData(EWriteDBIsamType itype,
                                     const string   & dbname,
                                     bool             protein,
                                     int              index,
                                     Uint8            max_file_size)
    : CWriteDB_File (dbname,
                     s_IsamExtension(itype, protein, false),
                     index,
                     max_file_size,
                     false)
{
}

CWriteDB_IsamData::~CWriteDB_IsamData()
{
}

void CWriteDB_IsamData::x_Flush()
{
}

bool CWriteDB_IsamIndex::CanFit(int num)
{
    return (m_DataFileSize + (num+1) * m_BytesPerElem) < m_MaxFileSize;
}

void CWriteDB_IsamIndex::x_Free()
{
    m_StringSort.Clear();
    vector<SIdOid> tmp;
    m_NumberTable.swap(tmp);
}

void CWriteDB_Isam::ListFiles(vector<string> & files) const
{
    if (! m_IFile->Empty()) {
        files.push_back(m_IFile->GetFilename());
        files.push_back(m_DFile->GetFilename());
    }
}

bool CWriteDB_IsamIndex::Empty() const
{
    // Also test 'created' bit in case the file data has already
    // been dumped and cleared.

    return ! (m_StringSort.Size() ||
              m_NumberTable.size() ||
              m_Created);
}

END_NCBI_SCOPE

