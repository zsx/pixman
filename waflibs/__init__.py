#!/usr/bin/env python
#encoding: utf-8
import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), 'github'))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from github.platinfo import PlatInfo
from github.autoconf.endian import *
from github.autoconf.sizeof import *
from github.autoconf.openmp import *

import misc
