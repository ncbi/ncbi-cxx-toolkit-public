/* xm_index.c
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
 * File Name:  xm_index.c
 *
 * Author: Sergey Bazhin
 *
 * File Description:
 * -----------------
 *      Parsing flat records to memory blocks in XML format.
 *
 */
#include <ncbi_pch.hpp>

#include <objtools/flatfile/ftacpp.hpp>

#include <objtools/flatfile/index.h>
#include <objtools/flatfile/utilfun.h>
#include <objtools/flatfile/indx_blk.h>
#include <objtools/flatfile/fta_xml.h>

#include "indx_def.h"

#ifdef THIS_FILE
#    undef THIS_FILE
#endif
#define THIS_FILE "xm_index.cpp"

#define XML_FAKE_ACC_TAG "AC   "

typedef struct _XmlKwordBlk {
    const char *str;
    Int4       order;
    Int4       tag;
} XmlKwordBlk, *XmlKwordBlkPtr;

XmlKwordBlk xmkwl[] = {
    {"<INSDSeq_locus>",                 1, INSDSEQ_LOCUS},
    {"<INSDSeq_length>",                2, INSDSEQ_LENGTH},
    {"<INSDSeq_strandedness>",          3, INSDSEQ_STRANDEDNESS},
    {"<INSDSeq_moltype>",               4, INSDSEQ_MOLTYPE},
    {"<INSDSeq_topology>",              5, INSDSEQ_TOPOLOGY},
    {"<INSDSeq_division>",              6, INSDSEQ_DIVISION},
    {"<INSDSeq_update-date>",           7, INSDSEQ_UPDATE_DATE},
    {"<INSDSeq_create-date>",           8, INSDSEQ_CREATE_DATE},
    {"<INSDSeq_update-release>",        9, INSDSEQ_UPDATE_RELEASE},
    {"<INSDSeq_create-release>",       10, INSDSEQ_CREATE_RELEASE},
    {"<INSDSeq_definition>",           11, INSDSEQ_DEFINITION},
    {"<INSDSeq_primary-accession>",    12, INSDSEQ_PRIMARY_ACCESSION},
    {"<INSDSeq_entry-version>",        13, INSDSEQ_ENTRY_VERSION},
    {"<INSDSeq_accession-version>",    14, INSDSEQ_ACCESSION_VERSION},
    {"<INSDSeq_other-seqids>",         15, INSDSEQ_OTHER_SEQIDS},
    {"<INSDSeq_secondary-accessions>", 16, INSDSEQ_SECONDARY_ACCESSIONS},
    {"<INSDSeq_keywords>",             17, INSDSEQ_KEYWORDS},
    {"<INSDSeq_segment>",              18, INSDSEQ_SEGMENT},
    {"<INSDSeq_source>",               19, INSDSEQ_SOURCE},
    {"<INSDSeq_organism>",             20, INSDSEQ_ORGANISM},
    {"<INSDSeq_taxonomy>",             21, INSDSEQ_TAXONOMY},
    {"<INSDSeq_references>",           22, INSDSEQ_REFERENCES},
    {"<INSDSeq_comment>",              23, INSDSEQ_COMMENT},
    {"<INSDSeq_primary>",              24, INSDSEQ_PRIMARY},
    {"<INSDSeq_source-db>",            25, INSDSEQ_SOURCE_DB},
    {"<INSDSeq_database-reference>",   26, INSDSEQ_DATABASE_REFERENCE},
    {"<INSDSeq_feature-table>",        27, INSDSEQ_FEATURE_TABLE},
    {"<INSDSeq_sequence>",             28, INSDSEQ_SEQUENCE},
    {"<INSDSeq_contig>",               29, INSDSEQ_CONTIG},
    {NULL,                             -1, -1}
};

XmlKwordBlk xmfeatkwl[] = {
    {"<INSDFeature_key>",               1, INSDFEATURE_KEY},
    {"<INSDFeature_location>",          2, INSDFEATURE_LOCATION},
    {"<INSDFeature_intervals>",         3, INSDFEATURE_INTERVALS},
    {"<INSDFeature_quals>",             4, INSDFEATURE_QUALS},
    {NULL,                             -1, -1}
};

XmlKwordBlk xmintkwl[] = {
    {"<INSDInterval_from>",             1, INSDINTERVAL_FROM},
    {"<INSDInterval_to>",               2, INSDINTERVAL_TO},
    {"<INSDInterval_point>",            3, INSDINTERVAL_POINT},
    {"<INSDInterval_accession>",        4, INSDINTERVAL_ACCESSION},
    {NULL,                             -1, -1}
};

XmlKwordBlk xmrefkwl[] = {
    {"<INSDReference_reference>",       1, INSDREFERENCE_REFERENCE},
    {"<INSDReference_position>",        2, INSDREFERENCE_POSITION},
    {"<INSDReference_authors>",         3, INSDREFERENCE_AUTHORS},
    {"<INSDReference_consortium>",      4, INSDREFERENCE_CONSORTIUM},
    {"<INSDReference_title>",           5, INSDREFERENCE_TITLE},
    {"<INSDReference_journal>",         6, INSDREFERENCE_JOURNAL},
    {"<INSDReference_xref>",            7, INSDREFERENCE_XREF},
    {"<INSDReference_medline>",         8, INSDREFERENCE_MEDLINE},
    {"<INSDReference_pubmed>",          9, INSDREFERENCE_PUBMED},
    {"<INSDReference_remark>",         10, INSDREFERENCE_REMARK},
    {NULL,                             -1, -1}
};

XmlKwordBlk xmqualkwl[] = {
    {"<INSDQualifier_name>",            1, INSDQUALIFIER_NAME},
    {"<INSDQualifier_value>",           2, INSDQUALIFIER_VALUE},
    {NULL,                             -1, -1}
};

XmlKwordBlk xmxrefkwl[] = {
    {"<INSDXref_dbname>",               1, INSDXREF_DBNAME},
    {"<INSDXref_id>",                   2, INSDXREF_ID},
    {NULL,                             -1, -1}
};

XmlKwordBlk xmsubkwl[] = {
    {"<INSDSecondary-accn>",            1, INSDSECONDARY_ACCN},
    {"<INSDKeyword>",                   1, INSDKEYWORD},
    {"<INSDFeature>",                   1, INSDFEATURE},
    {"<INSDInterval>",                  1, INSDINTERVAL},
    {"<INSDQualifier>",                 1, INSDQUALIFIER},
    {"<INSDReference>",                 1, INSDREFERENCE},
    {"<INSDAuthor>",                    1, INSDAUTHOR},
    {"<INSDXref>",                      1, INSDXREF},
    {NULL,                             -1, -1}
};

/**********************************************************/
static XmlIndexPtr XMLIndexNew(void)
{
    XmlIndexPtr xip;

    xip = (XmlIndexPtr) MemNew(sizeof(XmlIndex));
    xip->tag = -1;
    xip->order = -1;
    xip->start = 0;
    xip->end = 0;
    xip->start_line = -1;
    xip->end_line = -1;
    xip->subtags = NULL;
    xip->next = NULL;
    return(xip);
}

