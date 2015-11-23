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
 * Author:  Chris Lanczycki
 *
 * File Description:
 *
 *       Various utility functions for working with CCdd_book_ref objects
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>
#include <serial/enumvalues.hpp>
#include <util/xregexp/regexp.hpp>
#include <util/ncbi_url.hpp>
#include <objects/cdd/Cdd_descr.hpp>
#include <algo/structure/cd_utils/cuBookRefUtils.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils)


bool IsPortalDerivedBookRef(const CCdd_book_ref& bookRef)
{
    //  'bookname' is a mandatory CCdd_book_ref element.
    return (bookRef.GetBookname().substr(0, 3) == "NBK");
}

string CCddBookRefToString(const CCdd_book_ref& bookRef)
{
    if (IsPortalDerivedBookRef(bookRef)) {
        return CCddBookRefToPortalString(bookRef);
    } else {
        return CCddBookRefToBrString(bookRef);
    }
}


//  Adapted from CDTree.
//  Notes on doing esearch queries of the bookshelf:
//  1)  when have subelement, term is something like this:
//          eurekah/A17112/sec/A17113/pmc[rid]
//  2)  when there is no m_parsedSubelementId (or it is equal to m_parsedElement),
//  a slightly different format is used:
//          eurekah/chapter/A17112/pmc
//  3)  the esearch.fcgi 'term' allows 'OR' constructs for when it's not clear 
//  what the esearchType should be.   E.g., with '%20' being a URL-encoded space,
//          cmed6/sec/A41564/pmc[rid]%20OR%20cmed6/chapter/A41564/pmc[rid]%20or%20cmed6/part/A41564/pmc[rid]
string BrFcgiBookTermToEutilsTerm(const string& brfcgiBookTerm, bool forceOR)
{
    static const string section("section");
    static const string slash("/"), suffix("/pmc[rid]"), orString("%20OR%20"), dummyElement("DUMMY_ELEMENT");

    bool distinctSubelement;
    string term = kEmptyStr;
    string baseTerm;
    string bookname, elementType, elementId, subelementId;
    bookname = elementType = elementId = subelementId = kEmptyStr;

    //  Assumed format for a 'br.fcgi' derived book reference:
    //  figure/table
    //  bookname&part=part&rendertype=rendertype&id=id;
    //
    //  everything else is treated as a 'section'
    //  bookname&part=part[#id]

    CRegexp regexpSection("(.*)&part=(.*)$");
    CRegexp regexpSectionWithSubelement("(.*)&part=(.*)#(.*)");
    CRegexp regexpRendertype("(.*)&part=(.*)&rendertype=(.*)&id=(.*)");

    if (regexpRendertype.GetMatch(brfcgiBookTerm).length() > 0) {
        bookname = regexpRendertype.GetSub(brfcgiBookTerm, 1);
        elementType = regexpRendertype.GetSub(brfcgiBookTerm, 3);  // element type
        elementId = regexpRendertype.GetSub(brfcgiBookTerm, 2);  // element id
        subelementId = regexpRendertype.GetSub(brfcgiBookTerm, 4);
    } else if (regexpSectionWithSubelement.GetMatch(brfcgiBookTerm).length() > 0) {
        bookname = regexpSectionWithSubelement.GetSub(brfcgiBookTerm, 1);
        elementType = section;
        elementId = regexpSectionWithSubelement.GetSub(brfcgiBookTerm, 2);  // element id
        subelementId = regexpSectionWithSubelement.GetSub(brfcgiBookTerm, 3);
    } else if (regexpSection.GetMatch(brfcgiBookTerm).length() > 0) {
        bookname = regexpSection.GetSub(brfcgiBookTerm, 1);
        elementType = section;
        elementId = regexpSection.GetSub(brfcgiBookTerm, 2);  // element id
    }

    distinctSubelement = (subelementId.length() > 0 && subelementId != elementId);

    if (elementType == "glossary") {
        elementType = "def-item";
    } else if (elementType.substr(0, 3) == "app") {
        elementType = "appendix";
    } else if (elementType.substr(0, 3) == "box") {
        elementType = "box";
    } else if (elementType.substr(0, 3) == "fig") {
        elementType = "figgrp";
    } else if (elementType.substr(0, 3) == "sec" || elementType == "unassigned") {
        elementType = "sec";
    } else if (elementType.substr(0, 5) == "table") {
        elementType = "table";
    } 


    if (elementType.length() > 0) {

        if (distinctSubelement) {
            baseTerm = bookname + slash + elementId + slash + dummyElement + slash + subelementId + suffix;
        } else {
            baseTerm = bookname + slash + dummyElement + slash + elementId + suffix;
        }

        //  need to use the 'OR' syntax since we don't know what this is; 
        //  try 'sec', 'chapter', 'part', 'figgrp', 'table'
        if (forceOR || elementType == "book-part") {
            term = NStr::Replace(baseTerm, dummyElement, "sec") + orString;
            term += NStr::Replace(baseTerm, dummyElement, "chapter") + orString;
            term += NStr::Replace(baseTerm, dummyElement, "figgrp") + orString;
            term += NStr::Replace(baseTerm, dummyElement, "table") + orString;
            term += NStr::Replace(baseTerm, dummyElement, "part");
        } else {
            term = NStr::Replace(baseTerm, dummyElement, elementType);
        }

    }

    return term;
}

