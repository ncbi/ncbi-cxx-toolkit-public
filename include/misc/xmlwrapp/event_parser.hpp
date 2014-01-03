/*
 * Copyright (C) 2001-2003 Peter J Jones (pjones@pmade.org)
 * All Rights Reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of the Author nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * $Id$
 * NOTE: This file was modified from its original version 0.6.0
 *       to fit the NCBI C++ Toolkit build framework and
 *       API and functionality requirements.
 */

/** @file
 * This file contains the definition of the xml::event_parser class.
**/

#ifndef _xmlwrapp_event_parser_h_
#define _xmlwrapp_event_parser_h_

// for NCBI_DEPRECATED
#include <ncbiconf.h>

// xmlwrapp includes
#include <misc/xmlwrapp/xml_init.hpp>
#include <misc/xmlwrapp/errors.hpp>

// standard includes
#include <cstddef>
#include <string>
#include <iosfwd>
#include <map>
#include <vector>


// Forward declaration of a raw libxml2 structure
struct _xmlElementContent;

namespace xml {

namespace impl {
struct epimpl; // forward declaration of private implementation
}


/**
 * The xml::event_parser is used to parse an XML document by calling member
 * functions when certain things in the XML document are parsed. In order to
 * use this class you derive a sub-class from it and override the protected
 * virtual functions.
**/
class event_parser {
public:
    /**
     * The enumeration defines control bits to on/off specific SAX parser
     * handlers
    **/
    enum sax_handlers
    {
        start_document_handler       = (1 << 0),    ///< controls the start document handler
        end_document_handler         = (1 << 1),    ///< controls the end document handler
        start_element_handler        = (1 << 2),    ///< controls the start element handler
        end_element_handler          = (1 << 3),    ///< controls the end element handler
        characters_handler           = (1 << 4),    ///< controls the text handler
        pi_handler                   = (1 << 5),    ///< controls the processing instruction handler
        comment_handler              = (1 << 6),    ///< controls the comment handler
        cdata_handler                = (1 << 7),    ///< controls the cdata handler
        notation_decl_handler        = (1 << 8),    ///< controls the notation declaration handler
        entity_decl_handler          = (1 << 9),    ///< controls the entity declaration handler
        unparsed_entity_decl_handler = (1 << 10),   ///< controls the unparsed entity declaration handler
        external_subset_handler      = (1 << 11),   ///< controls the external subset handler
        internal_subset_handler      = (1 << 12),   ///< controls the internal subset handler
        attribute_decl_handler       = (1 << 13),   ///< controls the attribute declaration handler
        element_decl_handler         = (1 << 14),   ///< controls the element declaration handler
        reference_handler            = (1 << 15),   ///< controls the reference handler

        /**
         * Set of the control bits which makes the event_parser bahave exactly
         * the way it was in original xmlwrapp 0.6.0.
        **/
        default_set                  = start_element_handler |
                                       end_element_handler   |
                                       characters_handler    |
                                       pi_handler            |
                                       comment_handler       |
                                       cdata_handler
    };

    typedef std::map<std::string, std::string> attrs_type;  ///< a type for holding XML node attributes
    typedef std::size_t size_type;                          ///< size type
    typedef std::vector<std::string> values_type;           ///< a type for holding attribute declaration values
    typedef int sax_handlers_mask;                          ///< handlers mask type

    //####################################################################
    /**
     * xml::event_parser class constructor.
     *
     * @param mask The handlers mask. Default value makes it compatible with
     *             xmlwrapp 0.6.0.
     * @note The default mask switches on 6 handlers. If your code uses
     *       a subset of handlers (say, 3 out of 6) and uses the default mask
     *       the code  might be not 100% optimal. The performance
     *       will be lower than it could be. It is caused by conversion
     *       between libxml2 and C++ datatypes. The arguments will be
     *       converted even for those handlers which are not used in your
     *       code. So, to get the best performance, use an explicit mask for
     *       handlers which you are actually interested in.
     * @author Peter Jones; Sergey Satskiy, NCBI
    **/
    //####################################################################
    event_parser (sax_handlers_mask mask = default_set);

    //####################################################################
    /**
     * xml::event_parser class destructor.
     *
     * @author Peter Jones
    **/
    //####################################################################
    virtual ~event_parser (void);

