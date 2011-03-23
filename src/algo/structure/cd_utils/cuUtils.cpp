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
 *       Various utility functions for CDTree
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>
#include <util/xregexp/regexp.hpp>
#include <util/ncbi_url.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/PDB_seq_id.hpp>
#include <objects/seqloc/PDB_mol_id.hpp>
#include <objects/cdd/Cdd.hpp>
#include <objects/cdd/Cdd_id.hpp>
#include <objects/cdd/Cdd_id_set.hpp>
#include <objects/cdd/Global_id.hpp>
#include <objects/seqloc/Textseq_id.hpp>

#include <algorithm>
#include <stdio.h>
#include <algo/structure/cd_utils/cuUtils.hpp>

//  comma is now allowed (4/15/04)
//  single-quote is now allowed (4/28/04)


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils)

const int CDTreeColorCycle[] = {
    0x806000,  // dark brown
    0xff0000,  // red
    0x149600,  // dark green
    0x0000ff,  // dark blue
    0xb428ff,  // purple
    0xff8c1a,  // orange
    0x00ffff,  // light blue
    0xff499b,  // pink
    0xc89600,  // brown
    0x00ff00,  // light green
    0xaaaaaa,  // gray
    0xcee000,  // olive
    0x4a85ff,  // blue
    0xffc800,  // light orange
    0xc800bc,  // maroon
    0xeeee00,  // yellow
};
const int kNumColorsInCDTreeColorCycle = sizeof(CDTreeColorCycle)/sizeof(CDTreeColorCycle[0]);


void Make_GI_or_PDB_String_CN3D(const CRef< CSeq_id > SeqID, std::string& Str) {
//-------------------------------------------------------------------
// make a string for a seq-id
//-------------------------------------------------------------------
  int   GI;
  const CPDB_seq_id*  pPDB_ID;
  char buf[1024];

  if (SeqID.Empty()) {
      Str += "<Empty Sequence>";
      return;
  } else if (!SeqID->IsGi() && !SeqID->IsPdb()) {
      Str += "<Non-gi/pdb Sequence Types Unsupported>";
      return;
  }

  if (SeqID->IsGi()) {
    GI = SeqID->GetGi();
    sprintf(buf,"gi %d", GI);
  }
  if (SeqID->IsPdb()) {
    pPDB_ID = &(SeqID->GetPdb());
	char chain=pPDB_ID->GetChain();
    sprintf(buf,"pdb %s_%c",pPDB_ID->GetMol().Get().c_str(),chain);
	int len=strlen(buf);
	if(chain==' ')buf[len-2]=0; // if there is no chain info
  }

  Str += string(buf);

}

string Make_SeqID_String(const CRef< CSeq_id > SeqID, bool Pad, int Len) {
//-------------------------------------------------------------------
// make a string for a seq-id
//-------------------------------------------------------------------
  int   GI;
  const CPDB_seq_id*  pPDB_ID;
  char buf[1024];
  string Str;

  if (SeqID.Empty()) {
      return "<Empty Sequence>";
  } else if (!SeqID->IsGi() && !SeqID->IsPdb() && !SeqID->IsOther() && !SeqID->IsLocal()) {
      return SeqID->GetSeqIdString();  
  }

  //  Custom string construction for Gi, PDB, Other, or Local types
  if (SeqID->IsGi()) {
      GI = SeqID->GetGi();
      sprintf(buf,"%d", GI);
  }
  if (SeqID->IsPdb()) {
      pPDB_ID = &(SeqID->GetPdb());
      sprintf(buf,"%s %c",pPDB_ID->GetMol().Get().c_str(),pPDB_ID->GetChain());
  }
  if (SeqID->IsOther()) {
      sprintf(buf,"%s",SeqID->GetOther().GetAccession().c_str());
  }
  if (SeqID->IsLocal()) {
	  if (SeqID->GetLocal().IsId()) {
		  sprintf(buf,"%d",SeqID->GetLocal().GetId());
	  }
	  if (SeqID->GetLocal().IsStr()) {
		  string test = SeqID->GetLocal().GetStr();
		  sprintf(buf,"%s",test.c_str());
	  }
  }

  Str = string(buf);

  // pad with spaces to length of Len
  if (Pad) {
      if ( (int)Str.size() < Len) {
          Str.append(Len - Str.size(), ' ');
      }
  }
  return Str;
}



string GetSeqIDStr(const CSeq_id& SeqID) 
{
    CRef< CSeq_id > cRefSeqID(new CSeq_id);
    cRefSeqID->Assign(SeqID);
    string Str = Make_SeqID_String(cRefSeqID, false, 0);
    return Str;
}

string GetSeqIDStr(const CRef< CSeq_id >& SeqID) 
{
    string Str = Make_SeqID_String(SeqID, false, 0);
    return Str;
}

//  Return the first CCdd_id object in a CD.
string CddIdString(const CCdd& cdd) {
    string s;
    list< CRef< CCdd_id > >::const_iterator i, ibegin, iend;

    ibegin = cdd.GetId().Get().begin();
    iend   = cdd.GetId().Get().end();
    for (i = ibegin; i != iend; ++i) {
        if (i != ibegin) {
            s.append(", ");
        }
        s.append(CddIdString(**i));
    }
	return s;
}


