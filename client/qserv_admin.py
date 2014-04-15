#!/usr/bin/env python

# LSST Data Management System
# Copyright 2013-2014 LSST Corporation.
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
qserv client program used by all users that talk to qserv. A thin shell that parses
commands, reads all input data in the form of config files into arrays, and calls
corresponding function.

@author  Jacek Becla, SLAC

Known issues and todos:
 - deal with user authentication
 - many commands still need to be implemented
 - need to separate dangerous admin commands like DROP EVERYTHING
"""

# standard library imports
import ConfigParser
import logging
from optparse import OptionParser
import os
import re
import readline

# local imports
from lsst.db.exception import produceExceptionClass
from kvInterface import CssException
from qserv_admin_impl import QservAdminImpl

####################################################################################
QAdmException = produceExceptionClass('QAdmException', [
    (3001, "BAD_CMD",          "Bad command, see HELP for details."),
    (3002, "CONFIG_NOT_FOUND", "Config file not found."),
    (3003, "MISSING_PARAM",    "Missing parameter."),
    (3004, "WRONG_PARAM",      "Unrecognized parameter."),
    (3005, "WRONG_PARAM_VAL",  "Unrecognized value for parameter."),
    (9997, "CSSERR",           "CSS error."),
    (9998, "NOT_IMPLEMENTED",  "Feature not implemented yet."),
    (9999, "INTERNAL",         "Internal error.")])

####################################################################################
class CommandParser(object):
    """
    Parse commands and calls appropriate function from qserv_admin_impl.
    """

    def __init__(self, connInfo):
        """
        Initialize shared metadata, including list of supported commands.

        @param connInfo     Connection information.
        """
        self._initLogging()
        self._funcMap = {
            'CREATE':  self._parseCreate,
            'DROP':    self._parseDrop,
            'DUMP':    self._parseDump,
            'HELP':    self._printHelp,
            'RELEASE': self._parseRelease,
            'SHOW':    self._parseShow
            }
        self._impl = QservAdminImpl(connInfo)
        self._supportedCommands = """
  Supported commands:
    CREATE DATABASE <dbName> <configFile>;
    CREATE DATABASE <dbName> LIKE <dbName2>;
    CREATE TABLE <dbName> <tableName> <configFile>;
    CREATE TABLE <dbName> <tableName> LIKE <dbName2> <tableName2>;
    DROP DATABASE <dbName>;
    DROP EVERYTHING;
    DUMP EVERYTHING [<outFile>];
    SHOW DATABASES;
    QUIT;
    EXIT;
    ...more coming soon
