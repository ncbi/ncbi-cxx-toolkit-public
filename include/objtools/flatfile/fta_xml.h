/* fta_xml.h
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
 * File Name:  fta_xml.h
 *
 * Author: Sergey Bazhin
 *
 * File Description:
 * -----------------
 */

#ifndef _FTA_XML_
#define _FTA_XML_

#define INSDSEQ_START                "<INSDSeq>"
#define INSDSEQ_END                  "</INSDSeq>"

#define INSDSEQ_LOCUS                1
#define INSDSEQ_LENGTH               2
#define INSDSEQ_STRANDEDNESS         3
#define INSDSEQ_MOLTYPE              4
#define INSDSEQ_TOPOLOGY             5
#define INSDSEQ_DIVISION             6
#define INSDSEQ_UPDATE_DATE          7
#define INSDSEQ_CREATE_DATE          8
#define INSDSEQ_UPDATE_RELEASE       9
#define INSDSEQ_CREATE_RELEASE       10
#define INSDSEQ_DEFINITION           11
#define INSDSEQ_PRIMARY_ACCESSION    12
#define INSDSEQ_ENTRY_VERSION        13
#define INSDSEQ_ACCESSION_VERSION    14
#define INSDSEQ_OTHER_SEQIDS         15
#define INSDSEQ_SECONDARY_ACCESSIONS 16
#define INSDSEQ_KEYWORDS             17
#define INSDSEQ_SEGMENT              18
#define INSDSEQ_SOURCE               19
#define INSDSEQ_ORGANISM             20
#define INSDSEQ_TAXONOMY             21
#define INSDSEQ_REFERENCES           22
#define INSDSEQ_COMMENT              23
#define INSDSEQ_PRIMARY              24
#define INSDSEQ_SOURCE_DB            25
#define INSDSEQ_DATABASE_REFERENCE   26
#define INSDSEQ_FEATURE_TABLE        27
#define INSDSEQ_SEQUENCE             28
#define INSDSEQ_CONTIG               29

/* define subkeywords
 */
#define INSDSECONDARY_ACCN           30
#define INSDKEYWORD                  31
#define INSDFEATURE                  32
#define INSDFEATURE_KEY              33
#define INSDFEATURE_LOCATION         34
#define INSDFEATURE_INTERVALS        35
#define INSDFEATURE_QUALS            36
#define INSDINTERVAL                 37
#define INSDINTERVAL_FROM            38
#define INSDINTERVAL_TO              39
#define INSDINTERVAL_POINT           40
#define INSDINTERVAL_ACCESSION       41
#define INSDQUALIFIER                42
#define INSDQUALIFIER_NAME           43
#define INSDQUALIFIER_VALUE          44
#define INSDREFERENCE                45
#define INSDREFERENCE_REFERENCE      46
#define INSDREFERENCE_POSITION       47
#define INSDREFERENCE_AUTHORS        48
#define INSDREFERENCE_CONSORTIUM     49
#define INSDREFERENCE_TITLE          50
#define INSDREFERENCE_JOURNAL        51
#define INSDREFERENCE_MEDLINE        52
#define INSDREFERENCE_PUBMED         53
#define INSDREFERENCE_REMARK         54
#define INSDREFERENCE_XREF           55
#define INSDXREF                     56
#define INSDXREF_DBNAME              57
#define INSDXREF_ID                  58
#define INSDAUTHOR                   59

#define XML_FEATURES                 1

char*    XMLLoadEntry(ParserPtr pp, bool err);
char*    XMLGetTagValue(char* entry, XmlIndexPtr xip);
char*    XMLFindTagValue(char* entry, XmlIndexPtr xip, Int4 tag);
DataBlkPtr XMLBuildRefDataBlk(char* entry, XmlIndexPtr xip, Int2 type);
char*    XMLConcatSubTags(char* entry, XmlIndexPtr xip, Int4 tag, Char sep);
void       XMLGetKeywords(char* entry, XmlIndexPtr xip, TKeywordList& keywords);

#endif