    //####################################################################
    /**
     * Call this member function to parse the given file.
     *
     * @param filename The name of the file to parse.
     * @param messages A pointer to the object where all the warnings and error
     *                 messages are collected. If NULL then no messages will be
     *                 collected.
     * @param how How to treat warnings (default: warnings are not treated as
     *            errors). If warnings are treated as errors false is
     *            returned in case of both errors and/or warnings. If warnings
     *            are not treated as errors then false is returned
     *            only when there are errors.
     * @return True if the file was successfully parsed and was not interrupted
     *         by the user; false otherwise.
     * @author Peter Jones; Sergey Satskiy, NCBI
    **/
    //####################################################################
    bool parse_file (const char *filename, error_messages* messages,
                     warnings_as_errors_type how = type_warnings_not_errors);

    //####################################################################
    /**
     * Parse what ever data that can be read from the given stream.
     *
     * @param stream The stream to read data from.
     * @param messages A pointer to the object where all the warnings and error
     *                 messages are collected. If NULL then no messages will be
     *                 collected.
     * @param how How to treat warnings (default: warnings are not treated as
     *            errors). If warnings are treated as errors false is
     *            returned in case of both errors and/or warnings. If warnings
     *            are not treated as errors then false is returned
     *            only when there are errors.
     * @return True if the stream was successfully parsed and was not
     *         interrupted by the user; false otherwise.
     * @author Peter Jones; Sergey Satskiy, NCBI
    **/
    //####################################################################
    bool parse_stream (std::istream &stream, error_messages* messages,
                       warnings_as_errors_type how = type_warnings_not_errors);

    //####################################################################
    /**
     * Call this function to parse a chunk of xml data. When you are done
     * feeding the parser chunks of data you need to call the parse_finish
     * member function. If an error was detected while a chunk was parsed
     * or a callback returned false to stop parsing the parse_finish member
     * function should also be called.
     *
     * @param chunk The xml data chuck to parse.
     * @param messages A pointer to the object where all the warnings and error
     *                 messages are collected. If NULL then no messages will be
     *                 collected.
     * @param how How to treat warnings (default: warnings are not treated as
     *            errors). If warnings are treated as errors false is
     *            returned in case of both errors and/or warnings. If warnings
     *            are not treated as errors then false is returned
     *            only when there are errors.
     * @param length The size of the given data chunk
     * @return True if the chunk was parsed sucessfully and was not interrupted
     *         by the user; false otherwise.
     * @exception Throws xml::exception in case if missed parse_finish call is
     *            detected.
     * @author Peter Jones; Sergey Satskiy, NCBI
    **/
    //####################################################################
    bool parse_chunk (const char *chunk, size_type length,
                      error_messages* messages,
                      warnings_as_errors_type how = type_warnings_not_errors);

    //####################################################################
    /**
     * Finish parsing chunked data. You only need to call this member
     * function if you were parsing chunked xml data via the parse_chunk
     * member function.
     *
     * @param messages A pointer to the object where all the warnings and error
     *                 messages are collected. If NULL then no messages will be
     *                 collected.
     * @param how How to treat warnings (default: warnings are not treated as
     *            errors). If warnings are treated as errors false is
     *            returned in case of both errors and/or warnings. If warnings
     *            are not treated as errors then false is returned
     *            only when there are errors.
     * @return True if all parsing was successful; false otherwise.
     * @author Peter Jones
    **/
    //####################################################################
    bool parse_finish (error_messages* messages,
                       warnings_as_errors_type how = type_warnings_not_errors);

protected:
    /// enum for different types of XML entities
    enum entity_type {
        type_internal_general_entity,
        type_external_general_parsed_entity,
        type_external_general_unparsed_entity,
        type_internal_parameter_entity,
        type_external_parameter_entity,
        type_internal_predefined_entity
    };

    /// enum for different types of XML attributes
    enum attribute_type {
        type_attribute_cdata,
        type_attribute_id,
        type_attribute_idref,
        type_attribute_idrefs,
        type_attribute_entity,
        type_attribute_entities,
        type_attribute_nmtoken,
        type_attribute_nmtokens,
        type_attribute_enumeration,
        type_attribute_notation
    };

    /// enum for different default attribute definition
    enum attribute_default_type {
        type_attribute_none,
        type_attribute_required,
        type_attribute_implied,
        type_attribute_fixed
    };