"""

    def receiveCommands(self):
        """
        Receive user commands. End of command is determined by ';'. Multiple
        commands per line are allowed. Multi-line commands are allowed. To
        terminate: CTRL-D, or 'exit;' or 'quit;'.
        """
        line = ''
<<<<<<< HEAD
        sql = ''
        while True:
            line = raw_input("qserv > ")
            sql += line.strip()+' '
            while re.search(';', sql):
                pos = sql.index(';')
                try:
                    self._parse(sql[:pos])
                except QAdmException as e:
                    self._logger.error(e.__str__())
                    print "ERROR: ", e.__str__()
                sql = sql[pos+1:]
=======
        cmd = ''
        while True:
            line = raw_input("qserv > ")
            cmd += line.strip()+' '
            while re.search(';', cmd):
                pos = cmd.index(';')
                try:
                    self._parse(cmd[:pos])
                except QAdmException as e:
                    self._logger.error(e.__str__())
                    print "ERROR: ", e.__str__()
                cmd = cmd[pos+1:]
>>>>>>> u/jbecla/cssProto5

    def _parse(self, cmd):
        """
        Parse, and dispatch to subparsers based on first word. Raise exceptions on
        errors.
        """
        cmd = cmd.strip()
        # ignore empty commands, these can be generated by typing ;;
        if len(cmd) == 0: return 
        tokens = cmd.split()
        t = tokens[0].upper()
        if t in self._funcMap:
            self._funcMap[t](tokens[1:])
        elif t == 'EXIT' or t == 'QUIT':
            raise SystemExit()
        else:
            raise QAdmException(QAdmException.NOT_IMPLEMENTED, cmd)

    def _parseCreate(self, tokens):
        """
        Subparser - handles all CREATE requests.
        """
        t = tokens[0].upper()
        if t == 'DATABASE':
            self._parseCreateDatabase(tokens[1:])
        elif t == 'TABLE':
            self._parseCreateTable(tokens[1:])
        else:
            raise QAdmException(QAdmException.BAD_CMD)

    def _parseCreateDatabase(self, tokens):
        """
        Subparser - handles all CREATE DATABASE requests.
        """
        l = len(tokens)
        if l == 2:
            dbName = tokens[0]
            configFile = tokens[1]
            options = self._fetchOptionsFromConfigFile(configFile)
            options = self._processDbOptions(options)
            try:
                self._impl.createDb(dbName, options)
            except CssException as e:
                raise QAdmException(QAdmException.CSSERR, 
                                    "Failed to create database '" + dbName + \
                                    "', error was: " +  e.__str__())
        elif l == 3:
            if tokens[1].upper() != 'LIKE':
                raise QAdmException(QAdmException.BAD_CMD, 
                                    "Expected 'LIKE', found: '%s'." % tokens[1])
            dbName = tokens[0]
            dbName2 = tokens[2]
            try:
                self._impl.createDbLike(dbName, dbName2)
            except CssException as e:
                raise QAdmException(QAdmException.CSSERR, 
                             "Failed to create database '" + dbName + "' like '" + \
                             dbName2 + "', error was: ", e.__str__())
        else:
            raise QAdmException(QAdmException.BAD_CMD, 
                                "Unexpected number of arguments.")

    def _parseCreateTable(self, tokens):
        """
        Subparser - handles all CREATE TABLE requests.
        """
        l = len(tokens)
        if l == 3:
            (dbName, tbName, configFile) = tokens
            options = self._fetchOptionsFromConfigFile(configFile)
            options = self._processTbOptions(options)
            try:
                self._impl.createTable(dbName, tbName, options)
            except CssException as e:
                raise QAdmException(QAdmException.CSSERR, 
                          "Failed to create table '" + dbName + "." + tbName + \
                          "', error was: " +  e.__str__())
        elif l == 5:
            (dbName, tbName, likeToken, dbName2, tbName2) = tokens
            if likeToken.upper() != 'LIKE':
                raise QAdmException(QAdmException.BAD_CMD, 
                                    "Expected 'LIKE', found: '%s'." % tokens[2])
            try:
                # FIXME, createTableLike is not implemented!
                self._impl.createTableLike(dbName, tableName, dbName2, tableName2,
                                           options)
            except CssException as e:
                raise QAdmException(QAdmException.CSSERR, 
                         "Failed to create table '" + dbName + "." + tbName + \
                         "' LIKE '" + dbName2 + "." + tbName2 + "', " + \
                         "'error was: ", e.__str__())
        else:
            raise QAdmException(QAdmException.BAD_CMD, 
                                "Unexpected number of arguments.")

    def _parseDrop(self, tokens):
        """
        Subparser - handles all DROP requests.
        """
        t = tokens[0].upper()
        l = len(tokens)
        if t == 'DATABASE':
            if l != 2:
                raise QAdmException(QAdmException.BAD_CMD,  
                                    "unexpected number of arguments")
            try:
                self._impl.dropDb(tokens[1])
            except CssException as e:
                raise QAdmException(QAdmException.CSSERR, 
                                    "Failed to drop database '" + tokens[1] + 
                                    ", error was: ", e.__str__())
        elif t == 'TABLE':
            raise QAdmException(QAdmException.NOT_IMPLEMENTED, "DROP TABLE")

        elif t == 'EVERYTHING':
            try:
                self._impl.dropEverything()
            except CssException as e:
                raise QAdmException(QAdmException.CSSERR, 
                             "Failed to drop everything, error was: ", e.__str__())
        else:
            raise QAdmException(QAdmException.BAD_CMD)

    def _parseDump(self, tokens):
        """
        Subparser, handle all DUMP requests.
        """
        t = tokens[0].upper()
        dest = tokens[1] if len(tokens) > 1 else None
        if t == 'EVERYTHING':
            self._impl.dumpEverything(dest)
        else:
            raise QAdmException(QAdmException.BAD_CMD)

    def _printHelp(self, tokens):
        """
        Print available commands.
        """
        print self._supportedCommands

    def _parseRelease(self, tokens):
        """
        Subparser - handles all RELEASE requests.
        """
        raise QAdmException(QAdmException.NOT_IMPLEMENTED, "RELEASE")

    def _parseShow(self, tokens):
        """
        Subparser, handle all SHOW requests.
        """
        t = tokens[0].upper()
        if t == 'DATABASES':
            self._impl.showDatabases()
        else:
            raise QAdmException(QAdmException.BAD_CMD)

    def _createDb(self, dbName, configFile):
        """
        Create database through config file.
        """
        self._logger.info("Creating db '%s' using config '%s'" % \
                              (dbName, configFile))
        options = self._fetchOptionsFromConfigFile(configFile)
        self._impl.createDb(dbName, options)

    def _fetchOptionsFromConfigFile(self, fName):
        """
        Read config file <fName> for createDb and createTable command, and return
        key-value pair dictionary (flat, e.g., sections are ignored.)
        """
        if not os.access(fName, os.R_OK):
            raise QAdmException(QAdmException.CONFIG_NOT_FOUND, fName)
        config = ConfigParser.ConfigParser()
        config.optionxform = str # case sensitive
        config.read(fName)
        xx = {}
        for section in config.sections():
            for option in config.options(section):
                xx[option] = config.get(section, option)
        return xx

    def _processDbOptions(self, opts):
        """
        Validate options used by createDb, add default values for missing
        parameters.
        """
        if not opts.has_key("clusteredIndex"):
            self._logger.info(
                "param 'clusteredIndex' not found, will use default: ''")
            opts["clusteredIndex"] = ''
        if not opts.has_key("partitioning"):
            self._logger.info(
<<<<<<< HEAD
                "param 'partitioning' not found, will use default: off")
            opts["partitioning"] = "off"
=======
                "param 'partitioning' not found, will use default: 0")
            opts["partitioning"] = "0"
>>>>>>> u/jbecla/cssProto5
        if not opts.has_key("objIdIndex"):
            self._logger.info(
                "param 'objIdIndex' not found, will use default: ''")
            opts["objIdIndex"] = ''
        # these are required options for createDb
        _crDbOpts = { 
            "db_info": ("dbGroup", 
                        "partitioning", 
                        "partitioningStrategy")}
        _crDbPSOpts = {
            "sphBox": ("nStripes", 
                       "nSubStripes", 
                       "overlap")}
        # validate the options
        self._validateKVOptions(opts, _crDbOpts, _crDbPSOpts, "db_info")
        return opts

    def _processTbOptions(self, opts):
        """
        Validate options used by createTable, add default values for missing
        parameters.
        """
        if not opts.has_key("clusteredIndex"):
            self._logger.info(
                "param 'clusteredIndex' not found, will use default: ''")
            opts["clusteredIndex"] = "NULL"
        if not opts.has_key("isRefMatch"):
            self._logger.info("param 'isRefMatch' not found, will use default: No")
            opts["isRefMatch"] = "No"
        # these are required options for createTable
        _crTbOpts = {
            "table_info":("tableName",
                          "partitioning",
                          "schemaFile",
                          "clusteredIndex",
                          "isRefMatch",
                          "isView")}
        _crTbPSOpts = {
            "sphBox":("overlap",
                      "phiColName", 
                      "lonColName", 
                      "latColName")}
        # validate the options
        #self._validateKVOptions(opts,_crTbOpts,_crTbPSOpts,"table_info")
        return opts


    def _validateKVOptions(self, x, xxOpts, psOpts, whichInfo):
        if not x.has_key("partitioning"):
            raise QAdmException(QAdmException.MISSING_PARAM, "partitioning")

<<<<<<< HEAD
        partOff = x["partitioning"] == "off" 
=======
        partOff = x["partitioning"] == "0" 
>>>>>>> u/jbecla/cssProto5
        for (theName, theOpts) in xxOpts.items():
            for o in theOpts:
                # skip optional parameters
                if o == "partitioning":
                    continue
                # if partitioning is "off", partitioningStrategy does not 
                # need to be specified 
                if not (o == "partitiongStrategy" and partOff):
                    continue
                if not x.has_key(o):
                    raise QAdmException(QAdmException.MISSING_PARAM, o)
        if partOff:
            return
<<<<<<< HEAD
        if x["partitioning"] != "on":
=======
        if x["partitioning"] != "1":
>>>>>>> u/jbecla/cssProto5
            raise QAdmException(QAdmException.WRONG_PARAM_VAL, "partitioning",
                                "got: '%s'" % x["partitioning"],
                                "expecting: on/off")

        if not x.has_key("partitioningStrategy"):
            raise QAdmException(QAdmException.MISSING_PARAM, "partitioningStrategy",
                                "(required if partitioning is on)")

        psFound = False
        for (psName, theOpts) in psOpts.items():
            if x["partitioningStrategy"] == psName:
                psFound = True
                # check if all required options are specified
                for o in theOpts:
                    if not x.has_key(o):
                        raise QAdmException(QAdmException.MISSING_PARAM, o)

                # check if there are any unrecognized options
                for o in x:
                    if not ((o in xxOpts[whichInfo]) or (o in theOpts)):
                        # skip non required, these are not in xxOpts/theOpts
                        if whichInfo=="db_info" and o=="clusteredIndex":
                            continue
                        if whichInfo=="db_info" and o=="objIdIndex":
                            continue
                        if whichInfo=="table_info" and o=="partitioningStrategy":
                            continue
                        raise QAdmException(QAdmException.WRONG_PARAM, o)
        if not psFound:
            raise QAdmException(QAdmException.WRONG_PARAM,x["partitioningStrategy"])

    def _initLogging(self):
        self._logger = logging.getLogger("QADM")
        kL = os.getenv('KAZOO_LOGGING')
        if kL: logging.getLogger("kazoo.client").setLevel(int(kL))

####################################################################################
<<<<<<< HEAD
class VolcabCompleter:
    """
    Set auto-completion for commonly used words.
    """
    def __init__(self, volcab):
        self.volcab = volcab

    def complete(self, text, state):
        results = [x+' ' for x in self.volcab 
=======
class WordCompleter:
    """
    Set auto-completion for commonly used words.
    """
    def __init__(self, word):
        self.word = word

    def complete(self, text, state):
        results = [x+' ' for x in self.word 
>>>>>>> u/jbecla/cssProto5
                   if x.startswith(text.upper())] + [None]
        return results[state]

readline.parse_and_bind("tab: complete")
words = ['CONFIG',
         'CREATE',
         'DATABASE',
         'DATABASES',
         'DROP',
         'DUMP',
         'INTO',
         'LIKE',
         'LOAD',
         'RELEASE',
         'SHOW',
         'TABLE']
<<<<<<< HEAD
completer = VolcabCompleter(words)
=======
completer = WordCompleter(words)
>>>>>>> u/jbecla/cssProto5
readline.set_completer(completer.complete)

####################################################################################
class SimpleOptionParser:
    """
    Parse command line options.
    """

    def __init__(self):
        self._verbosityT = 40 # default is ERROR
        self._logFileName = None
        self._connInfo = '127.0.0.1:2181' # default for kazoo (single node, local)
        self._usage = \
"""

