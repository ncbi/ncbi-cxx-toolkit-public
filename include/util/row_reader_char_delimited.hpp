#ifndef UTIL___ROW_READER_CHAR_DELIMITED__HPP
#define UTIL___ROW_READER_CHAR_DELIMITED__HPP

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
* Authors: Denis Vakatov, Sergey Satskiy
*
* File Description:
*   Implementation of the CRowReader<> traits for a single character
*   separator between fields in rows.
*
* ===========================================================================
*/

#include <util/row_reader.hpp>
#include <util/row_reader_base.hpp>


BEGIN_NCBI_SCOPE

/// General traits implementation recommendations:
///
///  - Use IMessage API to communicate processing messages, errors and progress
///    to the upper-level code




/// Default traits:
///  - The very basic 1-char-delimited format (no header, comments,
///    empty lines, etc.)
///  - Illustrates and explains the trait's API
///  - Can be used to derive more complex user-defined traits
template <char DelimiterValue>
class CRowReaderStream_SingleCharDelimited : public CRowReaderStream_Base
{
public:
    /// Tokenize the raw line and put the tokens into the tokens vector
    ERR_Action Tokenize(const CTempString    raw_line,
                        vector<CTempString>& tokens)
    {
        NStr::Split(raw_line, string(1, DelimiterValue), tokens, 0);
        return eRR_Continue_Data;
    }

    // Standard typedefs and methods to bind to the "parent stream"
    RR_TRAITS_PARENT_STREAM(CRowReaderStream_SingleCharDelimited<DelimiterValue>);
};


typedef CRowReaderStream_SingleCharDelimited<'\t'> TRowReaderStream_SingleTabDelimited;
typedef CRowReaderStream_SingleCharDelimited<' '> TRowReaderStream_SingleSpaceDelimited;
typedef CRowReaderStream_SingleCharDelimited<','> TRowReaderStream_SingleCommaDelimited;




/////////////////////////////////////////////////////////
//  USAGE
//
//
//
// typedef CRowReader<CRowReaderStream_TabDelimited> TMyDelimitedStream;
//
//
// CNcbiIfstream       my_file("file.tab");
// TMyDelimitedStream  delim_stream(my_file);
//
//
// delim_stream.SetFieldName(1, "one");
// delim_stream.SetFieldName(2, "two");
// delim_stream.SetFieldName(3, "three");
//
//
// for (auto & row : delim_stream) {
//     cout << "Value of the first field: " << row["one"] << endl;
//
//     // Exception will be generated
//     row[4];
// }


END_NCBI_SCOPE

#endif  /* UTIL___ROW_READER_CHAR_DELIMITED__HPP */
