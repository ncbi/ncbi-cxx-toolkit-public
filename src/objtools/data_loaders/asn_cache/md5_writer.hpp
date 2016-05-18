#ifndef MD5_WRITER_HPP__
#define MD5_WRITER_HPP__

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
 * Authors:  Cheinan Marks
 *
 */

#include <corelib/ncbistd.hpp>
#include <util/checksum.hpp>

#if NCBI_PRODUCTION_VER > 20110000
#  include <util/buffer_writer.hpp>
#else
#  include "buffer_writer.hpp"
#endif


BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
///
/// Stream hook that calculates the MD5 digest of the data passed to a
/// CBufferWriter.
///

template <typename Buffer>
class CMD5StreamWriter : public CBufferWriter<Buffer>
{
public:
    typedef Buffer  TBuffer;
    typedef CBufferWriter<TBuffer>  TBufferWriter;

    CMD5StreamWriter(TBuffer& buf, EBufferWriter_CreateMode truncate = eCreateMode_Truncate)
        : TBufferWriter( buf, truncate ), m_Checksum(CChecksum::eMD5) {}

    virtual ERW_Result Write(const void* buf,
                            size_t count,
                            size_t* bytes_written = 0)
    {
        m_Checksum.AddChars((const char*)buf, count);
        return TBufferWriter::Write( buf, count, bytes_written );
    }

    virtual ERW_Result Flush()
    {
        return eRW_Success;
    }

    std::vector<unsigned char> GetMD5Sum() const
    {
        std::vector<unsigned char>   digest(16);
        m_Checksum.GetMD5Digest( &digest[0] );
        return  digest;
    }

private:
   CChecksum m_Checksum;
};

/*
           ///
           /// have we seen this alignment before?
           ///
           if (check_unique) {
               CMD5StreamWriter md5;
               CWStream wstr(&md5);
               wstr << MSerial_AsnBinary << align;
               wstr.flush();

               string md5_str;
               md5.GetMD5Sum(md5_str);
               TUniqueAligns::iterator it =
unique_aligns.find(md5_str);
               if (it != unique_aligns.end()) {
                   ++count_dup;
                   continue;
               }
               unique_aligns.insert(TUniqueAligns::value_type
                                    (md5_str, *iter));
           }
*/

END_NCBI_SCOPE

#endif  //  MD5_WRITER_HPP__