/**********************************************************/
static void XMLRestoreSpecialCharacters(char* buf)
{
    char* p;
    char* q;

    for(p = buf, q = buf; *p != '\0';)
    {
        if(StringNCmp(p, "&lt;", 4) == 0)
        {
            *q++ = '<';
            p += 4;
        }
        else if(StringNCmp(p, "&gt;", 4) == 0)
        {
            *q++ = '>';
            p += 4;
        }
        else if(StringNCmp(p, "&amp;", 5) == 0)
        {
            *q++ = '&';
            p += 5;
        }
        else if(StringNCmp(p, "&apos;", 6) == 0)
        {
            *q++ = '\'';
            p += 6;
        }
        else if(StringNCmp(p, "&quot;", 6) == 0)
        {
            *q++ = '\"';
            p += 6;
        }
        else
            *q++ = *p++;
    }
    *q = '\0';
}

/**********************************************************/
char* XMLGetTagValue(char* entry, XmlIndexPtr xip)
{
    if(entry == NULL || xip == NULL || xip->start == 0 || xip->end == 0 ||
       xip->start >= xip->end)
        return(NULL);

    size_t i = xip->end - xip->start;
    char* buf = (char*)MemNew(i + 1);
    StringNCpy(buf, entry + xip->start, i);
    buf[i] = '\0';

    XMLRestoreSpecialCharacters(buf);
    return(buf);
}

/**********************************************************/
char* XMLFindTagValue(char* entry, XmlIndexPtr xip, Int4 tag)
{
    for(; xip != NULL; xip = xip->next)
        if(xip->tag == tag)
            break;
    if(xip == NULL)
        return(NULL);
    return(XMLGetTagValue(entry, xip));
}

/**********************************************************/
static bool XMLDelSegnum(IndexblkPtr ibp, char* segnum, size_t len2)
{
    if(segnum == NULL)
        return false;
    size_t len1 = StringLen(segnum);
    if(len2 < len1)
        return false;

    /* check, is there enough digits to delete
     */
    size_t tlen = len1;
    char* str = ibp->blocusname;
    size_t i = StringLen(str) - 1;
    for(; tlen > 0 && str[i] >= '0' && str[i] <= '9'; i--)
        tlen--;

    if(tlen != 0 || i < 0)
        return false;

    if(len2 > len1 && str[i] == '0')
    {
        /* check, is there enough "0" appended
         */
        for(tlen = len2 - len1; tlen > 0 && str[i] == '0'; i--)
            tlen--;

        if(tlen != 0)
            return false;
    }

    char* p;
    char* q;
    for (q = &str[i + 1], p = q; *p == '0';)
        p++;

    i = atoi(segnum);
    if(atoi(p) != (int) i)
    {
        ErrPostEx(SEV_REJECT, ERR_SEGMENT_BadLocusName,
                  "Segment suffix in locus name \"%s\" does not match number in <INSDSEQ_segment> line = \"%d\". Entry dropped.",
                  str, i);
        ibp->drop = 1;
    }

    *q = '\0';                          /* strip off "len" characters */
    return true;
}

/**********************************************************/
static void XMLGetSegment(char* entry, IndexblkPtr ibp)
{
    TokenStatBlkPtr stoken;
    XmlIndexPtr     xip;
    char*         buf;
    char*         segnum;
    char*         segtotal;

    if(entry == NULL || ibp == NULL || ibp->xip == NULL)
        return;

    for(xip = ibp->xip; xip != NULL; xip = xip->next)
        if(xip->tag == INSDSEQ_SEGMENT)
            break;
    if(xip == NULL)
        return;

    buf = XMLGetTagValue(entry, xip);
    if(buf == NULL)
        return;

    stoken = TokenString(buf, ' ');

    if(stoken->num > 2)
    {
        segnum = stoken->list->str;
        segtotal = stoken->list->next->next->str;
        ibp->segnum = (Uint2) atoi(segnum);

        if (!XMLDelSegnum(ibp, segnum, StringLen(segtotal)))
        {
            ErrPostEx(SEV_ERROR, ERR_SEGMENT_BadLocusName,
                      "Bad locus name \"%s\".", ibp->blocusname);
        }

        ibp->segtotal = (Uint2) atoi(segtotal);
    }
    else
    {
        ErrPostEx(SEV_ERROR, ERR_SEGMENT_IncompSeg,
                  "Incomplete Segment information at line %d.",
                  xip->start_line);
    }

    FreeTokenstatblk(stoken);
    MemFree(buf);
}

/**********************************************************/
static void XMLPerformIndex(ParserPtr pp)
{
    XmlKwordBlkPtr xkbp;
    IndBlkNextPtr  ibnp;
    IndBlkNextPtr  tibnp;
    XmlIndexPtr    xip;
    IndexblkPtr    ibp;
    char*        p;
    Char           s[60];
    Char           ch;
    size_t         count;
    Int4           line;
    Int4           c;
    Int4           i;

    if(pp == NULL || pp->ifp == NULL)
        return;

    c = 0;
    s[0] = '\0';
    bool within = false;
    tibnp = NULL;
    ibnp = NULL;
    ibp = NULL;
    xip = NULL;
    pp->indx = 0;
    size_t start_len = StringLen(INSDSEQ_START);
    for(count = 0, line = 1;;)
    {
        if(c != '<')
        {
            c = fgetc(pp->ifp);
            if(c < 0)
                break;
            count++;
            if((Char) c == '\n')
                line++;
        }
        if(c != '<')
            continue;

        s[0] = '<';
        for(i = 1; i < 50; i++)
        {
            c = fgetc(pp->ifp);
            if(c < 0)
                break;
            count++;
            ch = (Char) c;
            if(ch == '\n')
                line++;
            s[i] = ch;
            if(ch == '<' || ch == '>')
                break;
        }
        if(c < 0)
            break;
        if(ch == '<')
            continue;
        s[++i] = '\0';
        if(StringCmp(s, INSDSEQ_START) == 0)
        {
            if (within)
                continue;

            within = true;
            if(ibnp == NULL)
            {
                ibnp = (IndBlkNextPtr) MemNew(sizeof(IndBlkNext));
                tibnp = ibnp;
            }
            else
            {
                tibnp->next = (IndBlkNextPtr) MemNew(sizeof(IndBlkNext));
                tibnp = tibnp->next;
            }
            tibnp->next = NULL;
            tibnp->ibp = new Indexblk;
            ibp = tibnp->ibp;
            ibp->xip = NULL;
            ibp->offset = count - start_len;
            ibp->linenum = line;
            ibp->len = 0;
            pp->indx++;
            continue;
        }
        if (!within)
        {
            if(StringCmp(s, INSDSEQ_END) == 0)
                ErrPostEx(SEV_ERROR, ERR_FORMAT_UnexpectedEnd,
                          "Unexpected end tag \"%s\" of XML record found at line %d.",
                          s, line);
            continue;
        }
        if(StringCmp(s, INSDSEQ_END) == 0)
        {
            ibp->len = count - ibp->offset;
            within = false;
            continue;
        }
        p = s + ((s[1] == '/') ? 2 : 1);
        for(xkbp = xmkwl; xkbp->str != NULL; xkbp++)
            if(StringCmp(p, xkbp->str + 1) == 0)
                break;
        if(xkbp->str == NULL)
            continue;
        if(ibp->xip == NULL || xip->tag != xkbp->tag)
        {
            if(ibp->xip == NULL)
            {
                ibp->xip = XMLIndexNew();
                xip = ibp->xip;
            }
            else
            {
                xip->next = XMLIndexNew();
                xip = xip->next;
            }
            xip->tag = xkbp->tag;
            xip->order = xkbp->order;
            if(s[1] == '/')
            {
                xip->end = count - i - ibp->offset;
                xip->end_line = line;
            }
            else
            {
                xip->start = count - ibp->offset;
                xip->start_line = line;
            }
            continue;
        }
        if(s[1] == '/')
        {
            if(xip->end != 0)
            {
                xip->next = XMLIndexNew();
                xip = xip->next;
                xip->tag = xkbp->tag;
                xip->order = xkbp->order;
            }
            xip->end = count - i - ibp->offset;
            xip->end_line = line;
        }
        else
        {
            if(xip->start != 0)
            {
                xip->next = XMLIndexNew();
                xip = xip->next;
                xip->tag = xkbp->tag;
                xip->order = xkbp->order;
            }
            xip->start = count - ibp->offset;
            xip->start_line = line;
        }
    }

    pp->entrylist = (IndexblkPtr*) MemNew((pp->indx + 1) *
                                              sizeof(IndexblkPtr));
    for(tibnp = ibnp, i = 0; tibnp != NULL; i++, tibnp = ibnp)
    {
        pp->entrylist[i] = tibnp->ibp;
        ibnp = tibnp->next;
        MemFree(tibnp);
    }
    pp->entrylist[i] = NULL;
}

