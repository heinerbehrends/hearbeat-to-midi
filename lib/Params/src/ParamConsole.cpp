#include "ParamConsole.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

using namespace params;

namespace
{

// Splits off the next space-separated word; returns nullptr when there is none.
char *nextWord(char **rest)
{
  char *p = *rest;
  while (*p == ' ' || *p == '\t')
    p++;
  if (!*p)
    return nullptr;
  char *word = p;
  while (*p && *p != ' ' && *p != '\t')
    p++;
  if (*p)
    *p++ = 0;
  *rest = p;
  return word;
}

const char *const HELP[] = {
    "help                list the commands",
    "get                 list all parameters: param <name> <type> <value> min= max= default= \"help\"",
    "get <name>          show one parameter",
    "set <name> <value>  change a value; applies at once, not saved",
    "save                store the current values, so they apply after a restart",
    "reset               all values back to the Settings.h defaults, and saved",
    "reset <name>        one value back to its default (not saved)",
};

} // namespace

void ParamConsole::feed(char c)
{
  if (c == '\r' || c == '\n')
  {
    if (overflow_)
      reply("err line too long (max %u characters)", (unsigned)kMaxLine);
    else if (len_ > 0)
    {
      line_[len_] = 0;
      handleLine(line_);
    }
    len_ = 0;
    overflow_ = false;
    return;
  }
  if (len_ < kMaxLine)
    line_[len_++] = c;
  else
    overflow_ = true;
}

void ParamConsole::reply(const char *fmt, ...)
{
  char buf[200];
  va_list args;
  va_start(args, fmt);
  std::vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  if (out_)
    out_(buf);
}

void ParamConsole::handleLine(const char *line)
{
  char buf[kMaxLine + 1];
  std::strncpy(buf, line, kMaxLine);
  buf[kMaxLine] = 0;
  char *rest = buf;
  char *cmd = nextWord(&rest);
  if (!cmd)
    return; // blank line
  char *a = nextWord(&rest);
  char *b = nextWord(&rest);
  char *extra = nextWord(&rest);

  if (std::strcmp(cmd, "help") == 0 && !a)
    help();
  else if (std::strcmp(cmd, "get") == 0 && !b)
    get(a);
  else if (std::strcmp(cmd, "set") == 0 && b && !extra)
    set(a, b);
  else if (std::strcmp(cmd, "save") == 0 && !a)
    save();
  else if (std::strcmp(cmd, "reset") == 0 && !b)
    reset(a);
  else if (std::strcmp(cmd, "help") == 0 || std::strcmp(cmd, "get") == 0 || std::strcmp(cmd, "set") == 0 ||
           std::strcmp(cmd, "save") == 0 || std::strcmp(cmd, "reset") == 0)
    reply("err %s: wrong arguments (try help)", cmd);
  else
    reply("err unknown command %s (try help)", cmd);
}

void ParamConsole::help()
{
  for (const char *h : HELP)
    reply("help %s", h);
  reply("ok help");
}

void ParamConsole::printParam(int id)
{
  const Def &d = def(id);
  char v[24], lo[24], hi[24], dv[24];
  params_.format(id, params_.get(id), v, sizeof(v));
  params_.format(id, d.min, lo, sizeof(lo));
  params_.format(id, d.max, hi, sizeof(hi));
  params_.format(id, d.def, dv, sizeof(dv));
  reply("param %s %s %s min=%s max=%s default=%s \"%s\"", d.name, typeName(d.type), v, lo, hi, dv, d.help);
}

int ParamConsole::findOrComplain(const char *name)
{
  int id = find(name);
  if (id < 0)
    reply("err unknown parameter %s (try get)", name);
  return id;
}

void ParamConsole::get(const char *name)
{
  if (!name)
  {
    for (int i = 0; i < COUNT; i++)
      printParam(i);
    reply("ok get count=%d", (int)COUNT);
    return;
  }
  int id = findOrComplain(name);
  if (id < 0)
    return;
  printParam(id);
  reply("ok get %s", name);
}

bool ParamConsole::apply(int id, float value)
{
  float old = params_.get(id);
  if (!params_.set(id, value))
  {
    reply("err %s", params_.error());
    return false;
  }
  char v[24];
  params_.format(id, value, v, sizeof(v));
  reply("ok %s=%s", def(id).name, v);
  if (value != old && changed_)
    changed_(id, old, value);
  return true;
}

void ParamConsole::set(const char *name, const char *value)
{
  int id = findOrComplain(name);
  if (id < 0)
    return;
  float x;
  if (!params_.parse(id, value, &x))
  {
    reply("err %s", params_.error());
    return;
  }
  apply(id, x);
}

void ParamConsole::reset(const char *name)
{
  if (name)
  {
    int id = findOrComplain(name);
    if (id >= 0)
      apply(id, def(id).def);
    return;
  }
  float old[COUNT];
  for (int i = 0; i < COUNT; i++)
    old[i] = params_.get(i);
  params_.resetAll();
  bool saved = store_.clear() && params_.save(store_);
  for (int i = 0; i < COUNT; i++)
    if (old[i] != params_.get(i) && changed_)
      changed_(i, old[i], params_.get(i));
  if (saved)
    reply("ok reset");
  else
    reply("err reset: defaults apply, but saving them failed");
}

void ParamConsole::save()
{
  if (params_.save(store_))
    reply("ok saved");
  else
    reply("err save failed");
}
