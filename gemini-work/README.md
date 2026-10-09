# gemini-work: handoff between Claude (owner) and Antigravity/Gemini (read-only helper)

Tool: Antigravity CLI (`agy`, installed at %LOCALAPPDATA%\agy\bin\agy.exe, logged in with the user's Google AI Pro account).
Gemini CLI is dead for personal accounts since 2026-06-18; do not use it.

Flow:
1. Claude writes a task to `requests/<name>.md` (what to analyze, which files, wanted output).
2. Run from this folder:
   `agy --add-dir .. -p "Do the task in requests/<name>.md. Print only the answer." --print-timeout 300s > results/<name>.md`
3. Claude reads `results/<name>.md`, verifies it, and moves confirmed facts into `../docs/` (the helper never edits docs).

Permissions (headless mode cannot prompt): ~/.gemini/antigravity-cli/settings.json allows read_file on `God Of War/` and
write_file only on `gemini-work/results`. Do NOT use --dangerously-skip-permissions. Use forward slashes in that JSON
(backslashes made the file malformed and silently ignored; check the --log-file for "malformed").

Good tasks: summarize big logs/disassembly dumps, search how other projects solved a problem, review a patch, write helper scripts.
Bad tasks: editing port/runtime code, builds, runs of the game.
