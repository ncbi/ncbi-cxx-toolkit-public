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
 * Authors:  Vahram Avagyan
 *
 */

/// @file gene_info.cpp
/// Implementation of the Gene information class.

//==========================================================================//

#include <ncbi_pch.hpp>
#include <objtools/readers/gene_info/gene_info.hpp>

BEGIN_NCBI_SCOPE

//==========================================================================//
// Constants

/// Default number of characters to print per line, used for
/// converting Gene info objects to text / HTML.
static const unsigned int k_nGeneInfoLineLength = 80;

/// String to output when no Gene info is available.
static const string k_strNoGeneInfo = "(No Gene Info)";

/// String to output before the Gene ID.
static const string k_strGeneIdBegin = "GENE ID: ";

/// Show a message when the PubMed link count is zero?.
static const bool k_bShowNoPubMedLinks = false;
/// Show a message in case of a few (<10) PubMed links?.
static const bool k_bShowFewPubMedLinks = true;
/// Show information about the number of PubMed links?.
static const bool k_bShowPubMedInfo = true;

/// String to output when the PubMed link count is zero.
static const string k_strNoPubMedLinks = "(No PubMed links)";
/// First part of the "few PubMed links" message.
static const string k_strFewPubMedLinksBegin = "(";
/// Second part of the "few PubMed links" message.
static const string k_strFewPubMedLinksEnd = " or fewer PubMed links)";
/// First part of the "many PubMed links" message.
static const string k_strManyPubMedLinksBegin = "(Over ";
/// Second part of the "many PubMed links" message.
static const string k_strManyPubMedLinksEnd = " PubMed links)";

/// URL prefix for linking to the Entrez Gene entry.
static const string k_strGeneLinkURL =
    "http://www.ncbi.nlm.nih.gov/sites/entrez?db=gene&cmd=search&term=";

/// HTML opening tag for the PubMed link information.
static const string k_strSpanPubMedBegin =
                        "<span class=\"Gene_PubMedLinks\">";
/// HTML closing tag for the PubMed link information.
static const string k_strSpanPubMedEnd =
                        "</span>";

//==========================================================================//

CGeneInfo::CGeneInfo()
: m_bIsInitialized(false), m_nGeneId(0), m_nPubMedLinks(0)
{}

CGeneInfo::CGeneInfo(int nGeneId,
                     const string& strSymbol,
                     const string& strDescription,
                     const string& strOrgname,
                     int nPubMedLinks)
: m_bIsInitialized(true),
  m_nGeneId(nGeneId),
  m_strSymbol(strSymbol),
  m_strDescription(strDescription),
  m_strOrgname(strOrgname),
  m_nPubMedLinks(nPubMedLinks)
{}

CGeneInfo::~CGeneInfo()
{}

void CGeneInfo::x_Append(string& strDest,
                         unsigned int& nCurLineEffLength,
                         const string& strSrc,
                         unsigned int nSrcEffLength,
                         unsigned int nMaxLineLength)
{
    if (nCurLineEffLength + nSrcEffLength >= nMaxLineLength)
    {
        strDest += "\n" + strSrc;
        nCurLineEffLength = nSrcEffLength;
    }
    else
    {
        strDest += " " + strSrc;
        nCurLineEffLength += 1 + nSrcEffLength;
    }
}

