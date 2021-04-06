#!/usr/bin/env python3

import tachyon

result = tachyon.phaserize('shoot')

if not isinstance(result, int):
    raise SystemExit('Returned result not an integer.')

if result != 1:
    raise SystemExit('Returned result {} is not 1.'.format(result))
