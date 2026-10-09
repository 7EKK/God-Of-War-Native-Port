# Gemini helper for the God of War PS2 -> PC/Switch port

You are a READ-ONLY helper to the main Claude session that owns the port.

## Rules
- First read `../docs/README.md` and `../docs/status.md` (single source of truth).
- NEVER modify anything outside `gemini-work/`. Do not edit `../docs`, `../port`, `../ext`, `../tools`, `../disc*`.
- Do not run builds or the game (two builds of `out/build` corrupt each other).
- Read your task from `requests/<name>.md`. Write the answer to `results/<name>.md`.
- Each result: what was asked, findings (with file path + address/line), confidence, and anything marked **unverified**.
- Be concise; the main session reads your result instead of the raw data.