    /// enum for element content types
    enum element_content_type {
        type_undefined,
        type_empty,
        type_any,
        type_mixed,
        type_element
    };

    //####################################################################
    /**
     * Override this member function to receive the start_document message.
     * This member function is called when the document start is being
     * processed.
     *
     * @return You should return true to continue parsing; false to stop.
     * @note If an exception is generated in the overloaded version of the
     *       member it will be intercepted and two actions will take place:
     *       - a fatal error message will be stored in the messages contaner.
     *         The message text is taken from .what() if the generated
     *         exception derives from std::exception, otherwise a generic
     *         error message is generated.
     *       - parsing of the document will be stopped.
     *       So check the completion status and the error messages after any
     *       usage of the parse*() family members.
     * @author Sergey Satskiy, NCBI
    **/
    //####################################################################
    virtual bool start_document ();
    //####################################################################
    /**
     * Override this member function to receive the end_document message.
     * This member function is called when the document end has been
     * detected.
     *
     * @return You should return true to continue parsing; false to stop.
     * @note If an exception is generated in the overloaded version of the
     *       member it will be intercepted and two actions will take place:
     *       - a fatal error message will be stored in the messages contaner.
     *         The message text is taken from .what() if the generated
     *         exception derives from std::exception, otherwise a generic
     *         error message is generated.
     *       - parsing of the document will be stopped.
     *       So check the completion status and the error messages after any
     *       usage of the parse*() family members.
     * @author Sergey Satskiy, NCBI
    **/
    //####################################################################
    virtual bool end_document ();
    //####################################################################
    /**
     * Override this member function to receive the start_element message.
     * This member function is called when the parser encounters an xml
     * element.
     *
     * @param name The name of the element
     * @param attrs The element's attributes
     * @return You should return true to continue parsing; false to stop.
     * @note If an exception is generated in the overloaded version of the
     *       member it will be intercepted and two actions will take place:
     *       - a fatal error message will be stored in the messages contaner.
     *         The message text is taken from .what() if the generated
     *         exception derives from std::exception, otherwise a generic
     *         error message is generated.
     *       - parsing of the document will be stopped.
     *       So check the completion status and the error messages after any
     *       usage of the parse*() family members.
     * @author Peter Jones
    **/
    //####################################################################
    virtual bool start_element (const std::string &name, const attrs_type &attrs) = 0;

    //####################################################################
    /**
     * Override this member function to receive the end_element message.
     * This member function is called when the parser encounters the closing
     * of an element.
     *
     * @param name The name of the element that was closed.
     * @return You should return true to continue parsing; false to stop.
     * @note If an exception is generated in the overloaded version of the
     *       member it will be intercepted and two actions will take place:
     *       - a fatal error message will be stored in the messages contaner.
     *         The message text is taken from .what() if the generated
     *         exception derives from std::exception, otherwise a generic
     *         error message is generated.
     *       - parsing of the document will be stopped.
     *       So check the completion status and the error messages after any
     *       usage of the parse*() family members.
     * @author Peter Jones
    **/
    //####################################################################
    virtual bool end_element (const std::string &name) = 0;

    //####################################################################
    /**
     * Override this member function to receive the text message. This
     * member function is called when the parser encounters text nodes.
     *
     * @param contents The contents of the text node.
     * @return You should return true to continue parsing; false to stop.
     * @note In case of named entities the callback will have the substituted
     *       entity value and the 'entity_reference' callback may follow
     *       it (see  the 'entity_reference' callback notes).
     * @note If an exception is generated in the overloaded version of the
     *       member it will be intercepted and two actions will take place:
     *       - a fatal error message will be stored in the messages contaner.
     *         The message text is taken from .what() if the generated
     *         exception derives from std::exception, otherwise a generic
     *         error message is generated.
     *       - parsing of the document will be stopped.
     *       So check the completion status and the error messages after any
     *       usage of the parse*() family members.
     * @author Peter Jones
    **/
    //####################################################################
    virtual bool text (const std::string &contents) = 0;