/**********************************************************/
static void XMLParseVersion(IndexblkPtr ibp, char* line)
{
    char* p;
    char* q;

    if(line == NULL)
    {
        ErrPostEx(SEV_FATAL, ERR_VERSION_BadVersionLine,
                  "Empty <INSDSeq_accession-version> line. Entry dropped.");
        ibp->drop = 1;
        return;
    }

    for(p = line; *p != '\0' && *p != ' ' && *p != '\t';)
        p++;
    if(*p != '\0')
    {
        ErrPostEx(SEV_FATAL, ERR_VERSION_BadVersionLine,
                  "Incorrect <INSDSeq_accession-version> line: \"%s\". Entry dropped.",
                  line);
        ibp->drop = 1;
        return;
    }
    q = StringRChr(line, '.');
    if(q == NULL)
    {
        ErrPostEx(SEV_FATAL, ERR_VERSION_MissingVerNum,
                  "Missing version number in <INSDSeq_accession-version> line: \"%s\". Entry dropped.",
                  line);
        ibp->drop = 1;
        return;
    }
    for(p = q + 1; *p >= '0' && *p <= '9';)
        p++;
    if(*p != '\0')
    {
        ErrPostEx(SEV_FATAL, ERR_VERSION_NonDigitVerNum,
                  "Incorrect VERSION number in <INSDSeq_accession-version> line: \"%s\". Entry dropped.",
                  line);
        ibp->drop = 1;
        return;
    }
    *q = '\0';
    if(ibp->acnum == NULL || StringCmp(ibp->acnum, line) != 0)
    {
        *q = '.';
        ErrPostEx(SEV_FATAL, ERR_VERSION_AccessionsDontMatch,
                  "Accessions in <INSDSeq_accession-version> and <INSDSeq_primary-accession> lines don't match: \"%s\" vs \"%s\". Entry dropped.",
                  line, (ibp->acnum == NULL) ? "NULL" : ibp->acnum);
        ibp->drop = 1;
        return;
    }
    *q++ = '.';
    ibp->vernum = atoi(q);

    if(ibp->vernum > 0)
        return;

    ErrPostEx(SEV_FATAL, ERR_VERSION_InvalidVersion,
              "Version number \"%d\" from Accession.Version value \"%s.%d\" is not a positive integer. Entry dropped.",
              ibp->vernum, ibp->acnum, ibp->vernum);
    ibp->drop = 1;
}

/**********************************************************/
static void XMLInitialEntry(IndexblkPtr ibp, char* entry, bool accver,
                            Int2 source)
{
    XmlIndexPtr xip;
    char*     buf;

    if(ibp == NULL || ibp->xip == NULL || entry == NULL)
        return;
    xip = ibp->xip;
    ibp->locusname[0] = '\0';
    ibp->acnum[0] = '\0';
    for(xip = ibp->xip; xip != NULL; xip = xip->next)
    {
        if(xip->tag == INSDSEQ_LOCUS && ibp->locusname[0] == '\0')
        {
            if(xip->start == 0 || xip->end == 0 || xip->start >= xip->end)
            {
                StringCpy(ibp->locusname, "???");
                StringCpy(ibp->blocusname, "???");
                continue;
            }
            size_t imax = xip->end - xip->start;
            if(imax > (int) sizeof(ibp->locusname) - 1)
                imax = sizeof(ibp->locusname) - 1;
            StringNCpy(ibp->locusname, entry + xip->start, imax);
            ibp->locusname[imax] = '\0';
            StringCpy(ibp->blocusname, ibp->locusname);
        }
        else if(xip->tag == INSDSEQ_PRIMARY_ACCESSION && ibp->acnum[0] == '\0')
        {
            if(xip->start == 0 || xip->end == 0 || xip->start >= xip->end)
            {
                StringCpy(ibp->acnum, "???");
                continue;
            }
            size_t imax = xip->end - xip->start;
            if(imax > (int) sizeof(ibp->acnum) - 1)
                imax = sizeof(ibp->acnum) - 1;
            StringNCpy(ibp->acnum, entry + xip->start, imax);
            ibp->acnum[imax] = '\0';
        }
        if(ibp->locusname[0] != '\0' && ibp->acnum[0] != '\0')
            break;
    }

    FtaInstallPrefix(PREFIX_LOCUS, ibp->locusname, NULL);
    if(ibp->acnum[0] == '\0')
        FtaInstallPrefix(PREFIX_ACCESSION, ibp->locusname, NULL);
    else
        FtaInstallPrefix(PREFIX_ACCESSION, ibp->acnum, NULL);

    if(accver)
    {
        for(xip = ibp->xip; xip != NULL; xip = xip->next)
        {
            if(xip->tag != INSDSEQ_ACCESSION_VERSION)
                continue;
            buf = XMLGetTagValue(entry, xip);
            XMLParseVersion(ibp, buf);
            if(buf != NULL)
            {
                FtaInstallPrefix(PREFIX_ACCESSION, buf, NULL);
                MemFree(buf);
            }
            break;
        }
    }

    ibp->bases = 0;
    ibp->date = NULL;
    StringCpy(ibp->division, "???");
    for(xip = ibp->xip; xip != NULL; xip = xip->next)
    {
        if(xip->tag == INSDSEQ_LENGTH && ibp->bases == 0)
        {
            buf = XMLGetTagValue(entry, xip);
            if(buf == NULL)
                continue;
            ibp->bases = (size_t) atoi(buf);
            MemFree(buf);
        }
        else if(xip->tag == INSDSEQ_UPDATE_DATE && ibp->date == NULL)
        {
            buf = XMLGetTagValue(entry, xip);
            if(buf == NULL)
                continue;
            ibp->date = GetUpdateDate(buf, source);
            MemFree(buf);
        }
        else if(xip->tag == INSDSEQ_DIVISION && ibp->division[0] == '?')
        {
            if(xip->start == 0 || xip->end == 0 || xip->start >= xip->end ||
               xip->end - xip->start < 3)
                continue;
            StringNCpy(ibp->division, entry + xip->start, 3);
            ibp->division[3] = '\0';
            if(StringICmp(ibp->division, "EST") == 0)
                ibp->EST = true;
            else if(StringCmp(ibp->division, "STS") == 0)
                ibp->STS = true;
            else if(StringCmp(ibp->division, "GSS") == 0)
                ibp->GSS = true;
            else if(StringCmp(ibp->division, "HTC") == 0)
                ibp->HTC = true;
        }
        if(ibp->bases > 0 && ibp->date != NULL && ibp->division[0] != '?')
            break;
    }
}

