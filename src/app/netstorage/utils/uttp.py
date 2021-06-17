"""Implementation of UTTP (Untyped Tree Transfer Protocol) in Python."""

# $Id$

# Tell pylint not to obsess about old-style class definitions
# pylint: disable=C1001


import sys

PY3 = sys.version_info[ 0 ] == 3


class Reader:
    """Parse input buffer and sequentially return a stream of UTTP tokens."""

    class FormatError(Exception):
        """Exception raised for unexpected characters in the input stream."""
        pass

    CHUNK_PART = 0
    CHUNK = 1
    CONTROL_SYMBOL = 2
    NUMBER = 3
    END_OF_BUFFER = 4

    __CONTROL_CHARS = 0
    __CHUNK_LENGTH = 1
    __CHUNK = 2

    def __init__(self, offset=0):
        """Initialize the state machine of this object."""
        self.__state = Reader.__CONTROL_CHARS
        self.__offset = offset
        self.__buf = ''
        self.__buf_offset = 0
        self.__token = ''
        self.__length_acc = 0
        self.__chunk_continued = False

    def reset_offset(self, offset=0):
        """Set the current offset in the input stream to the new value."""
        self.__offset = offset

    def set_new_buf(self, buf):
        """Start processing of the next chunk of data."""
        self.__buf = buf
        self.__buf_offset = 0

    def next_event(self):
        """Parse the input buffer until a parsing event occurs."""
        if self.__buf_offset == len(self.__buf):
            return Reader.END_OF_BUFFER
        if self.__state == Reader.__CONTROL_CHARS:
            # At least one character will be consumed in this block.
            self.__offset += 1

            # Slicing is used to make it compatible between python 2 and
            # python 3. In case of python 2 the buf is a string while in case
            # of python 3 it is a bytes array
            next_char = self.__buf[self.__buf_offset : self.__buf_offset + 1]
            self.__buf_offset += 1

            # All non-digit characters are considered control symbols.
            if not next_char.isdigit():
                self.__token = next_char
                return Reader.CONTROL_SYMBOL

            # The current character is a digit, which is the first
            # character of the next chunk length. Proceed with reading
            # the chunk length.
            self.__state = Reader.__CHUNK_LENGTH
            self.__length_acc = int(next_char)
            if self.__buf_offset == len(self.__buf):
                return Reader.END_OF_BUFFER
            return self.__continue_reading_chunk_length()
        if self.__state == Reader.__CHUNK_LENGTH:
            return self.__continue_reading_chunk_length()
        return self.__continue_reading_chunk()

    def read_raw_data(self, data_size):
        """Read a block of fixed size data. Return a "parsing event"."""
        if self.__state != Reader.__CONTROL_CHARS:
            raise Reader.FormatError('invalid reader state')

        self.__length_acc = data_size
        self.__state = Reader.__CHUNK

        if self.__buf_offset == len(self.__buf):
            return Reader.END_OF_BUFFER

        return self.__continue_reading_chunk()

    def get_chunk(self):
        """Return the chunk (part) if next_event() was CHUNK(_PART)."""
        return self.__token

    def get_control_symbol(self):
        """Return the control symbol if next_event() was CONTROL_SYMBOL."""
        return self.__token

    def get_number(self):
        """Return the number if next_event() was NUMBER."""
        return self.__length_acc

    def get_offset(self):
        """Return the offset of the current character in the input stream."""
        return self.__offset

    def __continue_reading_chunk_length(self):
        """The current state is __CHUNK_LENGTH, proceed with parsing."""
        # Slicing is used to make it compatible between python 2 and
        # python 3. In case of python 2 the buf is a string while in case
        # of python 3 it is a bytes array
        next_char = self.__buf[self.__buf_offset : self.__buf_offset + 1]
        while next_char.isdigit():
            self.__length_acc = self.__length_acc * 10 + int(next_char)
            self.__offset += 1
            self.__buf_offset += 1
            if self.__buf_offset == len(self.__buf):
                return Reader.END_OF_BUFFER
            next_char = self.__buf[self.__buf_offset : self.__buf_offset + 1]

        self.__offset += 1
        self.__buf_offset += 1

        # For python 3 ...
        if type(next_char) == bytes:
            next_char = next_char.decode()

        if next_char == '+':
            self.__chunk_continued = True
        elif next_char == ' ':
            self.__chunk_continued = False
        else:
            self.__state = Reader.__CONTROL_CHARS
            if next_char == '=':
                return Reader.NUMBER
            elif next_char == '-':
                self.__length_acc = -self.__length_acc
                return Reader.NUMBER
            else:
                self.__token = next_char
                raise Reader.FormatError('invalid character (' +
                                         repr(next_char) + ') '
                                         'after chunk length ' +
                                         str(self.__length_acc))

        self.__state = Reader.__CHUNK
        if self.__buf_offset == len(self.__buf):
            return Reader.END_OF_BUFFER

        return self.__continue_reading_chunk()

    def __continue_reading_chunk(self):
        """The current state is __CHUNK, proceed with reading the chunk."""
        chunk_end = self.__buf_offset + self.__length_acc
        if chunk_end <= len(self.__buf):
            self.__token = self.__buf[self.__buf_offset:chunk_end]
            self.__offset += self.__length_acc
            self.__buf_offset = chunk_end
            # The last part of the chunk has been read --
            # get back to reading control symbols.
            self.__state = Reader.__CONTROL_CHARS
            if self.__chunk_continued:
                return Reader.CHUNK_PART
            return Reader.CHUNK
        else:
            self.__token = self.__buf[self.__buf_offset:]
            self.__offset += len(self.__token)
            self.__length_acc -= len(self.__token)
            self.__buf_offset = len(self.__buf)
            return Reader.CHUNK_PART

