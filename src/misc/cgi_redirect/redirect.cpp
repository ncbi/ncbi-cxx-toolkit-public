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
 * Author: Vladimir Ivanov
 *
 * File Description:
 *    Class CCgiRedirectApplication - framework to redirec CGI requests.
 */


#include <ncbi_pch.hpp>
#include <misc/cgi_redirect/redirect.hpp>
#include <misc/error_codes.hpp>
#include <cgi/cgictx.hpp>


#define NCBI_USE_ERRCODE_X   Misc_CgiRedirect


BEGIN_NCBI_SCOPE


//////////////////////////////////////////////////////////////////////////////
//
// CCgiRedirectApplication
//

// Default page template
const char* const kDefaultRedirectTemplate = "\
<html><head><title><@_TITLE_@></title></head>\n \
<body>\n \
    <center>\n \
    <h1><@_HEADER_@></h1>\n \
    <h3><@_MESSAGE_@></h3>\n \
    <p>This web page has moved. Please, update your bookmarks and links.</p>\n \
    <p><a href=\"<@_URL_@>\">Go to the new page</a></p>.\n \
    </center> \
</body> \
</html>";


void CCgiRedirectApplication::Init(void)
{
    CParent::Init();
    const CNcbiRegistry& reg = GetConfig();

    // Set page title and messages
    m_Page.AddTagMap("_TITLE_",  new CHTMLText(reg.Get("Main","PageTitle")));
    m_Page.AddTagMap("_HEADER_", new CHTMLText(reg.Get("Main","Header")));
    m_Page.AddTagMap("_MESSAGE_",new CHTMLText(reg.Get("Main","Message")));
    
    // Wait time before redirect (if used by template)
    m_Page.AddTagMap("_TIMER_",  new CHTMLText(reg.Get("Main", "Timer")));

    // Set page template.
    string template_name = reg.Get("Main", "Template");
    if ( !template_name.empty() ) {
        m_Page.SetTemplateFile(template_name);
    } else {
        m_Page.SetTemplateBuffer(kDefaultRedirectTemplate,
                                 strlen(kDefaultRedirectTemplate));
    }
}


bool s_FindEntryName(const string& name, list<string>& lst)
{
    ITERATE(list<string>, i, lst) {
        if (*i == name)
            return true;
    }
    return false;
}


void s_AssignEntryValue(const string& name,
                        const string& value,
                        TCgiEntries&  old_entries,
                        TCgiEntries&  new_entries)
{
    SIZE_TYPE pos = 0;
    string s(value);

    while ( (pos = s.find("&{", pos)) != NPOS ) {
        SIZE_TYPE pos_end = s.find("}", pos + 2);
        if ( pos_end == NPOS ) {
            break;
        }
        string name = value.substr(pos + 2, pos_end - pos - 2);
        TCgiEntriesCI ci = old_entries.find(name);
        if (ci != old_entries.end()) {
            s.replace(pos, pos_end - pos + 1, NStr::URLEncode((ci->second).GetValue()));
        } else {
            s.erase(pos, pos_end - pos + 1);
        }
        pos++;
    }
    s = NStr::URLDecode(s);
    CCgiEntry entry(s);
    new_entries.insert(TCgiEntries::value_type(name, entry));
}


int CCgiRedirectApplication::ProcessRequest(CCgiContext& ctx)
{
    const CNcbiRegistry& reg = ctx.GetConfig();

    TCgiEntries new_entries;

    // Remap CGI entries.
    try {
        RemapEntries(ctx, new_entries);
    } catch (exception& e) {
        ERR_POST_X(1, "Failed to remap CGI entries: " << e.what());
        return 1;
    }

    // Gnerate URLs

    string base_url = reg.Get("Main", "BaseUrl");
    m_Page.AddTagMap("_BASEURL_", new CHTMLText(base_url));

    string url;

    ITERATE(TCgiEntries, i, new_entries) {
        if ( (i->first).empty() ) {
            continue;
         }
        if ( !url.empty() ) {
            url += '&';
        }
        url += i->first;
        if ( !(i->second).empty() ) {
            url += '=';
            url += NStr::URLEncode(i->second);
        }
        // Map each entry.
        m_Page.AddTagMap(i->first, new CHTMLText(i->second));
    }
    m_Page.AddTagMap("_URL_", new CHTMLText(base_url + "?" + url));

    // Compose and flush the resultant HTML page
    try {
        CCgiResponse& response = ctx.GetResponse();
        response.WriteHeader();
        m_Page.Print(response.out(), CNCBINode::eHTML);
    } catch (exception& e) {
        ERR_POST_X(2, "Failed to compose/send HTML page: " << e.what());
        return 1;
    }
    return 0;
}


TCgiEntries& CCgiRedirectApplication::RemapEntries(CCgiContext& ctx,
                                                   TCgiEntries& new_entries)
{
    const CCgiRequest& request = ctx.GetRequest();
    TCgiEntries& entries = const_cast<TCgiEntries&>(request.GetEntries());
    const CNcbiRegistry& reg = ctx.GetConfig();

    // Substitute entries
    // Get list of entries to remove
    list<string> remove_entries;
    string str = reg.Get("Entries", "Remove");
    NStr::Split(str, " ", remove_entries,
                NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);

    // Get list of entries to clear
    list<string> clear_entries;
    str = reg.Get("Entries", "Clear");
    NStr::Split(str, " ", clear_entries,
                NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);

    // Get list of entries to add as empty
    list<string> add_empty_entries;
    str = reg.Get("Entries", "Add");
    NStr::Split(str, " ", add_empty_entries,
                NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);

    // Get substitutions for present entries
    list<string> change_entries;
    reg.EnumerateEntries("Change", &change_entries);

    // Get list of new entries to add
    list<string> add_value_entries;
    reg.EnumerateEntries("Add", &add_value_entries);

    // Create empty entry
    CCgiEntry empty_entry(kEmptyStr);


    ITERATE(TCgiEntries, i, entries) {

        // Check entries marked to remove.
        bool is_removed = false;
        if ( s_FindEntryName(i->first, remove_entries)) {
            is_removed = true;
        }

        // Replace entries marked to clear.
        if ( s_FindEntryName(i->first, clear_entries) ) {
            _TRACE("Add empty: " << i->first);
            new_entries.insert(TCgiEntries::value_type(i->first, empty_entry));
            continue;
        }

        // Change entry as specified in the [Change] section.
        if ( s_FindEntryName(i->first, change_entries) ) {
            _TRACE("Change: " << i->first);
            str = reg.Get("Change", i->first);
            s_AssignEntryValue(i->first, str, entries, new_entries);
            continue;
        }

        // The entry was marked for deletion, do not add it.
        if ( is_removed ) {
            continue;
        }
        // Here we have entry, for which rules is not defined,
        /// so just copy it as is.
        new_entries.insert(*i);
    }

    // Add new empty entries.
    ITERATE(list<string>, i, add_empty_entries) {
        TCgiEntriesCI ci = entries.find(*i);
        if (ci == entries.end()) {
            new_entries.insert(TCgiEntries::value_type(*i, empty_entry));
        }
    }
    // Add new values entries.
    ITERATE(list<string>, i, add_value_entries) {
        TCgiEntriesCI ci = entries.find(*i);
        if (ci == entries.end()) {
            str = reg.Get("Add", *i);
            s_AssignEntryValue(*i, str, entries, new_entries);
        }
    }

    return new_entries;
}


END_NCBI_SCOPE