/**********************************************************/
static const char *XMLStringByTag(XmlKwordBlkPtr xkbp, Int4 tag)
{
    for(; xkbp->str != NULL; xkbp++)
        if(xkbp->tag == tag)
            break;
    if(xkbp->str == NULL)
        return("???");
    return(xkbp->str);
}

/**********************************************************/
static bool XMLTagCheck(XmlIndexPtr xip, XmlKwordBlkPtr xkbp)
{
    XmlIndexPtr txip;
    bool ret = true;
    for(txip = xip; txip != NULL; txip = txip->next)
    {
        if(txip->start == 0)
        {
            ErrPostEx(SEV_ERROR, ERR_FORMAT_XMLMissingStartTag,
                      "XML record's missing start tag for \"%s\" at line %d.",
                      XMLStringByTag(xkbp, txip->tag), txip->end_line);
            ret = false;
        }
        if(txip->end == 0)
        {
            ErrPostEx(SEV_ERROR, ERR_FORMAT_XMLMissingEndTag,
                      "XML record's missing end tag for \"%s\" at line %d.",
                      XMLStringByTag(xkbp, txip->tag), txip->start_line);
            ret = false;
        }
        if(txip->next != NULL && txip->order >= txip->next->order)
        {
            ErrPostEx(SEV_ERROR, ERR_FORMAT_LineTypeOrder,
                      "XML tag \"%s\" at line %d is out of order.",
                      XMLStringByTag(xkbp, txip->next->tag),
                      (txip->next->start > 0) ? txip->next->start_line :
                                                txip->next->end_line);
            ret = false;
        }
    }
    return(ret);
}

/**********************************************************/
static bool XMLSameTagsCheck(XmlIndexPtr xip, char* name)
{
    bool ret = true;

    for (XmlIndexPtr txip = xip; txip != NULL; txip = txip->next)
    {
        if(txip->start == 0)
        {
            ErrPostEx(SEV_ERROR, ERR_FORMAT_XMLMissingStartTag,
                      "XML record's missing start tag for \"%s\" at line %d.",
                      name, txip->end_line);
            ret = false;
        }
        if(txip->end == 0)
        {
            ErrPostEx(SEV_ERROR, ERR_FORMAT_XMLMissingEndTag,
                      "XML record's missing end tag for \"%s\" at line %d.",
                      name, txip->start_line);
            ret = false;
        }
    }
    return(ret);
}

/**********************************************************/
static XmlIndexPtr XMLIndexSameSubTags(char* entry, XmlIndexPtr xip,
                                       Int4 tag)
{
    XmlIndexPtr xipsub;
    XmlIndexPtr txipsub;
    char*     name;
    char*     c;
    char*     p;
    size_t      count;
    Char        s[60];
    Int4        line;
    Int4        i;

    if(entry == NULL || xip == NULL)
        return(NULL);

    name = (char*) XMLStringByTag(xmsubkwl, tag);
    if(name == NULL)
        return(NULL);

    s[0] = '\0';
    xipsub = NULL;
    txipsub = NULL;
    line = xip->start_line;
    c = entry + xip->start;
    for(count = xip->start + 1;;)
    {
        if(*c != '<')
        {
            c++;
            count++;
            if(*c == '\0' || count > xip->end)
                break;
            if(*c == '\n')
                line++;
        }
        if(*c != '<')
            continue;

        for(s[0] = '<', i = 1; i < 50; i++)
        {
            c++;
            count++;
            if(*c == '\0' || count > xip->end)
                break;
            if(*c == '\n')
                line++;
            s[i] = *c;
            if(*c == '<' || *c == '>')
                break;
        }
        if(*c == '\0' || count > xip->end)
            break;
        if(*c == '<')
            continue;
        s[++i] = '\0';
        p = s + ((s[1] == '/') ? 2 : 1);
        if(StringCmp(p, name + 1) != 0)
            continue;

        if(xipsub == NULL)
        {
            xipsub = XMLIndexNew();
            txipsub = xipsub;
        }
        else if((s[1] != '/' && txipsub->start != 0) ||
                (s[1] == '/' && txipsub->end != 0))
        {
            txipsub->next = XMLIndexNew();
            txipsub = txipsub->next;
        }
        if(s[1] == '/')
        {
            txipsub->end = count - i;
            txipsub->end_line = line;
        }
        else
        {
            txipsub->start = count;
            txipsub->start_line = line;
        }
        txipsub->tag = tag;
    }

    if(XMLSameTagsCheck(xipsub, name))
        return(xipsub);

    XMLIndexFree(xipsub);
    return(NULL);
}

/**********************************************************/
static bool XMLAccessionsCheck(ParserPtr pp, IndexblkPtr ibp, char* entry)
{
    XmlIndexPtr xip;
    XmlIndexPtr xipsec;
    char*     buf;
    char*     p;

    bool ret = true;
    size_t len = StringLen(ibp->acnum) + StringLen(XML_FAKE_ACC_TAG) + 1;

    for(xip = ibp->xip; xip != NULL; xip = xip->next)
        if(xip->tag == INSDSEQ_SECONDARY_ACCESSIONS)
            break;

    if(xip == NULL)
    {
        buf = (char*) MemNew(len);
        StringCpy(buf, XML_FAKE_ACC_TAG);
        StringCat(buf, ibp->acnum);
        ret = GetAccession(pp, buf, ibp, 2);
        MemFree(buf);
        return(ret);
    }

    xip->subtags = XMLIndexSameSubTags(entry, xip, INSDSECONDARY_ACCN);
    if(xip->subtags == NULL)
    {
        p = (char*) XMLStringByTag(xmkwl, INSDSEQ_SECONDARY_ACCESSIONS);
        ErrPostEx(SEV_ERROR, ERR_FORMAT_XMLFormatError,
                  "Incorrectly formatted \"%s\" XML block. Entry dropped.", p);
        ibp->drop = 1;
        return false;
    }

    for(xipsec = xip->subtags; xipsec != NULL; xipsec = xipsec->next)
        len += (xipsec->end - xipsec->start + 1);

    buf = (char*) MemNew(len);
    StringCpy(buf, XML_FAKE_ACC_TAG);
    StringCat(buf, ibp->acnum);
    for(xipsec = xip->subtags; xipsec != NULL; xipsec = xipsec->next)
    {
        p = XMLGetTagValue(entry, xipsec);
        if(p == NULL)
            continue;
        StringCat(buf, " ");
        StringCat(buf, p);
        MemFree(p);
    }
    ret = GetAccession(pp, buf, ibp, 2);
    MemFree(buf);
    return(ret);
}

