#ifndef _ALNREAD_HPP_
#define _ALNREAD_HPP_

/*
 * $Id$
 *
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
 * Authors:  Colleen Bollin
 *
 */
#include <corelib/ncbistd.hpp>


#if defined (WIN32)
#    define ALIGNMENT_CALLBACK __stdcall
#else
#    define ALIGNMENT_CALLBACK
#endif

BEGIN_NCBI_SCOPE

typedef char * (ALIGNMENT_CALLBACK *FReadLineFunction) (void * userdata);

typedef enum {
    eAlnErr_Unknown = -1,
    eAlnErr_NoError = 0,
    eAlnErr_Fatal,
    eAlnErr_BadData,
    eAlnErr_BadFormat
} EAlnErr;

//  =====================================================================
class CErrorInfo
//  =====================================================================
{
public:
    static const int NO_LINE_NUMBER = -1;

    CErrorInfo(
        EAlnErr category = eAlnErr_Unknown,
        int lineNumber = NO_LINE_NUMBER,
        const string& id = "",
        const string& message = "",
        CErrorInfo* next = nullptr):
        mCategory(category),
        mLineNumber(lineNumber),
        mId(id),
        mMessage(message),
        mNext(next)
    {};

    ~CErrorInfo()
    { delete mNext; };

    const CErrorInfo* 
    Next() const { return mNext; };

    string
    Message() const { return mMessage; };

    string
    Id() const { return mId; };

    int
    LineNumber() const { return mLineNumber; };

    EAlnErr
    Category() const { return mCategory; };

protected:
    EAlnErr mCategory;
    int mLineNumber;
    string mId;
    string mMessage;
    CErrorInfo* mNext;
};

typedef void (ALIGNMENT_CALLBACK *FReportErrorFunction) (
  const CErrorInfo&, /* error to report */
  void* userdata /* data supplied by calling program to library */
);

typedef struct SSequenceInfo {
    char * missing;
    char * match;
    char * beginning_gap;
    char * middle_gap;
    char * end_gap;
    const char * alphabet;
} SSequenceInfo, * TSequenceInfoPtr;

TSequenceInfoPtr SequenceInfoNew (void);
void SequenceInfoFree (TSequenceInfoPtr sip);

typedef struct SAlignmentFile {
    int     num_sequences;
    int     num_organisms;
    int     num_deflines;
    int     num_segments;
    char ** ids;
    char ** sequences;
    char ** organisms;
    char ** deflines;
    char    align_format_found;
} SAlignmentFile, * TAlignmentFilePtr;

TAlignmentFilePtr AlignmentFileNew (void);
void AlignmentFileFree (TAlignmentFilePtr afp);

TAlignmentFilePtr ReadAlignmentFile (
  FReadLineFunction    readfunc,      /* function for reading lines of 
                                       * alignment file
                                       */
  void *               fileuserdata,  /* data to be passed back each time
                                       * readfunc is invoked
                                       */
  FReportErrorFunction errfunc,       /* function for reporting errors */
  void *               erroruserdata, /* data to be passed back each time
                                       * errfunc is invoked
                                       */
  TSequenceInfoPtr     sequence_info  /* structure containing sequence
                                       * alphabet and special characters
                                       */
);

TAlignmentFilePtr ReadAlignmentFileEx (
  FReadLineFunction    readfunc,      /* function for reading lines of 
                                       * alignment file
                                       */
  void *               fileuserdata,  /* data to be passed back each time
                                       * readfunc is invoked
                                       */
  FReportErrorFunction errfunc,       /* function for reporting errors */
  void *               erroruserdata, /* data to be passed back each time
                                       * errfunc is invoked
                                       */
  TSequenceInfoPtr     sequence_info, /* structure containing sequence
                                       * alphabet and special characters
                                       */
  bool           use_nexus_file_info /* set to nonzero to replace data in 
                                       * sequence_info with characters
                                       * read from NEXUS comment in file,
                                       * set to 0 otherwise.
                                       */
);

/*
 * The following are to accommodate creating of local IDs to replace the IDs
 * found in the actual alignment file. We are retaining the original API for
 * legacy code compatibility, hence the new functions with almost the same
 * signature.
 */
NCBI_XOBJREAD_EXPORT TAlignmentFilePtr ReadAlignmentFile2 (
  FReadLineFunction    readfunc,      /* function for reading lines of 
                                       * alignment file
                                       */
  void *               fileuserdata,  /* data to be passed back each time
                                       * readfunc is invoked
                                       */
  FReportErrorFunction errfunc,       /* function for reporting errors */
  void *               erroruserdata, /* data to be passed back each time
                                       * errfunc is invoked
                                       */
  TSequenceInfoPtr     sequence_info, /* structure containing sequence
                                       * alphabet and special characters
                                       */
  bool                gen_local_ids  /* flag indicating whether input IDs
                                       * should be replaced with unique
                                       * local IDs
                                       */ 
);

NCBI_XOBJREAD_EXPORT TAlignmentFilePtr ReadAlignmentFileEx2 (
  FReadLineFunction    readfunc,      /* function for reading lines of 
                                       * alignment file
                                       */
  void *               fileuserdata,  /* data to be passed back each time
                                       * readfunc is invoked
                                       */
  FReportErrorFunction errfunc,       /* function for reporting errors */
  void *               erroruserdata, /* data to be passed back each time
                                       * errfunc is invoked
                                       */
  TSequenceInfoPtr     sequence_info, /* structure containing sequence
                                       * alphabet and special characters
                                       */
  bool          use_nexus_file_info, /* set to nonzero to replace data in 
                                       * sequence_info with characters
                                       * read from NEXUS comment in file,
                                       * set to 0 otherwise.
                                       */
  bool                gen_local_ids  /* flag indicating whether input IDs
                                       * should be replaced with unique
                                       * local IDs
                                       */ 
);

END_NCBI_SCOPE

#endif // _ALNREAD_HPP_
