// ParamStore on the ESP32's NVS (Arduino Preferences), for the runtime settings.
#pragma once
#include <Preferences.h>
#include <Params.h>

class ParamNvs : public ParamStore
{
public:
  // Opens the namespace (max 15 characters); false if NVS is unusable, in
  // which case every call fails and the defaults apply.
  bool begin(const char *ns);
  void end();

  bool load(const char *key, float *value) override;
  bool store(const char *key, float value) override;
  bool remove(const char *key) override;
  int schema() override;
  bool setSchema(int version) override;
  bool clear() override;

private:
  Preferences prefs_;
  bool open_ = false;
};
