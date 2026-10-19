#pragma once
#include <string>

// On first run, prompts for the user's own USA ISO or fully extracted disc.
// Returns false when the user cancels or import fails.
bool ensureGameData(const std::wstring &root);
