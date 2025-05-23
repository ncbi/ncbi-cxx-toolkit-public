/* $Id$
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
 * File Name: xmlmisc.cpp
 *
 * Author: Alexey Dobronadezhdin
 *
 * File Description:
 *      XML functionality from C-toolkit.
 */

#include <ncbi_pch.hpp>

#include "ftacpp.hpp"
#include "ftaerr.hpp"
#include "xmlmisc.h"

#ifdef THIS_FILE
#  undef THIS_FILE
#endif
#define THIS_FILE "xmlmisc.cpp"

#define XML_START_TAG 1
#define XML_END_TAG   2
#define XML_ATTRIBUTE 3
#define XML_CONTENT   4

/* function to decode ampersand-protected symbols */

BEGIN_NCBI_SCOPE

struct XmlTable {
    const Char* code;
    size_t      len;
    char        letter;
};

static const XmlTable xmlcodes[] = {
    { "&amp;", 5, '&' },
    { "&apos;", 6, '\'' },
    { "&gt;", 4, '>' },
    { "&lt;", 4, '<' },
    { "&quot;", 6, '"' },
    { nullptr, 0, '\0' }
};

static char* DecodeXml(char* str)
{
    char  ch;
    char* dst;
    char* src;
    short i;

    if (StringHasNoText(str))
        return str;

    src = str;
    dst = str;
    ch  = *src;
    while (ch != '\0') {
        if (ch == '&') {
            const XmlTable* xtp = nullptr;
            for (i = 0; xmlcodes[i].code; i++) {
                if (StringEquNI(src, xmlcodes[i].code, xmlcodes[i].len)) {
                    xtp = &(xmlcodes[i]);
                    break;
                }
            }
            if (xtp) {
                *dst = xtp->letter;
                dst++;
                src += xtp->len;
            } else {
                *dst = ch;
                dst++;
                src++;
            }
        } else {
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
    char*         dst;
    char*         ptr;

    if (str && str[0] != '\0') {
        dst = str;
        ptr = str;
        ch  = *ptr;
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
        dst  = nullptr;
        ptr  = str;
        ch   = *ptr;
        while (ch != '\0') {
            if (ch > ' ') {
                dst = nullptr;
            } else if (! dst) {
                dst = ptr;
            }
            ptr++;
            ch = *ptr;
        }
        if (dst) {
            *dst = '\0';
        }
    }
    return str;
}


static void TokenizeXmlLine(ValNodeList& L, ValNodePtr* tailp, char* str)
{
    char *atr, *fst, *lst, *nxt, *ptr;
    char  ch, cha, chf, chl, quo;

    bool doStart, doEnd;

    if (! L.head || ! tailp)
        return;
    if (StringHasNoText(str))
        return;

    ptr = str;
    ch  = *ptr;

    while (ch != '\0') {
        if (ch == ' ' || ch == '\r' || ch == '\n' || ch == '\t') {
            /* ignore whitespace between tags */
            ptr++;
            ch = *ptr;

        } else if (ch == '<') {

            /* process XML tag */
            /* skip past left angle bracket */
            ptr++;

            /* keep track of pointers to first character after < and last character before > in XML tag */
            fst = ptr;
            lst = ptr;
            ch  = *ptr;
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
            } else {
                /* initial default - if no slashes are present, just do start tag */
                doStart = true;
                doEnd   = false;
                /* check for slash just after < or just before > symbol */
                if (chf == '/') {
                    /* slash after <, just do end tag */
                    fst++;
                    doStart = false;
                    doEnd   = true;
                } else if (chl == '/') {
                    /* slash before > - self-closing tag - do start tag and end tag - content will be empty */
                    *lst  = '\0';
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
                    ValNodeCopyStrEx(L, tailp, XML_START_TAG, fst);
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
                    ValNodeCopyStrEx(L, tailp, XML_ATTRIBUTE, atr);
                    *nxt = cha;
                    atr  = nxt;
                }

                /* report end tag */
                if (doEnd) {
                    TrimSpacesAroundString(fst);
                    ValNodeCopyStrEx(L, tailp, XML_END_TAG, fst);
                }
            }

        } else {

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
            ValNodeCopyStrEx(L, tailp, XML_CONTENT, fst);
            /*
            if (ch != '\0') {
            *ptr = ch;
            }
            */
        }
    }
}

static ValNodeList TokenizeXmlString(char* str)
{
    ValNodeList L;
    ValNodePtr tail = nullptr;

    if (StringHasNoText(str))
        return {};

    TokenizeXmlLine(L, &tail, str);

    return L;
}

/* second pass - process ValNode chain into hierarchical structure */

static XmlObjPtr ProcessAttribute(char* str)
{
    XmlObjPtr attr = nullptr;
    char      ch, chf, chl, quo;
    char *    eql, *lst;

    if (StringHasNoText(str))
        return nullptr;

    eql = str;
    ch  = *eql;
    while (ch != '\0' && ch != '=') {
        eql++;
        ch = *eql;
    }
    if (ch == '\0')
        return nullptr;

    *eql = '\0';
    eql++;
    ch  = *eql;
    quo = ch;
    if (quo == '"' || quo == '\'') {
        eql++;
        ch = *eql;
    }
    chf = ch;
    if (chf == '\0')
        return nullptr;

    lst = eql;
    chl = *lst;
    while (chl != '\0' && chl != quo) {
        lst++;
        chl = *lst;
    }
    if (chl != '\0') {
        *lst = '\0';
    }

    if (StringHasNoText(str) || StringHasNoText(eql))
        return nullptr;

    attr = new XmlObj;
    if (! attr)
        return nullptr;

    TrimSpacesAroundString(str);
    TrimSpacesAroundString(eql);
    DecodeXml(str);
    DecodeXml(eql);
    attr->name     = StringSave(str);
    attr->contents = StringSave(eql);

    return attr;
}

static XmlObjPtr ProcessStartTag(ValNodePtr* curr, XmlObjPtr parent, const Char* name)
{
    XmlObjPtr     attr, child, lastattr = nullptr, lastchild = nullptr, xop = nullptr;
    unsigned char choice;
    char*         str;
    ValNodePtr    vnp;

    if (! curr)
        return nullptr;

    xop = new XmlObj;
    if (! xop)
        return nullptr;

    xop->name   = StringSave(name);
    xop->parent = parent;

    while (*curr) {

        vnp    = *curr;
        str    = vnp->data;
        choice = vnp->choice;

        /* advance to next token */
        *curr = vnp->next;

        TrimSpacesAroundString(str);

        if (StringHasNoText(str)) {
            /* skip */
        } else if (choice == XML_START_TAG) {

            /* recursive call to process next level */
            child = ProcessStartTag(curr, xop, str);
            /* link into children list */
            if (child) {
                if (! xop->children) {
                    xop->children = child;
                }
                if (lastchild) {
                    lastchild->next = child;
                }
                lastchild = child;
            }

        } else if (choice == XML_END_TAG) {

            /* pop out of recursive call */
            return xop;

        } else if (choice == XML_ATTRIBUTE) {

            /* get attributes within tag */
            attr = ProcessAttribute(str);
            if (attr) {
                if (! xop->attributes) {
                    xop->attributes = attr;
                }
                if (lastattr) {
                    lastattr->next = attr;
                }
                lastattr = attr;
            }

        } else if (choice == XML_CONTENT) {

            /* get contact between start and end tags */
            xop->contents = StringSave(str);
        }
    }

    return xop;
}

static XmlObjPtr SetSuccessors(XmlObjPtr xop, XmlObjPtr prev, short level)
{
    XmlObjPtr tmp;

    if (! xop)
        return nullptr;
    xop->level = level;

    if (prev) {
        prev->successor = xop;
    }

    prev = xop;
    for (tmp = xop->children; tmp; tmp = tmp->next) {
        prev = SetSuccessors(tmp, prev, level + 1);
    }

    return prev;
}

static XmlObjPtr ParseXmlTokens(ValNodePtr head)
{
    ValNodePtr curr;
    XmlObjPtr  xop;

    if (! head)
        return nullptr;

    curr = head;

    xop = ProcessStartTag(&curr, nullptr, "root");
    if (! xop)
        return nullptr;

    SetSuccessors(xop, nullptr, 1);

    return xop;
}

XmlObjPtr FreeXmlObject(XmlObjPtr xop)
{
    XmlObjPtr curr, next;

    if (! xop)
        return nullptr;

    MemFree(xop->name);
    MemFree(xop->contents);

    curr = xop->attributes;
    while (curr) {
        next       = curr->next;
        curr->next = nullptr;
        FreeXmlObject(curr);
        curr = next;
    }

    curr = xop->children;
    while (curr) {
        next       = curr->next;
        curr->next = nullptr;
        FreeXmlObject(curr);
        curr = next;
    }

    delete xop;

    return nullptr;
}

XmlObjPtr ParseXmlString(const Char* str)
{
    ValNodeList L;
    XmlObjPtr  root, xop;
    char*      tmp;

    if (StringHasNoText(str))
        return nullptr;
    tmp = StringSave(str);
    if (! tmp)
        return nullptr;

    L = TokenizeXmlString(tmp);
    MemFree(tmp);

    if (! L.head)
        return nullptr;

    root = ParseXmlTokens(L.head);
    ValNodeFreeData(L);

    if (! root)
        return nullptr;
    xop            = root->children;
    root->children = nullptr;
    FreeXmlObject(root);

    return xop;
}

static int VisitXmlNodeProc(
    XmlObjPtr        xop,
    XmlObjPtr        parent,
    short            level,
    void*            userdata,
    VisitXmlNodeFunc callback,
    char*            nodeFilter,
    char*            parentFilter,
    char*            attrTagFilter,
    char*            attrValFilter,
    short            maxDepth)
{
    XmlObjPtr attr, tmp;
    int       index = 0;

    bool okay;

    if (! xop)
        return index;

    /* check depth limit */
    if (level > maxDepth)
        return index;

    okay = true;

    /* check attribute filters */
    if (StringDoesHaveText(attrTagFilter)) {
        okay = false;
        for (attr = xop->attributes; attr; attr = attr->next) {
            if (NStr::EqualNocase(attr->name, attrTagFilter)) {
                if (StringHasNoText(attrValFilter) || NStr::EqualNocase(attr->contents, attrValFilter)) {
                    okay = true;
                    break;
                }
            }
        }
    } else if (StringDoesHaveText(attrValFilter)) {
        okay = false;
        for (attr = xop->attributes; attr; attr = attr->next) {
            if (NStr::EqualNocase(attr->contents, attrValFilter)) {
                okay = true;
                break;
            }
        }
    }

    /* check node name filter */
    if (StringDoesHaveText(nodeFilter)) {
        if (! NStr::EqualNocase(xop->name, nodeFilter)) {
            okay = false;
        }
    }

    /* check parent name filter */
    if (StringDoesHaveText(parentFilter)) {
        if (parent && ! NStr::EqualNocase(parent->name, parentFilter)) {
            okay = false;
        }
    }

    if (okay) {
        /* call callback for this node if all filter tests pass */
        if (callback) {
            callback(xop, parent, level, userdata);
        }
        index++;
    }

    /* visit children */
    for (tmp = xop->children; tmp; tmp = tmp->next) {
        index += VisitXmlNodeProc(tmp, xop, level + 1, userdata, callback, nodeFilter, parentFilter, attrTagFilter, attrValFilter, maxDepth);
    }

    return index;
}

int VisitXmlNodes(XmlObjPtr xop, void* userdata, VisitXmlNodeFunc callback, char* nodeFilter, char* parentFilter, char* attrTagFilter, char* attrValFilter, short maxDepth)
{
    int index = 0;

    if (! xop)
        return index;

    if (maxDepth == 0) {
        maxDepth = numeric_limits<short>::max();
    }

    index += VisitXmlNodeProc(xop, nullptr, 1, userdata, callback, nodeFilter, parentFilter, attrTagFilter, attrValFilter, maxDepth);

    return index;
}

END_NCBI_SCOPE
