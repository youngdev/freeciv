/***********************************************************************
 Freeciv - Copyright (C) 1996 - A Kjeldberg, L Gregersen, P Unold
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
***********************************************************************/

#ifdef HAVE_CONFIG_H
#include <fc_config.h>
#endif

/* utility */
#include "capability.h"
#include "log.h"

/* server */
#include "aiiface.h"

#include "savecompat.h"

bool sg_success;

static char *special_names[] =
  {
    "Irrigation", "Mine", "Pollution", "Hut", "Farmland",
    "Fallout", NULL
  };

static void compat_load_020400(struct loaddata *loading);
static void compat_load_020500(struct loaddata *loading);
static void compat_load_020600(struct loaddata *loading);

typedef void (*load_version_func_t) (struct loaddata *loading);

struct compatibility {
  int version;
  const load_version_func_t load;
};

/* The struct below contains the information about the savegame versions. It
 * is identified by the version number (first element), which should be
 * steadily increasing. It is saved as 'savefile.version'. The support
 * string (first element of 'name') is not saved in the savegame; it is
 * saved in settings files (so, once assigned, cannot be changed). The
 * 'pretty' string (second element of 'name') can be changed if necessary
 * For changes in the development version, edit the definitions above and
 * add the needed code to load the old version below. Thus, old
 * savegames can still be loaded while the main definition
 * represents the current state of the art. */
/* While developing freeciv 2.6.0, add the compatibility functions to
 * - compat_load_020600 to load old savegame. */
static struct compatibility compat[] = {
  /* dummy; equal to the current version (last element) */
  { 0, NULL },
  /* version 1 and 2 is not used */
  /* version 3: first savegame2 format, so no compat functions for translation
   * from previous format */
  { 3, NULL },
  /* version 4 to 9 are reserved for possible changes in 2.3.x */
  { 10, compat_load_020400 },
  /* version 11 to 19 are reserved for possible changes in 2.4.x */
  { 20, compat_load_020500 },
  /* version 21 to 29 are reserved for possible changes in 2.5.x */
  { 30, compat_load_020600 },
  /* Current savefile version is listed above this line; it corresponds to
     the definitions in this file. */
};

static const int compat_num = ARRAY_SIZE(compat);
#define compat_current (compat_num - 1)

/****************************************************************************
  Compatibility functions for loaded game.

  This function is called at the beginning of loading a savegame. The data in
  loading->file should be change such, that the current loading functions can
  be executed without errors.
****************************************************************************/
void sg_load_compat(struct loaddata *loading)
{
  int i;

  /* Check status and return if not OK (sg_success != TRUE). */
  sg_check_ret();

  loading->version = secfile_lookup_int_default(loading->file, -1,
                                                "savefile.version");
#ifdef DEBUG
  sg_failure_ret(0 < loading->version, "Invalid savefile format version (%d).",
                 loading->version);
  if (loading->version > compat[compat_current].version) {
    /* Debug build can (TRY TO!) load newer versions but ... */
    log_error("Savegame version newer than this build found (%d > %d). "
              "Trying to load the game nevertheless ...", loading->version,
              compat[compat_current].version);
  }
#else
  sg_failure_ret(0 < loading->version
                 && loading->version <= compat[compat_current].version,
                 "Unknown savefile format version (%d).", loading->version);
#endif /* DEBUG */


  for (i = 0; i < compat_num; i++) {
    if (loading->version < compat[i].version && compat[i].load != NULL) {
      log_normal(_("Run compatibility function for version: <%d "
                   "(save file: %d; server: %d)."), compat[i].version,
                 loading->version, compat[compat_current].version);
      compat[i].load(loading);
    }
  }
}

/****************************************************************************
  Return current compatibility version
****************************************************************************/
int current_compat_ver(void)
{
  return compat[compat_current].version;
}