string CCddBookRefToEsearchTerm(const CCdd_book_ref& bookRef)
{
    string term;
    if (IsPortalDerivedBookRef(bookRef)) {
        string portalBookTerm = CCddBookRefToPortalString(bookRef);
        term = NStr::Replace(portalBookTerm, "/#", "/");
    } else {
        // transform brBookTerm (as per CDTree's BrFcgiBookTermToXMLViaEutils)
        string brBookTerm = CCddBookRefToBrString(bookRef);
        term = BrFcgiBookTermToEutilsTerm(brBookTerm, false);
    }
    return term;
}

string CCddBookRefToBvString(const CCdd_book_ref& bookRef)
{
    string result;
    string elementid, subelementid, typeString;
    string bookname = bookRef.GetBookname();

    if (!bookRef.IsSetElementid() && !bookRef.IsSetCelementid())
        result = "unexpected book_ref format:  neither elementid nor celementid is set";

    else if (bookRef.IsSetElementid() && bookRef.IsSetCelementid())
        result = "unexpected book_ref format:  both elementid and celementid are set";

    else if (bookRef.IsSetSubelementid() && bookRef.IsSetCsubelementid())
        result = "unexpected book_ref format:  both subelementid and csubelementid are set";

    else {
        elementid = bookRef.IsSetElementid() ? NStr::IntToString(bookRef.GetElementid()) : bookRef.GetCelementid();
        subelementid = (bookRef.IsSetSubelementid() || bookRef.IsSetCsubelementid()) ?
                (bookRef.IsSetSubelementid() ? NStr::IntToString(bookRef.GetSubelementid()) : bookRef.GetCsubelementid()) : kEmptyStr;

        //  trim any whitespace on elements/sub-elements
        NStr::TruncateSpacesInPlace(elementid);
        NStr::TruncateSpacesInPlace(subelementid);

        //  note:  I don't have ownership of the CEnumeratedTypeValues pointer
        const CEnumeratedTypeValues* allowedElements = CCdd_book_ref::GetTypeInfo_enum_ETextelement();
        typeString = (allowedElements) ? allowedElements->FindName(bookRef.GetTextelement(), true) : "unassigned";
    
        char buf[2048];

        //  Make sure don't have overflow; 15 > length of the longest string in elementStringMap + 1.
        _ASSERT(bookname.length() + elementid.length() + subelementid.length() + 15 < 2048);

        if (subelementid.size() > 0)
            sprintf(buf, "%s.%s.%s#%s", bookname.c_str(), typeString.c_str(), elementid.c_str(), subelementid.c_str());
        else
            sprintf(buf, "%s.%s.%s", bookname.c_str(), typeString.c_str(), elementid.c_str());
        result = string(buf);
    }
    
    return result;
}


