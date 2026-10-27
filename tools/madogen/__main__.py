# pyright: strict
# SPDX-FileCopyrightText: 2026 Gustavo Franco
# SPDX-License-Identifier: MIT

import sys

from .cli import main

sys.exit(main(sys.argv[1:]))
