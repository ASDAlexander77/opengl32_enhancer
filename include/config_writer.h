#pragma once

#include "config.h"

// Writes an AnaxConfig back out to an existing ini file IN PLACE, rewriting only the value on
// each recognized `key=` line and leaving every other byte of the file alone.
//
// It works this way rather than regenerating the file because opengl32_enhancer.ini is mostly
// documentation - every stage and parameter is explained inline, next to the value it controls,
// and that prose is the actual reference for what the settings mean. A writer that emitted a
// fresh file from the struct would silently destroy all of it the first time anyone pressed
// Save in the config editor.
//
// Lines that are blank, commented (`;`/`#`) or hold a key this doesn't manage are copied
// verbatim - which is also what keeps the commented-out `;effect=...` preset lines in the
// shipped ini intact instead of being treated as the active `effect=` line. An inline trailing
// comment on a rewritten line is preserved too: only the value itself is replaced.
//
// A managed key that appears nowhere in the file is appended at the end under a short header,
// so a config saved from the editor is always complete even if it started from a partial ini.
//
// Returns false (and logs) if the file could not be read or written; on failure the original
// file is left untouched, since the rewrite is staged in memory and only written once complete.
bool WriteConfigToIni(const char* path, const AnaxConfig& config);