/****************************************************************************
  This returns an ascii hex value of the given half-byte of the binary
  integer. See ascii_hex2bin().
  example: bin2ascii_hex(0xa00, 2) == 'a'
****************************************************************************/
char bin2ascii_hex(int value, int halfbyte_wanted)
{
  return hex_chars[((value) >> ((halfbyte_wanted) * 4)) & 0xf];
}

/****************************************************************************
  This returns a binary integer value of the ascii hex char, offset by the
  given number of half-bytes. See bin2ascii_hex().
  example: ascii_hex2bin('a', 2) == 0xa00
  This is only used in loading games, and it requires some error checking so
  it's done as a function.
****************************************************************************/
int ascii_hex2bin(char ch, int halfbyte)
{
  const char *pch;

  if (ch == ' ') {
    /* Sane value. It is unknow if there are savegames out there which
     * need this fix. Savegame.c doesn't write such savegames
     * (anymore) since the inclusion into CVS (2000-08-25). */
    return 0;
  }

  pch = strchr(hex_chars, ch);

  sg_failure_ret_val(NULL != pch && '\0' != ch, 0,
                     "Unknown hex value: '%c' %d", ch, ch);
  return (pch - hex_chars) << (halfbyte * 4);
}

/****************************************************************************
  Return the special with the given name, or S_LAST.
****************************************************************************/
enum tile_special_type special_by_rule_name(const char *name)
{
  int i;

  for (i = 0; special_names[i] != NULL; i++) {
    if (!strcmp(name, special_names[i])) {
      return i;
    }
  }

  return S_LAST;
}

/****************************************************************************
  Return the untranslated name of the given special.
****************************************************************************/
const char *special_rule_name(enum tile_special_type type)
{
  fc_assert(type >= 0 && type < S_LAST);

  return special_names[type];
}

/* =======================================================================
 * Compatibility functions for loading a game.
 * ======================================================================= */