/**********************************************************/
static bool XMLKeywordsCheck(char* entry, IndexblkPtr ibp, Int2 source)
{
    XmlIndexPtr xip;
    XmlIndexPtr xipkwd;
    ValNodePtr  vnp;
    char*     buf;
    char*     p;

    bool tpa_check = (source == ParFlat_EMBL);

    if(entry == NULL || ibp == NULL || ibp->xip == NULL)
        return true;

    for(xip = ibp->xip; xip != NULL; xip = xip->next)
        if(xip->tag == INSDSEQ_KEYWORDS)
            break;
    if(xip == NULL)
        return true;

    xip->subtags = XMLIndexSameSubTags(entry, xip, INSDKEYWORD);
    if(xip->subtags == NULL)
    {
        p = (char*) XMLStringByTag(xmkwl, INSDSEQ_KEYWORDS);
        ErrPostEx(SEV_ERROR, ERR_FORMAT_XMLFormatError,
                  "Incorrectly formatted \"%s\" XML block. Entry dropped.", p);
        ibp->drop = 1;
        return false;
    }

    size_t len = 0;
    for(xipkwd = xip->subtags; xipkwd != NULL; xipkwd = xipkwd->next)
        len += (xipkwd->end - xipkwd->start + 2);

    buf = (char*) MemNew(len);
    *buf = '\0';
    for(xipkwd = xip->subtags; xipkwd != NULL; xipkwd = xipkwd->next)
    {
        p = XMLGetTagValue(entry, xipkwd);
        if(p == NULL)
            continue;
        if(*buf != '\0')
            StringCat(buf, "; ");
        StringCat(buf, p);
        MemFree(p);
    }

    vnp = ConstructValNode(NULL, 0, buf);
    check_est_sts_gss_tpa_kwds(vnp, len, ibp, tpa_check, ibp->specialist_db,
                               ibp->inferential, ibp->experimental,
                               ibp->assembly);
    MemFree(buf);
    MemFree(vnp);
    return true;
}

/**********************************************************/
static bool XMLErrField(Int4 tag)
{
    ErrPostEx(SEV_ERROR, ERR_FORMAT_MissingField,
              "No %s data in XML format file. Entry dropped.",
              XMLStringByTag(xmkwl, tag));
    return false;
}

/**********************************************************/
static bool XMLCheckRequiredTags(ParserPtr pp, IndexblkPtr ibp)
{
    XmlIndexPtr xip;
    bool     got_locus = false;
    bool     got_length = false;
    bool     got_moltype = false;
    bool     got_division = false;
    bool     got_update_date = false;
    bool     got_definition = false;
    bool     got_accession = false;
    bool     got_version = false;
    bool     got_source = false;
    bool     got_organism = false;
    bool     got_reference = false;
    bool     got_primary = false;
    bool     got_features = false;
    bool     ret = true;

    ibp->origin = false;
    ibp->is_contig = false;
    for(xip = ibp->xip; xip != NULL; xip = xip->next)
    {
        if(xip->tag == INSDSEQ_LOCUS)
            got_locus = true;
        else if(xip->tag == INSDSEQ_LENGTH)
            got_length = true;
        else if(xip->tag == INSDSEQ_MOLTYPE)
            got_moltype = true;
        else if(xip->tag == INSDSEQ_DIVISION)
            got_division = true;
        else if(xip->tag == INSDSEQ_UPDATE_DATE)
            got_update_date = true;
        else if(xip->tag == INSDSEQ_DEFINITION)
            got_definition = true;
        else if(xip->tag == INSDSEQ_PRIMARY_ACCESSION)
            got_accession = true;
        else if(xip->tag == INSDSEQ_ACCESSION_VERSION)
            got_version = true;
        else if(xip->tag == INSDSEQ_SOURCE)
            got_source = true;
        else if(xip->tag == INSDSEQ_ORGANISM)
            got_organism = true;
        else if(xip->tag == INSDSEQ_REFERENCES)
            got_reference = true;
        else if(xip->tag == INSDSEQ_PRIMARY)
            got_primary = true;
        else if(xip->tag == INSDSEQ_FEATURE_TABLE)
            got_features = true;
        else if(xip->tag == INSDSEQ_CONTIG)
            ibp->is_contig = true;
        else if(xip->tag == INSDSEQ_SEQUENCE)
            ibp->origin = true;
    }

    if(got_locus == false)
        ret = XMLErrField(INSDSEQ_LOCUS);
    if(got_length == false)
        ret = XMLErrField(INSDSEQ_LENGTH);
    if(got_moltype == false)
        ret = XMLErrField(INSDSEQ_MOLTYPE);
    if(got_division == false)
        ret = XMLErrField(INSDSEQ_DIVISION);
    if(got_update_date == false)
        ret = XMLErrField(INSDSEQ_UPDATE_DATE);
    if(got_definition == false)
        ret = XMLErrField(INSDSEQ_DEFINITION);
    if(got_accession == false)
    {
        ErrPostEx(SEV_ERROR, ERR_ACCESSION_NoAccessNum,
                  "No accession number for this record. Entry dropped.");
        ret = false;
    }
    if(got_version == false && pp->accver)
        ret = XMLErrField(INSDSEQ_ACCESSION_VERSION);
    if(got_source == false)
        ret = XMLErrField(INSDSEQ_SOURCE);
    if(got_organism == false)
        ret = XMLErrField(INSDSEQ_ORGANISM);
    if(got_reference == false && pp->source != ParFlat_FLYBASE &&
       ibp->is_wgs == false &&
       (pp->source != ParFlat_REFSEQ ||
        StringNCmp(ibp->acnum, "NW_", 3) != 0))
        ret = XMLErrField(INSDSEQ_REFERENCES);
    if (got_primary && ibp->is_tpa == false && ibp->tsa_allowed == false)
    {
        ErrPostEx(SEV_ERROR, ERR_ENTRY_InvalidLineType,
                  "Line type %s is allowed for TPA or TSA records only. Continue anyway.",
                  XMLStringByTag(xmkwl, INSDSEQ_PRIMARY));
    }
    if(got_features == false)
        ret = XMLErrField(INSDSEQ_FEATURE_TABLE);
    if(ibp->is_contig && ibp->segnum != 0)
    {
        ErrPostEx(SEV_ERROR, ERR_FORMAT_ContigInSegset,
                  "%s data are not allowed for members of segmented sets. Entry dropped.",
                  XMLStringByTag(xmkwl, INSDSEQ_CONTIG));
        ret = false;
    }

    ibp->is_tpa_wgs_con = (ibp->is_contig && ibp->is_wgs && ibp->is_tpa);

    return(ret);
}

