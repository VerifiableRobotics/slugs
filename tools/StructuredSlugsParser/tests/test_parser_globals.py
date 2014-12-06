#!/usr/bin/env python
"""Fails if global state is used in slugs.compiler"""

import logging
logging.basicConfig(level=logging.DEBUG)

import slugs

s1 = """
[OUTPUT]
x

[SYS_INIT]
x
"""

s2 = """
[OUTPUT]
x: 0...5

[SYS_LIVENESS]
( x <= 5 )
"""

slugs.convert_to_slugsin(s1, False)
slugs.convert_to_slugsin(s2, False)
