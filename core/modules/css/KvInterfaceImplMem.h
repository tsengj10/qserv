// -*- LSST-C++ -*-
/*
 * LSST Data Management System
 * Copyright 2014 LSST Corporation.
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

/**
  * @file
  *
  * @brief Interface to the Common State System - in memory key-value based
  * implementation.
  *
  * @Author Jacek Becla, SLAC
  */

#ifndef LSST_QSERV_KVINTERFACE_IMPL_MEM_HH
#define LSST_QSERV_KVINTERFACE_IMPL_MEM_HH

// System headers
#include <map>
#include <string>
#include <vector>

// Local headers
#include "css/KvInterface.h"


namespace lsst {
namespace qserv {
namespace css {

class KvInterfaceImplMem : public KvInterface {
public:
    KvInterfaceImplMem() {}
    KvInterfaceImplMem(std::istream& mapStream);
    virtual ~KvInterfaceImplMem();

    virtual void create(std::string const& key, std::string const& value);
    virtual bool exists(std::string const& key);
    virtual std::string get(std::string const& key);
    virtual std::string get(std::string const& key,
                            std::string const& defaultValue);
    virtual std::vector<std::string> getChildren(std::string const& key);
    virtual void deleteKey(std::string const& key);

private:
    std::map<std::string, std::string> _kvMap;
};

}}} // namespace lsst::qserv::css

#endif // LSST_QSERV_CSS_INTERFACE_IMPL_MEM_HH