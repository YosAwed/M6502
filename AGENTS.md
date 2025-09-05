# Repository Guidelines

## Project Structure & Module Organization
- Source: C modules in repo root (e.g., `parser.c`, `expression_evaluator.c`, `main.c`).
- Header: `basic.h` in repo root.
- Assembly reference: `m6502.asm` (historical context).
- Makefile: expects `src/`, `include/`, and `obj/` directories. Until the tree is refactored, treat the repo root as the source directory.
- Docs: multiple reference notes (`*.md`) and `todo.md`.

## Build, Test, and Development Commands
- Build (direct): `gcc *.c -o basic -lm` — quick local build from the root.
- Build (Make): `make` — uses `obj/` and C99 flags; may require moving sources to `src/` and `basic.h` to `include/`.
- Clean: `make clean`
- Smoke tests: `make test` — pipes a few BASIC lines into the binary to sanity‑check I/O.
- Run: `./basic` (Unix) or `./basic.exe` (Windows PowerShell: `./basic.exe`).

## Coding Style & Naming Conventions
- Standard: C99, `-Wall -Wextra` clean.
- Indentation: 4 spaces, no tabs; keep lines < 100 chars.
- Naming: snake_case for functions/variables (e.g., `evaluate_expression`), `UPPER_SNAKE_CASE` for macros, `*_t` for typedefs.
- Headers: include `basic.h`; mark file‑local helpers `static`.
- Includes: If retaining current layout, update `#include "../include/basic.h"` to `"basic.h"` or move files under `src/`/`include/` to match the Makefile.

## Testing Guidelines
- Framework: none yet. Use `make test` for smoke tests and add targeted unit checks when introducing complex parsing/eval logic.
- Add minimal repro programs (small `.bas` snippets in PR description) and expected output.

## Commit & Pull Request Guidelines
- Commits: imperative mood, scope prefix when helpful (e.g., `parser: handle string literals`, `eval: fix division by zero`).
- PRs: clear description, linked issues, steps to reproduce, before/after output, and notes on any layout changes (e.g., moving files to `src/`/`include/`).

## Architecture Notes
- Goal: a small, C‑based interpreter approximating Microsoft BASIC (6502 era) semantics.
- Keep behavior compatible with 2‑char variable names, legacy numeric model, and command set unless explicitly evolving the spec.

