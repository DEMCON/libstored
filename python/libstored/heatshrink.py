# SPDX-FileCopyrightText: 2024 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

from enum import Enum
import logging

class HSD_sink_res(Enum):
    HSDR_SINK_OK = 0
    HSDR_SINK_FULL = 1
    HSDR_SINK_ERROR_NULL = -1

class HSD_poll_res(Enum):
    HSDR_POLL_EMPTY = 0
    HSDR_POLL_MORE = 1
    HSDR_POLL_ERROR_NULL = -1
    HSDR_POLL_ERROR_UNKNOWN = -2

class HSD_finish_res(Enum):
    HSDR_FINISH_DONE = 0
    HSDR_FINISH_MORE = 1
    HSDR_FINISH_ERROR_NULL = -1

class HSD_state(Enum):
    HSDS_TAG_BIT = 0
    HSDS_YIELD_LITERAL = 1
    HSDS_BACKREF_INDEX_MSB = 2
    HSDS_BACKREF_INDEX_LSB = 3
    HSDS_BACKREF_COUNT_MSB = 4
    HSDS_BACKREF_COUNT_LSB = 5
    HSDS_YIELD_BACKREF = 6

NO_BITS = 0xffff

class HeatshrinkDecoder:
    '''
    This is the decoder implementation of heatshrink: https://github.com/atomicobject/heatshrink

    Although there is a python wrapper available at https://github.com/eerimoq/pyheatshrink,
    this implementation exists here to break dependencies and compatibility issues.
    '''

    logger = logging.getLogger(__name__)

    def __init__(self, window_sz2=8, lookahead_sz2=4):
        self._window_sz2 = window_sz2
        self._lookahead_sz2 = lookahead_sz2
        self._input_buffer_size = 32
        self._reset()

    def fill(self, x):
        start = 0
        rem = len(x)
        out_buf = bytearray()
        while True:
            if rem > 0:
                res, size = self._sink(x[start:start + rem])
                start += size
                rem -= size

            if self._poll(out_buf) == HSD_poll_res.HSDR_POLL_EMPTY and rem == 0:
                return out_buf

    def finish(self, x = b''):
        out_buf = self.fill(x)

        while self._finish() == HSD_finish_res.HSDR_FINISH_MORE:
            self._poll(out_buf)

        self._reset()
        return out_buf

    # heatshrink_decoder...

    def _reset(self):
        self._input_size = 0
        self._input_index = 0
        self._output_count = 0
        self._output_index = 0
        self._head_index = 0
        self._state = HSD_state.HSDS_TAG_BIT
        self._current_byte = 0
        self._bit_index = 0
        self._buffers = bytearray(b'\0' * (self._input_buffer_size + 2 ** self._window_sz2))

    def _sink(self, x):
        rem = self._input_buffer_size - self._input_size
        if rem == 0:
            return (HSD_sink_res.HSDR_SINK_FULL, 0)

        size = min(len(x), rem)
        self._buffers[self._input_size:size] = x[:size]
        self._input_size += size
        return (HSD_sink_res.HSDR_SINK_OK, size)

    def _poll(self, out_buf):
        assert isinstance(out_buf, bytearray)

        while True:
#            self.logger.debug('-- poll, state is %s, input_size %d', self._state, self._input_size)
            in_state = self._state
            if in_state == HSD_state.HSDS_TAG_BIT:
                self._state = self._st_tag_bit()
            elif in_state == HSD_state.HSDS_YIELD_LITERAL:
                self._state = self._st_yield_literal(out_buf)
            elif in_state == HSD_state.HSDS_BACKREF_INDEX_MSB:
                self._state = self._st_backref_index_msb()
            elif in_state == HSD_state.HSDS_BACKREF_INDEX_LSB:
                self._state = self._st_backref_index_lsb()
            elif in_state == HSD_state.HSDS_BACKREF_COUNT_MSB:
                self._state = self._st_backref_count_msb()
            elif in_state == HSD_state.HSDS_BACKREF_COUNT_LSB:
                self._state = self._st_backref_count_lsb()
            elif in_state == HSD_state.HSDS_YIELD_BACKREF:
                self._state = self._st_yield_backref(out_buf)
            else:
                raise RuntimeError()

            if self._state == in_state:
                # We will never return HSDR_POLL_MORE, as our out_buf can grow as much as needed.
                return HSD_poll_res.HSDR_POLL_EMPTY

    def _st_tag_bit(self):
        bits = self._get_bits(1)
        if bits == NO_BITS:
            return HSD_state.HSDS_TAG_BIT
        elif bits != 0:
            return HSD_state.HSDS_YIELD_LITERAL
        elif self._window_sz2 > 8:
            return HSD_state.HSDS_BACKREF_INDEX_MSB
        else:
            self._output_index = 0
            return HSD_state.HSDS_BACKREF_INDEX_LSB

    def _st_yield_literal(self, out_buf):
        byte = self._get_bits(8)
        if byte == NO_BITS:
            return HSD_state.HSDS_YIELD_LITERAL

        buf_i = self._input_buffer_size
        mask = 2 ** self._window_sz2 - 1
        c = byte & 0xff
        self._buffers[buf_i + (self._head_index & mask)] = c
        self._head_index += 1
        self._push_byte(out_buf, c)
        return HSD_state.HSDS_TAG_BIT

    def _st_backref_index_msb(self):
        bit_ct = self._window_sz2
        assert bit_ct > 8
        bits = self._get_bits(bit_ct - 8)
        if bits == NO_BITS:
            return HSD_state.HSDS_BACKREF_INDEX_MSB
        self._output_index = bits << 8
        return HSD_state.HSDS_BACKREF_INDEX_LSB

    def _st_backref_index_lsb(self):
        bit_ct = self._window_sz2
        bits = self._get_bits(bit_ct if bit_ct < 8 else 8)
        if bits == NO_BITS:
            return HSD_state.HSDS_BACKREF_INDEX_LSB
        self._output_index = (self._output_index | bits) + 1
        br_bit_ct = self._lookahead_sz2
        self._output_count = 0
        return HSD_state.HSDS_BACKREF_COUNT_MSB if br_bit_ct > 8 else HSD_state.HSDS_BACKREF_COUNT_LSB

    def _st_backref_count_msb(self):
        br_bit_ct = self._lookahead_sz2
        assert br_bit_ct > 8
        bits = self._get_bits(br_bit_ct - 8)
        if bits == NO_BITS:
            return HSD_state.HSDS_BACKREF_COUNT_MSB
        self._output_count = bits << 8
        return HSD_state.HSDS_BACKREF_COUNT_LSB

    def _st_backref_count_lsb(self):
        br_bit_ct = self._lookahead_sz2
        bits = self._get_bits(br_bit_ct if br_bit_ct < 8 else 8)
        if bits == NO_BITS:
            return HSD_state.HSDS_BACKREF_COUNT_LSB
        self._output_count = (self._output_count | bits) + 1
        return HSD_state.HSDS_YIELD_BACKREF

    def _st_yield_backref(self, out_buf):
        count = self._output_count
        buf_i = self._input_buffer_size
        mask = 2 ** self._window_sz2 - 1
        neg_offset = self._output_index
        assert neg_offset <= mask + 1
        assert count <= 2 ** self._lookahead_sz2

        for i in range(0, count):
            c = self._buffers[buf_i + ((self._head_index - neg_offset) & mask)]
            self._push_byte(out_buf, c)
            self._buffers[buf_i + (self._head_index & mask)] = c
            self._head_index += 1

        self._output_count = 0
        return HSD_state.HSDS_TAG_BIT

    def _get_bits(self, count):
        accumulator = 0

        if count > 15:
            return NO_BITS

        if self._input_size == 0:
            if self._bit_index < 1 << (count - 1):
                return NO_BITS

        for i in range(0, count):
            if self._bit_index == 0:
                if self._input_size == 0:
                    return NO_BITS
                self._current_byte = self._buffers[self._input_index]
                self._input_index += 1
                if self._input_index == self._input_size:
                    self._input_index = 0
                    self._input_size = 0
                self._bit_index = 0x80

            accumulator = accumulator << 1
            if self._current_byte & self._bit_index:
                accumulator = accumulator | 0x01
            self._bit_index = self._bit_index >> 1

        return accumulator

    def _finish(self):
        if self._state == HSD_state.HSDS_TAG_BIT:
            return HSD_finish_res.HSDR_FINISH_DONE if self._input_size == 0 else HSD_finish_res.HSDR_FINISH_MORE
        elif self._state == HSD_state.HSDS_BACKREF_INDEX_LSB or \
            self._state == HSD_state.HSDS_BACKREF_INDEX_MSB or \
            self._state == HSD_state.HSDS_BACKREF_COUNT_LSB or \
            self._state == HSD_state.HSDS_BACKREF_COUNT_MSB:
            return HSD_finish_res.HSDR_FINISH_DONE if self._input_size == 0 else HSD_finish_res.HSDR_FINISH_MORE
        elif self._state == HSD_state.HSDS_YIELD_LITERAL:
            return HSD_finish_res.HSDR_FINISH_DONE if self._input_size == 0 else HSD_finish_res.HSDR_FINISH_MORE
        else:
            return HSD_finish_res.HSDR_FINISH_MORE

    def _push_byte(self, out_buf, x):
        out_buf.append(x)

