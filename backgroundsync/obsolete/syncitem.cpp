#include "stdafx.h"
#include <syncitem.h>

using std::string;

//########################################################################################################
SyncItem::SyncItem(const std::string& type, const std::string& s, const std::string& t, const std::string& w, const Exclusions& e, bool delAftercopy,
                   bool twoWaySync, bool cwtn, bool fsp, bool enabled, const CompareItems& ci)
    :  type_(type)
    , source_(s)
    , target_(t)
    , wildcard_(w)
	, exclusions_(e)
    , flags_(delAftercopy, twoWaySync, cwtn, fsp, enabled)
    , compareItems_(ci)
{}

SyncItem::SyncItem(const std::string& type, const std::string& s, const std::string& t, const std::string& w, const Exclusions& e,
         const SyncFlags& syncFlags, const CompareItems& ci)
    :  type_(type)
    , source_(s)
    , target_(t)
    , wildcard_(w)
	, exclusions_(e)
    , flags_(syncFlags)
    , compareItems_(ci)
{}

string SyncItem::typeStr() const
{
    return type_;
}

