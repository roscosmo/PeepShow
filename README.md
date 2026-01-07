# PeepShowV5-FreeRTOS

Embedded firmware for an STM32U575 handheld device using FreeRTOS (CMSIS-RTOS2).
Low-power and deterministic behavior are core requirements. Hardware peripherals are tightly
coupled to timing, power domains, and RTOS scheduling.

This project is not experimental. Avoid speculative refactors.

---

## Goals

- **Deterministic behavior**
  - No ambiguous ownership of peripherals
  - No hidden concurrency on HAL handles
  - Predictable scheduling and bounded latency
- **STOP2-first power design**
  - STOP2 is the primary power saver
  - Wake on IRQ/input, do work, return to STOP2
- **Maintainability**
  - Clear task responsibilities
  - Clear message passing and state transitions
  - Minimal ISR logic
- **Performance where needed**
  - Display flushes are DMA-driven
  - Streaming uses stable high-speed kernel clocks
  - Audio starts quickly on wake

---

## Device Behavior Model (Authoritative)

**User-visible pattern:**

Wake → ~15s no interaction → Sleep (STOP2) → Wake on input/IRQ → repeat

**Implications:**
- Awake window is long → PLL/HSI startup cost is irrelevant
- STOP2 is the primary power saver
- Streaming and audio occur only while awake
- During STOP2 the core domain is off, so firmware must not depend on APB register access

---

## Hardware Overview

- MCU: STM32U575
- Display: Sharp Memory LCD (MIP) on SPI3
- 5 buttons: A, B, L, R, BOOT
- External flash: AT25SL128A on OCTOSPI1 (littlefs)
- Power: ADP5360 PMIC/charger + fuel gauge
- Sensors:
  - TMAG5273 magnetometer (joystick/orientation)
  - LIS2DUX12 accelerometer
- Comms:
  - LPUART1 / USART1 (Wio E5 UART bridge + debug)
- Audio:
  - SAI1 (I2S TX)
- RTC:
  - 1 Hz wake for sleep clock face (LSE)

---

## Power Domains & Architecture (Critical)

| Domain     | RUN | STOP2 | Contents |
|-----------|-----|-------|----------|
| Core      | ON  | OFF   | CPU, SYSCLK, APB register access |
| SmartRun  | ON  | ON    | LPDMA, SPI3 (kernel clock), selected SRAM retention |
| Backup    | ON  | ON    | RTC (LSE) |

**Rules:**
- STOP2 kills SYSCLK/APB, not SmartRun kernel clocks.
- Any activity that must continue or be valid across STOP2 must rely on:
  - STOP2-capable kernel clocks
  - SmartRun peripherals (LPDMA)
  - retained SRAM
- APB clocks are for register access; kernel clocks are for datapath. Do not confuse them.

---

## Clock Taxonomy (No shortcuts)

### Oscillators / sources
- MSI (source) → feeds MSIS and MSIK
- MSIS → CPU/SYSCLK (not STOP2)
- MSIK → kernel clocks usable in STOP2
- HSI → independent oscillator, enabled on-demand
- LSE → RTC, and LPUART kernel clock
- LSI → fallback RTC (if needed)

### System and bus clocks
- SYSCLK: CPU execution clock (MSIS ↔ PLL1)
- HCLK (AHB): CPU + DMA fabric (prescaler can change at runtime)
- PCLKx (APB): register access only (not datapath)

**Key rule:** APB clocks ≠ kernel clocks.

---

## Performance Scaling Strategy (FreeRTOS)

Modes (policy enforced by power manager):
- **Sleep**: STOP2 (core off)
- **Cruise**: MSIS ~24 MHz (UI + light logic)
- **Mid**: PLL1 160 with AHB /2 (80 MHz) for heavier rendering
- **Turbo**: PLL1 full (160 MHz) for burst work

Notes:
- AHB prescaler may change at runtime.
- APB clocks do not need to track CPU speed.
- PLLs are mode-based, not permanent.

---

## DMA Strategy (Very Important)

