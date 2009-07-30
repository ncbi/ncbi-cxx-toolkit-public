#if !defined(IN_FILE_HPP)
#define IN_FILE_HPP

/* ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/Database is a "United States Government Work" under the
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
 * File Description: Create input streams with various compression strategies.
 */

#include <iosfwd>
#include <memory>
#include <string>

enum CompressionType {
  UNCOMPRESSED,
  BZIP2_COMPRESSED,
  GZIP_COMPRESSED
};

class InFile;
typedef std::auto_ptr<InFile> InFilePtr;

/**
   The input stream making class. The lifetime of the class controls the
   lifetime of the created stream.
*/
class InFile {
  
public:

  virtual ~InFile();

  /**
     The created input stream.
     
     @return the created input stream.
  */
  virtual std::istream & inStream() = 0;
  
  /**
     The factory method to create the input streams with the compression
     strategies.
     
     @param pathname The pathname of the file to associate with stream. An
     empty string means the standard input.
     
     @param compress The type of compression of the created stream.
     
     @return a class that contains and controls the created stream.
  */
  static InFilePtr create(const std::string & pathname,
			  CompressionType     compress);

};
  
#endif