    //####################################################################
    /**
     * Override this member function to receive the cdata mesage. This
     * member function is called when the parser encounters a <![CDATA[]]>
     * section in the XML data.
     *
     * The default implementation just calls the text() member function to
     * handle the text inside the CDATA section.
     *
     * @param contents The contents of the CDATA section.
     * @return You should return true to continue parsing.
     * @return Return false if you want to stop.
     * @note If an exception is generated in the overloaded version of the
     *       member it will be intercepted and two actions will take place:
     *       - a fatal error message will be stored in the messages contaner.
     *         The message text is taken from .what() if the generated
     *         exception derives from std::exception, otherwise a generic
     *         error message is generated.
     *       - parsing of the document will be stopped.
     *       So check the completion status and the error messages after any
     *       usage of the parse*() family members.
     * @author Peter Jones
    **/
    //####################################################################
    virtual bool cdata (const std::string &contents);

    //####################################################################
    /**
     * Override this member function to receive the procesing_instruction
     * message. This member function will be called when the XML parser
     * encounters a processing instruction <?target data?>.
     *
     * The default implementation will ignore processing instructions and
     * return true.
     *
     * @param target The target of the processing instruction
     * @param data The data of the processing instruction.
     * @return You should return true to continue parsing.
     * @return Return false if you want to stop.
     * @note If an exception is generated in the overloaded version of the
     *       member it will be intercepted and two actions will take place:
     *       - a fatal error message will be stored in the messages contaner.
     *         The message text is taken from .what() if the generated
     *         exception derives from std::exception, otherwise a generic
     *         error message is generated.
     *       - parsing of the document will be stopped.
     *       So check the completion status and the error messages after any
     *       usage of the parse*() family members.
     * @author Peter Jones
    **/
    //####################################################################
    virtual bool processing_instruction (const std::string &target, const std::string &data);

    //####################################################################
    /**
     * Override this member function to receive the comment message. This
     * member function will be called when the XML parser encounters a
     * comment <!-- contents -->.
     *
     * The default implementation will ignore XML comments and return true.
     *
     * @param contents The contents of the XML comment.
     * @return You should return true to continue parsing.
     * @return Return false if you want to stop.
     * @note If an exception is generated in the overloaded version of the
     *       member it will be intercepted and two actions will take place:
     *       - a fatal error message will be stored in the messages contaner.
     *         The message text is taken from .what() if the generated
     *         exception derives from std::exception, otherwise a generic
     *         error message is generated.
     *       - parsing of the document will be stopped.
     *       So check the completion status and the error messages after any
     *       usage of the parse*() family members.
     * @author Peter Jones
    **/
    //####################################################################
    virtual bool comment (const std::string &contents);

    //####################################################################
    /**
     * Override this memeber function to receive parser warnings. The
     * default behaviour is to ignore warnings.
     *
     * @param message The warning message from the parser.
     * @return You should return true to continue parsing.
     * @return Return false if you want to stop.
     * @note If an exception is generated in the overloaded version of the
     *       member it will be intercepted and two actions will take place:
     *       - a fatal error message will be stored in the messages contaner.
     *         The message text is taken from .what() if the generated
     *         exception derives from std::exception, otherwise a generic
     *         error message is generated.
     *       - parsing of the document will be stopped.
     *       So check the completion status and the error messages after any
     *       usage of the parse*() family members.
     * @author Peter Jones
    **/
    //####################################################################
    virtual bool warning (const std::string &message);

    //####################################################################
    /**
     * Override this memeber function to receive parser errors. The
     * default behaviour is to stop parsing.
     * Note: There could also be fatal errors. The parser will save such fatal
     * errors in the list (which is available by calling get_parser_messages()
     * member) and will stop parsing.
     *
     * @param message The error message from the parser.
     * @return You should return true to continue parsing.
     * @return Return false if you want to stop.
     * @note If an exception is generated in the overloaded version of the
     *       member it will be intercepted and two actions will take place:
     *       - a fatal error message will be stored in the messages contaner.
     *         The message text is taken from .what() if the generated
     *         exception derives from std::exception, otherwise a generic
     *         error message is generated.
     *       - parsing of the document will be stopped.
     *       So check the completion status and the error messages after any
     *       usage of the parse*() family members.
     * @author Sergey Satskiy, NCBI
    **/
    //####################################################################
    virtual bool error (const std::string &message);