| DMA  | Domain    | STOP2 | Intended use |
|------|-----------|-------|--------------|
| GPDMA | Core     | ❌    | high-speed work while CPU running |
| LPDMA | SmartRun | ✅    | SPI3, LPUART RX, STOP2-safe datapaths |
| LPBAM | SmartRun | ✅    | future autonomous chains |

**Rule:** anything expected to function safely around STOP2 must use LPDMA (SmartRun).

---

## Peripheral Policies (Locked)

### Display (Sharp Memory LCD)
- Interface: SPI3
- DMA: LPDMA
- SPI3 kernel clock: MSIK (~4 MHz)
- SPI speed target: ~1.5 Mb/s
- Update strategy: dirty-region only
- STOP2: display flush can be made STOP2-compatible, but initial integration will be stricter (see TODO)

### UARTs / LoRa / Debug
**LPUART1 (Wio E5)**
- Baud: 9600
- Clock: LSE
- STOP2 RX: yes
- DMA: LPDMA circular
- Wake on: IDLE/RX event

**USART1 (Debug only)**
- Baud: 9600
- Clock: HSI (on-demand)
- STOP2 not required
- Enabled only when debug flag set

### Audio (SAI)
- Use: UI + game SFX
- Sample rate: 16 kHz
- Channels: mono
- Depth: 16-bit
- MCLK: 4.096 MHz (256×FS) via PLL2P
- Continuous audio: no
- Latency handling: prewarm on wake
- Quality tradeoff accepted (not audiophile-grade)

### PLL2 (Multimedia PLL) (FINAL)
- Source: HSI
- Enabled while awake only
- Outputs:
  - PLL2P: 4.096 MHz → SAI audio
  - PLL2Q: 64 MHz → OCTOSPI streaming
  - PLL2R: spare

### External flash / OCTOSPI
- Usage: streaming assets
- Kernel clock: PLL2Q = 64 MHz
- Avoid SYSCLK dependence for streaming
- STOP2: flash deep power-down
- Avoid clock switching while streaming

### MSIK usage justification
- SPI3 uses MSIK: STOP2-capable flush / background kernel clock
- OCTOSPI does not use MSIK: needs speed from PLL2Q
- LPUART uses LSE
- I2C may use MSIK for low power (if configured)

---

## Design Principles (Locked)

- STOP2 is first-class
- Kernel clocks > bus clocks
- CPU speed ≠ peripheral speed
- PLLs are mode-based, not permanent
- Clock ownership is centralized (no random toggles)
- Avoid refactors of working drivers unless explicitly asked
- Keep ISRs deterministic and short
- No blocking delays in ISRs

---

## FreeRTOS Configuration (Project Policy)

### Kernel/API
- FreeRTOS via CMSIS-RTOS2
- Preemption enabled
- Tick rate: 1000 Hz
- Tickless idle: enabled

### Allocation strategy
- CubeMX objects are **Dynamic** (as per current working setup)
- Heap scheme: **Heap_4**
- TOTAL_HEAP_SIZE: **64 KB**
- Policy: RTOS objects are created at boot (init-time). Avoid runtime creation of RTOS objects.

### Safety hooks
- malloc-failed hook enabled (trap + log if possible)
- stack overflow checking enabled (level 2)

---

## Architecture Overview (Single-owner peripheral model)

This firmware uses strict ownership:
- Each time-sensitive peripheral datapath is owned by exactly one task.
- Other tasks request work via queues/messages.
- No “quick HAL call” from UI/game/etc.

This prevents:
- incomplete/partial display flushes due to concurrent SPI use
- timing jitter from cross-task peripheral access
- power bugs from uncontrolled clock changes

---

## Tasks (Naming Convention)

All RTOS objects use:
- tasks: `tskXxx`
- queues: `qXxx`
- timers: `tmrXxx`
- mutex: `mtxXxx`
- semaphore: `semXxx`
- event groups: `egXxx`

---

## Task Responsibilities and Ownership

### tskDisplay (High)
Owns:
- SPI3
- LPDMA for SPI3
- Sharp MIP flush pipeline