/****************************************************************************
  Translate savegame secfile data from 2.3.x to 2.4.0 format.
****************************************************************************/
static void compat_load_020400(struct loaddata *loading)
{
  /* Check status and return if not OK (sg_success != TRUE). */
  sg_check_ret();

  log_debug("Upgrading data from savegame to version 2.4.0");

  /* Add the default player AI. */
  player_slots_iterate(pslot) {
    int ncities, i, plrno = player_slot_index(pslot);

    if (NULL == secfile_section_lookup(loading->file, "player%d", plrno)) {
      continue;
    }

    secfile_insert_str(loading->file, default_ai_type_name(),
                       "player%d.ai_type", player_slot_index(pslot));

    /* Create dummy citizens informations. We do not know if citizens are
     * activated due to the fact that this information
     * (game.info.citizen_nationality) is not available, but adding the
     * information does no harm. */
    ncities = secfile_lookup_int_default(loading->file, 0,
                                         "player%d.ncities", plrno);
    if (ncities > 0) {
      for (i = 0; i < ncities; i++) {
        int size = secfile_lookup_int_default(loading->file, 0,
                                              "player%d.c%d.size", plrno, i);
        if (size > 0) {
          secfile_insert_int(loading->file, size,
                             "player%d.c%d.citizen%d", plrno, i, plrno);
        }
      }
    }

  } player_slots_iterate_end;

  /* Player colors are assigned at the end of player loading, as this
   * needs information not available here. */

  /* Deal with buggy known tiles information from 2.3.0/2.3.1 (and the
   * workaround in later 2.3.x); see gna bug #19029.
   * (The structure of this code is odd as it avoids relying on knowledge of
   * xsize/ysize, which haven't been extracted from the savefile yet.) */
  {
    if (has_capability("knownv2",
                       secfile_lookup_str(loading->file, "savefile.options"))) {
      /* This savefile contains known information in a sane format.
       * Just move any entries to where 2.4.x+ expect to find them. */
      struct section *map = secfile_section_by_name(loading->file, "map");
      if (map) {
        entry_list_iterate(section_entries(map), pentry) {
          const char *name = entry_name(pentry);
          if (strncmp(name, "kvb", 3) == 0) {
            /* Rename the "kvb..." entry to "k..." */
            char *name2 = fc_strdup(name), *newname = name2 + 2;
            *newname = 'k';
            /* Savefile probably contains existing "k" entries, which are bogus
             * so we trash them */
            secfile_entry_delete(loading->file, "map.%s", newname);
            entry_set_name(pentry, newname);
            FC_FREE(name2);
          }
        } entry_list_iterate_end;
      }
      /* Could remove "knownv2" from savefile.options, but it's doing
       * no harm there. */
    } else {
      /* This savefile only contains known information in the broken
       * format. Try to recover it to a sane format. */
      /* MAX_NUM_PLAYER_SLOTS in 2.3.x was 128 */
      /* MAP_MAX_LINEAR_SIZE in 2.3.x was 512 */
      const int maxslots = 128, maxmapsize = 512;
      const int lines = maxslots/32;
      int xsize = 0, y, l, j, x;
      unsigned int known_row_old[lines * maxmapsize],
                   known_row[lines * maxmapsize];
      /* Process a map row at a time */
      for (y = 0; y < maxmapsize; y++) {
        /* Look for broken info to convert */
        bool found = FALSE;
        memset(known_row_old, 0, sizeof(known_row_old));
        for (l = 0; l < lines; l++) {
          for (j = 0; j < 8; j++) {
            const char *s =
              secfile_lookup_str_default(loading->file, NULL,
                                         "map.k%02d_%04d", l * 8 + j, y);
            if (s) {
              found = TRUE;
              if (xsize == 0) {
                xsize = strlen(s);
              }
              sg_failure_ret(xsize == strlen(s),
                             "Inconsistent xsize in map.k%02d_%04d",
                             l * 8 + j, y);
              for (x = 0; x < xsize; x++) {
                known_row_old[l * xsize + x] |= ascii_hex2bin(s[x], j);
              }
            }
          }
        }
        if (found) {
          /* At least one entry found for this row. Let's hope they were
           * all there. */
          /* Attempt to munge into sane format */
          int p;
          memset(known_row, 0, sizeof(known_row));
          /* Iterate over possible player slots */
          for (p = 0; p < maxslots; p++) {
            l = p / 32;
            for (x = 0; x < xsize; x++) {
              /* This test causes bit-shifts of >=32 (undefined behaviour), but
               * on common platforms, information happens not to be lost, just
               * oddly arranged. */
              if (known_row_old[l * xsize + x] & (1u << (p - l * 8))) {
                known_row[l * xsize + x] |= (1u << (p - l * 32));
              }
            }
          }
          /* Save sane format back to memory representation of secfile for
           * real loading code to pick up */
          for (l = 0; l < lines; l++) {
            for (j = 0; j < 8; j++) {
              /* Save info for all slots (not just used ones). It's only
               * memory, after all. */
              char row[xsize+1];
              for (x = 0; x < xsize; x++) {
                row[x] = bin2ascii_hex(known_row[l * xsize + x], j);
              }
              row[xsize] = '\0';
              secfile_replace_str(loading->file, row,
                                  "map.k%02d_%04d", l * 8 + j, y);
            }
          }
        }
      }
    }
  }

  /* Server setting migration. */
  {
    int set_count;
    if (secfile_lookup_int(loading->file, &set_count, "settings.set_count")) {
      int i, new_opt = set_count;
      bool gamestart_valid
        = secfile_lookup_bool_default(loading->file, FALSE,
                                      "settings.gamestart_valid");
      for (i = 0; i < set_count; i++) {
        const char *name
          = secfile_lookup_str(loading->file, "settings.set%d.name", i);
        if (!name) {
          continue;
        }

        /* In 2.3.x and prior, saveturns=0 meant no turn-based saves.
         * This is now controlled by the "autosaves" setting. */
        if (!fc_strcasecmp("saveturns", name)) {
          int nturns, nturns_start;
          (void) secfile_lookup_int(loading->file, &nturns,
                                    "settings.set%d.value", i);
          (void) secfile_lookup_int(loading->file, &nturns_start,
                                    "settings.set%d.gamestart", i);
          if (nturns == 0 || (gamestart_valid && nturns_start == 0)) {
            /* Invent a new "autosaves" setting */
            /* XXX: hardcodes details from GAME_AUTOSAVES_DEFAULT
             * and settings.c:autosaves_name() (but these defaults reflect
             * 2.3's behaviour) */
            const char *const nosave = "GAMEOVER|QUITIDLE|INTERRUPT",
                       *const save = "TURN|GAMEOVER|QUITIDLE|INTERRUPT";
            secfile_replace_int(loading->file, new_opt+1, "settings.set_count");
            secfile_insert_str(loading->file, "autosaves",
                               "settings.set%d.name", new_opt);
            if (nturns == 0) {
              secfile_insert_str(loading->file, nosave,
                                 "settings.set%d.value", new_opt);
              /* Pick something valid for saveturns */
              secfile_replace_int(loading->file, GAME_DEFAULT_SAVETURNS,
                                  "settings.set%d.value", i);
            } else {
              secfile_insert_str(loading->file, save,
                                 "settings.set%d.value", new_opt);
            }
            if (gamestart_valid) {
              if (nturns_start == 0) {
                secfile_insert_str(loading->file, nosave,
                                   "settings.set%d.gamestart", new_opt);
                secfile_replace_int(loading->file, GAME_DEFAULT_SAVETURNS,
                                    "settings.set%d.gamestart", i);
              } else {
                secfile_insert_str(loading->file, save,
                                   "settings.set%d.gamestart", new_opt);
              }
            }
          }
        } else if (!fc_strcasecmp("autosaves", name)) {
          /* Sanity check. This won't trigger on an option we've just
           * invented, as the loop won't include it. */
          log_sg("Unexpected \"autosaves\" setting found in pre-2.4 "
                 "savefile. It may have been overridden.");
        }
      }
    }
  }
}

