# 017 – Manual test: web settings page

**Status:** todo
**Depends on:** 010, 011, the web page in `docs/`

In Chrome or Edge, with the PlatformIO serial monitor closed (only one program can use the port).

## Acceptance criteria

- [ ] [hw] Connect to the DevKit's UART port (CH343): the board isn't held in reset. If opening the port
      restarts it once, the page waits and retries `get` by itself, and the form loads without further
      action.
- [ ] [hw] Setting a value applies at once; an invalid value shows the error and restores the field.
- [ ] [hw] The "unsaved changes" indicator appears after a change and clears after Save.
- [ ] [hw] Saved values survive a restart of the device.
- [ ] [hw] Restore all defaults works (with confirmation) and needs Save to be permanent.
- [ ] [hw] The live panel shows heart rate, tempo and state while worn.
- [ ] [hw] A CC sent from the DAW updates the matching field on the page.
- [ ] [hw] XIAO: the same over its single USB port.