/**********************************************************/
char* XMLLoadEntry(ParserPtr pp, bool err)
{
    IndexblkPtr ibp;
    char*     entry;
    char*     p;
    size_t      i;
    Int4        c;

    if(pp == NULL || pp->ifp == NULL)
        return(NULL);

    ibp = pp->entrylist[pp->curindx];
    if(ibp == NULL || ibp->len == 0)
        return(NULL);

    entry = (char*) MemNew(ibp->len + 1);
    fseek(pp->ifp, static_cast<long>(ibp->offset), 0);
    for(p = entry, i = 0; i < ibp->len; i++)
    {
        c = fgetc(pp->ifp);
        if(c < 0)
            break;
        if (c == 13) {
            c = 10;
        }
        if(c > 126 || (c < 32 && c != 10))
        {
            if (err)
                ErrPostEx(SEV_WARNING, ERR_FORMAT_NonAsciiChar,
                          "None-ASCII character within the record which begins at line %d, decimal value %d, replaced by #.",
                          ibp->linenum, c);
            *p++ = '#';
        }
        else
            *p++ = (Char) c;
    }
    if(i != ibp->len)
    {
        MemFree(entry);
        return(NULL);
    }
    *p = '\0';

    return(entry);
}

/**********************************************************/
static bool XMLIndexSubTags(char* entry, XmlIndexPtr xip, XmlKwordBlkPtr xkbp)
{
    XmlKwordBlkPtr txkbp;
    XmlIndexPtr    xipsub;
    char*        c;
    char*        p;
    Char           s[60];
    size_t         count;
    Int4           line;
    Int4           i;

    if(entry == NULL || xip == NULL)
        return false;

    s[0] = '\0';
    xipsub = NULL;
    line = xip->start_line;
    c = entry + xip->start;
    for(count = xip->start + 1;;)
    {
        if(*c != '<')
        {
            c++;
            count++;
            if(*c == '\0' || count > xip->end)
                break;
            if(*c == '\n')
                line++;
        }
        if(*c != '<')
            continue;

        for(s[0] = '<', i = 1; i < 50; i++)
        {
            c++;
            count++;
            if(*c == '\0' || count > xip->end)
                break;
            if(*c == '\n')
                line++;
            s[i] = *c;
            if(*c == '<' || *c == '>')
                break;
        }
        if(*c == '\0' || count > xip->end)
            break;
        if(*c == '<')
            continue;
        s[++i] = '\0';
        p = s + ((s[1] == '/') ? 2 : 1);
        for(txkbp = xkbp; txkbp->str != NULL; txkbp++)
            if(StringCmp(p, txkbp->str + 1) == 0)
                break;
        if(txkbp->str == NULL)
            continue;
        if(xipsub == NULL || xipsub->tag != txkbp->tag)
        {
            if(xipsub == NULL)
            {
                xipsub = XMLIndexNew();
                xip->subtags = xipsub;
            }
            else
            {
                xipsub->next = XMLIndexNew();
                xipsub = xipsub->next;
            }
            xipsub->tag = txkbp->tag;
            xipsub->order = txkbp->order;
            if(s[1] == '/')
            {
                xipsub->end = count - i;
                xipsub->end_line = line;
            }
            else
            {
                xipsub->start = count;
                xipsub->start_line = line;
            }
            continue;
        }
        if(s[1] == '/')
        {
            if(xipsub->end != 0)
            {
                xipsub->next = XMLIndexNew();
                xipsub = xipsub->next;
                xipsub->tag = txkbp->tag;
                xipsub->order = txkbp->order;
            }
            xipsub->end = count - i;
            xipsub->end_line = line;
        }
        else
        {
            if(xipsub->start != 0)
            {
                xipsub->next = XMLIndexNew();
                xipsub = xipsub->next;
                xipsub->tag = txkbp->tag;
                xipsub->order = txkbp->order;
            }
            xipsub->start = count;
            xipsub->start_line = line;
        }
    }

    if (!XMLTagCheck(xip->subtags, xkbp))
        return false;

    return true;
}

/**********************************************************/
static bool XMLCheckRequiredFeatTags(XmlIndexPtr xip)
{
    bool got_key = false;
    bool got_location = false;
    bool ret = true;

    for(; xip != NULL; xip = xip->next)
    {
        if(xip->tag == INSDFEATURE_KEY)
            got_key = true;
        else if(xip->tag == INSDFEATURE_LOCATION)
            got_location = true;
    }

    if(!got_key)
    {
        ErrPostEx(SEV_ERROR, ERR_FORMAT_MissingField,
                  "Feature table is missing %s data in XML format file.",
                  XMLStringByTag(xmfeatkwl, INSDFEATURE_KEY));
        ret = false;
    }

    if(!got_location)
    {
        ErrPostEx(SEV_ERROR, ERR_FORMAT_MissingField,
                  "Feature table is missing %s data in XML format file.",
                  XMLStringByTag(xmfeatkwl, INSDFEATURE_LOCATION));
        ret = false;
    }
    return(ret);
}

/**********************************************************/
static bool XMLCheckRequiredIntTags(XmlIndexPtr xip)
{
    bool got_from = false;
    bool got_to = false;
    bool got_point = false;
    bool got_accession = false;
    bool ret = true;

    for(; xip != NULL; xip = xip->next)
    {
        if(xip->tag == INSDINTERVAL_FROM)
            got_from = true;
        else if(xip->tag == INSDINTERVAL_TO)
            got_to = true;
        else if(xip->tag == INSDINTERVAL_POINT)
            got_point = true;
        else if(xip->tag == INSDINTERVAL_ACCESSION)
            got_accession = true;
    }

    if(!got_accession)
    {
        ErrPostEx(SEV_ERROR, ERR_FORMAT_MissingField,
                  "Feature's interval block is missing %s data in XML format file.",
                  XMLStringByTag(xmintkwl, INSDINTERVAL_ACCESSION));
        ret = false;
    }

    if(got_point)
    {
        if(got_from || got_to)
        {
            ErrPostEx(SEV_ERROR, ERR_FORMAT_XMLInvalidINSDInterval,
                      "%s tag cannot co-exist with %s or %s or both in XML format.",
                      XMLStringByTag(xmintkwl, INSDINTERVAL_POINT),
                      XMLStringByTag(xmintkwl, INSDINTERVAL_FROM),
                      XMLStringByTag(xmintkwl, INSDINTERVAL_TO));
            ret = false;
        }
    }
    else if(got_from == false || got_to == false)
    {
        ErrPostEx(SEV_ERROR, ERR_FORMAT_XMLInvalidINSDInterval,
                  "%s must contain either both of %s and %s, or %s.",
                  XMLStringByTag(xmsubkwl, INSDINTERVAL),
                  XMLStringByTag(xmintkwl, INSDINTERVAL_FROM),
                  XMLStringByTag(xmintkwl, INSDINTERVAL_TO),
                  XMLStringByTag(xmintkwl, INSDINTERVAL_POINT));
        ret = false;
    }

    return(ret);
}

/**********************************************************/
static bool XMLCheckRequiredQualTags(XmlIndexPtr xip)
{
    for (; xip != NULL; xip = xip->next)
    {
        if (xip->tag == INSDQUALIFIER_NAME)
            break;
    }

    if(xip != NULL)
        return true;

    ErrPostEx(SEV_ERROR, ERR_FORMAT_MissingField,
              "Qualifier block is missing %s data in XML format file.",
              XMLStringByTag(xmqualkwl, INSDQUALIFIER_NAME));
    return false;
}

