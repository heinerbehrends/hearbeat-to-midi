-- Heart Rate To MIDI: make REAPER follow the device's tempo.
--
-- REAPER doesn't take its tempo from incoming MIDI clock, so the device also
-- sends its tempo as a 14-bit CC pair (bpm x 64, CC 20 MSB / CC 52 LSB, channel
-- 16; TEMPO_CC in include/Settings.h). This script sets the project's first tempo
-- marker from it. Setup, running, messages and limitations: README.md next to
-- this script. Needs REAPER 6.41 or newer (MIDI_GetRecentInputEvent).

local CHANNEL = 16              -- 1-16, TEMPO_CC_CHANNEL in Settings.h
local CC_MSB = 20               -- TEMPO_CC in Settings.h
local CC_LSB = CC_MSB + 32
local DEVICE_NAME = "Heart Rate To MIDI" -- only listen to this input (nil = any input)
local MIN_CHANGE_BPM = 0.05     -- ignore smaller differences (the device resolves 1/64 bpm)
local SILENCE_WARNING_S = 5     -- say so if no tempo has arrived after this long

local STATUS = 0xB0 + CHANNEL - 1
local lastSeq = nil
local msb = nil
local started = reaper.time_precise()
local heardTempo = false
local warnedSilence = false
local warnedMarkers = false

local function log(msg)
  reaper.ShowConsoleMsg("Heart Rate To MIDI: " .. msg .. "\n")
end

local function fromOurDevice(devIdx)
  if not DEVICE_NAME then return true end
  local ok, name = reaper.GetMIDIInputName(devIdx & 0xFFFF, "")
  return ok and name:find(DEVICE_NAME, 1, true) ~= nil
end

local function setTempo(bpm)
  local proj = 0
  local count = reaper.CountTempoTimeSigMarkers(proj)
  if count == 0 then
    -- No marker yet: create one at the project start
    reaper.SetTempoTimeSigMarker(proj, -1, 0, -1, -1, bpm, 0, 0, false)
  else
    -- Compare with the marker's actual tempo rather than what we last set, so a
    -- manual edit, an undo or a project switch is corrected by the next resend
    local _, timepos, _, _, current, num, denom, linear = reaper.GetTempoTimeSigMarker(proj, 0)
    if math.abs(current - bpm) < MIN_CHANGE_BPM then return end
    reaper.SetTempoTimeSigMarker(proj, 0, timepos, -1, -1, bpm, num, denom, linear)
    if count > 1 and not warnedMarkers then
      log("the project has " .. count .. " tempo markers; only the first follows the heart rate")
      warnedMarkers = true
    end
  end
  reaper.UpdateTimeline()
end

local function handle(buf)
  local cc, value = buf:byte(2), buf:byte(3)
  if cc == CC_MSB then
    msb = value
  elseif cc == CC_LSB and msb then
    local bpm = (msb * 128 + value) / 64
    msb = nil
    if bpm >= 20 then
      if not heardTempo then
        log(string.format("following the device, %.2f bpm", bpm))
        heardTempo = true
      end
      setTempo(bpm)
    end
  end
end

local function poll()
  -- idx 0 latches REAPER's input history and returns the newest event; walk back
  -- from there to the last event we handled
  local newest, buf0, _, dev0 = reaper.MIDI_GetRecentInputEvent(0)
  if lastSeq == nil then
    lastSeq = newest -- first run: skip the old history, the device resends every 2 s
  elseif newest ~= 0 and newest ~= lastSeq then
    local pending = {}
    local seq, buf, devIdx, idx = newest, buf0, dev0, 0
    while seq ~= 0 and seq > lastSeq and idx < 256 do
      -- check the cheap status byte first: most traffic is clock (0xF8)
      if #buf == 3 and buf:byte(1) == STATUS and fromOurDevice(devIdx) then
        pending[#pending + 1] = buf
      end
      idx = idx + 1
      seq, buf, _, devIdx = reaper.MIDI_GetRecentInputEvent(idx)
    end
    for i = #pending, 1, -1 do handle(pending[i]) end -- oldest first
    lastSeq = newest
  end

  if not heardTempo and not warnedSilence and reaper.time_precise() - started > SILENCE_WARNING_S then
    log("no tempo received yet. Is \"" .. tostring(DEVICE_NAME) .. "\" enabled as a MIDI input, "
      .. "and is the device connected? (The name must match; set DEVICE_NAME = nil to accept any input.)")
    warnedSilence = true
  end
  reaper.defer(poll)
end

-- Toolbar toggle: re-running ends the running instance without a prompt
if reaper.set_action_options then
  reaper.set_action_options(1 | 4)
  reaper.atexit(function() reaper.set_action_options(8) end)
end

poll()