Consumes:
- `qDisplayCmd`

Rules:
- Exactly one flush in-flight at a time
- Dirty-region rendering + command coalescing
- Never allow any other task to touch SPI3 HAL handle

Signals:
- Uses DMA complete thread-flag from ISR
- Provides a “busy/idle” state for power manager quiesce checks

Initial integration rule:
- Do **not** enter STOP2 while a flush is in-flight (introduce STOP2 later, after awake-mode display is rock solid)

---

### tskInput (AboveNormal)
Owns:
- Input routing (UI vs Game)
- Global hotkeys that bypass game mode
- Inactivity timer reset on user activity

Consumes:
- Raw input events from ISRs/UART (exact raw transport is implementation detail)

Produces:
- `qUIEvents` (normal mode)
- `qGameEvents` (game mode)
- `qSysEvents` (system/hotkey events)

Rules:
- ISRs never decide routing. Routing is always in tskInput.
- Global hotkeys always go to system even in game mode (power/exit combo/etc).
- Restart `tmrInactivity` on any user interaction.

---

### tskUI (Normal)
Owns:
- Router/pages/menu logic

Consumes:
- `qUIEvents`

Produces:
- `qDisplayCmd` (invalidate/refresh requests)
- `qAudioCmd` (SFX requests)
- `qStorageReq` (settings/asset reads)
- `qSensorReq` (poll requests)
- `qSysEvents` (enter game / exit game / sleep requests)

Rules:
- UI never blocks waiting for display flush completion.
- UI never touches HAL peripherals directly.

---

### tskGame (Normal)
Owns:
- Game runtime
- Game module lifecycle (“modules that get called”)

Consumes:
- `qGameEvents`
- enter/exit commands (delivered via system events or a dedicated command path)

Produces:
- `qDisplayCmd` (present/invalidate)
- `qAudioCmd` (SFX)
- optional: `qSensorReq` (if a game needs sensor reads)
- optional: `qStorageReq` (if a game needs assets while awake)

Rules:
- Game and game modules do not own peripherals.
- Game modules are pure logic + rendering into RAM buffers.
- Modules must not create RTOS objects or call HAL.

---

### tskSensor (BelowNormal)
Owns:
- I2C bus and all sensor/PMIC transactions

Consumes:
- `qSensorReq`

Produces:
- Updates “latest sample” structs (prefer seq/versioned snapshots over queue spam)

Rules:
- No other task calls I2C HAL.
- Keep periodic polling minimal.
- Must be quiesce-aware for STOP2.

---

### tskStorage (Low)
Owns:
- OCTOSPI and flash driver
- littlefs
- flash deep power-down / wake policies

Consumes:
- `qStorageReq`

Produces:
- `qSysEvents` for stream on/off requests (so power can enable PLL2Q when needed)

Rules:
- Only storage task calls littlefs.
- Do not switch OCTOSPI clock source while streaming.
- Must be quiesce-aware for STOP2 (finish ops, DPD flash).

---

### tskAudio (Realtime)
Owns:
- SAI
- audio DMA buffers
- amp SD_MODE pin

Consumes:
- `qAudioCmd`

Produces:
- `qSysEvents` audio on/off requests (so power can enable HSI + PLL2P)

Rules:
- Only audio task touches SAI HAL.
- Must stop quickly on quiesce.
- Audio is awake-only; no expectation of operation in STOP2.

---

### tskPower (BelowNormal, system authority)
Owns:
- STOP2 entry/exit
- wake handling
- centralized clock policy (HSI/PLL2 and mode scaling)
- quiesce barrier coordination

Consumes:
- `qSysEvents`

Controls:
- `egMode` bits (mode state)
- `egPower` quiesce state (if used)
- clock refcount policy

Rules:
- No other task may toggle clocks or enter STOP2.
- STOP2 entry requires all owners to be idle and quiesced.

---

## Queue/Event Model (High level)