void CGeneInfo::ToString(string& strGeneInfo,
                         bool bFormatAsHTML,
                         unsigned int nMaxLineLength) const
{
    if (!IsInitialized())
    {
        strGeneInfo = k_strNoGeneInfo;
    }
    else
    {
        if (nMaxLineLength <= 0)
        {
            // use default line length for gene info lines
            nMaxLineLength = k_nGeneInfoLineLength;
        }

        unsigned int nCurLineEffLength = 0;

        // Append Gene ID, Gene symbol,
        // and the link to Entrez page (if requested)

        string strGeneId = NStr::IntToString(GetGeneId());
        string strGeneSymbol = GetSymbol();

        string strGeneIdSection;
        if (bFormatAsHTML)
        {
            strGeneIdSection += "<a href=\"";
            strGeneIdSection += k_strGeneLinkURL + strGeneId;
            strGeneIdSection += "\">";
        }

        strGeneIdSection += k_strGeneIdBegin;
        strGeneIdSection += strGeneId;
        strGeneIdSection += " " + strGeneSymbol;

        if (bFormatAsHTML)
        {
            strGeneIdSection += "</a>";
        }

        unsigned int nGeneIdSectionLength = k_strGeneIdBegin.length() +
                                            strGeneId.length() + 1 +
                                            strGeneSymbol.length();
        x_Append(strGeneInfo, nCurLineEffLength,
                 strGeneIdSection, nGeneIdSectionLength,
                 nMaxLineLength);

        // Append the separator

        string strSeparator = "|";
        x_Append(strGeneInfo, nCurLineEffLength,
                 strSeparator, strSeparator.length(),
                 nMaxLineLength);

        // Append Gene description. It can be pretty long, split it into
        // words and append them one by one.

        vector<string> strDescrWords;
        NStr::Tokenize(GetDescription(), " ", strDescrWords);
        for (size_t iWord = 0; iWord < strDescrWords.size(); iWord++)
        {
            string strCurWord = strDescrWords[iWord];
            x_Append(strGeneInfo, nCurLineEffLength,
                     strCurWord, strCurWord.length(),
                     nMaxLineLength);
        }

        // Append the organism name

        string strOrgName = "[" + GetOrganismName() + "]";
        x_Append(strGeneInfo, nCurLineEffLength,
                 strOrgName, strOrgName.length(),
                 nMaxLineLength);

        // Append the estimated number of pubmed links (if requested)

        if (k_bShowPubMedInfo)
        {
            string strNumPubMedLinks;
            if (GetNumPubMedLinks() == 0)
            {
                if (k_bShowNoPubMedLinks)
                    strNumPubMedLinks = k_strNoPubMedLinks;
                else
                    strNumPubMedLinks = "";
            }
            else
            {
                int nBase = 10, nMaxExp = 2;
                int nUpperBound = nBase;
                for (int iExp = 1; iExp <= nMaxExp; iExp++)
                {
                    if (GetNumPubMedLinks() < nUpperBound)
                        break;

                    nUpperBound *= nBase;
                }

                if (nUpperBound == nBase)
                {
                    if (k_bShowFewPubMedLinks)
                    {
                        strNumPubMedLinks += k_strFewPubMedLinksBegin;
                        strNumPubMedLinks += NStr::IntToString(nBase);
                        strNumPubMedLinks += k_strFewPubMedLinksEnd;
                    }
                    else
                        strNumPubMedLinks = "";
                }
                else
                {
                    strNumPubMedLinks += k_strManyPubMedLinksBegin;
                    strNumPubMedLinks += NStr::IntToString(nUpperBound/nBase);
                    strNumPubMedLinks += k_strManyPubMedLinksEnd;
                }
            }

            int nPMEffLength = strNumPubMedLinks.length();
            if (nPMEffLength > 0)
            {
                if (bFormatAsHTML)
                    strNumPubMedLinks = k_strSpanPubMedBegin +
                                        strNumPubMedLinks +
                                        k_strSpanPubMedEnd;

                x_Append(strGeneInfo, nCurLineEffLength,
                         strNumPubMedLinks, nPMEffLength,
                         nMaxLineLength);
            }
        }
    }
}

CNcbiOstream& operator<<(CNcbiOstream& out, const CGeneInfo& geneInfo)
{
    string strGeneInfo;
    geneInfo.ToString(strGeneInfo);

    return out << strGeneInfo << endl;
}

//==========================================================================//

END_NCBI_SCOPE
