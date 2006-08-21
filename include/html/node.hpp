#ifndef HTML___NODE__HPP
#define HTML___NODE__HPP

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
 * Author:  Lewis Geer
 *
 */

/// @file node.hpp 
/// The standard node class.


#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <map>
#include <list>
#include <memory>


/** @addtogroup HTMLcomp
 *
 * @{
 */


BEGIN_NCBI_SCOPE


class CNCBINode;
typedef CRef<CNCBINode> CNodeRef;
//#define NCBI_LIGHTWEIGHT_LIST 1

// Base class for a graph node.
class NCBI_XHTML_EXPORT CNCBINode : public CObject
{
public:
    friend class CRef<CNCBINode>;
    typedef list<CNodeRef> TChildren;
#if NCBI_LIGHTWEIGHT_LIST
    typedef TChildren TChildrenMember;
#else
    typedef auto_ptr<TChildren> TChildrenMember;
#endif
    struct SAttributeValue
    {
        SAttributeValue(void)
            : m_Optional(true)
            {
                return;
            }
        SAttributeValue(const string& value, bool optional)
            : m_Value(value), m_Optional(optional)
            {
                return;
            }
        SAttributeValue& operator=(const string& value)
            {
                m_Value = value;
                m_Optional = true;
                return *this;
            }
        const string& GetValue(void) const
            {
                return m_Value;
            }
        operator const string&(void) const
            {
                return m_Value;
            }
        bool IsOptional(void) const
            {
                return m_Optional;
            }
        void SetOptional(bool optional = true)
            {
                m_Optional = optional;
            }
    private:
        string m_Value;
        bool m_Optional;
    };
    typedef map<string, SAttributeValue, PNocase> TAttributes;
    
    enum EMode {
        ePlainText = 0,
        eHTML      = 1,
        eXHTML     = 2
    };

    class TMode {
    public:
        TMode(EMode mode = eHTML)
            : m_Mode(mode), m_Node(0), m_Previous(0)
            {
                return;
            }
        TMode(int mode)
            : m_Mode(EMode(mode)), m_Node(0), m_Previous(0)
            {
                return;
            }
        TMode(const TMode* mode, CNCBINode* node)
            : m_Mode(mode->m_Mode), m_Node(node), m_Previous(mode)
            {
                return;
            }
        operator EMode(void) const
            {
                return m_Mode;
            }
        bool operator==(EMode mode) const
            {
                return mode == m_Mode;
            }

        CNCBINode* GetNode(void) const
            {
                return m_Node;
            }
        const TMode* GetPreviousContext(void) const
            {
                return m_Previous;
            }
    private:
        // to avoid allocation in 
        EMode m_Mode;
        CNCBINode* m_Node;
        const TMode* m_Previous;
    };

    // 'structors
    CNCBINode(void);
    CNCBINode(const string& name);
    CNCBINode(const char* name);
    virtual ~CNCBINode();

    // Add a Node * to the end of m_Children.
    // Returns 'this' for chained AppendChild().
    CNCBINode* AppendChild(CNCBINode* child);
    CNCBINode* AppendChild(CNodeRef& ref);

    // Remove all occurencies of the child from this node's subtree
    // (along with its subtree).
    // Throw exception if the child is not found.
    // Return smart pointer to the removed child node.
    CNodeRef RemoveChild(CNCBINode* child);
    CNodeRef RemoveChild(CNodeRef&  child);
    void RemoveAllChildren(void);

    // All child operations (except AppendChild) are valid only if
    // have children return true
    bool HaveChildren(void) const;
    TChildren& Children(void);
    const TChildren& Children(void) const;
    TChildren::iterator ChildBegin(void);
    TChildren::iterator ChildEnd(void);
    static CNCBINode* Node(TChildren::iterator i);
    TChildren::const_iterator ChildBegin(void) const;
    TChildren::const_iterator ChildEnd(void) const;
    static const CNCBINode* Node(TChildren::const_iterator i);

