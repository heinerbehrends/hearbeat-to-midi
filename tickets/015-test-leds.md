# 015 – Manual test: both LEDs

**Status:** todo (needs both LEDs wired)
**Depends on:** 003, 009, 012

## Acceptance criteria

### Sensor LED
- [ ] [hw] Red → amber → green as the sensor locks on; red blinking with the sensor unplugged.
- [ ] [hw] Brightness comfortable from ~50 cm in a dim room (`led_level`).

### Transport LED
- [ ] [hw] Stopped: dim white.
- [ ] [hw] Playing: flashes in time with the DAW's beat indicator, brighter on beat 1 of each bar.
- [ ] [hw] Glide mode: orange while the tempo rises, blue while it falls, white while it holds.
- [ ] [hw] Stepped mode: count-in over the last 4 beats before a step in the step's colour, getting
      brighter, then one long flash on the step.
- [ ] [hw] Both LEDs stay dark during a reset (with the optional 10 kΩ pull-downs).
