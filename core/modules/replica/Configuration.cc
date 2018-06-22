/*
 * LSST Data Management System
 * Copyright 2017 LSST Corporation.
 *
 * This product includes software developed by the
 * LSST Project (http://www.lsst.org/).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the LSST License Statement and
 * the GNU General Public License along with this program.  If not,
 * see <http://www.lsstcorp.org/LegalNotices/>.
 */

// Class header
#include "replica/Configuration.h"

// System headers
#include <set>
#include <stdexcept>

// Qserv headers
#include "lsst/log/Log.h"
#include "replica/ChunkNumber.h"
#include "replica/ConfigurationFile.h"
#include "replica/ConfigurationMap.h"
#include "replica/ConfigurationMySQL.h"
#include "replica/FileUtils.h"
#include "util/IterableFormatter.h"

namespace {

LOG_LOGGER _log = LOG_GET("lsst.qserv.replica.Configuration");

} // namespace

namespace lsst {
namespace qserv {
namespace replica {

std::ostream& operator <<(std::ostream& os, WorkerInfo const& info) {
    os  << "WorkerInfo ("
        << "name:'"      <<      info.name       << "',"
        << "isEnabled:"  << (int)info.isEnabled  << ","
        << "isReadOnly:" << (int)info.isReadOnly << ","
        << "svcHost:'"   <<      info.svcHost    << "',"
        << "svcPort:"    <<      info.svcPort    << ","
        << "fsHost:'"    <<      info.fsHost     << "',"
        << "fsPort:"     <<      info.fsPort     << ","
        << "dataDir:'"   <<      info.dataDir    << "')";
    return os;
}

std::ostream& operator <<(std::ostream& os, DatabaseInfo const& info) {
    os  << "DatabaseInfo ("
        << "name:'" << info.name << "',"
        << "family:'" << info.family << "',"
        << "partitionedTables:" << util::printable(info.partitionedTables) << ","
        << "regularTables:" << util::printable(info.regularTables) << ")";
    return os;
}

std::ostream& operator <<(std::ostream& os, DatabaseFamilyInfo const& info) {
    os  << "DatabaseFamilyInfo ("
        << "name:'" << info.name << "',"
        << "replicationLevel:'" << info.replicationLevel << "',"
        << "numStripes:" << info.numStripes << ","
        << "numSubStripes:" << info.numSubStripes << ")";
    return os;
}

Configuration::Ptr Configuration::load(std::string const& configUrl) {

    std::string::size_type const pos = configUrl.find(':');
    if (pos != std::string::npos) {

        std::string const prefix = configUrl.substr(0, pos);
        std::string const suffix = configUrl.substr(pos+1);

        if ("file"  == prefix) {
            return std::make_shared<ConfigurationFile>(suffix);

        } else if ("mysql" == prefix) {
            return std::make_shared<ConfigurationMySQL>(
                database::mysql::ConnectionParams::parse(
                    configUrl,
                    Configuration::defaultDatabaseHost,
                    Configuration::defaultDatabasePort,
                    Configuration::defaultDatabaseUser,
                    Configuration::defaultDatabasePassword
                )
            );
        }
    }
    throw std::invalid_argument(
            "Configuration::load:  configUrl must start with 'file:' or 'mysql:'");
}

Configuration::Ptr Configuration::load(std::map<std::string, std::string> const& kvMap) {
    return std::make_shared<ConfigurationMap>(kvMap);
}

// Set some reasonable defaults

size_t       const Configuration::defaultRequestBufferSizeBytes      (1024);
unsigned int const Configuration::defaultRetryTimeoutSec             (1);
size_t       const Configuration::defaultControllerThreads           (1);
uint16_t     const Configuration::defaultControllerHttpPort          (80);
size_t       const Configuration::defaultControllerHttpThreads       (1);
unsigned int const Configuration::defaultControllerRequestTimeoutSec (3600);
unsigned int const Configuration::defaultJobTimeoutSec               (6000);
unsigned int const Configuration::defaultJobHeartbeatTimeoutSec      (60);
bool         const Configuration::defaultXrootdAutoNotify            (false);
std::string  const Configuration::defaultXrootdHost                  ("localhost");
uint16_t     const Configuration::defaultXrootdPort                  (1094);
unsigned int const Configuration::defaultXrootdTimeoutSec            (3600);
std::string  const Configuration::defaultWorkerTechnology            ("TEST");
size_t       const Configuration::defaultWorkerNumProcessingThreads  (1);
size_t       const Configuration::defaultFsNumProcessingThreads      (1);
size_t       const Configuration::defaultWorkerFsBufferSizeBytes     (1048576);
std::string  const Configuration::defaultWorkerSvcHost               ("localhost");
uint16_t     const Configuration::defaultWorkerSvcPort               (50000);
std::string  const Configuration::defaultWorkerFsHost                ("localhost");
uint16_t     const Configuration::defaultWorkerFsPort                (50001);
std::string  const Configuration::defaultDataDir                     ("{worker}");
std::string  const Configuration::defaultDatabaseTechnology          ("mysql");
std::string  const Configuration::defaultDatabaseHost                ("localhost");
uint16_t     const Configuration::defaultDatabasePort                (3306);
std::string  const Configuration::defaultDatabaseUser                (FileUtils::getEffectiveUser());
std::string  const Configuration::defaultDatabasePassword            ("");
std::string  const Configuration::defaultDatabaseName                ("replica");
size_t       const Configuration::defaultReplicationLevel            (1);
unsigned int const Configuration::defaultNumStripes                  (340);
unsigned int const Configuration::defaultNumSubStripes               (12);

void Configuration::translateDataDir(std::string&       dataDir,
                                     std::string const& workerName) {

    std::string::size_type const leftPos = dataDir.find('{');
    if (leftPos == std::string::npos) return;

    std::string::size_type const rightPos = dataDir.find('}');
    if (rightPos == std::string::npos) return;

    if (rightPos <= leftPos) {
        throw std::invalid_argument(
                "Configuration::translateDataDir  misformed template in the data directory path: '" +
                dataDir + "'");
    }
    if (dataDir.substr (leftPos, rightPos - leftPos + 1) == "{worker}") {
        dataDir.replace(leftPos, rightPos - leftPos + 1, workerName);
    }
}

Configuration::Configuration()
    :   _requestBufferSizeBytes     (defaultRequestBufferSizeBytes),
        _retryTimeoutSec            (defaultRetryTimeoutSec),
        _controllerThreads          (defaultControllerThreads),
        _controllerHttpPort         (defaultControllerHttpPort),
        _controllerHttpThreads      (defaultControllerHttpThreads),
        _controllerRequestTimeoutSec(defaultControllerRequestTimeoutSec),
        _jobTimeoutSec              (defaultJobTimeoutSec),
        _jobHeartbeatTimeoutSec     (defaultJobHeartbeatTimeoutSec),
        _xrootdAutoNotify           (defaultXrootdAutoNotify),
        _xrootdHost                 (defaultXrootdHost),
        _xrootdPort                 (defaultXrootdPort),
        _xrootdTimeoutSec           (defaultXrootdTimeoutSec),
        _workerTechnology           (defaultWorkerTechnology),
        _workerNumProcessingThreads (defaultWorkerNumProcessingThreads),
        _fsNumProcessingThreads     (defaultFsNumProcessingThreads),
        _workerFsBufferSizeBytes    (defaultWorkerFsBufferSizeBytes),
        _databaseTechnology         (defaultDatabaseTechnology),
        _databaseHost               (defaultDatabaseHost),
        _databasePort               (defaultDatabasePort),
        _databaseUser               (defaultDatabaseUser),
        _databasePassword           (defaultDatabasePassword),
        _databaseName               (defaultDatabaseName) {
}

std::string Configuration::context() const {
    static std::string const str = "CONFIG   ";
    return str;
}

std::vector<std::string> Configuration::workers(bool isEnabled,
                                                bool isReadOnly) const {

    util::Lock lock(_mtx, context() + "workers");

    std::vector<std::string> names;
    for (auto&& entry: _workerInfo) {
        auto const& name = entry.first;
        auto const& info = entry.second;
        if (isEnabled) {
            if (info.isEnabled and (isReadOnly == info.isReadOnly)) {
                names.push_back(name);
            }
        } else {
            if (not info.isEnabled) {
                names.push_back(name);
            }
        }
    }
    return names;
}

std::vector<std::string> Configuration::databaseFamilies() const {

    util::Lock lock(_mtx, context() + "databaseFamilies");

    std::set<std::string> familyNames;
    for (auto&& elem: _databaseInfo) {
        familyNames.insert(elem.second.family);
    }
    std::vector<std::string> families;
    for (auto&& name: familyNames) {
        families.push_back(name);
    }
    return families;
}

bool Configuration::isKnownDatabaseFamily(std::string const& name) const {

    util::Lock lock(_mtx, context() + "isKnownDatabaseFamily");

    return _databaseFamilyInfo.count(name);
}

size_t Configuration::replicationLevel(std::string const& family) const {

    util::Lock lock(_mtx, context() + "databaseFamilies");

    auto const itr = _databaseFamilyInfo.find(family);
    if (itr == _databaseFamilyInfo.end()) {
        throw std::invalid_argument(
                "Configuration::replicationLevel  unknown database family: '" +
                family + "'");
    }
    return itr->second.replicationLevel;
}

DatabaseFamilyInfo const Configuration::databaseFamilyInfo(std::string const& name) const {

    util::Lock lock(_mtx, context() + "databaseFamilyInfo");

    auto&& itr = _databaseFamilyInfo.find(name);
    if (itr == _databaseFamilyInfo.end()) {
        throw std::invalid_argument(
                "Configuration::databaseFamilyInfo  uknown database family: '" + name + "'");
    }
    return itr->second;
}

std::vector<std::string> Configuration::databases(std::string const& family) const {

    util::Lock lock(_mtx, context() + "databases(family)");

    if (not family.empty() and not _databaseFamilyInfo.count(family)) {
        throw std::invalid_argument(
                "Configuration::databases  unknown database family: '" +
                family + "'");
    }
    std::vector<std::string> names;
    for (auto&& entry: _databaseInfo) {
        if (not family.empty() and (family != entry.second.family)) {
            continue;
        }
        names.push_back(entry.first);
    }
    return names;
}

bool Configuration::isKnownWorker(std::string const& name) const {

    util::Lock lock(_mtx, context() + "isKnownWorker");

    return _workerInfo.count(name) > 0;
}

WorkerInfo const Configuration::workerInfo(std::string const& name) const {

    util::Lock lock(_mtx, context() + "workerInfo");

    auto const itr = _workerInfo.find(name);
    if (itr == _workerInfo.end()) {
        throw std::invalid_argument(
                "Configuration::workerInfo() uknown worker: '" + name + "'");
    }
    return itr->second;
}

bool Configuration::isKnownDatabase(std::string const& name) const {

    util::Lock lock(_mtx, context() + "isKnownDatabase");

    return _databaseInfo.count(name) > 0;
}

DatabaseInfo const Configuration::databaseInfo(std::string const& name) const {

    util::Lock lock(_mtx, context() + "databaseInfo");

    auto&& itr = _databaseInfo.find(name);
    if (itr == _databaseInfo.end()) {
        throw std::invalid_argument(
                "Configuration::databaseInfo() uknown database: '" + name + "'");
    }
    return itr->second;
}

void Configuration::dumpIntoLogger() {

    LOGS(_log, LOG_LVL_DEBUG, context() << "defaultRequestBufferSizeBytes:       " << defaultRequestBufferSizeBytes);
    LOGS(_log, LOG_LVL_DEBUG, context() << "defaultRetryTimeoutSec:              " << defaultRetryTimeoutSec);
    LOGS(_log, LOG_LVL_DEBUG, context() << "defaultControllerThreads:            " << defaultControllerThreads);
    LOGS(_log, LOG_LVL_DEBUG, context() << "defaultControllerHttpPort:           " << defaultControllerHttpPort);
    LOGS(_log, LOG_LVL_DEBUG, context() << "defaultControllerHttpThreads:        " << defaultControllerHttpThreads);
    LOGS(_log, LOG_LVL_DEBUG, context() << "defaultControllerRequestTimeoutSec:  " << defaultControllerRequestTimeoutSec);
    LOGS(_log, LOG_LVL_DEBUG, context() << "defaultJobTimeoutSec:                " << defaultJobTimeoutSec);
    LOGS(_log, LOG_LVL_DEBUG, context() << "defaultJobHeartbeatTimeoutSec:       " << defaultJobHeartbeatTimeoutSec);
    LOGS(_log, LOG_LVL_DEBUG, context() << "defaultXrootdAutoNotify:             " << (defaultXrootdAutoNotify ? "true" : "false"));
    LOGS(_log, LOG_LVL_DEBUG, context() << "defaultXrootdHost:                   " << defaultXrootdHost);
    LOGS(_log, LOG_LVL_DEBUG, context() << "defaultXrootdPort:                   " << defaultXrootdPort);
    LOGS(_log, LOG_LVL_DEBUG, context() << "defaultXrootdTimeoutSec:             " << defaultXrootdTimeoutSec);
    LOGS(_log, LOG_LVL_DEBUG, context() << "defaultWorkerTechnology:             " << defaultWorkerTechnology);
    LOGS(_log, LOG_LVL_DEBUG, context() << "defaultWorkerNumProcessingThreads:   " << defaultWorkerNumProcessingThreads);
    LOGS(_log, LOG_LVL_DEBUG, context() << "defaultFsNumProcessingThreads:       " << defaultFsNumProcessingThreads);
    LOGS(_log, LOG_LVL_DEBUG, context() << "defaultWorkerFsBufferSizeBytes:      " << defaultWorkerFsBufferSizeBytes);
    LOGS(_log, LOG_LVL_DEBUG, context() << "defaultWorkerSvcHost:                " << defaultWorkerSvcHost);
    LOGS(_log, LOG_LVL_DEBUG, context() << "defaultWorkerSvcPort:                " << defaultWorkerSvcPort);
    LOGS(_log, LOG_LVL_DEBUG, context() << "defaultWorkerFsHost:                 " << defaultWorkerFsHost);
    LOGS(_log, LOG_LVL_DEBUG, context() << "defaultWorkerFsPort:                 " << defaultWorkerFsPort);
    LOGS(_log, LOG_LVL_DEBUG, context() << "defaultDataDir:                      " << defaultDataDir);
    LOGS(_log, LOG_LVL_DEBUG, context() << "defaultDatabaseTechnology:           " << defaultDatabaseTechnology);
    LOGS(_log, LOG_LVL_DEBUG, context() << "defaultDatabaseHost:                 " << defaultDatabaseHost);
    LOGS(_log, LOG_LVL_DEBUG, context() << "defaultDatabasePort:                 " << defaultDatabasePort);
    LOGS(_log, LOG_LVL_DEBUG, context() << "defaultDatabaseUser:                 " << defaultDatabaseUser);
    LOGS(_log, LOG_LVL_DEBUG, context() << "defaultDatabasePassword:             " << "*****");
    LOGS(_log, LOG_LVL_DEBUG, context() << "defaultDatabaseName:                 " << defaultDatabaseName);
    LOGS(_log, LOG_LVL_DEBUG, context() << "defaultReplicationLevel:             " << defaultReplicationLevel);
    LOGS(_log, LOG_LVL_DEBUG, context() << "defaultNumStripes:                   " << defaultNumStripes);
    LOGS(_log, LOG_LVL_DEBUG, context() << "defaultNumSubStripes:                " << defaultNumSubStripes);
    LOGS(_log, LOG_LVL_DEBUG, context() << "_requestBufferSizeBytes:             " << _requestBufferSizeBytes);
    LOGS(_log, LOG_LVL_DEBUG, context() << "_retryTimeoutSec:                    " << _retryTimeoutSec);
    LOGS(_log, LOG_LVL_DEBUG, context() << "_controllerThreads:                  " << _controllerThreads);
    LOGS(_log, LOG_LVL_DEBUG, context() << "_controllerHttpPort:                 " << _controllerHttpPort);
    LOGS(_log, LOG_LVL_DEBUG, context() << "_controllerHttpThreads:              " << _controllerHttpThreads);
    LOGS(_log, LOG_LVL_DEBUG, context() << "_controllerRequestTimeoutSec:        " << _controllerRequestTimeoutSec);
    LOGS(_log, LOG_LVL_DEBUG, context() << "_jobTimeoutSec:                      " << _jobTimeoutSec);
    LOGS(_log, LOG_LVL_DEBUG, context() << "_jobHeartbeatTimeoutSec:             " << _jobHeartbeatTimeoutSec);
    LOGS(_log, LOG_LVL_DEBUG, context() << "_xrootdAutoNotify:                   " << (_xrootdAutoNotify ? "true" : "false"));
    LOGS(_log, LOG_LVL_DEBUG, context() << "_xrootdHost:                         " << _xrootdHost);
    LOGS(_log, LOG_LVL_DEBUG, context() << "_xrootdPort:                         " << _xrootdPort);
    LOGS(_log, LOG_LVL_DEBUG, context() << "_xrootdTimeoutSec:                   " << _xrootdTimeoutSec);
    LOGS(_log, LOG_LVL_DEBUG, context() << "_workerTechnology:                   " << _workerTechnology);
    LOGS(_log, LOG_LVL_DEBUG, context() << "_workerNumProcessingThreads:         " << _workerNumProcessingThreads);
    LOGS(_log, LOG_LVL_DEBUG, context() << "_fsNumProcessingThreads:             " << _fsNumProcessingThreads);
    LOGS(_log, LOG_LVL_DEBUG, context() << "_workerFsBufferSizeBytes:            " << _workerFsBufferSizeBytes);
    LOGS(_log, LOG_LVL_DEBUG, context() << "_databaseTechnology:                 " << _databaseTechnology);
    LOGS(_log, LOG_LVL_DEBUG, context() << "_databaseHost:                       " << _databaseHost);
    LOGS(_log, LOG_LVL_DEBUG, context() << "_databasePort:                       " << _databasePort);
    LOGS(_log, LOG_LVL_DEBUG, context() << "_databaseUser:                       " << _databaseUser);
    LOGS(_log, LOG_LVL_DEBUG, context() << "_databasePassword:                   " << "*****");
    LOGS(_log, LOG_LVL_DEBUG, context() << "_databaseName:                       " << _databaseName);
    for (auto&& elem: _workerInfo) {
        LOGS(_log, LOG_LVL_DEBUG, context() << elem.second);
    }
    for (auto&& elem: _databaseInfo) {
        LOGS(_log, LOG_LVL_DEBUG, context() << elem.second);
    }
    for (auto&& elem: _databaseFamilyInfo) {
        LOGS(_log, LOG_LVL_DEBUG, context()
             << "databaseFamilyInfo["<< elem.first << "]: " << elem.second);
    }
}

}}} // namespace lsst::qserv::replica