/**********************************************************/
static bool XMLIndexFeatures(char* entry, XmlIndexPtr xip)
{
    XmlIndexPtr xipfeat;
    XmlIndexPtr xipsub;
    XmlIndexPtr txip;

    if(xip == NULL || entry == NULL)
        return true;

    for (; xip != NULL; xip = xip->next)
    {
        if (xip->tag == INSDSEQ_FEATURE_TABLE)
            break;
    }

    if(xip == NULL)
        return true;

    xip->subtags = XMLIndexSameSubTags(entry, xip, INSDFEATURE);
    if(xip->subtags == NULL)
    {
        ErrPostEx(SEV_ERROR, ERR_FORMAT_XMLFormatError,
                  "Incorrectly formatted \"%s\" XML block. Entry dropped.",
                  XMLStringByTag(xmkwl, INSDSEQ_FEATURE_TABLE));
        return false;
    }

    for(xipfeat = xip->subtags; xipfeat != NULL; xipfeat = xipfeat->next)
    {
        if(XMLIndexSubTags(entry, xipfeat, xmfeatkwl) == false ||
           XMLCheckRequiredFeatTags(xipfeat->subtags) == false)
            break;
        for(txip = xipfeat->subtags; txip != NULL; txip = txip->next)
        {
            if(txip->tag == INSDFEATURE_INTERVALS)
            {
                txip->subtags = XMLIndexSameSubTags(entry, txip, INSDINTERVAL);
                if(txip->subtags == NULL)
                    break;
                xipsub = txip->subtags;
                for(; xipsub != NULL; xipsub = xipsub->next)
                    if(XMLIndexSubTags(entry, xipsub, xmintkwl) == false ||
                       XMLCheckRequiredIntTags(xipsub->subtags) == false)
                        break;
            }
            else if(txip->tag == INSDFEATURE_QUALS)
            {
                txip->subtags = XMLIndexSameSubTags(entry, txip,
                                                    INSDQUALIFIER);
                if(txip->subtags == NULL)
                    break;
                xipsub = txip->subtags;
                for(; xipsub != NULL; xipsub = xipsub->next)
                    if(XMLIndexSubTags(entry, xipsub, xmqualkwl) == false ||
                       XMLCheckRequiredQualTags(xipsub->subtags) == false)
                        break;
            }
        }
        if(txip != NULL)
            break;
    }

    if(xipfeat == NULL)
        return true;

    ErrPostEx(SEV_ERROR, ERR_FORMAT_XMLFormatError,
              "Incorrectly formatted \"%s\" XML block. Entry dropped.",
              XMLStringByTag(xmkwl, INSDSEQ_FEATURE_TABLE));
    return false;
}

/**********************************************************/
static bool XMLCheckRequiredRefTags(XmlIndexPtr xip)
{
    bool got_reference = false;
    bool got_journal = false;
    bool ret = true;

    for(; xip != NULL; xip = xip->next)
    {
        if(xip->tag == INSDREFERENCE_REFERENCE)
            got_reference = true;
        else if(xip->tag == INSDREFERENCE_JOURNAL)
            got_journal = true;
    }

    if (!got_reference)
    {
        ErrPostEx(SEV_ERROR, ERR_FORMAT_MissingField,
                  "%s block is missing %s data in XML format file.",
                  XMLStringByTag(xmsubkwl, INSDREFERENCE),
                  XMLStringByTag(xmrefkwl, INSDREFERENCE_REFERENCE));
        ret = false;
    }

    if (!got_journal)
    {
        ErrPostEx(SEV_ERROR, ERR_FORMAT_MissingField,
                  "%s block is missing %s data in XML format file.",
                  XMLStringByTag(xmsubkwl, INSDREFERENCE),
                  XMLStringByTag(xmrefkwl, INSDREFERENCE_JOURNAL));
        ret = false;
    }
    return(ret);
}

/**********************************************************/
static Int2 XMLGetRefTypePos(char* reftag, size_t bases)
{
    Char str[100];

    if(reftag == NULL || *reftag == '\0')
        return(ParFlat_REF_NO_TARGET);

    sprintf(str, "1..%d", (int) bases);

    if(StringCmp(reftag, str) == 0)
        return(ParFlat_REF_END);
    if(StringCmp(reftag, "sites") == 0)
        return(ParFlat_REF_SITES);
    return(ParFlat_REF_BTW);
}

/**********************************************************/
static Int2 XMLGetRefType(char* reftag, size_t bases)
{
    char* p;
    Char    str[100];
    Char    str1[100];

    if(reftag == NULL)
        return(ParFlat_REF_NO_TARGET);

    for(p = reftag; *p != '\0' && *p != '(';)
        p++;
    if(*p == '\0')
        return(ParFlat_REF_NO_TARGET);

    sprintf(str, "(bases 1 to %d)", (int) bases);
    sprintf(str1, "(bases 1 to %d;", (int) bases);

    if(StringStr(p, str) != NULL || StringStr(p, str1) != NULL)
        return(ParFlat_REF_END);
    if(StringStr(p, "(sites)") != NULL)
        return(ParFlat_REF_SITES);
    return(ParFlat_REF_BTW);
}

/**********************************************************/
static bool XMLCheckRequiredXrefTags(XmlIndexPtr xip)
{
    bool got_dbname = false;
    bool got_id = false;
    bool ret = true;

    for(; xip != NULL; xip = xip->next)
    {
        if(xip->tag == INSDXREF_DBNAME)
            got_dbname = true;
        else if(xip->tag == INSDXREF_ID)
            got_id = true;
    }

    if (!got_dbname)
    {
        ErrPostEx(SEV_ERROR, ERR_FORMAT_MissingField,
                  "%s block is missing %s data in XML format file.",
                  XMLStringByTag(xmsubkwl, INSDXREF),
                  XMLStringByTag(xmrefkwl, INSDXREF_DBNAME));
        ret = false;
    }

    if (!got_id)
    {
        ErrPostEx(SEV_ERROR, ERR_FORMAT_MissingField,
                  "%s block is missing %s data in XML format file.",
                  XMLStringByTag(xmsubkwl, INSDXREF),
                  XMLStringByTag(xmrefkwl, INSDXREF_ID));
        ret = false;
    }
    return(ret);
}