### Queues
- `qUIEvents`: tskInput → tskUI
- `qGameEvents`: tskInput → tskGame
- `qSysEvents`: many producers → tskPower (system authority)
- `qDisplayCmd`: UI/game → display
- `qAudioCmd`: UI/game → audio
- `qStorageReq`: UI/game/audio → storage
- `qSensorReq`: UI/game → sensor

### Event Groups (optional)
- `egMode`: MODE_GAME, MODE_SLEEPFACE, etc.
- `egPower`: QUIESCE_REQ (broadcast), ack tracking scheme (implementation choice)

### DMA done signaling
- Prefer thread flags to the owning task:
  - SPI3 LPDMA done → tskDisplay flag
  - SAI DMA half/full → tskAudio flags

---

## STOP2 Implementation Rules (When introduced)

STOP2 is introduced only after awake-mode pipelines are stable.

### Quiesce Barrier (Mandatory)
Before STOP2:
1. power requests quiesce (broadcast)
2. stop UI timers that would keep CPU awake
3. wait for each owner to reach a safe state:
   - display: not flushing
   - audio: DMA stopped + amp asleep
   - storage: no ops + flash deep power-down
   - sensors: I2C idle + low-power config applied
   - game: module stopped (if needed)
4. enter STOP2

Wake:
- restore cruise clocks
- restore peripherals that require it
- decide mode based on wake source (button/UART/RTC tick)
- resume normal operation

### RTC sleep clock face (1 Hz)
- RTC tick wakes the system
- power requests a minimal redraw
- return to STOP2 immediately after display idle

No FreeRTOS software timer is used for the 1 Hz sleep face in STOP2.

---

## Debugging and Bring-up Policies

- Do not introduce STOP2 early. It complicates all debugging.
- Do not introduce streaming + audio at the same time.
- Validate each “owner task” works in isolation before combining.
- No one touches SPI3/I2C/OCTOSPI/SAI outside the owning task.
- If something “sometimes” fails, assume concurrent access or missing quiesce rules until proven otherwise.

---

# TODO (Logical Implementation Order)

This order is designed to minimize complexity and isolate fault domains.

## Phase 0 — RTOS Bring-up (no DMA, no STOP2)
Goal: tasks run, queues work, heap stable.

- [ ] Confirm FreeRTOS heap scheme is Heap_4 and TOTAL_HEAP_SIZE is 64 KB
- [ ] Confirm stack overflow check and malloc failed hook are enabled
- [ ] Create tasks and ensure each blocks correctly (no polling loops)
- [ ] Confirm qSysEvents path to tskPower exists (tskPower can be a stub initially)
- [ ] Confirm button EXTI events can reach tskInput then tskUI (UI mode only)

Acceptance:
- Boots reliably
- No malloc-failed
- Input events are received and logged/handled
- CPU idle time is visible (no runaway tasks)

---

## Phase 1 — Display Awake-Mode (SPI3 + LPDMA, still no STOP2)
Goal: eliminate partial/incomplete flush issues before power complexity.

- [ ] Implement tskDisplay as exclusive SPI3 owner
- [ ] Implement one-in-flight flush rule
- [ ] Implement dirty-region tracking and basic coalescing
- [ ] Wire SPI3 LPDMA complete → signal tskDisplay (thread flag)
- [ ] Add a minimal UI action to trigger repeated redraw/invalidate
- [ ] Stress test: thousands of flushes, no corruption

Acceptance:
- No partial flushes
- No SPI abort hacks required
- System remains responsive during flushes
- Only tskDisplay touches SPI3

---

## Phase 2 — UI Rendering Discipline (no STOP2)
Goal: UI generates commands, display consumes, no redraw loops.

- [ ] Implement UI router/page structure minimal
- [ ] UI sends invalidate commands only when state changes
- [ ] Ensure no periodic redraw unless explicitly needed
- [ ] Confirm display idle most of the time in static menus

Acceptance:
- UI navigation works
- Display updates only when required
- No “busy flashing” or unnecessary flushes

---