string CCddBookRefToBrString(const CCdd_book_ref& bookRef)
{
    string result;
    string elementid, subelementid, renderType;
    string bookname = bookRef.GetBookname();

    if (!bookRef.IsSetElementid() && !bookRef.IsSetCelementid())
        result = "unexpected book_ref format:  neither elementid nor celementid is set";

    else if (bookRef.IsSetElementid() && bookRef.IsSetCelementid())
        result = "unexpected book_ref format:  both elementid and celementid are set";

    else if (bookRef.IsSetSubelementid() && bookRef.IsSetCsubelementid())
        result = "unexpected book_ref format:  both subelementid and csubelementid are set";

    else {

        CCdd_book_ref::ETextelement elementType;
        string part, id;
        
        //  Numerical element ids need to be prefixed by 'A' in br.fcgi URLs.
        if (bookRef.IsSetElementid()) {
            part = "A" + NStr::IntToString(bookRef.GetElementid());
        } else {
            part = bookRef.GetCelementid();
        }

        if (bookRef.IsSetSubelementid() || bookRef.IsSetCsubelementid()) {
            //  Numerical subelement ids need to be prefixed by 'A' in br.fcgi URLs.
            if (bookRef.IsSetSubelementid()) {
                id = "A" + NStr::IntToString(bookRef.GetSubelementid());
            } else {
                id = bookRef.GetCsubelementid();
            }
        } else {
            id = kEmptyStr;
        }

        //  For br.fcgi, 'chapter' and 'section' are treated equivalently.
        //  Expect anything else to be encoded in the 'rendertype' attribute.
        elementType = bookRef.GetTextelement();

        if (elementType != CCdd_book_ref::eTextelement_chapter && elementType != CCdd_book_ref::eTextelement_section) {
            if (elementType == CCdd_book_ref::eTextelement_unassigned || elementType == CCdd_book_ref::eTextelement_other) {
                result = bookname + "&part=" + part;
            } else {

                //  note:  I don't have ownership of the CEnumeratedTypeValues pointer
                const CEnumeratedTypeValues* allowedElements = CCdd_book_ref::GetTypeInfo_enum_ETextelement();
                renderType = kEmptyStr;

                //  For 'eTextelement_figgrp', use 'figure'.
                if (elementType == CCdd_book_ref::eTextelement_figgrp) {
                    renderType = "figure";
                //  For 'eTextelement_glossary', use 'def-item'.
                } else if (elementType == CCdd_book_ref::eTextelement_glossary) {
                    renderType = "def-item";
                } else if (allowedElements) {
                    renderType = allowedElements->FindName(elementType, true);
                }
                if (renderType.length() == 0) {
                    renderType = "unassigned";
                }
            
                if (id.length() == 0) {
                    id = part;
                }
                result = bookname + "&part=" + part + "&rendertype=" + renderType + "&id=" + id;
            }
        } else {
            result = bookname + "&part=" + part;
            if (id.size() > 0)
                result += "#" + id;
        }

    }

    return result;
}

//  The return value is a string used in Portal style URLs based on 
//  .../books/NBK....  The only fields that should be present in 
//  book ref objects are the bookname, text-element type, and a
//  celementid.  Any other field in the spec is ignored.  
string CCddBookRefToPortalString(const CCdd_book_ref& bookRef)
{    
    string result = bookRef.GetBookname();
    string idStr, renderType;

    CCdd_book_ref::ETextelement elementType = bookRef.GetTextelement();
    const CEnumeratedTypeValues* allowedElements = CCdd_book_ref::GetTypeInfo_enum_ETextelement();

    //  'idStr' should exist for everything except chapters.  However, 
    //  if we find a chapter with non-empty 'idStr' format it as a 
    //  section.  Conversely, format sections with no 'idStr' as a chapter.
    if (bookRef.IsSetCelementid()) {
        idStr = bookRef.GetCelementid();
    }

    if (elementType != CCdd_book_ref::eTextelement_chapter && elementType != CCdd_book_ref::eTextelement_section) {

        //  For 'eTextelement_figgrp', Portal URL requires 'figure'.
        //  For 'eTextelement_glossary', Portal URL requires 'def-item'.
        renderType = kEmptyStr;
        if (elementType == CCdd_book_ref::eTextelement_figgrp)
            renderType = "figure";
        else if (elementType == CCdd_book_ref::eTextelement_glossary)
            renderType = "def-item";
        else if (allowedElements && elementType != CCdd_book_ref::eTextelement_unassigned)
            renderType = allowedElements->FindName(elementType, true);

        if (renderType.length() > 0) 
            result += "/" + renderType + "/" + idStr;
        else if (idStr.length() > 0)
            result += "/#" + idStr;

    } else if (idStr.length() > 0) {
        result += "/#" + idStr;
    }

    return result;
}

