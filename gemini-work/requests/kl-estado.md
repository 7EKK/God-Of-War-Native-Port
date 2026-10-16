# Task: summarize one file for our port

Read-only. Use read_file only (no shell commands). Read ONLY this file:
../ref/other-recomp-git/docs/ESTADO.md (Spanish; another person's God of War PS2 recomp, same code base as ours).

We currently fight: (1) Kratos' upper body/head missing intermittently while walking, caused by compiled VU1
*blocks* (generar_vu1 block path; interpreter and single compiled pairs are fine); (2) game fps ~20 in gameplay,
target >= 25; (3) purple/lavender haze in some levels (16-bit channel-shuffle framebuffer passes).

Output (max 35 lines, English):
- Their current state in 5 lines (fps numbers with the settings used).
- Every item in the file related to (1), (2) or (3): quote the env var / patch / file names exactly.
- Any known bug or workaround they list for VU1 compiled code, GS worker, or rendering artifacts.
Mark anything you are unsure of as UNVERIFIED.
