// Line-based commands for the runtime settings, for a serial terminal now and a
// Web Serial settings page later. Pure C++, tested on the board.
//
//   help                 list the commands
//   get                  list all parameters
//   get <name>           one parameter
//   set <name> <value>   change a value (applies at once, not saved)
//   save                 store the current values (NVS)
//   reset                all values back to the Settings.h defaults, and saved
//   reset <name>         one value back to its default (not saved)
//
// Every command answers with zero or more info lines and then exactly one
// "ok ..." or "err ..." line, so a program can wait for that line:
//
//   param step_beats int 16 min=1 max=64 default=16 "stepped mode: ..."
//   help set <name> <value>   change a value (applies at once, not saved)
//   ok step_beats=32
//   err step_beats: must be a power of two 1..64
//
// No reply starts with '>', which marks the serial-plotter log on the same port.
#pragma once
#include <cstddef>
#include <functional>
#include "Params.h"

class ParamConsole
{
public:
  // Called with each reply line (no line ending).
  using Output = std::function<void(const char *line)>;
  // Called after a value changed (by set or reset), with old and new value.
  using Changed = std::function<void(int id, float oldValue, float newValue)>;

  static const size_t kMaxLine = 96;

  ParamConsole(Params &params, ParamStore &store, Output out, Changed changed = nullptr)
      : params_(params), store_(store), out_(out), changed_(changed) {}

  // Feed received characters one at a time; a line ends with \r or \n (\r\n counts once).
  void feed(char c);
  // Run one command line.
  void handleLine(const char *line);

private:
  void reply(const char *fmt, ...);
  void printParam(int id);
  void help();
  void get(const char *name);
  void set(const char *name, const char *value);
  void reset(const char *name);
  void save();
  // Applies a validated value to params_ and reports the change.
  bool apply(int id, float value);
  int findOrComplain(const char *name);

  Params &params_;
  ParamStore &store_;
  Output out_;
  Changed changed_;
  char line_[kMaxLine + 1] = "";
  size_t len_ = 0;
  bool overflow_ = false;
};
