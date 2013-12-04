#!/usr/bin/env python

# LSST Data Management System
# Copyright 2013 LSST Corporation.
# 
# This product includes software developed by the
# LSST Project (http://www.lsst.org/).
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the LSST License Statement and 
# the GNU General Public License along with this program.  If not, 
# see <http://www.lsstcorp.org/LegalNotices/>.

"""
Internals that do the actual work for the qserv client program.
Note that this depends on kazoo, so it'd be best to avoid distributing
this to every user. For that reason in the future we might run this
separately from the client, so this may not have access to local
config files provided by user.
"""


SUCCESS = 0

class QservAdminImpl(object):
    """
    Implements functions needed by qserv_admin client program.
    """
    def __init__(self):
        None

    # ---------------------------------------------------------------------------
    def createDb(self, dbName, options):
        """Creates database (options specified explicitly)."""
        print "createDb impl not finished"
        return SUCCESS

    # ---------------------------------------------------------------------------
    def createDbLike(self, dbName, dbName2):
        """Creates database using an existing database as a template."""
        print "Creating db '%s' like '%s'" % (dbName, dbName2)
        print "createDbLike impl not finished"
        return SUCCESS
