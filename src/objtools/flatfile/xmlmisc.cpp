/* xmlmisc.cpp
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
 * File Name:  xmlmisc.cpp
 *
 * Author: Alexey Dobronadezhdin
 *
 * File Description:
 * -----------------
 *      XML functionality from C-toolkit.
 */
#include <ncbi_pch.hpp>

#include "ftacpp.hpp"
#include "ftaerr.hpp"
#include "xmlmisc.h"

#ifdef THIS_FILE
#    undef THIS_FILE
#endif
#define THIS_FILE "xmlmisc.cpp"

#define XML_START_TAG  1
#define XML_END_TAG    2
#define XML_ATTRIBUTE  3
#define XML_CONTENT    4

/* function to decode ampersand-protected symbols */

BEGIN_NCBI_SCOPE

typedef struct xmltable {
    const Char*  code;
    size_t       len;
    char     letter;
} Nlm_XmlTable, *Nlm_XmlTablePtr;

static Nlm_XmlTable xmlcodes[] = {
    { "&amp;", 5, '&' },
    { "&apos;", 6, '\'' },
    { "&gt;", 4, '>' },
    { "&lt;", 4, '<' },
    { "&quot;", 6, '"' },
    { NULL, 0, '\0' }
};

static char* DecodeXml(char* str)
{
    char ch;
    char* dst;
    char* src;
    short i;
    Nlm_XmlTablePtr  xtp;

    if (StringHasNoText(str)) return str;

    src = str;
    dst = str;
    ch = *src;
    while (ch != '\0') {
        if (ch == '&') {
            xtp = NULL;
            for (i = 0; xmlcodes[i].code != NULL; i++) {
                if (StringNICmp(src, xmlcodes[i].code, xmlcodes[i].len) == 0) {
                    xtp = &(xmlcodes[i]);
                    break;
                }
            }
            if (xtp != NULL) {
                *dst = xtp->letter;
                dst++;
                src += xtp->len;
            }
            else {
                *dst = ch;
                dst++;
                src++;
            }
        }
        else {
            *dst = ch;
            dst++;
            src++;
        }
        ch = *src;
    }
    *dst = '\0';

    return str;
}

static char* TrimSpacesAroundString(char* str)
{
    unsigned char ch;
    char* dst;
    char* ptr;

    if (str != NULL && str[0] != '\0') {
        dst = str;
        ptr = str;
        ch = *ptr;
        while (ch != '\0' && ch <= ' ') {
            ptr++;
            ch = *ptr;
        }
        while (ch != '\0') {
            *dst = ch;
            dst++;
            ptr++;
            ch = *ptr;
        }
        *dst = '\0';
        dst = NULL;
        ptr = str;
        ch = *ptr;
        while (ch != '\0') {
            if (ch > ' ') {
                dst = NULL;
            }
            else if (dst == NULL) {
                dst = ptr;
            }
            ptr++;
            ch = *ptr;
        }
        if (dst != NULL) {
            *dst = '\0';
        }
    }
    return str;
}


static void TokenizeXmlLine(ValNodePtr* headp, ValNodePtr* tailp, char* str)
{
    char     *atr, *fst, *lst, *nxt, *ptr;
    char        ch, cha, chf, chl, quo;

    bool     doStart, doEnd;

    if (headp == NULL || tailp == NULL) return;
    if (StringHasNoText(str)) return;

    ptr = str;
    ch = *ptr;

    while (ch != '\0') {
        if (ch == ' ' || ch == '\r' || ch == '\n' || ch == '\t') {
            /* ignore whitespace between tags */
            ptr++;
            ch = *ptr;

        }
        else if (ch == '<') {

            /* process XML tag */
            /* skip past left angle bracket */
            ptr++;

            /* keep track of pointers to first character after < and last character before > in XML tag */
            fst = ptr;
            lst = ptr;
            ch = *ptr;
            while (ch != '\0' && ch != '>') {
                lst = ptr;
                ptr++;
                ch = *ptr;
            }
            if (ch != '\0') {
                *ptr = '\0';
                ptr++;
                ch = *ptr;
            }

            chf = *fst;
            chl = *lst;
            if (chf == '?' || chf == '!') {
                /* skip processing instructions */
            }
            else {
                /* initial default - if no slashes are present, just do start tag */
                doStart = true;
                doEnd = false;
                /* check for slash just after < or just before > symbol */
                if (chf == '/') {
                    /* slash after <, just do end tag */
                    fst++;
                    doStart = false;
                    doEnd = true;
                }
                else if (chl == '/') {
                    /* slash before > - self-closing tag - do start tag and end tag - content will be empty */
                    *lst = '\0';
                    doEnd = true;
                }

                /* skip past first space to look for attribute strings before closing > symbol */
                atr = fst;
                cha = *atr;
                while (cha != '\0' && cha != ' ') {
                    atr++;
                    cha = *atr;
                }
                if (cha != '\0') {
                    *atr = '\0';
                    atr++;
                    cha = *atr;
                }

                /* report start tag */
                if (doStart) {
                    TrimSpacesAroundString(fst);
                    ValNodeCopyStrEx(headp, tailp, XML_START_TAG, fst);
                }

                /* report individual attribute tag="value" clauses */
                while (cha != '\0') {
                    nxt = atr;
                    cha = *nxt;
                    /* skip to equal sign */
                    while (cha != '\0' && cha != '=') {
                        nxt++;
                        cha = *nxt;
                    }
                    if (cha != '\0') {
                        nxt++;
                        cha = *nxt;
                    }
                    quo = '\0';
                    if (cha == '"' || cha == '\'') {
                        quo = cha;
                        nxt++;
                        cha = *nxt;
                    }
                    while (cha != '\0' && cha != quo) {
                        nxt++;
                        cha = *nxt;
                    }
                    if (cha != '\0') {
                        nxt++;
                        cha = *nxt;
                    }
                    *nxt = '\0';
                    TrimSpacesAroundString(atr);
                    ValNodeCopyStrEx(headp, tailp, XML_ATTRIBUTE, atr);
                    *nxt = cha;
                    atr = nxt;
                }

                /* report end tag */
                if (doEnd) {
                    TrimSpacesAroundString(fst);
                    ValNodeCopyStrEx(headp, tailp, XML_END_TAG, fst);
                }
            }

        }
        else {

            /* process content between tags */
            fst = ptr;
            ptr++;
            ch = *ptr;
            while (ch != '\0' && ch != '<') {
                ptr++;
                ch = *ptr;
            }
            if (ch != '\0') {
                *ptr = '\0';
            }

            /* report content string */
            TrimSpacesAroundString(fst);
            DecodeXml(fst);
            ValNodeCopyStrEx(headp, tailp, XML_CONTENT, fst);
            /*
            if (ch != '\0') {
            *ptr = ch;
            }
            */
        }
    }
}

