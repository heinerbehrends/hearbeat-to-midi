# Settings page

`index.html` is a settings page for the device: it changes the runtime settings (see the main README,
"Settings at runtime") over the USB serial port, from the browser, without a terminal. It is one static
file (HTML + JavaScript, no build step, no external resources), so it also works offline.

## What it does

- **Connect** picks the serial port and reads every setting with `get`. The form is built from that
  listing, so settings added by newer firmware show up by themselves (unknown ones under "Other").
- Changing a field sends `set <name> <value>` at once. If the device refuses it, the reason is shown next
  to the field and the field goes back to the device's value. **Default** sends `reset <name>`.
- **Unsaved changes**: a change applies at once but is lost on a restart until you press **Save**
  (`save`). Fields changed since the last save are marked with a dot and show their saved value.
- **Restore all defaults** (`reset`) asks first, and offers "Restore and save" to make it permanent.
- Changes from elsewhere (a DAW's MIDI CC, a `set` typed in another way) arrive as `param:` lines and
  update the form; they also count as unsaved.
- **Live** shows heart rate, target, tempo, clock tempo, state, transport, feedback and rate switch, the
  last sensor reading and MIDI drops, with a 2-minute chart, plus the serial log and a command box for
  any command (`help`, `get step_beats`, ...).
- If the device restarts while connected (it prints `params: … loaded`), the page reloads the values.

**Try without a device** (or open the page with `?demo`) simulates a device with the same commands,
rules and log lines, so the page can be explored and tested without hardware.

## Requirements

- **Chrome or Edge on a desktop computer** (Windows, macOS, Linux, ChromeOS). Firefox and Safari have
  no Web Serial; neither do phones.
- **Only one program can use the port**: close the PlatformIO serial monitor (and anything else using
  the port) before connecting, and disconnect the page before uploading firmware.
- The page must come from `https://` or `http://localhost`. Opening `index.html` directly from disk
  (`file://`) should also work in Chrome and Edge (they treat local files as secure); if **Connect** is
  greyed out that way, use one of the ways below.

## Opening it

- **Online**: <https://heinerbehrends.github.io/hearbeat-to-midi/> (GitHub Pages, deployed from branch
  `main`, folder `/docs`; note the repository's spelling "hearbeat").
- **Locally**: in this folder run `python -m http.server` and open <http://localhost:8000>
  (add `?demo` for the simulated device).

## Which port

| Board | Port to choose |
| --- | --- |
| DevKit (two USB-C ports) | the **UART** port with the CH343 chip, VID:PID `1A86:55D3` (often "USB Single Serial"). The other port (native USB) is MIDI only. |
| XIAO ESP32-S3 | its only USB port (USB CDC, next to MIDI) |

After opening the port the page releases DTR and RTS: on the DevKit they drive the auto-reset circuit
and would hold the board in reset. Opening the port can still restart the DevKit once; the page waits
and retries `get` until the device answers. On the XIAO (USB CDC) DTR may need to be set before
serial data arrives: if nothing arrives within 2 s the page offers a **Set DTR** checkbox.