bool BrBookURLToCCddBookRef(const string& brBookUrl, CRef< CCdd_book_ref>& bookRef)
{
    bool result = false;
    string bookname, address, subaddress, typeStr, firstTokenStr;
    list<string> sharpTokens;
    CRegexp regexpCommon("book=(.*)&part=(.*)");
    CRegexp regexpRendertype("&part=(.*)&rendertype=(.*)&id=(.*)");

    //  note:  I don't have ownership of the CEnumeratedTypeValues pointer
    const CEnumeratedTypeValues* allowedElements = CCdd_book_ref::GetTypeInfo_enum_ETextelement();
    
    NStr::Split(brBookUrl, "#", sharpTokens, 0);
    if (sharpTokens.size() == 1 || sharpTokens.size() == 2) {
        bool haveSubaddr = (sharpTokens.size() == 2);
        
        //  All URLs have 'book' and 'part' parameters.
        firstTokenStr = sharpTokens.front();
        regexpCommon.GetMatch(firstTokenStr, 0, 0, CRegexp::fMatch_default, true);
        if (regexpCommon.NumFound() == 3) {  //  i.e., found full pattern + two subpatterns

            bookname = regexpCommon.GetSub(firstTokenStr, 1);

            regexpRendertype.GetMatch(firstTokenStr, 0, 0, CRegexp::fMatch_default, true);
            if (regexpRendertype.NumFound() == 4) {  //  i.e., expecting anything but a chapter or section 
                address = regexpRendertype.GetSub(firstTokenStr, 1);
                typeStr = regexpRendertype.GetSub(firstTokenStr, 2);

                //  'figure' maps to 'eTextelement_figgrp'
                //  'def-item' maps to 'eTextelement_glossary'
                if (typeStr == "figure")
                    typeStr = "figgrp";
                else if (typeStr == "def-item")
                    typeStr = "glossary";

                if (allowedElements && !allowedElements->IsValidName(typeStr)) { 
                    typeStr = kEmptyStr;  //  problem if we don't have a known type
                } else {
                    subaddress = regexpRendertype.GetSub(firstTokenStr, 3);
                }
            } else {  //  treat this as a 'section'
                address = regexpCommon.GetSub(firstTokenStr, 2);
                typeStr = (allowedElements) ? allowedElements->FindName(CCdd_book_ref::eTextelement_section, true) : "section";

                //  If there's something after the '#', if it's an old-style
                //  URL it could be numeric -> prepend 'A' in that case.
                if (haveSubaddr) {
                    subaddress = sharpTokens.back();
                    if (NStr::StringToULong(subaddress, NStr::fConvErr_NoThrow) != 0) {
                        subaddress = "A" + subaddress;
                    }
                }
            }
        }

        if (typeStr.length() > 0) {            
            try {
                //  Throws an error is 'typeStr' is not found.
                CCdd_book_ref::ETextelement typeEnum = (CCdd_book_ref::ETextelement) allowedElements->FindValue(typeStr);
                bookRef->SetBookname(bookname);
                bookRef->SetTextelement(typeEnum);
                bookRef->SetCelementid(address);
                if (subaddress.length() > 0) {
                    bookRef->SetCsubelementid(subaddress);
                }
                result = true;
            } catch (...) {
                result = false;
            }
        }
    }

    if (!result) {
        bookRef.Reset();
    }

    return result;
}

