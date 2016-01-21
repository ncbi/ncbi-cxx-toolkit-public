#ifndef UTIL___STREAM_SOURCE__HPP
#define UTIL___STREAM_SOURCE__HPP

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
 * Authors:  Mike DiCuccio
 *
 * File Description:
 *
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiargs.hpp>

BEGIN_NCBI_SCOPE


///
/// class CInputStreamSource encapsulates details of how we supply applications
/// with input data through sets of command-line options, and permits code to
/// work with a standard API for accepting input data from the command-line.
///
/// This class offers many variants for accepting and managing input data.
/// Currently supported modes are:
///
///  - Supply a single argument for an input stream of data
///  - Supply an argument indicating a manifest file (a file whose contents
///    list a set of files, one per line; comment lines preceded by '#' will be
///    skipped
///  - Supply an argument indicating an input search path, with or without a
///    search mask, defining the files to be processed
///
/// Once instantiated, this class supports the ability to iterate a set of
/// input streams using the code metaphor:
///
/// \code
///     const CArgs& args = GetArgs();
///     for (CInputStreamSource source(args);  source;  ++source) {
///         CNcbiIstream& istr = *source;
///         ...
///     }
/// \encode
///
/// Streams are checked for error conditions at the start (badbit or
/// failbit before being returned from the iterator) and end (badbit
/// after use, prior to iterating to the next stream),
/// throwing an exception in such case. The former handles cases of files
/// which don't exist, and the latter handles cases of disk read errors
/// without preventing operations line getline (which sets failbit
/// on reading the last line of input, with a terminator).
class NCBI_XUTIL_EXPORT CInputStreamSource
{
public:
    /// Supply a standard set of arguments via argument descriptions to an
    /// application
    ///
    /// Currently supported arguments are:
    ///
    ///  - -i (-input) for a single input stream
    ///  - -input-manifest for a manifest file
    ///  - -input-path (with or without -input-mask) for a file search path
    ///
    /// Any or all of these arguments may be supplied by an application
    ///
    /// @param arg_desc Argument description class into which arguments will be
    /// added
    /// @param prefix The base prefix to use.  Providing a different value here
    /// can be used to add a standard panel of additional arguments to control
    /// input sources.
    /// @param description The description that will appear in -help
    /// @param is_mandatory A flag to indicate whether one of the configured
    /// arguments must be provided.
    ///
    static void SetStandardInputArgs(CArgDescriptions& arg_desc,
                                     const string &prefix = "input",
                                     const string &description = "data to process",
                                     bool is_mandatory = false);

    /// Get the standard input arguments that are present in args
    /// so we can pass them on to some other program that also uses
    /// CInputStreamSource.
    static vector<string> RecreateInputArgs(const CArgs& args,
                                            const string &prefix = "input");

    /// Default ctor
    /// This ctor leaves the stream source empty
    ///
    CInputStreamSource();

    /// Initialize our stream source through the arguments provided on a
    /// command-line
    ///
    /// This constructor will interpret the commands supplied via
    /// SetStandardInputArgs()
    ///
    /// @param args Argument class for interpretation
    ///
    CInputStreamSource(const CArgs& args, const string& prefix = "input");

    /// Initialize from a stream
    /// No ownership is claimed by this class - lifetime management of the
    /// stream is the responsibility of the caller.
    ///
    /// @pre The stream is in a good condition, else throws an exception.
    /// @param fname (optional) file name from whence the stream was
    ///        created, for use in output and debug messages.
    ///
    void InitStream(CNcbiIstream& istr,
                    const string& fname = kEmptyStr);

    /// Initialize from a single file path.
    ///
    void InitFile(const string& file_path);

    /// Initialize from a manifest file
    ///
    /// @see CFileManifest
    ///
    void InitManifest(const string& manifest);

    /// Initialize from a file search path
    ///
    void InitFilesInDirSubtree(const string& file_path,
                               const string& file_mask = kEmptyStr);

    /// Initialize from a set of arguments
    ///
    void InitArgs(const CArgs& args, const string &prefix = "input");

    /// Access the current stream
    ///
    /// @return The current stream
    ///
    CNcbiIstream& GetStream(void);

    /// Access the current stream, and get the file name
    ///
    /// @param fname receives the name of the current file
    /// @return The current stream
    ///
    NCBI_DEPRECATED
    CNcbiIstream& GetStream(string* fname);

    /// Dereferencing the stream class returns the current stream
    /// @return The current stream
    ///
    CNcbiIstream& operator*();

    /// Advance to the next stream in the class
    ///
    /// @return self, satisfying chainability of commands
    ///
    /// @post The old stream, if there was one, is not in a bad condition
    ///       (badbit set as may occur if there was a disk read error;
    ///       the failbit is ignored, since operations like geline will
    ///       set it to indicate the last line with a line terminator).
    ///
    ///       The current stream, if it exists, is in a good condition
    ///       (badbit or failbit set).
    ///
    ///       If these conditions aren't met, throws an exception.
    ///
    CInputStreamSource& operator++();

    /// Determine if there are any more streams to be processed
    ///
    /// @return boolean, true if there are more streams
    ///
    operator bool();

    /// Resets the iterator to the first stream in the class
    ///
    /// @return self
    CInputStreamSource& Rewind(void);

    /// Returns the current file name
    string GetCurrentFileName(void) const;

    /// Returns the current file index and the total number of files
    ///
    /// @param count
    ///   address of variable which receives the total number of files
    /// @return
    ///   the current file index
    size_t GetCurrentStreamIndex(size_t* count = nullptr) const;

private:
    CArgs m_Args;
    string m_Prefix;

    CNcbiIstream* m_Istr;
    auto_ptr<CNcbiIfstream> m_IstrOwned;
    vector<string> m_Files;
    size_t m_CurrIndex;
    string m_CurrFile;

    /// forbidden
    CInputStreamSource(const CInputStreamSource&);
    CInputStreamSource& operator=(const CInputStreamSource&);
};





END_NCBI_SCOPE


#endif  // UTIL___STREAM_SOURCE__HPP