    virtual CNcbiOstream& Print(CNcbiOstream& out, TMode mode = eHTML);
    virtual CNcbiOstream& PrintBegin(CNcbiOstream& out, TMode mode);
    virtual CNcbiOstream& PrintChildren(CNcbiOstream& out, TMode mode);
    virtual CNcbiOstream& PrintEnd(CNcbiOstream& out, TMode mode);

    void    SetRepeatCount(size_t count = 0);
    size_t  GetRepeatCount(void);

    // This method will be called once before Print().
    virtual void CreateSubNodes(void);
    // Call CreateSubNodes() if it's not called yet.
    void Initialize(void);
    // Reinitialize node, so hierarhy can be created anew.
    // All previously set attributes remains unchanged. 
    // On the next Print() the CreateSubNodes() method
    // will be called again.
    void ReInitialize(void);

    // Find and replace text with a node.
    virtual CNCBINode* MapTag(const string& tagname);
    CNodeRef MapTagAll(const string& tagname, const TMode& mode);

    // Repeat tag node (works only inside tag node mappers)
    void RepeatTag(bool enable = true);
    bool NeedRepeatTag(void);

    const string& GetName(void) const;

    bool HaveAttributes(void) const;
    TAttributes& Attributes(void);
    const TAttributes& Attributes(void) const;
    // Retrieve attribute.
    bool HaveAttribute(const string& name) const;
    const string& GetAttribute(const string& name) const;
    bool AttributeIsOptional(const string& name) const;
    bool AttributeIsOptional(const char* name) const;
    void SetAttributeOptional(const string& name, bool optional = true);
    void SetAttributeOptional(const char* name, bool optional = true);
    const string* GetAttributeValue(const string& name) const;

    // Set attribute.
    void SetAttribute(const string& name, const string& value);
    void SetAttribute(const string& name);
    void SetAttribute(const string& name, int value);
    void SetOptionalAttribute(const string& name, const string& value);
    void SetOptionalAttribute(const string& name, bool set);

    void SetAttribute(const char* name, const string& value);
    void SetAttribute(const char* name);
    void SetAttribute(const char* name, int value);
    void SetOptionalAttribute(const char* name, const string& value);
    void SetOptionalAttribute(const char* name, bool set);

    // Exception handling.

    /// Flags defining how to catch and process exceptions.
    /// By default flags are unsettled.
    /// Note that without the fCatchAll flag only CHTMLExceptions and
    /// all derived exceptons can be traced.
    enum EExceptionFlags {
        fAddTrace              = 0x1, ///< Enable tag trace.
        fCatchAll              = 0x2, ///< Catch all other exceptions and
                                      ///< rethrow CHTMLException.
        fDisableCheckRecursion = 0x4  ///< Disable to throw exception if
                                      ///<  nodes tree have endless recursion.
    };
    typedef int TExceptionFlags;      ///< Binary OR of "EExceptionFlags"

    // Set/get global exception handling flags.
    static void SetExceptionFlags(TExceptionFlags flags);
    static TExceptionFlags GetExceptionFlags(void);

protected:
    virtual void DoAppendChild(CNCBINode* child);
    virtual void DoSetAttribute(const string& name,
                                const string& value, bool optional);

    bool            m_CreateSubNodesCalled;
    TChildrenMember m_Children;         ///< Child nodes
    string          m_Name;             ///< Node name
    size_t          m_RepeatCount;      ///< How many times repeat node

    // Repeat tag flag (used only inside tag node mappers hooks). See RepeatTag().
    bool            m_RepeatTag; 

    // Attributes, e.g. href="link.html"
    auto_ptr<TAttributes> m_Attributes;     

private:
    // To prevent copy constructor.
    CNCBINode(const CNCBINode& node);
    // To prevent assignment operator.
    CNCBINode& operator=(const CNCBINode& node);

    // Return children list (create if needed).
    TChildren& GetChildren(void);
    // Return attributes map (create if needed).
    TAttributes& GetAttributes(void);
};


// Inline functions are defined here:
#include <html/node.inl>


