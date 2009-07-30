/* ===========================================================================
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
 * $Id$
 * Author:  Dennis Troup
 * File Description: Allow Xerces-C to read compressed binary input streams.
 */

#include "BinCompressedInputStream.hpp"
#include <istream>
#include <iostream>
#include <iomanip>

#include <xercesc/util/XMLString.hpp>

BinCompressedInputStream::
BinCompressedInputStream(const std::string & filePath,
			 CompressionType     compression) :
  bytesRead_(0)
{
  in_ = InFile::create(filePath, compression);
}

unsigned int BinCompressedInputStream::curPos() const
{
  return bytesRead_;
}

unsigned int BinCompressedInputStream::readBytes(XMLByte * const    toFill,
						 const unsigned int maxToRead)
{
  std::istream & input(in_->inStream());

  input.read(reinterpret_cast<char * const>(toFill), maxToRead);

  std::streamsize bytesRead(input.gcount());
  //++bufferedReads_[maxToRead];
      
  bytesRead_ += bytesRead;

  return bytesRead;      
}

bool  BinCompressedInputStream::isOpen() const
{
  return (const_cast<InFile *>(in_.get()))->inStream();
}
