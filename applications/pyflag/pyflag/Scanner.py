#!/usr/bin/env python
# ******************************************************
# Copyright 2010: Commonwealth of Australia.
#
# Michael Cohen <scudette@users.sourceforge.net>
#
# ******************************************************
#  Version: FLAG  $Version: 0.87-pre1 Date: Thu Jun 12 00:48:38 EST 2008$
# ******************************************************
#
# * This program is free software; you can redistribute it and/or
# * modify it under the terms of the GNU General Public License
# * as published by the Free Software Foundation; either version 2
# * of the License, or (at your option) any later version.
# *
# * This program is distributed in the hope that it will be useful,
# * but WITHOUT ANY WARRANTY; without even the implied warranty of
# * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# * GNU General Public License for more details.
# *
# * You should have received a copy of the GNU General Public License
# * along with this program; if not, write to the Free Software
# * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
# ******************************************************

class BaseScanner:
    """ This is the base class for all scanners """
    def scan(self, buffer, fd, scanners):
        """ When a scanner is called it is given a buffer which is the
        start of the fd, and the fd.

        The scanner is able to check the buffer to see if its
        interested in scanning the file.  It is also provided with the
        list of all other scanners that are run in this run, and if
        new files are created, they can also be scanned recursively.
        """

import pyaff4

oracle = pyaff4.Resolver()

def scan_urn(inurn, scanners):
    ## Run all scanners on the same fd
    fd = oracle.open(inurn, 'r')
    try:
        buffer = fd.read(1024)
        fd.seek(0)
        for s in scanners:
            s.scan(buffer, fd, scanners)
    finally:
        fd.cache_return()