END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.30  2006/08/21 16:01:59  ivanov
 * CNCBINode::EMode += eXHTML
 *
 * Revision 1.29  2006/05/10 14:54:01  ivanov
 * + CNCBINode::RemoveChild
 *
 * Revision 1.28  2005/08/22 12:13:16  ivanov
 * Removed extra empty lines
 *
 * Revision 1.27  2005/06/10 13:47:57  ivanov
 * + CNCBINode::ReInitialize()
 *
 * Revision 1.26  2004/03/18 12:28:15  ivanov
 * Remove extra comma after last enum value in EExceptionFlags.
 *
 * Revision 1.25  2004/03/10 20:14:44  ivanov
 * Added new exception flag fDisableCheckRecursion.
 * By default endless nodes recursion check is enabled.
 *
 * Revision 1.24  2004/02/02 14:26:04  ivanov
 * CNCBINode: added ability to repeat stored context
 *
 * Revision 1.23  2003/12/23 17:58:51  ivanov
 * Added exception tracing
 *
 * Revision 1.22  2003/11/05 18:41:06  dicuccio
 * Added export specifiers
 *
 * Revision 1.21  2003/11/03 17:02:53  ivanov
 * Some formal code rearrangement. Move log to end.
 *
 * Revision 1.20  2003/04/25 13:45:33  siyan
 * Added doxygen groupings
 *
 * Revision 1.19  2001/05/17 14:55:37  lavr
 * Typos corrected
 *
 * Revision 1.18  2000/12/12 14:38:37  vasilche
 * Changed the way CHTMLNode::CreateSubNodes() is called.
 *
 * Revision 1.17  2000/07/18 17:21:34  vasilche
 * Added possibility to force output of empty attribute value.
 * Added caching to CHTML_table, now large tables work much faster.
 * Changed algorithm of emitting EOL symbols in html output.
 *
 * Revision 1.16  2000/03/29 15:50:38  vasilche
 * Added const version of CRef - CConstRef.
 * CRef and CConstRef now accept classes inherited from CObject.
 *
 * Revision 1.15  2000/03/07 15:40:37  vasilche
 * Added AppendChild(CNodeRef&)
 *
 * Revision 1.14  2000/03/07 15:26:06  vasilche
 * Removed second definition of CRef.
 *
 * Revision 1.13  1999/12/28 18:55:29  vasilche
 * Reduced size of compiled object files:
 * 1. avoid inline or implicit virtual methods (especially destructors).
 * 2. avoid std::string's methods usage in inline methods.
 * 3. avoid string literals ("xxx") in inline methods.
 *
 * Revision 1.12  1999/11/19 15:45:32  vasilche
 * CNodeRef implemented as CRef<CNCBINode>
 *
 * Revision 1.11  1999/10/28 13:40:30  vasilche
 * Added reference counters to CNCBINode.
 *
 * Revision 1.10  1999/05/28 16:32:09  vasilche
 * Fixed memory leak in page tag mappers.
 *
 * Revision 1.9  1999/05/20 16:49:13  pubmed
 * Changes for SaveAsText: all Print() methods get mode parameter that can be HTML or PlainText
 *
 * Revision 1.8  1999/01/04 20:06:11  vasilche
 * Redesigned CHTML_table.
 * Added selection support to HTML forms (via hidden values).
 *
 * Revision 1.7  1998/12/28 20:29:13  vakatov
 * New CVS and development tree structure for the NCBI C++ projects
 *
 * Revision 1.6  1998/12/23 21:20:58  vasilche
 * Added more HTML tags (almost all).
 * Importent ones: all lists (OL, UL, DIR, MENU), fonts (FONT, BASEFONT).
 *
 * Revision 1.5  1998/12/23 14:28:08  vasilche
 * Most of closed HTML tags made via template.
 *
 * Revision 1.4  1998/12/21 22:24:57  vasilche
 * A lot of cleaning.
 *
 * Revision 1.3  1998/11/23 23:47:50  lewisg
 * *** empty log message ***
 *
 * Revision 1.2  1998/10/29 16:15:52  lewisg
 * version 2
 *
 * Revision 1.1  1998/10/06 20:34:31  lewisg
 * html library includes
 *
 * ===========================================================================
 */ 

#endif  /*  HTML___NODE__HPP */
