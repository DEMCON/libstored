# SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

import sys
import re

with open(sys.argv[2]) as f:
    for l in f:
        print(re.sub(r'\bSTORE_T\b', f'STORE_T_{sys.argv[1]}', l), end='')