/****************************************************************************
  Callback to get name of old killcitizen setting bit.
****************************************************************************/
static const char *killcitizen_enum_str(secfile_data_t data, int bit)
{
  switch (bit) {
  case UMT_LAND:
    return "LAND";
  case UMT_SEA:
    return "SEA";
  case UMT_BOTH:
    return "BOTH";
  }

  return NULL;
}

/****************************************************************************
  Translate savegame secfile data from 2.4.x to 2.5.0 format.
****************************************************************************/
static void compat_load_020500(struct loaddata *loading)
{
  const char *modname[] = { "Road", "Railroad" };

  /* Check status and return if not OK (sg_success != TRUE). */
  sg_check_ret();

  log_debug("Upgrading data from savegame to version 2.5.0");

  secfile_insert_int(loading->file, 2, "savefile.roads_size");

  secfile_insert_str_vec(loading->file, modname, 2,
                         "savefile.roads_vector");

  /* Server setting migration. */
  {
    int set_count;

    if (secfile_lookup_int(loading->file, &set_count, "settings.set_count")) {
      int i;
      bool gamestart_valid
        = secfile_lookup_bool_default(loading->file, FALSE,
                                      "settings.gamestart_valid");
      for (i = 0; i < set_count; i++) {
        const char *name
          = secfile_lookup_str(loading->file, "settings.set%d.name", i);
        if (!name) {
          continue;
        }

        /* In 2.4.x and prior, "killcitizen" listed move types that
         * killed citizens after succesfull attack. Now killcitizen
         * is just boolean and classes affected are defined in ruleset. */
        if (!fc_strcasecmp("killcitizen", name)) {
          int value, value_start;

          (void) secfile_lookup_enum_data(loading->file, &value, TRUE,
                                          killcitizen_enum_str, NULL,
                                          "settings.set%d.value", i);
          (void) secfile_lookup_enum_data(loading->file, &value_start, TRUE,
                                          killcitizen_enum_str, NULL,
                                          "settings.set%d.gamestart", i);

          /* Lowest bit of old killcitizen value indicates if
           * land units should kill citizens. We take that as
           * new boolean killcitizen value. */
          if (value & 0x1) {
            secfile_replace_bool(loading->file, TRUE, "settings.set%d.value", i);
          } else {
            secfile_replace_bool(loading->file, FALSE, "settings.set%d.value", i);
          }
          if (gamestart_valid) {
            if (value_start & 0x1) {
              secfile_replace_bool(loading->file, TRUE, "settings.set%d.gamestart", i);
            } else {
              secfile_replace_bool(loading->file, FALSE, "settings.set%d.gamestart", i);
            }
          }
        }
      }
    }
  }
}