NAME
        qserv_admin - the client program for Central State System (CSS)

SYNOPSIS
        qserv_admin [OPTIONS]

OPTIONS
   -v
        Verbosity threshold. Logging messages which are less severe than
        provided will be ignored. Expected value range: 0=50: (CRITICAL=50,
        ERROR=40, WARNING=30, INFO=20, DEBUG=10). Default value is ERROR.
   -f
        Name of the output log file. If not specified, the output goes to stderr.
   -c
        Connection information.
"""

    def getVerbosityT(self):
        """
        Return verbosity threshold.
        """
        return self._verbosityT

    def getLogFileName(self):
        """
        Return the name of the output log file.
        """
        return self._logFileName

    def getConnInfo(self):
        """
        Return connection information. 
        """
        return self._connInfo

    def parse(self):
        """
        Parse options.
        """
        parser = OptionParser(usage=self._usage)
        parser.add_option("-v", dest="verbT")
        parser.add_option("-f", dest="logF")
        parser.add_option("-c", dest="connI")
        (options, args) = parser.parse_args()
        if options.verbT: 
            self._verbosityT = int(options.verbT)
            if   self._verbosityT > 50: self._verbosityT = 50
            elif self._verbosityT <  0: self._verbosityT = 0
        if options.logF:
            self._logFileName = options.logF
        if options.connI:
            self._connInfo = options.connI

####################################################################################
def main():
    # parse arguments
    p = SimpleOptionParser()
    p.parse()

    # configure logging
    if p.getLogFileName():
        logging.basicConfig(
            filename=p.getLogFileName(),
            format='%(asctime)s %(name)s %(levelname)s: %(message)s', 
            datefmt='%m/%d/%Y %I:%M:%S', 
            level=p.getVerbosityT())
    else:
        logging.basicConfig(
            format='%(asctime)s %(name)s %(levelname)s: %(message)s', 
            datefmt='%m/%d/%Y %I:%M:%S', 
            level=p.getVerbosityT())

    # wait for commands and process
    try:
        CommandParser(p.getConnInfo()).receiveCommands()
    except(KeyboardInterrupt, SystemExit, EOFError):
        print ""

if __name__ == "__main__":
    main()