string CddIdString(const CCdd_id& id) {

    CCdd_id::E_Choice e = id.Which();
    if (e == CCdd_id::e_Uid) {
        return "UID " + I2S(id.GetUid());
    } else if (e == CCdd_id::e_Gid) {
        string s = "Accession " + id.GetGid().GetAccession();
        if (id.GetGid().IsSetDatabase()) {
            s.append(" Database " + id.GetGid().GetDatabase());
        }
        if (id.GetGid().IsSetRelease()) {
            s.append(" Release " + id.GetGid().GetRelease());
        }
        if (id.GetGid().IsSetVersion()) {
            s.append(" Version " + I2S(id.GetGid().GetVersion()));
        }
        return s;
    } else {
        return "Unset/Unknown Cdd_id";
    }

}

bool SameCDAccession(const CCdd_id& id1, const CCdd_id& id2) {

    bool result = false;
    CCdd_id::E_Choice e1 = id1.Which(), e2 = id2.Which();
    if (e1 == CCdd_id::e_Gid && e1 == e2) {
        if (id1.GetGid().GetAccession() == id2.GetGid().GetAccession()) {
            result = true;
        }
    }
    return result;
}

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
    vector<string> sharpTokens;
    CRegexp regexpCommon("book=(.*)&part=(.*)");
    CRegexp regexpRendertype("&part=(.*)&rendertype=(.*)&id=(.*)");

    //  note:  I don't have ownership of the CEnumeratedTypeValues pointer
    const CEnumeratedTypeValues* allowedElements = CCdd_book_ref::GetTypeInfo_enum_ETextelement();
    
    NStr::Tokenize(brBookUrl, "#", sharpTokens);
    if (sharpTokens.size() == 1 || sharpTokens.size() == 2) {
        bool haveSubaddr = (sharpTokens.size() == 2);
        
        //  All URLs have 'book' and 'part' parameters.
        firstTokenStr = sharpTokens[0];
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
                    subaddress = sharpTokens[1];
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

//  Break up URLs formatted as per Bookshelf URL scheme released 2010;
//  allow use of the entire URL.
//  
//  section/chapter:
//  books/<bookname>/#<elementid>
//
//  table/figure/box/glossary item:
//  books/<bookname>/<elementtype>/<elementid>/
//      -- OR --
//  books/<bookname>/<elementtype>/<elementid>/?report=objectonly
//      -- OR --
//  books/<bookname>/?rendertype=<elementtype>&id=<elementid>
//
//  The 'bookname' is the prefix 'NBK' plus an "article ID" - usually numeric but
//  allow for non-numeric values to be safe.  The 'elementid' is a string typically
//  starting with 'A' if the remainder of the elementid is numeric.  However, in rare
//  cases the elementid may have a different format for certain element types.
//  For 'chapter' legacy book refs, or pages refering to an entire page vs. a specific
//  location in a bookshelf document, the elementid may be undefined.
//  Redirection by Entrez can generate the alternate 'rendertype' URL format
//  for figures, tables, boxes, and glossary items (the latter being a 'def-item').
//  All derived book references will exclusively use the Celementid CCdd_book_ref field; 
//  the Csubelementid field no longer appears necessary in this URL scheme.
bool PortalBookURLToCCddBookRef(const string& portalBookUrl, CRef < CCdd_descr >& descr)
{
    bool result = false;
    if (descr.Empty()) return result;

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

        typeStr = kEmptyStr;

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

        descr->SetBook_ref().SetBookname(nbkCode);
        descr->SetBook_ref().SetTextelement(typeEnum);
        descr->SetBook_ref().SetCelementid(idStr);
        result = true;
    }

    return result;
}



bool Prosite2Regex(const std::string& prosite, std::string* regex, std::string* errString) {
//-------------------------------------------------------------------
// copied from Paul.  see sequence_set.cpp
//-------------------------------------------------------------------
  errString->erase();
  try {
    // check allowed characters
    static const std::string allowed = "-ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789[],(){}<>.";
    unsigned int i;
    for (i=0; i<prosite.size(); i++) {
      if (allowed.find(toupper((unsigned char) prosite[i])) == std::string::npos) break;
    }
    if (i != prosite.size()) throw "invalid ProSite character";
    if (prosite[prosite.size() - 1] != '.') throw "ProSite pattern must end with '.'";
    
    // translate into real regex syntax;
    regex->erase();
    
    bool inGroup = false;
    for (i=0; i<prosite.size(); i++) {
      
      // handle grouping and termini
      bool characterHandled = true;
      switch (prosite[i]) {
      case '-': case '.': case '>':
        if (inGroup) {
          *regex += ')';
          inGroup = false;
        }
        if (prosite[i] == '>') *regex += '$';
        break;
      case '<':
        *regex += '^';
        break;
      default:
        characterHandled = false;
        break;
      }
      if (characterHandled) continue;
      if (!inGroup && (
        (isalpha((unsigned char) prosite[i]) && toupper((unsigned char) prosite[i]) != 'X') ||
        prosite[i] == '[' || prosite[i] == '{')) {
        *regex += '(';
        inGroup = true;
      }
      
      // translate syntax
      switch (prosite[i]) {
      case '(':
        *regex += '{';
        break;
      case ')':
        *regex += '}';
        break;
      case '{':
        *regex += "[^";
        break;
      case '}':
        *regex += ']';
        break;
      case 'X': case 'x':
        *regex += '.';
        break;
      default:
        *regex += toupper((unsigned char) prosite[i]);
        break;
      }
    }
  }
 
  catch (const char *err) {
    *errString = string(err);
      //    AFrame::ShowError(err);
  }
  
  return true;
}

END_SCOPE(cd_utils)
END_NCBI_SCOPE
