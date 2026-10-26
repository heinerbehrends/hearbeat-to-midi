#include "ParamNvs.h"

#include <cmath>

// Not a parameter name: those are snake_case, this has no underscore
static const char *const SCHEMA_KEY = "schema";

bool ParamNvs::begin(const char *ns)
{
  open_ = prefs_.begin(ns, false);
  return open_;
}

void ParamNvs::end()
{
  if (open_)
    prefs_.end();
  open_ = false;
}

bool ParamNvs::load(const char *key, float *value)
{
  // isKey first: getFloat on a missing key logs an error
  if (!open_ || !prefs_.isKey(key))
    return false;
  float v = prefs_.getFloat(key, NAN); // NAN also when stored with another type
  if (std::isnan(v))
    return false;
  *value = v;
  return true;
}

bool ParamNvs::store(const char *key, float value)
{
  // NVS skips writing an unchanged value, so saving often doesn't wear the flash
  return open_ && prefs_.putFloat(key, value) == sizeof(float);
}

bool ParamNvs::remove(const char *key)
{
  return open_ && (!prefs_.isKey(key) || prefs_.remove(key));
}

int ParamNvs::schema()
{
  if (!open_ || !prefs_.isKey(SCHEMA_KEY))
    return -1;
  return prefs_.getUShort(SCHEMA_KEY, 0);
}

bool ParamNvs::setSchema(int version)
{
  return open_ && prefs_.putUShort(SCHEMA_KEY, (uint16_t)version) == sizeof(uint16_t);
}

bool ParamNvs::clear()
{
  return open_ && prefs_.clear();
}