static ValNodePtr TokenizeXmlString(char* str)
{
    ValNodePtr  head = NULL, tail = NULL;

    if (StringHasNoText(str)) return NULL;

    TokenizeXmlLine(&head, &tail, str);

    return head;
}

/* second pass - process ValNode chain into hierarchical structure */

static Nlm_XmlObjPtr ProcessAttribute(char* str)
{
    Nlm_XmlObjPtr  attr = NULL;
    char    ch, chf, chl, quo;
    char    *eql, *lst;

    if (StringHasNoText(str)) return NULL;

    eql = str;
    ch = *eql;
    while (ch != '\0' && ch != '=') {
        eql++;
        ch = *eql;
    }
    if (ch == '\0') return NULL;

    *eql = '\0';
    eql++;
    ch = *eql;
    quo = ch;
    if (quo == '"' || quo == '\'') {
        eql++;
        ch = *eql;
    }
    chf = *eql;
    if (chf == '\0') return NULL;

    lst = eql;
    chl = *lst;
    while (chl != '\0' && chl != quo) {
        lst++;
        chl = *lst;
    }
    if (chl != '\0') {
        *lst = '\0';
    }

    if (StringHasNoText(str) || StringHasNoText(eql)) return NULL;

    attr = (Nlm_XmlObjPtr)MemNew(sizeof(Nlm_XmlObj));
    if (attr == NULL) return NULL;

    TrimSpacesAroundString(str);
    TrimSpacesAroundString(eql);
    DecodeXml(str);
    DecodeXml(eql);
    attr->name = StringSave(str);
    attr->contents = StringSave(eql);

    return attr;
}

static Nlm_XmlObjPtr ProcessStartTag(ValNodePtr* curr, Nlm_XmlObjPtr parent, const Char* name)
{
    Nlm_XmlObjPtr  attr, child, lastattr = NULL, lastchild = NULL, xop = NULL;
    unsigned char  choice;
    char*    str;
    ValNodePtr     vnp;

    if (curr == NULL) return NULL;

    xop = (Nlm_XmlObjPtr)MemNew(sizeof(Nlm_XmlObj));
    if (xop == NULL) return NULL;

    xop->name = StringSave(name);
    xop->parent = parent;

    while (*curr != NULL) {

        vnp = *curr;
        str = (char*)vnp->data.ptrvalue;
        choice = vnp->choice;

        /* advance to next token */
        *curr = vnp->next;

        TrimSpacesAroundString(str);

        if (StringHasNoText(str)) {
            /* skip */
        }
        else if (choice == XML_START_TAG) {

            /* recursive call to process next level */
            child = ProcessStartTag(curr, xop, str);
            /* link into children list */
            if (child != NULL) {
                if (xop->children == NULL) {
                    xop->children = child;
                }
                if (lastchild != NULL) {
                    lastchild->next = child;
                }
                lastchild = child;
            }

        }
        else if (choice == XML_END_TAG) {

            /* pop out of recursive call */
            return xop;

        }
        else if (choice == XML_ATTRIBUTE) {

            /* get attributes within tag */
            attr = ProcessAttribute(str);
            if (attr != NULL) {
                if (xop->attributes == NULL) {
                    xop->attributes = attr;
                }
                if (lastattr != NULL) {
                    lastattr->next = attr;
                }
                lastattr = attr;
            }

        }
        else if (choice == XML_CONTENT) {

            /* get contact between start and end tags */
            xop->contents = StringSave(str);
        }
    }

    return xop;
}

