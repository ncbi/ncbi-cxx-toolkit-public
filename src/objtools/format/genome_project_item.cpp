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
* Author:  Frank Ludwig, NCBI
*
* File Description:
*   flat-file generator -- genome project item implementation
*
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/User_field.hpp>
#include <objmgr/seqdesc_ci.hpp>

#include <objtools/format/formatter.hpp>
#include <objtools/format/text_ostream.hpp>
#include <objtools/format/items/genome_project_item.hpp>
#include <objtools/format/context.hpp>

#include "utils.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CGenomeProjectItem::CGenomeProjectItem(CBioseqContext& ctx) :
    CFlatItem(&ctx)
{
    x_GatherInfo(ctx);
}


void CGenomeProjectItem::Format
(IFormatter& formatter,
 IFlatTextOStream& text_os) const

{
    formatter.FormatGenomeProject(*this, text_os);
}

const vector<int> & CGenomeProjectItem::GetProjectNumbers() const {
    return m_ProjectNumbers;
}

const CGenomeProjectItem::TDBLinkLineVec & CGenomeProjectItem::GetDBLinkLines() const {
    return m_DBLinkLines;
}

/***************************************************************************/
/*                                  PRIVATE                                */
/***************************************************************************/

static string
s_JoinLinkableStrs(const CUser_field_Base::C_Data::TStrs &strs, 
                   const string &url_prefix, const bool is_html )
{
    const char * pchPrefix = "";

    CNcbiOstrstream result;
    ITERATE( CUser_field_Base::C_Data::TStrs, str_iter, strs ) {
        const string &id = *str_iter;
        if( id.empty() ) {
            continue;
        }
        result << pchPrefix;
        if( is_html && ! url_prefix.empty() ) {
            result << "<a href=\"" << url_prefix << id << "\">";
        }
        result << id;
        if( is_html && ! url_prefix.empty() ) {
            result << "</a>";
        }
        pchPrefix = ", ";
    }
    
    return CNcbiOstrstreamToString( result );
}

static string
s_JoinNumbers( const CUser_field_Base::C_Data::TInts & ints, const string & separator )
{
    const char * pchPrefix = "";

    CNcbiOstrstream result;
    ITERATE( CUser_field_Base::C_Data::TInts, int_iter, ints ) {
        if( *int_iter == 0 ) {
            continue;
        }
        result << pchPrefix;
        result << *int_iter;
        pchPrefix = separator.c_str();
    }
    return CNcbiOstrstreamToString( result );
}

namespace {
    struct SDBLinkLineLessThan {
        bool operator()(const string & line1, const string & line2 ) {
            const int line1_prefix_order = x_GetPrefixOrder(line1);
            const int line2_prefix_order = x_GetPrefixOrder(line2);
            if( line1_prefix_order != line2_prefix_order ) {
                return (line1_prefix_order < line2_prefix_order);
            }

            // fall back on traditional sorting
            return line1 < line2;
        }

    private:

        int x_GetPrefixOrder(const string & line)
        {
            // this is what's returned if we encounter any problems
            const static int kDefaultPrefixOrder = kMax_Int; // last

            // first, extract prefix
            string::size_type colon_pos = line.find(':');
            if( colon_pos == string::npos ) {
                return kDefaultPrefixOrder;
            }

            const string sPrefix = line.substr(0, colon_pos);

            // translate prefix to ordering
            typedef SStaticPair<const char *, int>  TPrefixElem;
            static const TPrefixElem sc_prefix_map[] = {
                // we skip numbers just to make it easier to insert things in between.
                // the exact number used and the amount skipped doesn't matter, as long
                // as the smallest is first, largest is last, etc.
                { "Assembly", 20 },
                { "BioProject", 10 },
                { "BioSample", 30 },
                { "ProbeDB", 40 },
                { "Sequence Read Archive", 50 },
                { "Trace Assembly Archive", 60 }
            };
            typedef CStaticArrayMap<const char *, int, PNocase_CStr> TPrefixMap;
            DEFINE_STATIC_ARRAY_MAP(TPrefixMap, sc_PrefixMap, sc_prefix_map);

            TPrefixMap::const_iterator find_iter = sc_PrefixMap.find(sPrefix.c_str());
            if( find_iter == sc_PrefixMap.end() ) {
                // unknown prefix type
                return kDefaultPrefixOrder;
            }
            
            return find_iter->second;
        }
    };

    struct SDbLinkLabelInfo {
        bool   allow_text; // true if we accept use-field str and strs
        bool   allow_numeric; // true if we accept use-field int and ints
        string url; // empty if no url
    };
}