## Phase 3 — Input Routing + Game Mode Switching (no STOP2)
Goal: games are modules, inputs route cleanly.

- [ ] tskInput implements mode routing:
  - normal → qUIEvents
  - game → qGameEvents
- [ ] Implement global hotkeys bypassing game mode
- [ ] UI triggers enter/exit game via sys event
- [ ] tskGame runs one module at a time (logic + draw only)
- [ ] Exiting game returns routing back to UI

Acceptance:
- Inputs go to game only in game mode
- Exit works reliably
- Game does not touch peripherals

---

## Phase 4 — Sensors + I2C Serialization (awake-mode)
Goal: stable I2C behavior, no UI stalls.

- [ ] tskSensor becomes sole I2C owner
- [ ] Add request model via qSensorReq
- [ ] Publish latest samples via snapshot (seq/version)
- [ ] Optional: joystick mapping path integrated

Acceptance:
- No I2C contention
- UI/game remain responsive
- Sensor reads don’t cause display glitches

---

## Phase 5 — Storage + littlefs (awake-mode, no streaming yet)
Goal: filesystem stability first.

- [ ] tskStorage becomes sole OCTOSPI + littlefs owner
- [ ] Implement init/mount
- [ ] Implement basic read/write requests
- [ ] Implement flash deep power-down callable (but don’t tie to STOP2 yet)

Acceptance:
- Filesystem operations are reliable
- No concurrent FS calls elsewhere
- No lockups during read/write

---

## Phase 6 — Audio + Clock Refcount Policy (awake-mode)
Goal: audio stable, clocks controlled centrally.

- [ ] Implement tskPower refcount handling for:
  - audio_ref
  - debug_ref
- [ ] tskAudio requests AUDIO_ON before starting SAI
- [ ] tskAudio requests AUDIO_OFF after stopping
- [ ] Verify amp SD_MODE behavior

Acceptance:
- Audio starts/stops repeatedly without hang
- HSI/PLL2 enabled only when needed
- No clock toggles outside tskPower

---

## Phase 7 — Streaming (awake-mode) using PLL2Q=64MHz
Goal: stable streaming independent of SYSCLK.

- [ ] Implement stream_ref in tskPower
- [ ] tskStorage requests STREAM_ON before sustained streaming
- [ ] Enforce: no OCTOSPI clock switching during streaming
- [ ] Stress: stream + display + UI together

Acceptance:
- Streaming does not stall UI/game
- No corruption
- Clock stability maintained

---

## Phase 8 — STOP2 Integration (only now)
Goal: STOP2 becomes reliable without breaking peripherals.

- [ ] Implement inactivity timer action in tskPower (~15s)
- [ ] Implement quiesce barrier:
  - request quiesce
  - wait for owner idles/acks
- [ ] Initial strict rule: do not enter STOP2 while display busy
- [ ] Wake on button/UART
- [ ] Verify post-wake peripheral reinit order

Acceptance:
- Enters STOP2 reliably
- Wakes reliably
- No “first draw after wake” corruption
- No stuck DMA/driver states

---

## Phase 9 — RTC 1 Hz Sleep Clock Face
Goal: minimal wake + minimal redraw + return to STOP2.

- [ ] RTC wake triggers sys event to power
- [ ] In sleepface mode: power requests minimal invalidate
- [ ] Return to STOP2 immediately after display idle

Acceptance:
- 1 Hz tick works
- No extra wakeups
- Minimal power impact

---

## Phase 10 — Hardening & Optimization
Goal: reduce power and tighten memory.

- [ ] Stack high-water checks and trimming
- [ ] Queue depth tuning
- [ ] Cruise/mid/turbo policy refinement
- [ ] Optional: allow display flush continuation into STOP2 (only after proven stable)

---

## Constraints and Rules (Do Not Break)

- Do not modify CubeMX-generated code outside USER CODE blocks.
- No HAL peripheral calls outside the owning task.
- ISRs must remain short and deterministic.
- Clock/power control remains centralized in tskPower.
- STOP2 is first-class; no polling loops that prevent sleep.

---
