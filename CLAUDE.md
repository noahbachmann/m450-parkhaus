# m322-countdown — agent context

Countdown timer, Raspberry Pi Pico, serial-driven. School project, module M323
(functional programming). Requirements live as `// USn` comments in
`test/test_countdown/test_countdown.cpp`.

## Commands

PlatformIO only. Parent `M323/CLAUDE.md` says cmake/make/ctest — wrong here.

    pio run -e pico                build
    pio run -e pico -t upload      flash
    pio device monitor -b 115200   serial console
    pio test -e native             logic suite, host, no board
    pio test -e pico               hardware smoke test, board required

## Layout

    lib/countdown/         logic core, no Arduino.h    NOT IMPLEMENTED YET
    src/main.cpp           serial in, LED out          still the blink demo
    test/test_countdown/   logic tests, host only
    test/test_hardware/    LED smoke test, pico only

## Rules for lib/countdown/

Module requirement, not preference — do not relax:

- pure functions; state in by value, new state out
- never mutate arguments
- no global or static mutable state
- no `millis()`, no `Serial`, no `digitalWrite()`
- no `Arduino.h` — breaks the native build
- time is always a parameter
- `ledOn()` is a pure function of state: no blink timer, no toggle flag

Side effects only in `src/main.cpp`.

## API contract

Fixed by `test_countdown.cpp`. Implementation matches the tests, not the reverse.

```cpp
enum Phase { IDLE, RUNNING, FINISHED };   // plain enum, IDLE == 0, Unity needs no cast

struct Countdown {
  uint32_t durationMs;
  uint32_t remainingMs;
  uint32_t sinceFinishedMs;   // since expiry; drives the blink
  Phase    phase;
};

Countdown setDuration(Countdown c, uint32_t seconds);  // 1..3600, else unchanged
Countdown start(Countdown c);                          // -> RUNNING, full time
Countdown reset(Countdown c);                          // -> IDLE, full time
Countdown tick(Countdown c, uint32_t dtMs);            // counts only while RUNNING
bool      ledOn(Countdown c);
int32_t   parseSeconds(const char* line);              // "set 30" -> 30, else -1
```

`Countdown{}` value-inits to a valid IDLE state — no factory function needed,
and `IDLE` must stay the first enum value.

## Behaviour

Serial 115200. Invalid input leaves state unchanged.

| cmd         | effect                                            |
|-------------|---------------------------------------------------|
| `set <sec>` | duration 1–3600 s; outside that range: ignored     |
| `start`     | -> RUNNING, remaining = duration                   |
| `reset`     | -> IDLE, remaining = duration, duration kept       |

| phase      | LED                                         |
|------------|---------------------------------------------|
| `IDLE`     | off                                         |
| `RUNNING`  | solid on                                    |
| `FINISHED` | 500 ms on / 500 ms off, forever until reset |

`tick` past remaining: clamp `remainingMs` to 0, overshoot into `sinceFinishedMs`.

## Style

- Allman braces: opening brace on its own line
- comments only where non-obvious; no file-header comment blocks
- tests: group assertions per user story, one function per story aspect.
  No one-function-per-assertion, no helper factories — `const` struct literals.

## Known issue

`platformio.ini` has `board = pico`, commit `09694a5` says "Base for Pico W".
Pico W LED sits on the CYW43 chip, not GPIO25 → needs `board = rpipicow`,
otherwise nothing blinks. Ask before changing.