/****************************************************************************
  Translate savegame secfile data from 2.5.x to 2.6.0 format.
****************************************************************************/
static void compat_load_020600(struct loaddata *loading)
{
  /* Check status and return if not OK (sg_success != TRUE). */
  sg_check_ret();

  log_debug("Upgrading data from savegame to version 2.6.0");

  /* Server setting migration. */
  {
    int set_count;

    if (secfile_lookup_int(loading->file, &set_count, "settings.set_count")) {
      char value_buffer[1024] = "";
      char gamestart_buffer[1024] = "";
      int i;
      bool gamestart_valid
        = secfile_lookup_bool_default(loading->file, FALSE,
                                      "settings.gamestart_valid");

      for (i = 0; i < set_count; i++) {
        const char *name
          = secfile_lookup_str(loading->file, "settings.set%d.name", i);
        if (!name) {
          continue;
        }

        /* In 2.5.x and prior, "spacerace" boolean controlled if
         * spacerace victory condition was active. */
        if (!fc_strcasecmp("spacerace", name)) {
          bool value, value_start;

          (void) secfile_lookup_bool(loading->file, &value,
                                     "settings.set%d.value", i);
          (void) secfile_lookup_bool(loading->file, &value_start,
                                     "settings.set%d.gamestart", i);

          if (value) {
            sz_strlcat(value_buffer, "|SPACERACE");
          }
          if (value_start) {
            sz_strlcat(gamestart_buffer, "|SPACERACE");
          }

          /* We cannot delete old values from the secfile, or rather cannot
           * change index of the later settings. Renumbering them is not easy as
           * we don't know type of each setting we would encounter.
           * So we keep old setting values and only add new "victories" setting. */
        } else if (!fc_strcasecmp("alliedvictory", name)) {
          bool value, value_start;

          (void) secfile_lookup_bool(loading->file, &value,
                                     "settings.set%d.value", i);
          (void) secfile_lookup_bool(loading->file, &value_start,
                                     "settings.set%d.gamestart", i);

          if (value) {
            sz_strlcat(value_buffer, "|ALLIED");
          }
          if (value_start) {
            sz_strlcat(gamestart_buffer, "|ALLIED");
          }
        }
      }

      secfile_replace_int(loading->file, set_count + 1, "settings.set_count");

      secfile_insert_str(loading->file, "victories", "settings.set%d.name",
                         set_count);
      secfile_insert_str(loading->file, value_buffer, "settings.set%d.value",
                         set_count);
      if (gamestart_valid) {
        secfile_insert_str(loading->file, gamestart_buffer, "settings.set%d.gamestart",
                           set_count);
      }
    }
  }
}

/****************************************************************************
  Convert old ai level value to ai_level
****************************************************************************/
enum ai_level ai_level_convert(int old_level)
{
  switch (old_level) {
  case 1:
    return AI_LEVEL_AWAY;
  case 2:
    return AI_LEVEL_NOVICE;
  case 3:
    return AI_LEVEL_EASY;
  case 5:
    return AI_LEVEL_NORMAL;
  case 7:
    return AI_LEVEL_HARD;
  case 8:
    return AI_LEVEL_CHEATING;
  case 10:
    return AI_LEVEL_EXPERIMENTAL;
  }

  return ai_level_invalid();
}
