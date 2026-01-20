# AGENTS.md

Hello agent! Here's some instructions for you to follow so we can develop this project together while keeping in managable, interpretable and efficient. 

# ABSOLUTE RULE: CLI / SHELL USAGE (READ CAREFULLY)

Minimize command-line (CLI) whereever possible. 
if you think a job will require many CLIs give a breakdown and estimate. 
If requirements are unclear, pause and ask instead of guessing.
The user is here to help you too, they can debug and get information to make your job easier.

# TOP PRIORITY: NO-CODE-FIRST GATE (HARD REQUIREMENT)

If the user gives a task/problem, you MUST follow this order:

A) PLAN (no code)
B) SUMMARY FOR APPROVAL (no code)
C) CODE (only after approval)

## HARD PROHIBITION
Do NOT output any code blocks, diffs, patches, or file contents until you have produced
the "SUMMARY FOR APPROVAL" section AND the user gives you the go ahead.

## EXAMPLE FORMAT, MODIFY TO SUIT SITUATION

### PLAN
- Goal:
- Files to touch:
- Steps:
- Risks/assumptions:
- Test plan:

### SUMMARY FOR APPROVAL
- What will change:
- Why this approach:
- How we’ll verify:

END SUMMARY — (WAIT FOR APPROVAL)

*** NOTE: if you are given a build error, you do not need to ask for approval, proceed immediately to fix ***



## Hard Rules (Do Not Violate)
- Do not modify CubeMX-generated code outside `/* USER CODE BEGIN */` blocks:
  - `Core/Src/main.c`
  - `Core/Src/app_freertos.c`
  - `Core/Src/stm32u5xx_it.c`
  - `Core/Src/stm32u5xx_hal_msp.c`
  - Anything under `Drivers/`, `Middlewares/`, or `cmake/stm32cubemx/`
- Do not introduce dynamic memory or new FreeRTOS heaps.
- Do not add blocking delays inside ISRs; defer work via queues/semaphores.
- Do not add functions to apps_freertos unless they do not belong anywhere else.
- Do not refactor working drivers (Sharp_MIP, ADP5360, AT25, TMAG, LIS2, UI)
  unless explicitly asked.
- Do not change clock tree, power modes, linker scripts, or startup files unless asked.
- If unsure, ask first.

---

## Code Style
- Prefer `static` functions and file-scope state.
- Explicit types (`uint32_t`, `int16_t`, etc.), avoid plain `int`.
- No recursion.
- Minimal macros; prefer `static const` tables for menus/pages.
- ISR code must be short and deterministic; post to RTOS objects instead.
- RTOS objects are created once at init (`MX_FREERTOS_Init`).


---

If requirements are unclear, pause and ask instead of guessing.
The user is here to help you too, they can debug and get information to make your job easier.
