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
 * Author:  Ning Ma
 *
 */

/// @file writedb_gimask.cpp
/// Implementation for the CWriteDB_GiMask and related classes.

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <ncbi_pch.hpp>
#include <objtools/blast/seqdb_writer/writedb.hpp>
#include "writedb_gimask.hpp"

BEGIN_NCBI_SCOPE

/// Import C++ std namespace.
USING_SCOPE(std);

#if ((!defined(NCBI_COMPILER_WORKSHOP) || (NCBI_COMPILER_VERSION  > 550)) && \
     (!defined(NCBI_COMPILER_MIPSPRO)) )

// CWriteDB_GiMask

CWriteDB_GiMask::CWriteDB_GiMask(const string      & maskname,
                                 const string      & desc,
                                 Uint8               max_file_size):
       m_MaskName    (maskname),
       m_MaxFileSize (max_file_size),
       m_DFile       (new CWriteDB_GiMaskData(maskname, "gmd", 0, max_file_size)),
       m_DFile_LE    (new CWriteDB_GiMaskData(maskname, "gnd", 0, max_file_size, true)),
       m_OFile       (new CWriteDB_GiMaskOffset(maskname, "gmo", max_file_size)),
       m_OFile_LE    (new CWriteDB_GiMaskOffset(maskname, "gno", max_file_size, true)),
       m_IFile       (new CWriteDB_GiMaskIndex(maskname, "gmi", desc, max_file_size)),
       m_IFile_LE    (new CWriteDB_GiMaskIndex(maskname, "gni", desc, max_file_size, true))
{ }

void CWriteDB_GiMask::ListFiles(vector<string> & files) const
{
    if (!m_GiOffset.size()) return;
    files.push_back(m_IFile->GetFilename());
    files.push_back(m_IFile_LE->GetFilename());
    files.push_back(m_OFile->GetFilename());
    files.push_back(m_OFile_LE->GetFilename());
    files.push_back(m_DFile->GetFilename());
    files.push_back(m_DFile_LE->GetFilename());
}

void CWriteDB_GiMask::AddGiMask(const vector<TGi> & GIs,
                                const TPairVector & mask)
{
    if (!m_DFile->CanFit(mask.size())) {
        int index = m_DFile->GetIndex() + 1;
        m_DFile->Close();
        m_DFile_LE->Close();
        m_DFile.Reset(new CWriteDB_GiMaskData(m_MaskName, "gmd", index, m_MaxFileSize));
        m_DFile_LE.Reset(new CWriteDB_GiMaskData(m_MaskName, "gnd", index, m_MaxFileSize, true));
    }

    TOffset offset = m_DFile->GetOffset();
    m_DFile->WriteMask(mask);
    m_DFile_LE->WriteMask(mask);

    ITERATE(vector<TGi>, gi, GIs) {
        m_GiOffset.push_back(pair<TGi, TOffset> (*gi, offset));
    }
}

void CWriteDB_GiMask::Close()
{
    if (!m_GiOffset.size()) {
        // un_used mask file
        m_MaskName = "";
        return;
    }

    m_DFile->Close();
    m_DFile_LE->Close();

    int num_vols = m_DFile->GetIndex() + 1;
    if (num_vols == 1) {
        m_DFile->RenameSingle();
        m_DFile_LE->RenameSingle();
    }

    sort(m_GiOffset.begin(), m_GiOffset.end());

    m_IFile->AddGIs(m_GiOffset, num_vols);
    m_IFile->Close();

    m_IFile_LE->AddGIs(m_GiOffset, num_vols);
    m_IFile_LE->Close();

    m_OFile->AddGIs(m_GiOffset);
    m_OFile->Close();

    m_OFile_LE->AddGIs(m_GiOffset);
    m_OFile_LE->Close();
}

// CWriteDB_GiMaskOffset

CWriteDB_GiMaskOffset::
CWriteDB_GiMaskOffset(const string       & maskname,
                      const string       & extn,
                      Uint8                max_file_size,
                      bool                 le)
    : CWriteDB_File (maskname, extn, -1, max_file_size, false),
      m_UseLE (le)
{ }

