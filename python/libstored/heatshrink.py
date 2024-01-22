# SPDX-FileCopyrightText: 2020-2024 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

class HeatshrinkDecoder:
    '''
    This is the decoder implementation of heatshrink: https://github.com/atomicobject/heatshrink

    Although there is a python wrapper available at https://github.com/eerimoq/pyheatshrink,
    this implementation exists here to break dependencies and compatibility issues.
    '''

    def __init__(self, window_sz2=8, lookahead_sz2=4):
        pass

    def fill(self, x):
        return x

    def finish(self, x):
        return x
