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
 * Author:  Sergey Satskiy, NCBI
 * Credits: Denis Vakatov, NCBI (API design)
 *
 */


/** @file
 * Extension to work with raw libxml2 objects
**/

#ifndef _xmlwrapp_libxml2_xmlwrapp_hpp_
#define _xmlwrapp_libxml2_xmlwrapp_hpp_


#include <misc/xmlwrapp/document.hpp>

// Forward declaration
#include <libxml/tree.h>


namespace xml {

/// Used to specify if xmlwrapp should grab the ownership of a libxml2 object
enum ownership_type {
    type_own,       ///< Grab the ownership, i.e. the object
                    ///< will be freed by xmlwrapp.
    type_not_own    ///< Do not grab the ownership, i.e. the object
                    ///< will not be freed by xmlwrapp.
};



/// Extension to the xml::document class which allows working with raw libxml2
/// documents.
class libxml2_document : public document
{
    public:
        /**
         * Create a new document using the given raw libxml2 object.
         * Use it at your own risk.
         *
         * @param raw_doc
         *  Raw libxml2 document.
         *  If passed NULL, then an exception will be thrown.
         * @param ownership
         *  Whether to grab the libxml2 document ownership or not. If the
         *  ownership is grabbed then the object will be freed in the
         *  destructor.
         * @author Denis Vakatov, NCBI
        **/
        libxml2_document(xmlDoc *  raw_doc, ownership_type  ownership);

        /**
         * Create a new XML document with the default settings. The new
         * document will contain a root node with a name of "blank".
         * The document ownership is set to type_own.
         *
         * @author Peter Jones
        **/
        libxml2_document(void);

        /**
         * Create a new XML document object by parsing the given XML file.
         * The document ownership is set to type_own.
         *
         * @param filename
         *  The XML file name.
         * @param messages
         *  A pointer to the object where all the warnings are collected.
         *  If NULL then no messages will be collected.
         * @param how
         *  How to treat warnings (default: warnings are not treated as
         *  errors). If warnings are treated as errors then an exception
         *  is thrown in case of both errors and/or warnings. If warnings
         *  are not treated as errors then an exception will be thrown
         *  only when there are errors.
         * @exception Throws xml::parser_exception in case of parsing errors
         *            and std::exception in case of other problems.
         * @author Sergey Satskiy, NCBI
        **/
        libxml2_document(const char* filename, error_messages* messages,
                         warnings_as_errors_type how =
                                             type_warnings_not_errors);

        /**
         * Create a new XML documant object by parsing the given XML from a
         * memory buffer.
         * The document ownership is set to type_own.
         *
         * @param data
         *  The XML memory buffer.
         * @param size
         *  Size of the memory buffer.
         * @param messages
         *  A pointer to the object where all the warnings are collected.
         *  If NULL then no messages will be collected.
         * @param how
         *  How to treat warnings (default: warnings are not treated as
         *  errors). If warnings are treated as errors then an exception
         *  is thrown in case of both errors and/or warnings. If warnings
         *  are not treated as errors then an exception will be thrown
         *  only when there are errors.
         * @exception Throws xml::parser_exception in case of parsing errors
         *            and std::exception in case of other problems.
         * @author Sergey Satskiy, NCBI
        **/
        libxml2_document(const char* data, size_type size,
                         error_messages* messages,
                         warnings_as_errors_type how =
                                             type_warnings_not_errors);

        /**
         * Create a new XML document and set the name of the root element
         * to the given text.
         * The document ownership is set to type_own.
         *
         * @param root_name
         *  What to set the name of the root element to.
         * @author Peter Jones
        **/
        explicit libxml2_document(const char *root_name);

        /**
         * Create a new XML document and set the root node.
         * The document ownership is set to type_own.
         *
         * @param n
         *  The node to use as the root node. n will be copied.
         * @author Peter Jones
        **/
        explicit libxml2_document(const node &n);

        /**
         * Creates a new XML document using the document_proxy,
         * i.e. essentially xslt results. (see CXX-2458).
         * The document ownership is set to type_own.
         *
         * @param doc_proxy
         *  XSLT results
         * @author Denis Vakatov
        **/
        libxml2_document(const document_proxy &  doc_proxy);

        /**
         * Creates a new XML document by parsing the given XML from a stream.
         * The document ownership is set to type_own.
         *
         * @param stream
         *  The stream to read XML document from.
         * @param messages
         *  A pointer to the object where all the warnings are collected.
         *  If NULL then no messages will be collected.
         * @param how
         *  How to treat warnings (default: warnings are not treated as
         *  errors). If warnings are treated as errors then an exception
         *  is thrown in case of both errors and/or warnings. If warnings
         *  are not treated as errors then an exception will be thrown
         *  only when there are errors.
         * @exception Throws xml::parser_exception in case of parsing errors
         *            and std::exception in case of other problems.
         * @author Denis Vakatov
        **/
        libxml2_document(std::istream &    stream,
                         error_messages *  messages,
                         warnings_as_errors_type  how =
                                             type_warnings_not_errors);

        /**
         * Provides a raw libxml2 document.
         *
         * @return
         *  Raw libxml2 document
         * @author Denis Vakatov, NCBI
        **/
        xmlDoc *  get_raw_doc(void);

        /**
         * Set a raw libxml2 document.
         * If the previous document was owned by the object, then it 'll be
         * destroyed.
         *
         * @param raw_doc
         *  Raw libxml2 document to manage from now on.
         *  If passed NULL, then an exception will be thrown.
         *  If passed the same libxml2 document as the object has then this
         *  method behaves as set_ownership(...).
         * @param ownersip
         *  Whether to take the ownersip over the 'raw_doc'
         * @author Greg Schuler, NCBI
        **/
        void  set_raw_doc(xmlDoc *  raw_doc, ownership_type  ownership);

        /**
         * Provides the document ownership type.
         *
         * @return
         *  type_own if the document is owned by the object, i.e. will be freed
         *  in the destructor.
        **/
        ownership_type get_ownership(void) const;

        /**
         * Set the document ownership.
         *
         * @param ownership
         *  The new ownership. If it is 'type_own' then the libxml2 document
         *  will be freed in the destructor.
        **/
        void set_ownership(ownership_type  ownership);

    private:
        libxml2_document(const libxml2_document &);
        libxml2_document &  operator=(const libxml2_document &);
};

}

#endif