void CWriteDB_GiMaskOffset::AddGIs(const TGiOffset & gi_offset)
{
    CBlastDbBlob gis(kPageSize * kGISize);
    CBlastDbBlob offsets(kPageSize * kOffsetSize);

    if ( ! m_Created) {
        Create();
    }

    int i = 0;
    ITERATE(TGiOffset, iter, gi_offset) {
#ifdef NCBI_INT8_GI
        if (m_UseLE) {
            gis.WriteInt4_LE(GI_TO(int, iter->first));
            offsets.WriteInt4_LE(iter->second.first);
            offsets.WriteInt4_LE(iter->second.second);
        } else {
            gis.WriteInt4(GI_TO(int, iter->first));
            offsets.WriteInt4(iter->second.first);
            offsets.WriteInt4(iter->second.second);
        }
#else
        if (m_UseLE) {
            gis.WriteInt4_LE(iter->first);
            offsets.WriteInt4_LE(iter->second.first);
            offsets.WriteInt4_LE(iter->second.second);
        } else {
            gis.WriteInt4(iter->first);
            offsets.WriteInt4(iter->second.first);
            offsets.WriteInt4(iter->second.second);
        }
#endif

        ++i;

        if (i== kPageSize) {
            Write(gis.Str());
            Write(offsets.Str());
            gis.Clear();
            offsets.Clear();
            i = 0;
        }
    }

    // flush the residual records
    if (i) {
        Write(gis.Str());
        Write(offsets.Str());
        gis.Clear();
        offsets.Clear();
    }
}

// CWriteDB_GiMaskIndex

CWriteDB_GiMaskIndex::
CWriteDB_GiMaskIndex(const string        & maskname,
                     const string        & extn,
                     const string        & desc,
                     Uint8                 max_file_size,
                     bool                  le)
    : CWriteDB_GiMaskOffset (maskname, extn, max_file_size, le),
      m_Desc                (desc)
{
    m_Date = CTime(CTime::eCurrent).AsString();
}

void CWriteDB_GiMaskIndex::AddGIs(const TGiOffset & gi_offset,
                                  int               num_vols)
{
    m_NumGIs   = gi_offset.size();
    m_NumIndex = m_NumGIs / kPageSize + 2;

    CBlastDbBlob gis(m_NumIndex * kGISize);
    CBlastDbBlob offsets(m_NumIndex * kOffsetSize);

    if ( ! m_Created) {
        Create();
    }

    int i = 0;
    m_NumIndex = 0;

    ITERATE(TGiOffset, iter, gi_offset) {
        if (i % kPageSize && i < m_NumGIs-1) {
            ++i;
            continue;
        }

        ++i;

#ifdef NCBI_INT8_GI
        if (m_UseLE ) {
            gis.WriteInt4_LE(GI_TO(int, iter->first));
            offsets.WriteInt4_LE(iter->second.first);
            offsets.WriteInt4_LE(iter->second.second);
        } else {
            gis.WriteInt4(GI_TO(int, iter->first));
            offsets.WriteInt4(iter->second.first);
            offsets.WriteInt4(iter->second.second);
        }
#else
        if (m_UseLE ) {
            gis.WriteInt4_LE(iter->first);
            offsets.WriteInt4_LE(iter->second.first);
            offsets.WriteInt4_LE(iter->second.second);
        } else {
            gis.WriteInt4(iter->first);
            offsets.WriteInt4(iter->second.first);
            offsets.WriteInt4(iter->second.second);
        }
#endif
        ++m_NumIndex;
    }

    x_BuildHeaderFields(num_vols);
    Write(gis.Str());
    Write(offsets.Str());
}

void CWriteDB_GiMaskIndex::x_BuildHeaderFields(int num_vols)
{
    const int kFormatVersion = 1; // SeqDB has one of these.

    CBlastDbBlob header;

    header.WriteInt4(kFormatVersion);
    header.WriteInt4(num_vols);
    header.WriteInt4(kGISize);
    header.WriteInt4(kOffsetSize);
    header.WriteInt4(kPageSize);
    header.WriteInt4(m_NumIndex);
    header.WriteInt4(m_NumGIs);
    header.WriteInt4(0);   // index start will be calculated later
    header.WriteString(m_Desc, kStringFmt);
    header.WriteString(m_Date, kStringFmt);
    header.WritePadBytes(8, CBlastDbBlob::eString);

    Int4 size = header.GetWriteOffset();
    header.WriteInt4(size, 28);

    Write(header.Str());
}

// CWriteDB_GiMaskData

CWriteDB_GiMaskData::CWriteDB_GiMaskData(const string     & maskname,
                                         const string     & extn,
                                         int                index,
                                         Uint8              max_file_size,
                                         bool               le)
    : CWriteDB_File (maskname, extn, index, max_file_size, false),
      m_DataLength  (0),
      m_UseLE       (le),
      m_Index       (index)
{ }

void CWriteDB_GiMaskData::WriteMask(const TPairVector & mask)
{

    if (! mask.size()) return;

    if (! m_Created) Create();

    CBlastDbBlob data;

    if (m_UseLE ) {
        data.WriteInt4_LE(mask.size());
        ITERATE(TPairVector, range, mask) {
            data.WriteInt4_LE(range->first);
            data.WriteInt4_LE(range->second);
        }
    } else {
        data.WriteInt4(mask.size());
        ITERATE(TPairVector, range, mask) {
            data.WriteInt4(range->first);
            data.WriteInt4(range->second);
        }
    }

    Write(data.Str());
    m_DataLength += (1+2*mask.size()) * 4;
}

#endif

END_NCBI_SCOPE