/**********************************************************/
static bool XMLIndexReferences(char* entry, XmlIndexPtr xip, size_t bases)
{
    XmlIndexPtr xipref;
    XmlIndexPtr txip;
    XmlIndexPtr xipsub;
    char*     reftagref;
    char*     reftagpos;

    if(xip == NULL || entry == NULL)
        return true;

    for (; xip != NULL; xip = xip->next)
    {
        if (xip->tag == INSDSEQ_REFERENCES)
            break;
    }
    if(xip == NULL)
        return true;

    xip->subtags = XMLIndexSameSubTags(entry, xip, INSDREFERENCE);
    if(xip->subtags == NULL)
    {
        ErrPostEx(SEV_ERROR, ERR_FORMAT_XMLFormatError,
                  "Incorrectly formatted \"%s\" XML block. Entry dropped.",
                  XMLStringByTag(xmkwl, INSDSEQ_REFERENCES));
        return false;
    }

    for(xipref = xip->subtags; xipref != NULL; xipref = xipref->next)
    {
        if(XMLIndexSubTags(entry, xipref, xmrefkwl) == false ||
           XMLCheckRequiredRefTags(xipref->subtags) == false)
            break;

        reftagref = NULL;
        reftagpos = NULL;
        for(txip = xipref->subtags; txip != NULL; txip = txip->next)
        {
            if(txip->tag == INSDREFERENCE_REFERENCE)
            {
                if(reftagref != NULL)
                    MemFree(reftagref);
                reftagref = XMLGetTagValue(entry, txip);
                continue;
            }
            if(txip->tag == INSDREFERENCE_POSITION)
            {
                if(reftagpos != NULL)
                    MemFree(reftagpos);
                reftagpos = XMLGetTagValue(entry, txip);
                continue;
            }
            if(txip->tag == INSDREFERENCE_AUTHORS)
            {
                txip->subtags = XMLIndexSameSubTags(entry, txip, INSDAUTHOR);
                if(txip->subtags == NULL)
                    break;
            }
            else if(txip->tag == INSDREFERENCE_XREF)
            {
                txip->subtags = XMLIndexSameSubTags(entry, txip, INSDXREF);
                if(txip->subtags == NULL)
                    break;
                xipsub = txip->subtags;
                for(; xipsub != NULL; xipsub = xipsub->next)
                    if(XMLIndexSubTags(entry, xipsub, xmxrefkwl) == false ||
                       XMLCheckRequiredXrefTags(xipsub->subtags) == false)
                        break;
            }
        }

        if(reftagpos != NULL)
        {
            xipref->type = XMLGetRefTypePos(reftagpos, bases);
            MemFree(reftagpos);
        }
        else
            xipref->type = XMLGetRefType(reftagref, bases);
        if(reftagref != NULL)
            MemFree(reftagref);

        if(txip != NULL)
            break;
    }

    if(xipref == NULL)
        return true;

    ErrPostEx(SEV_ERROR, ERR_FORMAT_XMLFormatError,
              "Incorrectly formatted \"%s\" XML block. Entry dropped.",
              XMLStringByTag(xmkwl, INSDSEQ_REFERENCES));
    return false;
}

/**********************************************************/
bool XMLIndex(ParserPtr pp)
{
    IndexblkPtr* ibpp;
    IndexblkPtr      ibp;
    char*          entry;

    XMLPerformIndex(pp);
    if(pp->indx == 0)
        return false;

    pp->curindx = 0;
    for(ibpp = pp->entrylist; *ibpp != NULL; ibpp++, pp->curindx++)
    {
        ibp = *ibpp;
        if(ibp->len == 0)
        {
            ErrPostEx(SEV_ERROR, ERR_FORMAT_MissingEnd,
                      "Missing end tag of XML record, which starts at line %d. Entry dropped.",
                      ibp->linenum);
            ibp->drop = 1;
            continue;
        }
        entry = XMLLoadEntry(pp, true);
        if(entry == NULL)
        {
            ErrPostEx(SEV_FATAL, ERR_INPUT_CannotReadEntry,
                      "Failed ro read entry from file, which starts at line %d. Entry dropped.",
                      ibp->linenum);
            ibp->drop = 1;
            continue;
        }

        XMLInitialEntry(ibp, entry, pp->accver, pp->source);
        if(ibp->drop != 0)
        {
            MemFree(entry);
            continue;
        }
        if(XMLTagCheck(ibp->xip, xmkwl) == false)
        {
            ErrPostEx(SEV_ERROR, ERR_FORMAT_XMLFormatError,
                      "Incorrectly formatted XML record. Entry dropped.");
            ibp->drop = 1;
            MemFree(entry);
            continue;
        }
        if(XMLAccessionsCheck(pp, ibp, entry) == false)
        {
            MemFree(entry);
            continue;
        }
        XMLGetSegment(entry, ibp);
        if(XMLCheckRequiredTags(pp, ibp) == false)
        {
            ibp->drop = 1;
            MemFree(entry);
            continue;
        }
        if(XMLKeywordsCheck(entry, ibp, pp->source) == false)
        {
            MemFree(entry);
            continue;
        }
        if(XMLIndexFeatures(entry, ibp->xip) == false ||
           XMLIndexReferences(entry, ibp->xip, ibp->bases) == false)
        {
            ibp->drop = 1;
            MemFree(entry);
            continue;
        }
        MemFree(entry);
    }

    for(pp->num_drop = 0, ibpp = pp->entrylist; *ibpp != NULL; ibpp++)
        if((*ibpp)->drop != 0)
            pp->num_drop++;

    if(pp->indx > 0)
        return true;
    return false;
}

/**********************************************************/
DataBlkPtr XMLBuildRefDataBlk(char* entry, XmlIndexPtr xip, Int2 type)
{
    XmlIndexPtr txip;
    DataBlkPtr  dbp;
    DataBlkPtr  tdbp;

    if(entry == NULL || xip == NULL)
        return(NULL);

    while(xip != NULL && xip->tag != INSDSEQ_REFERENCES)
        xip = xip->next;
    if(xip == NULL || xip->subtags == NULL)
        return(NULL);

    for(dbp = NULL, txip = xip->subtags; txip != NULL; txip = txip->next)
    {
        if(txip->type != type || txip->subtags == NULL)
            continue;
        if(dbp == NULL)
        {
            dbp = (DataBlkPtr) MemNew(sizeof(DataBlk));
            tdbp = dbp;
        }
        else
        {
            tdbp->next = (DataBlkPtr) MemNew(sizeof(DataBlk));
            tdbp = tdbp->next;
        }
        tdbp->type = txip->type;
        tdbp->offset = entry;
        tdbp->data = (void*) txip->subtags;
        tdbp->next = NULL;
    }
    return(dbp);
}

/**********************************************************/
void XMLGetKeywords(char* entry, XmlIndexPtr xip, TKeywordList& keywords)
{
    XmlIndexPtr xipkwd;
    char*     p;

    keywords.clear();
    if(entry == NULL || xip == NULL)
        return;

    for(; xip != NULL; xip = xip->next)
        if(xip->tag == INSDSEQ_KEYWORDS && xip->subtags != NULL)
            break;
    if(xip == NULL)
        return;

    for(xipkwd = xip->subtags; xipkwd != NULL; xipkwd = xipkwd->next)
    {
        p = XMLGetTagValue(entry, xipkwd);
        if(p == NULL)
            continue;

        keywords.push_back(p);
        MemFree(p);
    }
}

/**********************************************************/
char* XMLConcatSubTags(char* entry, XmlIndexPtr xip, Int4 tag, Char sep)
{
    XmlIndexPtr txip;
    char*     buf;
    char*     p;
    char*     q;
    size_t      i;

    if(entry == NULL || xip == NULL)
        return(NULL);

    while(xip != NULL && xip->tag != tag)
        xip = xip->next;

    if(xip == NULL || xip->subtags == NULL)
        return(NULL);

    for(i = 0, txip = xip->subtags; txip != NULL; txip = txip->next)
        i += (txip->end - txip->start + 2);

    buf = (char*) MemNew(i);
    buf[0] = '\0';
    for(q = buf, txip = xip->subtags; txip != NULL; txip = txip->next)
    {
        if(txip->end <= txip->start)
            continue;
        if(buf[0] != '\0')
        {
            *q++ = sep;
            *q++ = ' ';
        }
        for(i = txip->start, p = entry + txip->start; i < txip->end; i++)
            *q++ = *p++;
        *q = '\0';
    }
    XMLRestoreSpecialCharacters(buf);
    return(buf);
}