    //####################################################################
    /**
     * Override this memeber function to receive the notation declaration
     * message. This member function will be called when the XML parser
     * encounters <!NOTATION ...> declaration.
     *
     * @param name The name of the notation.
     * @param public_id The public ID of the entity.
     * @param system_id The system ID of the entity.
     * @return You should return true to continue parsing.
     * @return Return false if you want to stop.
     * @note If an exception is generated in the overloaded version of the
     *       member it will be intercepted and two actions will take place:
     *       - a fatal error message will be stored in the messages contaner.
     *         The message text is taken from .what() if the generated
     *         exception derives from std::exception, otherwise a generic
     *         error message is generated.
     *       - parsing of the document will be stopped.
     *       So check the completion status and the error messages after any
     *       usage of the parse*() family members.
     * @author Sergey Satskiy, NCBI
    **/
    //####################################################################
    virtual bool notation_declaration (const std::string &name,
                                       const std::string &public_id,
                                       const std::string &system_id);

    //####################################################################
    /**
     * Override this memeber function to receive the entity declaration
     * message. This member function will be called when the XML parser
     * encounters <!ENTITY ...> declaration.
     *
     * @param name The name of the entity.
     * @param type The type of the entity.
     * @param public_id The public ID of the entity.
     * @param system_id The system ID of the entity.
     * @param content The entity value.
     * @return You should return true to continue parsing.
     * @return Return false if you want to stop.
     * @note If an exception is generated in the overloaded version of the
     *       member it will be intercepted and two actions will take place:
     *       - a fatal error message will be stored in the messages contaner.
     *         The message text is taken from .what() if the generated
     *         exception derives from std::exception, otherwise a generic
     *         error message is generated.
     *       - parsing of the document will be stopped.
     *       So check the completion status and the error messages after any
     *       usage of the parse*() family members.
     * @author Sergey Satskiy, NCBI
    **/
    //####################################################################
    virtual bool entity_declaration (const std::string &name,
                                     entity_type        type,
                                     const std::string &public_id,
                                     const std::string &system_id,
                                     const std::string &content);

    //####################################################################
    /**
     * Override this memeber function to receive the unparsed entity
     * declaration message.
     *
     * @param name The name of the entity.
     * @param public_id The public ID of the entity.
     * @param system_id The system ID of the entity.
     * @param notation_name The notation name.
     * @return You should return true to continue parsing.
     * @return Return false if you want to stop.
     * @note If an exception is generated in the overloaded version of the
     *       member it will be intercepted and two actions will take place:
     *       - a fatal error message will be stored in the messages contaner.
     *         The message text is taken from .what() if the generated
     *         exception derives from std::exception, otherwise a generic
     *         error message is generated.
     *       - parsing of the document will be stopped.
     *       So check the completion status and the error messages after any
     *       usage of the parse*() family members.
     * @author Sergey Satskiy, NCBI
    **/
    //####################################################################
    virtual bool unparsed_entity_declaration (const std::string &name,
                                              const std::string &public_id,
                                              const std::string &system_id,
                                              const std::string &notation_name);

    //####################################################################
    /**
     * Override this memeber function to receive the external subset
     * declaration message.
     *
     * @param name The root element name.
     * @param external_id The external ID.
     * @param public_id The public ID.
     * @return You should return true to continue parsing.
     * @return Return false if you want to stop.
     * @note If an exception is generated in the overloaded version of the
     *       member it will be intercepted and two actions will take place:
     *       - a fatal error message will be stored in the messages contaner.
     *         The message text is taken from .what() if the generated
     *         exception derives from std::exception, otherwise a generic
     *         error message is generated.
     *       - parsing of the document will be stopped.
     *       So check the completion status and the error messages after any
     *       usage of the parse*() family members.
     * @author Sergey Satskiy, NCBI
    **/
    //####################################################################
    virtual bool external_subset_declaration (const std::string &name,
                                              const std::string &external_id,
                                              const std::string &system_id);

    //####################################################################
    /**
     * Override this memeber function to receive the internal subset
     * declaration message.
     *
     * @param name The root element name.
     * @param external_id The external ID.
     * @param public_id The public ID.
     * @return You should return true to continue parsing.
     * @return Return false if you want to stop.
     * @note If an exception is generated in the overloaded version of the
     *       member it will be intercepted and two actions will take place:
     *       - a fatal error message will be stored in the messages contaner.
     *         The message text is taken from .what() if the generated
     *         exception derives from std::exception, otherwise a generic
     *         error message is generated.
     *       - parsing of the document will be stopped.
     *       So check the completion status and the error messages after any
     *       usage of the parse*() family members.
     * @author Sergey Satskiy, NCBI
    **/
    //####################################################################
    virtual bool internal_subset_declaration (const std::string &name,
                                              const std::string &external_id,
                                              const std::string &system_id);

