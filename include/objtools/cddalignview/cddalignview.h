#ifndef CDDALIGNVIEW__H
#define CDDALIGNVIEW__H

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
 * Author:  Paul Thiessen
 *
 *
 */

/**
 * @file cddalignview.h
 *
 * C interface header for cddalignview as function call
 */

#ifdef __cplusplus
#include <corelib/ncbistd.hpp>
#include <corelib/ncbistre.hpp>
#include <objects/ncbimime/Ncbi_mime_asn1.hpp>

extern "C" {
#endif

/* output option bit flags (can be or'ed together) */
#define CAV_TEXT            0x01    /* plain-text output */
#define CAV_HTML            0x02    /* HTML output */
#define CAV_LEFTTAILS       0x04    /* show left (N-terminal) tails */
#define CAV_RIGHTTAILS      0x08    /* show right (C-terminal) tails */
#define CAV_CONDENSED       0x10    /* collapse incompletely aligned columns (text/HTML only) */
#define CAV_DEBUG           0x20    /* send debug/trace messages to stderr */
#define CAV_HTML_HEADER     0x40    /* include HTML header for complete page */
#define CAV_SHOW_IDENTITY   0x80    /* for HTML, show identity rather than using bit cutoff */
#define CAV_FASTA           0x0100  /* FASTA text output */
#define CAV_FASTA_LOWERCASE 0x0200  /* make unaligned residues lower case in FASTA output */
#define CAV_ANNOT_BOTTOM    0x0400  /* put annotations on bottom; default is on top */
#define CAV_NO_PARAG_COLOR  0x0800  /* don't use colored background for alignment paragraphs (HTML only) */
#define CAV_NO_CHANGE_DIAG  0x1000  /* don't change any diagnostic stream/level (e.g. for applog) */


/* data structure for holding alignment feature/annotation info */
typedef struct {
    int nLocations;             /* how many residues are marked */
    int *locations;             /* indexes on master (numbered from 0) to mark */
    const char *shortName;      /* short name (displayed in title column) */
    const char *description;    /* optional description, displayed in legend */
    char featChar;              /* character to use in annotation line */
} AlignmentFeature;


/*
 * The main function call:
 *  - 'asnDataBlock' is a pointer to in-memory asn data (can be binary or text).
 *    This memory block will *not* be overwritten or freed by this function.
 *  - 'asnSize' is the size of asnDataBlock
 *  - 'options' specifies the desired output, e.g. "CAV_HTML | CAV_LEFTTAILS"
 *    to get HTML output with left (N-terminal) sequence tails shown
 *  - 'paragraphWidth' is the size of the "paragraph" chunks of the alignment
 *  - 'conservationThreshhold' is the information content bit score above
 *    which a column is marked as conserved in the HTML display
 *  - 'title' if non-NULL is used as the <TITLE> of the HTML output when
 *    CAV_HTML_HEADER is used
 *  - 'nFeatures' is the number of feature annotations
 *  - features is an array of features (See AlignmentFeature struct above)
 *
 * The output (of whatever type) will be placed on the standard output stream.
 *
 * Upon success, will return CAV_SUCCESS (see below). Otherwise, will return
 * one of the given error values to give some indication of what went wrong.
 */
extern int CAV_DisplayMultiple(
    const void *asnDataBlock,
    int asnSize,
    unsigned int options,
    unsigned int paragraphWidth,
    double conservationThreshhold,
    const char *title,
    int nFeatures,
    const AlignmentFeature *features
);


/* error values returned by CAV_DisplayMultiple() */
#define CAV_SUCCESS             0
#define CAV_ERROR_BAD_PARAMS    1   /* incorrect/incompatible options */
#define CAV_ERROR_BAD_ASN       2   /* unrecognized asn data */
#define CAV_ERROR_SEQUENCES     3   /* error parsing sequence data */
#define CAV_ERROR_ALIGNMENTS    4   /* error parsing alignment data */
#define CAV_ERROR_PAIRWISE      5   /* error parsing a master/slave pairwise alignment */
#define CAV_ERROR_DISPLAY       6   /* error creating display structure */


#ifdef __cplusplus
}

/**
 * A C++ version of the function, that places output on the given streams.
 */
extern int CAV_DisplayMultiple(
    const void *asnDataBlock,
    int asnSize,
    unsigned int options,
    unsigned int paragraphWidth,
    double conservationThreshhold,
    const char *title,
    int nFeatures,
    const AlignmentFeature *features,
    ncbi::CNcbiOstream *outputStream,       /**< Regular program output (alignments); cout if NULL */
    ncbi::CNcbiOstream *diagnosticStream    /**< Diagnostic messages/errors; cerr if NULL */
);

/* and one that takes a mime object rather than an in-memory asn blob */
extern int CAV_DisplayMultiple(
    const ncbi::objects::CNcbi_mime_asn1& mime,
    unsigned int options,
    unsigned int paragraphWidth,
    double conservationThreshhold,
    const char *title,
    int nFeatures,
    const AlignmentFeature *features,
    ncbi::CNcbiOstream *outputStream,       /**< Regular program output (alignments); cout if NULL */
    ncbi::CNcbiOstream *diagnosticStream    /**< Diagnostic messages/errors; cerr if NULL */
);

#endif

#endif /* CDDALIGNVIEW__H */
