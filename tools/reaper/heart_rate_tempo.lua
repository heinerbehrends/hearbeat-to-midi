-- Heart Rate To MIDI: make REAPER follow the device's tempo.
--
-- REAPER doesn't take its tempo from incoming MIDI clock, so the device also
-- sends its tempo as a 14-bit CC pair: value = bpm x 64, MSB on CC 20 and LSB on
-- CC 52, channel 16 (see TEMPO_CC in include/Settings.h). This script reads those
-- messages and sets the tempo of the project's first tempo marker.
--
-- Setup (once):
--   * Preferences > MIDI Devices: enable "Heart Rate To MIDI" as input
--     ("control only" is enough).
--   * Disable audio time stretching for items that should not follow the tempo.
--   * For start/stop: right-click Play > external timecode synchronization,
--     SPP from that input; the device's pedal / BOOT button sends SPP + Start/Stop.
-- Run: Actions > Load ReaScript, then run it. It keeps running in the
-- background; run it again (or terminate it when REAPER asks) to stop.
-- Needs REAPER 6.41 or newer (MIDI_GetRecentInputEvent).

local CHANNEL = 16              -- 1-16, TEMPO_CC_CHANNEL in Settings.h
local CC_MSB = 20               -- TEMPO_CC in Settings.h
local CC_LSB = CC_MSB + 32
local DEVICE_NAME = "Heart Rate To MIDI" -- only listen to this input (nil = any input)
local MIN_CHANGE_BPM = 0.01     -- ignore smaller changes

local STATUS = 0xB0 + CHANNEL - 1
local lastSeq = nil
local msb = nil
local appliedBpm = nil

local function fromOurDevice(devIdx)
  if not DEVICE_NAME then return true end
  local ok, name = reaper.GetMIDIInputName(devIdx & 0xFFFF, "")
  return ok and name:find(DEVICE_NAME, 1, true) ~= nil
end

local function setTempo(bpm)
  local proj = 0
  if reaper.CountTempoTimeSigMarkers(proj) == 0 then
    -- No marker yet: create one at the project start
    reaper.SetTempoTimeSigMarker(proj, -1, 0, -1, -1, bpm, 0, 0, false)
  else
    -- Change only the tempo of the first marker, keep its position and signature
    local _, timepos, measurepos, beatpos, _, num, denom, linear = reaper.GetTempoTimeSigMarker(proj, 0)
    reaper.SetTempoTimeSigMarker(proj, 0, timepos, measurepos, beatpos, bpm, num, denom, linear)
  end
  reaper.UpdateTimeline()
end

local function handle(buf)
  if #buf ~= 3 or buf:byte(1) ~= STATUS then return end
  local cc, value = buf:byte(2), buf:byte(3)
  if cc == CC_MSB then
    msb = value
  elseif cc == CC_LSB and msb then
    local bpm = (msb * 128 + value) / 64
    msb = nil
    if bpm >= 20 and (not appliedBpm or math.abs(bpm - appliedBpm) >= MIN_CHANGE_BPM) then
      setTempo(bpm)
      appliedBpm = bpm
    end
  end
end

local function poll()
  -- idx 0 latches the newest event; walk back to the last one we handled
  local newest = reaper.MIDI_GetRecentInputEvent(0)
  if lastSeq == nil then
    -- First run: skip the old history, the device resends its tempo every 2 s
    lastSeq = newest
  elseif newest ~= 0 and newest ~= lastSeq then
    local pending = {}
    local idx = 0
    while idx < 256 do
      local seq, buf, _, devIdx = reaper.MIDI_GetRecentInputEvent(idx)
      if seq == 0 or seq == lastSeq then break end
      if fromOurDevice(devIdx) then pending[#pending + 1] = buf end
      idx = idx + 1
    end
    for i = #pending, 1, -1 do handle(pending[i]) end -- oldest first
    lastSeq = newest
  end
  reaper.defer(poll)
end

poll()