bool PortalBookURLToCCddBookRef(const string& portalBookUrl, CRef < CCdd_book_ref >& bookRef)
{
    bool result = false;
    if (bookRef.Empty()) return result;

    //  remove leading/trailing whitespace from input url
    string inputStr = NStr::TruncateSpaces(portalBookUrl);

    string baseStr, nbkCode, idStr;
    string typeStr = kEmptyStr;
    CRegexp regexpBase("/books/(NBK.+)");
    CRegexp regexpNBK("^(NBK[^/]+)");
    CRegexp regexpRendertype("^NBK[^/]+/(.+)/(.+)");

    CUrl url(inputStr);
    CUrlArgs& urlArgs = url.GetArgs();
    string urlPath = url.GetPath();
    string urlFrag = url.GetFragment();  //  fragment is text after trailing '#'

    //  remove a trailing '/' in the path (e.g., '.../?report=objectonly' form of URLs)
    if (NStr::EndsWith(urlPath, '/')) { 
        urlPath = urlPath.substr(0, urlPath.length() - 1);
    }

    regexpBase.GetMatch(urlPath, 0, 0, CRegexp::fMatch_default, true);
    if (regexpBase.NumFound() == 2) {  //  i.e., found full pattern + subpattern

        baseStr = regexpBase.GetSub(urlPath, 1);
        nbkCode = regexpNBK.GetMatch(baseStr);

        regexpRendertype.GetMatch(baseStr, 0, 0, CRegexp::fMatch_default, true);        
        if (regexpRendertype.NumFound() == 3) {  //  i.e., full pattern + two subpatterns
            typeStr = regexpRendertype.GetSub(baseStr, 1);
            idStr = regexpRendertype.GetSub(baseStr, 2);
        } else if (urlArgs.IsSetValue("rendertype") && urlArgs.IsSetValue("id")) {
            //  If the user somehow pasted a redirected br.fcgi URL.
            typeStr = urlArgs.GetValue("rendertype");
            idStr = urlArgs.GetValue("id");
        } else if (urlFrag.length() > 0) {
            //  A section id appears after the hash character.
            typeStr = "section";
            idStr = urlFrag;
        } else if (urlFrag.length() == 0) {
            //  If there is no URL fragment or obvious type, designate it 
            //  a chapter and point to the top of this book page.
            typeStr = "chapter";
            idStr = kEmptyStr;
        }
    }

    if (nbkCode.length() > 0) {

        //  'figure' maps to 'eTextelement_figgrp'
        //  'def-item' maps to 'eTextelement_glossary'
        //  if have no type at this point, treat as either a section or chapter
        if (typeStr == "figure")
            typeStr = "figgrp";
        else if (typeStr == "def-item")
            typeStr = "glossary";
//        else if (typeStr.length() == 0) 
//            typeStr = (idStr.length() > 0) ? "section" : "chapter";

        CCdd_book_ref::ETextelement typeEnum;
        const CEnumeratedTypeValues* allowedElements = CCdd_book_ref::GetTypeInfo_enum_ETextelement();
        if (allowedElements && allowedElements->IsValidName(typeStr)) { 
            //  Since 'IsValidName' is true, don't need to catch thrown 
            //  error in the case 'typeStr' is not found.
            typeEnum = (CCdd_book_ref::ETextelement) allowedElements->FindValue(typeStr);
        } else {  
            //  treat invalid typeStr as a 'section' or 'chapter'
            typeEnum  = (idStr.length() > 0) ? CCdd_book_ref::eTextelement_section : CCdd_book_ref::eTextelement_chapter;
        }

        bookRef->SetBookname(nbkCode);
        bookRef->SetTextelement(typeEnum);
        bookRef->SetCelementid(idStr);
        result = true;
    }

    return result;
}



END_SCOPE(cd_utils)
END_NCBI_SCOPE