void CGenomeProjectItem::x_GatherInfo(CBioseqContext& ctx)
{
    const bool bHtml = ctx.Config().DoHTML();

    const CUser_object *genome_projects_user_obje = NULL;
    const CUser_object *dblink_user_obj = NULL;

    // extract all the useful user objects
    for (CSeqdesc_CI desc(ctx.GetHandle(), CSeqdesc::e_User);  desc;  ++desc) {
        const CUser_object& uo = desc->GetUser();

        if ( !uo.GetType().IsStr() ) {
            continue;
        }
        string strHeader = uo.GetType().GetStr();
        if ( NStr::EqualNocase(strHeader, "GenomeProjectsDB")) {
            genome_projects_user_obje = &uo;
        } else if( NStr::EqualNocase( strHeader, "DBLink" ) ) {
            dblink_user_obj = &uo;
        }
    }

    // process GenomeProjectsDB
    if( genome_projects_user_obje != NULL ) {
        ITERATE (CUser_object::TData, uf_it, genome_projects_user_obje->GetData()) {
            const CUser_field& field = **uf_it;
            if ( field.IsSetLabel()  &&  field.GetLabel().IsStr() ) {
                const string& label = field.GetLabel().GetStr();
                if ( NStr::EqualNocase(label, "ProjectID")) {
                    m_ProjectNumbers.push_back( field.GetData().GetInt() );
                }
            }
        }
    }

    typedef SStaticPair<const char *, SDbLinkLabelInfo> TDbLinkLabelToInfo;
    static const TDbLinkLabelToInfo kDbLinkLabelToInfo[] = {
        { "Assembly",               { true,  false, "http://www.ncbi.nlm.nih.gov/assembly/" } },
        { "BioProject",             { true,  false, "http://www.ncbi.nlm.nih.gov/bioproject/" } },
        { "BioSample",              { true,  false, "http://www.ncbi.nlm.nih.gov/biosample/" } },
        { "ProbeDB",                { true,  false, "" } },
        { "Sequence Read Archive",  { true,  false, "http://www.ncbi.nlm.nih.gov/sra/" } },
        { "Trace Assembly Archive", { false, true,  "" } }
    };
    typedef const CStaticPairArrayMap<const char *, SDbLinkLabelInfo, PNocase> TDbLinkLabelToInfoMap;
    DEFINE_STATIC_ARRAY_MAP(TDbLinkLabelToInfoMap, kDbLinkLabelToInfoMap, kDbLinkLabelToInfo);

    // process DBLink
    // ( we have these temporary vectors because we can't push straight to m_DBLinkLines
    //  because we have to sort them in case they're out of order in the ASN.1 )
    vector<string> dblinkLines;
    if( dblink_user_obj != NULL ) {
        ITERATE (CUser_object::TData, uf_it, dblink_user_obj->GetData()) {
            const CUser_field& field = **uf_it;
            if ( field.IsSetLabel()  &&  field.GetLabel().IsStr() && field.CanGetData() ) {
                const string& label = field.GetLabel().GetStr();

                TDbLinkLabelToInfoMap::const_iterator find_iter =
                    kDbLinkLabelToInfoMap.find(label.c_str());
                if( find_iter == kDbLinkLabelToInfoMap.end() ) {
                    continue;
                }

                const char             * pchNormalizedDbLinkLabel = find_iter->first;
                const SDbLinkLabelInfo & dbLinkLabelInfo          = find_iter->second;

                typedef CUser_field::C_Data TFieldData;
                const TFieldData & field_data = field.GetData();

                if( dbLinkLabelInfo.allow_text && 
                    (field_data.IsStrs() || field_data.IsStr()) )
                {
                    const TFieldData::TStrs * pStrs = NULL;

                    // auto_ptr just used to destroy the pStrs if it's
                    // dynamically created.
                    auto_ptr<TFieldData::TStrs> pStrsDestroyer;

                    if(  field_data.IsStrs() ) {
                        pStrs = & field_data.GetStrs();
                    } else {
                        _ASSERT( field_data.IsStr() );
                        pStrsDestroyer.reset( new TFieldData::TStrs );
                        pStrsDestroyer->push_back( field_data.GetStr() );
                        pStrs = pStrsDestroyer.get();
                    }

                    string dblinkValue = s_JoinLinkableStrs( 
                        *pStrs, dbLinkLabelInfo.url, bHtml );
                    if( ! dblinkValue.empty() ) {
                        dblinkLines.push_back( 
                            pchNormalizedDbLinkLabel + string(": ") + dblinkValue );
                        if( bHtml ) {
                            TryToSanitizeHtml( dblinkLines.back() );
                        }
                    }

                } else if( dbLinkLabelInfo.allow_numeric && 
                    (field_data.IsInts() || field_data.IsInt()) )
                {

                    const TFieldData::TInts * pInts = NULL;
                    // destroys pInts if it's dynamically created
                    auto_ptr<TFieldData::TInts> pIntsDestroyer;

                    if( field_data.IsInts() ) {
                        pInts = & field_data.GetInts();
                    } else if( field_data.IsInt() ) {
                        pIntsDestroyer.reset( new TFieldData::TInts );
                        pIntsDestroyer->push_back( field_data.GetInt() );
                        pInts = pIntsDestroyer.get();
                    }

                    string dblinkValue = s_JoinNumbers( *pInts, ", " );
                    if( ! dblinkValue.empty() ) {
                        dblinkLines.push_back( 
                            pchNormalizedDbLinkLabel + string(": ") + 
                            dblinkValue );
                        // No need to sanitize; it's just numbers, commas, and spaces
                    }
                }
            }
        }
        sort( dblinkLines.begin(), dblinkLines.end(), SDBLinkLineLessThan() );
        copy( dblinkLines.begin(), dblinkLines.end(), back_inserter(m_DBLinkLines) );
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE
