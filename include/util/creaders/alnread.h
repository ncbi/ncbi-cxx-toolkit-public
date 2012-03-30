#ifndef UTIL_CREADERS___ALNREAD__H
#define UTIL_CREADERS___ALNREAD__H

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

#include <util/creaders/creaders_export.h>

#ifdef __cplusplus
extern "C" {
#endif

/* defines from ncbistd.h */
#ifndef FAR
#define FAR
#endif
#ifndef PASCAL
#define PASCAL
#endif
#ifndef EXPORT
#define EXPORT
#endif

#ifndef PASCAL
#define PASCAL
#endif
#ifndef EXPORT
#define EXPORT
#endif

#if defined (WIN32)
#    define ALIGNMENT_CALLBACK __stdcall
#else
#    define ALIGNMENT_CALLBACK
#endif

typedef char * (ALIGNMENT_CALLBACK *FReadLineFunction) (void * userdata);

typedef enum {
    eTrue = -1,
    eFalse = 0
} EBool;

typedef enum {
    eAlnErr_Unknown = -1,
    eAlnErr_NoError = 0,
    eAlnErr_Fatal,
    eAlnErr_BadData,
    eAlnErr_BadFormat
} EAlnErr;

/* This structure and the accompanying functions are used for storing
 * information about errors encountered while reading the alignment data.
 */
typedef struct SErrorInfo {
    EAlnErr             category;
    int                 line_num;
    char *              id;
    char *              message;
    struct SErrorInfo * next;
} SErrorInfo, * TErrorInfoPtr;

typedef void (ALIGNMENT_CALLBACK *FReportErrorFunction) (
  TErrorInfoPtr err_ptr, /* error to report */
  void *        userdata /* data supplied by calling program to library */
);

NCBI_CREADERS_EXPORT 
extern TErrorInfoPtr ErrorInfoNew (TErrorInfoPtr list);

NCBI_CREADERS_EXPORT 
extern void ErrorInfoFree (TErrorInfoPtr eip);

typedef struct SSequenceInfo {
    char * missing;
    char * match;
    char * beginning_gap;
    char * middle_gap;
    char * end_gap;
    const char * alphabet;
} SSequenceInfo, * TSequenceInfoPtr;

extern NCBI_CREADERS_EXPORT TSequenceInfoPtr SequenceInfoNew (void);
extern NCBI_CREADERS_EXPORT void SequenceInfoFree (TSequenceInfoPtr sip);

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

extern NCBI_CREADERS_EXPORT TAlignmentFilePtr AlignmentFileNew (void);
extern NCBI_CREADERS_EXPORT void AlignmentFileFree (TAlignmentFilePtr afp);

extern NCBI_CREADERS_EXPORT TAlignmentFilePtr ReadAlignmentFile (
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

extern NCBI_CREADERS_EXPORT TAlignmentFilePtr ReadAlignmentFileEx (
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
  EBool           use_nexus_file_info /* set to nonzero to replace data in 
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

extern NCBI_CREADERS_EXPORT TAlignmentFilePtr ReadAlignmentFile2 (
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
  EBool                gen_local_ids  /* flag indicating whether input IDs
                                       * should be replaced with unique
                                       * local IDs
                                       */ 
);

extern NCBI_CREADERS_EXPORT TAlignmentFilePtr ReadAlignmentFileEx2 (
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
  EBool          use_nexus_file_info, /* set to nonzero to replace data in 
                                       * sequence_info with characters
                                       * read from NEXUS comment in file,
                                       * set to 0 otherwise.
                                       */
  EBool                gen_local_ids  /* flag indicating whether input IDs
                                       * should be replaced with unique
                                       * local IDs
                                       */ 
);

#ifdef __cplusplus
}
#endif

#endif /* UTIL_CREADERS___ALNREAD__H */