static Nlm_XmlObjPtr SetSuccessors(Nlm_XmlObjPtr xop, Nlm_XmlObjPtr prev, short level)
{
    Nlm_XmlObjPtr  tmp;

    if (xop == NULL) return NULL;
    xop->level = level;

    if (prev != NULL) {
        prev->successor = xop;
    }

    prev = xop;
    for (tmp = xop->children; tmp != NULL; tmp = tmp->next) {
        prev = SetSuccessors(tmp, prev, level + 1);
    }

    return prev;
}

static Nlm_XmlObjPtr ParseXmlTokens(ValNodePtr head)
{
    ValNodePtr     curr;
    Nlm_XmlObjPtr  xop;

    if (head == NULL) return NULL;

    curr = head;

    xop = ProcessStartTag(&curr, NULL, "root");
    if (xop == NULL) return NULL;

    SetSuccessors(xop, NULL, 1);

    return xop;
}

Nlm_XmlObjPtr FreeXmlObject(Nlm_XmlObjPtr xop)
{
    Nlm_XmlObjPtr  curr, next;

    if (xop == NULL) return NULL;

    MemFree(xop->name);
    MemFree(xop->contents);

    curr = xop->attributes;
    while (curr != NULL) {
        next = curr->next;
        curr->next = NULL;
        FreeXmlObject(curr);
        curr = next;
    }

    curr = xop->children;
    while (curr != NULL) {
        next = curr->next;
        curr->next = NULL;
        FreeXmlObject(curr);
        curr = next;
    }

    MemFree(xop);

    return NULL;
}

Nlm_XmlObjPtr ParseXmlString(const Char* str)
{
    ValNodePtr     head;
    Nlm_XmlObjPtr  root, xop;
    char*    tmp;

    if (StringHasNoText(str)) return NULL;
    tmp = StringSave(str);
    if (tmp == NULL) return NULL;

    head = TokenizeXmlString(tmp);
    MemFree(tmp);

    if (head == NULL) return NULL;

    root = ParseXmlTokens(head);
    ValNodeFreeData(head);

    if (root == NULL) return NULL;
    xop = root->children;
    root->children = NULL;
    FreeXmlObject(root);

    return xop;
}

static int VisitXmlNodeProc(
    Nlm_XmlObjPtr xop,
    Nlm_XmlObjPtr parent,
    short level,
    void* userdata,
    VisitXmlNodeFunc callback,
    char* nodeFilter,
    char* parentFilter,
    char* attrTagFilter,
    char* attrValFilter,
    short maxDepth
    )

{
    Nlm_XmlObjPtr  attr, tmp;
    int       index = 0;

    bool okay;

    if (xop == NULL) return index;

    /* check depth limit */
    if (level > maxDepth) return index;

    okay = true;

    /* check attribute filters */
    if (StringDoesHaveText(attrTagFilter)) {
        okay = false;
        for (attr = xop->attributes; attr != NULL; attr = attr->next) {
            if (StringICmp(attr->name, attrTagFilter) == 0) {
                if (StringHasNoText(attrValFilter) || StringICmp(attr->contents, attrValFilter) == 0) {
                    okay = true;
                    break;
                }
            }
        }
    }
    else if (StringDoesHaveText(attrValFilter)) {
        okay = false;
        for (attr = xop->attributes; attr != NULL; attr = attr->next) {
            if (StringICmp(attr->contents, attrValFilter) == 0) {
                okay = true;
                break;
            }
        }
    }

    /* check node name filter */
    if (StringDoesHaveText(nodeFilter)) {
        if (StringICmp(xop->name, nodeFilter) != 0) {
            okay = false;
        }
    }

    /* check parent name filter */
    if (StringDoesHaveText(parentFilter)) {
        if (parent != NULL && StringICmp(parent->name, parentFilter) != 0) {
            okay = false;
        }
    }

    if (okay) {
        /* call callback for this node if all filter tests pass */
        if (callback != NULL) {
            callback(xop, parent, level, userdata);
        }
        index++;
    }

    /* visit children */
    for (tmp = xop->children; tmp != NULL; tmp = tmp->next) {
        index += VisitXmlNodeProc(tmp, xop, level + 1, userdata, callback, nodeFilter,
                                  parentFilter, attrTagFilter, attrValFilter, maxDepth);
    }

    return index;
}

int VisitXmlNodes(Nlm_XmlObjPtr xop, void* userdata, VisitXmlNodeFunc callback, char* nodeFilter,
                       char* parentFilter, char* attrTagFilter, char* attrValFilter, short maxDepth)
{
    int  index = 0;

    if (xop == NULL) return index;

    if (maxDepth == 0) {
        maxDepth = INT2_MAX;
    }

    index += VisitXmlNodeProc(xop, NULL, 1, userdata, callback, nodeFilter,
                              parentFilter, attrTagFilter, attrValFilter, maxDepth);

    return index;
}

END_NCBI_SCOPE
