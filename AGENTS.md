# AGENTS.md



# ABSOLUTE RULE: CLI / SHELL USAGE (READ CAREFULLY)

Minimize command-line (CLI) calls at all costs.

- Do NOT run shell/CLI commands unless they are strictly unavoidable.
- Prefer reasoning, inspection, and proposing changes over executing commands.
- Assume CLI access is expensive, slow, and disruptive to the developer workflow.
- Never run exploratory, curiosity-driven, or “just to check” commands.
- Never run build, flash, clean, configure, or diagnostic commands unless:
      1- The user explicitly asks you to, or
      2- There is no other way to answer the question correctly.

If you believe a CLI command is required:

- First explain why it is unavoidable.
- State exactly which command you would run and what information you expect.
- Wait for explicit approval before running anything.
- If many commands are expected to be required, only ask once

Violating this rule is considered a failure to follow project instructions.

# TOP PRIORITY: NO-CODE-FIRST GATE (HARD REQUIREMENT)

If the user gives a task/problem, you MUST follow this order:

A) PLAN (no code)
B) SUMMARY FOR APPROVAL (no code)
C) CODE (only after approval)

## HARD PROHIBITION
Do NOT output any code blocks, diffs, patches, or file contents until you have produced
the "SUMMARY FOR APPROVAL" section AND the user explicitly replies with "GO" or "APPROVED".

## REQUIRED FORMAT (copy exactly)

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

END SUMMARY — WAITING FOR "GO"

## If the user asks for code immediately
Still output PLAN + SUMMARY first, then wait for "GO".



---

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