class Writer:
    """Serialize series of chunks of data for sending over binary streams."""
    def __init__(self, min_buf_size):
        if PY3:
            self.__buf = b''
        else:
            self.__buf = ''
        self.__min_buf_size = min_buf_size

    def send_control_symbol(self, symbol):
        """Pack a control symbol into the internal buffer. Control
        symbol can be any single byte character except digits.
        Return a buffer to send to the output stream in case of overflow."""
        if PY3:
            if type(symbol) != bytes:
                symbol = symbol.encode()

        if len(self.__buf) < self.__min_buf_size:
            self.__buf += symbol
            return None

        buf = self.__buf
        self.__buf = symbol
        return buf

    def send_chunk(self, chunk, to_be_continued=False):
        """Copy a chunk of data to the internal buffer. Return a buffer
        to send to the output stream in case of overflow."""
        if PY3:
            if type(chunk) != bytes:
                chunk = chunk.encode()

        chunk_length = str(len(chunk))
        if PY3:
            chunk_length = chunk_length.encode()
        self.__buf += chunk_length


        if to_be_continued:
            if PY3:
                self.__buf += b'+'
            else:
                self.__buf += '+'
        else:
            if PY3:
                self.__buf += b' '
            else:
                self.__buf += ' '

        if len(self.__buf) + len(chunk) <= self.__min_buf_size:
            self.__buf += chunk
            return None

        buf = self.__buf
        self.__buf = chunk
        return buf

    def send_raw_data(self, data):
        """Send a block of fixed size data. Return a buffer
        to send to the output stream in case of overflow."""
        if PY3:
            if type(data) != bytes:
                data = data.encode()

        if len(self.__buf) + len(data) <= self.__min_buf_size:
            self.__buf += data
            return None

        buf = self.__buf
        self.__buf = data
        return buf

    def send_number(self, number):
        """Pack a number into the internal buffer. Return a buffer
        to send to the output stream in case of overflow."""
        if number >= 0:
            number = str(number) + '='
        else:
            number = str(-number) + '-'

        if PY3:
            number = number.encode()

        if len(self.__buf) + len(number) <= self.__min_buf_size:
            self.__buf += number
            return None

        buf = self.__buf
        self.__buf = number
        return buf

    def flush_buf(self):
        """Return the contents of the internal buffer and reset the buffer."""
        buf = self.__buf
        if PY3:
            self.__buf = b''
        else:
            self.__buf = ''
        return buf

