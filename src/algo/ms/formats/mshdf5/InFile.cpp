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
 * Author:  Dennis Troup and Dmitry Rudnev
 * File Description: Useful functionality for reading and writing files.
 */

#include "InFile.hpp"

#include <util/compress/tar.hpp>
#include <util/compress/zlib.hpp>
#include <util/compress/bzip2.hpp>
#include <util/compress/stream.hpp>

#include <fstream>
#include <iostream>

#include <sys/stat.h>
#include <sys/types.h>
#include <cstdlib>

using std::ofstream;
using std::ostream;
using std::ifstream;
using std::istream;
using std::string;

using ncbi::CZipStreamDecompressor;
using ncbi::CCompressionIStream;
using ncbi::CZipDecompressor;
using ncbi::CBZip2StreamDecompressor;


//namespace {

  InFile::~InFile()
  {
  }

  class InFileImpl : public InFile {

  public:

    explicit InFileImpl(const string & name);

    ~InFileImpl();

    virtual bool isGood();

    virtual void setGood();

    virtual void setBad();

    virtual string pathname() const;

  protected:

    std::istream * getStream();

  private:

    istream * inFile_;
    bool      isGood_;
    bool      ownStream_;
    string    pathname_;

  };

  InFileImpl::InFileImpl(const string & name) :
    isGood_(true),
    pathname_(name)
  {
    if (!name.empty()) {
      inFile_ = new ifstream(name.c_str());
      isGood_ = *inFile_;
    }
    else {
      inFile_ = &std::cin;
      ownStream_ = false;
    }
  }

  InFileImpl::~InFileImpl()
  {
    if (ownStream_) {
      delete inFile_;
    }
  }

  istream * InFileImpl::getStream()
  {
    return inFile_;
  }

  bool InFileImpl::isGood()
  {
    return isGood_;
  }

  void InFileImpl::setGood()
  {
    isGood_ = true;
  }

  void InFileImpl::setBad()
  {
    isGood_ = false;
  }

  string InFileImpl::pathname() const
  {
    return pathname_;
  }

  class UncompressedInFile : public InFileImpl {

  public:

    explicit UncompressedInFile(const string & name);

    virtual istream & inStream();

  };

  UncompressedInFile::UncompressedInFile(const string & name) :
    InFileImpl(name)
  {
  }

  istream & UncompressedInFile::inStream()
  {
    return *getStream();
  }

  class GZipInFile : public InFileImpl {

  public:

    explicit GZipInFile(const string & name);

    ~GZipInFile();

    virtual istream & inStream();

  private:

    CZipStreamDecompressor zipDecompressor_;
    CCompressionIStream *  inStream_;

  };

  GZipInFile::GZipInFile(const string & name) :
    InFileImpl(name),
    zipDecompressor_(CZipDecompressor::fCheckFileHeader)
  {
    inStream_ = new CCompressionIStream(*getStream(), &zipDecompressor_);
    if (!*inStream_) {
      setBad();
    }
  }

  GZipInFile::~GZipInFile()
  {
    inStream_->Finalize();
    delete inStream_;
  }

  istream & GZipInFile::inStream()
  {
    return *inStream_;
  }

  class BZip2InFile : public InFileImpl {

  public:

    explicit BZip2InFile(const string & name);

    ~BZip2InFile();

    virtual std::istream & inStream();

  private:

    CBZip2StreamDecompressor zipDecompressor_;
    CCompressionIStream *    inStream_;

  };

  BZip2InFile::BZip2InFile(const string & name) :
    InFileImpl(name)
  {
    inStream_ = new CCompressionIStream(*getStream(), &zipDecompressor_);
    if (!*inStream_) {
      setBad();
    }
  }

  BZip2InFile::~BZip2InFile()
  {
    inStream_->Finalize();
    delete inStream_;
  }

  istream & BZip2InFile::inStream()
  {
    return *inStream_;
  }

//}

InFilePtr InFile::create(const string &  pathname,
			 CompressionType compress)
{
  InFilePtr created(0);

  try {
    switch (compress) {
    case GZIP_COMPRESSED:
      created.reset(new GZipInFile(pathname));
      break;

    case BZIP2_COMPRESSED:
      created.reset(new BZip2InFile(pathname));
      break;

    case UNCOMPRESSED:
      created.reset(new UncompressedInFile(pathname));
      break;

    default:
      UncompressedInFile * badFile(new UncompressedInFile(pathname));
      badFile->setBad();
      created.reset(badFile);
      break;
    }
  }
  catch (...) {
  }

  return created;
}