    //####################################################################
    /**
     * Override this memeber function to receive the attribute
     * declaration message.
     *
     * @param element_name The element name.
     * @param attribute_name The attribute full name.
     * @param attr_type The attribute type.
     * @param default_type The attribute default value type.
     * @param default_value The attribute default value.
     * @param default_values The attribute possible default values.
     * @return You should return true to continue parsing.
     * @return Return false if you want to stop.
     * @note If an exception is generated in the overloaded version of the
     *       member it will be intercepted and two actions will take place:
     *       - a fatal error message will be stored in the messages contaner.
     *         The message text is taken from .what() if the generated
     *         exception derives from std::exception, otherwise a generic
     *         error message is generated.
     *       - parsing of the document will be stopped.
     *       So check the completion status and the error messages after any
     *       usage of the parse*() family members.
     * @author Sergey Satskiy, NCBI
    **/
    //####################################################################
    virtual bool attribute_declaration (const std::string &element_name,
                                        const std::string &attribute_name,
                                        attribute_type attr_type,
                                        attribute_default_type default_type,
                                        const std::string &default_value,
                                        const values_type &default_values);

    //####################################################################
    /**
     * Override this memeber function to receive the element
     * declaration message.
     *
     * @param name The element name.
     * @param type The element content type.
     * @param content The raw libxml2 structure pointer.
     * @return You should return true to continue parsing.
     * @return Return false if you want to stop.
     * @note If an exception is generated in the overloaded version of the
     *       member it will be intercepted and two actions will take place:
     *       - a fatal error message will be stored in the messages contaner.
     *         The message text is taken from .what() if the generated
     *         exception derives from std::exception, otherwise a generic
     *         error message is generated.
     *       - parsing of the document will be stopped.
     *       So check the completion status and the error messages after any
     *       usage of the parse*() family members.
     * @author Sergey Satskiy, NCBI
    **/
    //####################################################################
    virtual bool element_declaration (const std::string &name,
                                      element_content_type type,
                                      _xmlElementContent *content);

    //####################################################################
    /**
     * Override this memeber function to receive the entity
     * reference message.
     *
     * @param name The entity reference name.
     * @return You should return true to continue parsing.
     * @return Return false if you want to stop.
     * @note the callback appears straight after the 'text' callback (if the
     *       gobal setting to substutute entities is set to false, see
     *       xml::init::substitute_entities(bool)). The preceding 'text'
     *       callback delivers a substituted named entity value.
     * @note see http://mail.gnome.org/archives/xml/2009-May/msg00006.html
     *       and http://xmlsoft.org/entities.html for more details when you
     *       get errors on entity reference parsing
     * @note If an exception is generated in the overloaded version of the
     *       member it will be intercepted and two actions will take place:
     *       - a fatal error message will be stored in the messages contaner.
     *         The message text is taken from .what() if the generated
     *         exception derives from std::exception, otherwise a generic
     *         error message is generated.
     *       - parsing of the document will be stopped.
     *       So check the completion status and the error messages after any
     *       usage of the parse*() family members.
     * @author Sergey Satskiy, NCBI
    **/
    //####################################################################
    virtual bool entity_reference (const std::string &name);

private:
    friend struct impl::epimpl;
    impl::epimpl *pimpl_; // private implementation

    entity_type get_entity_type (int);
    attribute_type get_attribute_type (int);
    attribute_default_type get_attribute_default_type (int);
    element_content_type get_element_content_type (int);

    // It is necessary to know if it is required to reset the list of error
    // messages when the parse_chunk() or parse_stream() are called. The list
    // must be reset only if the previous parsing is finished i.e. parse_finished()
    // has been called for the previous document. This flag holds the state of
    // the whole parsing process.warnings
    bool    parse_finished_;

    bool is_failure (error_messages* messages,
                     warnings_as_errors_type how) const;

    /*
     * Don't allow anyone to copy construct an event_parser or to call the
     * assignment operator. It does not make sense to copy a parser if it is
     * half way done parsing. Plus, it would be a pain!
     */
    event_parser(const event_parser&);
    event_parser& operator= (const event_parser&);
}; // end xml::event_parser class

} // end xml namespace
#endif
