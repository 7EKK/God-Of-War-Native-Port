"""The port's wording for the save screens and PS2-only prompts (docs/settings.md "Save wording"): the save file
replaces the PS2 memory card, so no line names a MEMORY CARD (8MB)(for PlayStation 2), a card slot or a DUALSHOCK 2.

Each entry is (line on the USA disc, new line); tools/gow_text.py rewrites them in FLP_HUD (R_PERM.WAD) and FLP_Shell
(R_SHELL.WAD). "{SLOT}" keeps the spaces after the old line's "Slot", over which the game draws the save slot number. An empty new
line hides the old one. The card-name lines become empty: each message reads from its first and last line.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # tools/ (python -I leaves it out)
import gow_text  # noqa: E402

SLOT = gow_text.SLOT

LINES = [
    # the card name on its own line in every message
    ('MEMORY CARD (8MB)(for PlayStation 2)', ''),
    (' MEMORY CARD (8MB)(for PlayStation 2)', ''),
    ('MEMORY CARD (8MB)(for PlayStation  2)', ''),
    ('on MEMORY CARD (8MB)(for PlayStation 2)', ''),
    # "... in MEMORY CARD Slot 1." -> "... in save game slot 1."
    ('in MEMORY CARD Slot   .', 'in save game slot' + SLOT + '.'),
    ('in MEMORY CARD Slot   . ', 'in save game slot' + SLOT + '.'),
    (' in MEMORY CARD Slot   .', 'in save game slot' + SLOT + '.'),
    ('in MEMORY CARD Slot   ?', 'in save game slot' + SLOT + '?'),
    (' in MEMORY CARD Slot   ?', 'in save game slot' + SLOT + '?'),
    ('in MEMORY CARD Slot    ', 'in save game slot' + SLOT),
    ('found in MEMORY CARD Slot   .', 'in save game slot' + SLOT + '.'),
    (' found in MEMORY CARD Slot   .', 'in save game slot' + SLOT + '.'),
    ('found in MEMORY CARD Slot   ?', 'in save game slot' + SLOT + '?'),
    ('in MEMORY CARD Slot    is unformatted.', 'Save game slot' + SLOT + 'is not ready.'),
    ('in MEMORY CARD Slot     contains', 'Save game slot' + SLOT + 'contains'),
    ('Memory Card Slot   .', 'Save game slot' + SLOT + '.'),
    ('Memory Card Slot         ', 'Save game slot' + SLOT),
    # first lines of the messages
    ('Select Load file from', 'Select a saved game'),
    ('Select Save file from', 'Select where to save'),
    ('MEMORY CARD', 'SAVE FILE'),
    ('Error Saving to', 'Error saving'),
    ('Saving Game to', 'Saving game'),
    ('Do not remove', 'Do not quit'),
    ('MEMORY CARD (8MB)(for PlayStation 2).', 'the game while saving.'),
    ('Do not turn power off or reset.', ''),
    ('Do not power off or reset.', ''),
    ('Create new God of War file on', 'Create a new save'),
    ('Overwrite God of War file on', 'Overwrite the save'),
    ('No God of War Data found on', 'No saved game found'),
    ('Error loading from', 'Error loading'),
    ('Loading Game from', 'Loading game'),
    ('Load God of War file from', 'Load the saved game'),
    ('Checking for', 'Checking'),
    ('Error formatting', 'Error preparing'),
    ('Formatting Complete.', 'Save file ready.'),
    ('Formatting', 'Preparing'),
    ('Error reading from', 'Error reading'),
    ('Insufficient free space for new save game file', 'Not enough free space for a new save file'),
    ('Do you want to format?', 'Prepare it now?'),
    ('No MEMORY CARD (8MB)(for PlayStation 2)', 'No save file found'),
    ('MEMORY CARD (8mb)(for PlayStation 2)', 'Save file found'),
    # controller prompt (a gamepad or the keyboard on PC)
    ('Please insert a DUALSHOCK   2', 'Please connect a controller'),
    ('analog controller into', 'to continue.'),
    ('controller port 1 to continue.', ''),
    ('The DUALSHOCK   2', 'The controller'),
    ('Please insert a DualShock 2 Controller.  ', 'Please connect a controller.'),
]

TABLE = {'FLP_HUD': LINES, 'FLP_Shell': LINES}


def apply(wad):
    """the WAD with the port's wording (R_PERM.WAD and R_SHELL.WAD; other WADs come back unchanged)"""
    new, _ = gow_text.rewrite_wad(wad, TABLE)
    return new


if __name__ == '__main__':
    for path in sys.argv[1:]:
        _, report = gow_text.rewrite_wad(open(path, 'rb').read(), TABLE)
        for chunk, done in report.items():
            print(path, chunk, sum(done.values()), 'lines')
            for old, _ in LINES:
                if old not in done:
                    print('   not found:', repr(old))
