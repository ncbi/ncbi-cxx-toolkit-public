"""
JSON object marshalling (to and from UTTP token
sequences) for transmission over binary streams.
"""

# $Id$

# Tell pylint not to obsess about old-style class definitions
# pylint: disable=C1001


import uttp

import sys
import struct

PY3 = sys.version_info[ 0 ] == 3


class MessageExchange:
    """Class to send and receive serialized JSON objects over binary streams."""

    class ProtocolError(Exception):
        """Error in message parsing."""
        pass

    class InputError(Exception):
        """Input data structure cannot be represented as a JSON object."""
        pass

    __MIN_OUTPUT_BUF_SIZE = 8192
    __MAX_INPUT_SIZE = 8192

    def __init__(self, write_to, read_from):
        """Initialize this object with input and output streams."""
        self.__current_node = None
        self.__hash_key = None

        self.__output_stream = write_to
        self.__input_stream = read_from

        self.__writer = uttp.Writer(MessageExchange.__MIN_OUTPUT_BUF_SIZE)
        self.__reader = uttp.Reader()

        if sys.byteorder == 'big':
            self.__float_prefix = 'D'
        else:
            self.__float_prefix = 'd'

    def exchange(self, message, read_as_binary=False):
        """Exchange JSON array objects."""
        self.send(message)
        return self.receive(read_as_binary)

    @staticmethod
    def readline( s ):
        v = ""
        while True:
            c = s.recv( 1 )
            if c == "":
                return v
            v += c
            if c == '\n':
                return v

    def receive(self, read_as_binary=False):
        """Receive and return a JSON array object."""
        self.__current_node = None
        __message_complete = False
        __node_stack = []
        __current_chunk = None
        self.__hash_key = None
        float_format = None

        while True:
            event = self.__reader.next_event()
            if event == uttp.Reader.END_OF_BUFFER:
                # NB! The self.__input_stream object could be:
                #     - a file like object for grid_cli communications
                #     - a python socket
                # The python socket does not support the readline() member. So
                # it needs to be emulated. Usage of recv() instead does cause
                # hard to catch when two packaets are glued together by TCP/IP.
                # The use of makefile() for a socket does not work either because
                # the file like object is buffered and thus another read from a
                # socket misses some buffered portion.
                if hasattr( self.__input_stream, "readline" ):
                    buf = self.__input_stream.readline()
                else:
                    buf = MessageExchange.readline( self.__input_stream )

                if not buf:
                    raise MessageExchange.ProtocolError('Unexpected EOF while '
                                                        'reading UTTP stream')
                self.__reader.set_new_buf(buf)
            elif event == uttp.Reader.CONTROL_SYMBOL:
                if __current_chunk is not None:
                    raise MessageExchange.ProtocolError('Unexpected control '
                                                        'symbol while reading '
                                                        'chunk data')
                control_symbol = self.__reader.get_control_symbol()
                if type(control_symbol) == bytes:
                    control_symbol = control_symbol.decode()
                if control_symbol == '\n':
                    if not __message_complete:
                        raise MessageExchange.ProtocolError('Unexpected EOM')
                    ne = self.__reader.next_event()
                    if ne != uttp.Reader.END_OF_BUFFER:
                        raise MessageExchange.ProtocolError('Extra characters '
                                                            'past EOM')
                    return self.__current_node

                if control_symbol == '[':
                    new_node = []
                    if self.__add_new_node(new_node):
                        __node_stack.append(self.__current_node)
                        self.__current_node = new_node
                elif control_symbol == '{':
                    new_node = {}
                    if self.__add_new_node(new_node):
                        __node_stack.append(self.__current_node)
                        self.__current_node = new_node
                elif control_symbol == ']':
                    if self.__current_node is None or \
                            not isinstance(self.__current_node, list):
                        raise MessageExchange.ProtocolError('Mismatched '
                                                            'closing bracket')
                    if __node_stack:
                        self.__current_node = __node_stack.pop()
                    else:
                        __message_complete = True
                elif control_symbol == '}':
                    if self.__current_node is None or \
                            not isinstance(self.__current_node, dict):
                        raise MessageExchange.ProtocolError('Mismatched '
                                                            'closing bracket')
                    if __node_stack:
                        self.__current_node = __node_stack.pop()
                    else:
                        __message_complete = True
                elif control_symbol == 'N':
                    if not self.__add_new_node(False):
                        __message_complete = True
                elif control_symbol == 'Y':
                    if not self.__add_new_node(True):
                        __message_complete = True
                elif control_symbol == 'U':
                    if not self.__add_new_node(None):
                        __message_complete = True
                elif control_symbol in ('D', 'd'):
                    if control_symbol == 'D':
                        float_format = '>d'
                    else:
                        float_format = '<d'
                    event = self.__reader.read_raw_data(8)
                    if event == uttp.Reader.CHUNK_PART:
                        __current_chunk = self.__reader.get_chunk()
                    elif event == uttp.Reader.CHUNK:
                        (double,) = struct.unpack(float_format,
                                                  self.__reader.get_chunk())
                        float_format = None
                        if not self.__add_new_node(double):
                            __message_complete = True
                    else:
                        __current_chunk = ''
                else:
                    raise MessageExchange.ProtocolError('Unknown '
                                                        'control symbol: ' +
                                                        repr(control_symbol))
            elif event == uttp.Reader.NUMBER:
                if __current_chunk is not None:
                    raise MessageExchange.ProtocolError('Unexpected number '
                                                        'while reading '
                                                        'chunk data')
                if not self.__add_new_node(self.__reader.get_number()):
                    __message_complete = True
            else: # event is either CHUNK or CHUNK_PART
                if __current_chunk is None:
                    __current_chunk = self.__reader.get_chunk()
                else:
                    __current_chunk += self.__reader.get_chunk()

                if event == uttp.Reader.CHUNK:
                    if float_format is not None:
                        (double,) = struct.unpack(float_format, __current_chunk)
                        float_format = None
                        if isinstance(self.__current_node, list):
                            self.__current_node.append(double)
                        elif self.__hash_key is None:
                            raise MessageExchange.ProtocolError('Cannot use '
                                                                'float as '
                                                                'a key')
                        else:
                            self.__current_node[self.__hash_key] = double
                            self.__hash_key = None
                    else:
                        if isinstance(self.__current_node, list):
                            if read_as_binary:
                                self.__current_node.append(__current_chunk)
                            else:
                                self.__current_node.append(__current_chunk.decode())
                        elif self.__hash_key is None:
                            self.__hash_key = __current_chunk
                        else:
                            # That's a hack for binary data
                            if type(self.__hash_key) == bytes:
                                self.__hash_key = self.__hash_key.decode()
                            if self.__hash_key in [ 'embedded_data',
                                                    'raw_input',
                                                    'raw_output' ]:
                                self.__current_node[self.__hash_key] = \
                                        __current_chunk
                            else:
                                if type(__current_chunk) == bytes:
                                    __current_chunk = __current_chunk.decode()
                                self.__current_node[self.__hash_key] = \
                                        __current_chunk
                            self.__hash_key = None
                    __current_chunk = None

    def get_uttp_reader(self):
        """Provides the reader"""
        return self.__reader

    def get_uttp_writer(self):
        """Provides the writer"""
        return self.__writer

    def __add_new_node(self, new_node):
        """Append new_node to the current node (either list or dict)."""
        if self.__current_node is None:
            self.__current_node = new_node
            return False
        elif isinstance(self.__current_node, list):
            self.__current_node.append(new_node)
        elif self.__hash_key is not None:
            if type(self.__hash_key) == bytes:
                self.__hash_key = self.__hash_key.decode()
            self.__current_node[self.__hash_key] = new_node
            self.__hash_key = None
        else:
            raise MessageExchange.ProtocolError('JSON object key '
                                                'must be a string')
        return True

    def send(self, root_node):
        """Send a JSON node as a complete message."""
        self.__send_node(root_node)
        self.__sink(self.__writer.send_control_symbol('\n'))
        self.__sink(self.__writer.flush_buf())
        self.__output_stream.flush()

    def __send_node(self, node):
        """Send a single piece of JSON structure (object, array, or value)."""
        if isinstance(node, list):
            self.__sink(self.__writer.send_control_symbol('['))
            for subnode in node:
                self.__send_node(subnode)
            self.__sink(self.__writer.send_control_symbol(']'))
        elif isinstance(node, dict):
            self.__sink(self.__writer.send_control_symbol('{'))
            if PY3:
                iter_items = node.items()
            else:
                iter_items = node.iteritems()

            for key, subnode in iter_items:
                if not isinstance(key, str):
                    raise MessageExchange.InputError('Object attribute name '
                                                     'must be a string')
                self.__sink(self.__writer.send_chunk(key))
                self.__send_node(subnode)
            self.__sink(self.__writer.send_control_symbol('}'))
        elif isinstance(node, bool):
            if node:
                node = 'Y'
            else:
                node = 'N'
            self.__sink(self.__writer.send_control_symbol(node))
        elif isinstance(node, int):
            self.__sink(self.__writer.send_number(node))
        elif isinstance(node, float):
            self.__sink(self.__writer.send_raw_data(self.__float_prefix +
                                                    struct.pack('d', node)))
        elif node is None:
            self.__sink(self.__writer.send_control_symbol('U'))
        else:
            if PY3:
                self.__sink(self.__writer.send_chunk(node))
            else:
                self.__sink(self.__writer.send_chunk(str(node)))

    def __sink(self, buf):
        """Send the specified buffer to the output stream."""
        if buf:
            if PY3:
                if type( buf ) == bytes:
                    self.__output_stream.write(buf)
                else:
                    self.__output_stream.write(buf.encode( 'utf-8' ))
            else:
                self.__output_stream.write(buf)

