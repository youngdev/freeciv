/********************************************************************** 
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

/****************************************************************
 This module is for generic handling of help data, independent
 of gui considerations.
*****************************************************************/

#ifdef HAVE_CONFIG_H
#include <fc_config.h>
#endif

#include <stdio.h>
#include <string.h>

/* utility */
#include "astring.h"
#include "bitvector.h"
#include "fciconv.h"
#include "fcintl.h"
#include "log.h"
#include "mem.h"
#include "registry.h"
#include "string_vector.h"
#include "support.h"

/* common */
#include "achievements.h"
#include "actions.h"
#include "city.h"
#include "effects.h"
#include "game.h"
#include "government.h"
#include "map.h"
#include "movement.h"
#include "packets.h"
#include "requirements.h"
#include "road.h"
#include "specialist.h"
#include "unit.h"
#include "version.h"

/* client */
#include "client_main.h"
#include "gui_main_g.h" /* client_string */

#include "helpdata.h"

/* helper macro for easy conversion from snprintf and cat_snprintf */
#define CATLSTR(_b, _s, _t) fc_strlcat(_b, _t, _s)

/* This must be in same order as enum in helpdlg_g.h */
static const char * const help_type_names[] = {
  "(Any)", "(Text)", "Units", "Improvements", "Wonders",
  "Techs", "Terrain", "Bases", "Roads", "Specialists", "Governments",
  "Ruleset", "Nations", NULL
};

/*define MAX_LAST (MAX(MAX(MAX(A_LAST,B_LAST),U_LAST),terrain_count()))*/

#define SPECLIST_TAG help
#define SPECLIST_TYPE struct help_item
#include "speclist.h"

#define help_list_iterate(helplist, phelp) \
    TYPED_LIST_ITERATE(struct help_item, helplist, phelp)
#define help_list_iterate_end  LIST_ITERATE_END

static const struct help_list_link *help_nodes_iterator;
static struct help_list *help_nodes;
static bool help_nodes_init = FALSE;
/* helpnodes_init is not quite the same as booted in boot_help_texts();
   latter can be 0 even after call, eg if couldn't find helpdata.txt.
*/

/****************************************************************
  Initialize.
*****************************************************************/
void helpdata_init(void)
{
  help_nodes = help_list_new();
}

/****************************************************************
  Clean up.
*****************************************************************/
void helpdata_done(void)
{
  help_list_destroy(help_nodes);
}

/****************************************************************
  Make sure help_nodes is initialised.
  Should call this just about everywhere which uses help_nodes,
  to be careful...  or at least where called by external
  (non-static) functions.
*****************************************************************/
static void check_help_nodes_init(void)
{
  if (!help_nodes_init) {
    help_nodes_init = TRUE;    /* before help_iter_start to avoid recursion! */
    help_iter_start();
  }
}

/****************************************************************
  Free all allocations associated with help_nodes.
*****************************************************************/
void free_help_texts(void)
{
  check_help_nodes_init();
  help_list_iterate(help_nodes, ptmp) {
    free(ptmp->topic);
    free(ptmp->text);
    free(ptmp);
  } help_list_iterate_end;
  help_list_clear(help_nodes);
}

/****************************************************************************
  Insert fixed-width table describing veteran system.
  If only one veteran level, inserts 'nolevels' if non-NULL.
  Otherwise, insert 'intro' then a table.
****************************************************************************/
static bool insert_veteran_help(char *outbuf, size_t outlen,
                                const struct veteran_system *veteran,
                                const char *intro, const char *nolevels)
{
  /* game.veteran can be NULL in pregame; if so, keep quiet about
   * veteran levels */
  if (!veteran) {
    return FALSE;
  }

  fc_assert_ret_val(veteran->levels >= 1, FALSE);

  if (veteran->levels == 1) {
    /* Only a single veteran level. Don't bother to name it. */
    if (nolevels) {
      CATLSTR(outbuf, outlen, nolevels);
      return TRUE;
    } else {
      return FALSE;
    }
  } else {
    int i;
    fc_assert_ret_val(veteran->definitions != NULL, FALSE);
    if (intro) {
      CATLSTR(outbuf, outlen, intro);
      CATLSTR(outbuf, outlen, "\n\n");
    }
    /* raise_chance and work_raise_chance don't get to the client, so we
     * can't report them */
    CATLSTR(outbuf, outlen,
            /* TRANS: Header for fixed-width veteran level table.
             * TRANS: Translators cannot change column widths :(
             * TRANS: "Level name" left-justified, other two right-justified */
            _("Veteran level      Power factor   Move bonus\n"));
    CATLSTR(outbuf, outlen,
            /* TRANS: Part of header for veteran level table. */
            _("--------------------------------------------"));
    for (i = 0; i < veteran->levels; i++) {
      const struct veteran_level *level = &veteran->definitions[i];
      const char *name = name_translation(&level->name);
      /* Use get_internal_string_length() for correct alignment with
       * multibyte character encodings */
      cat_snprintf(outbuf, outlen,
          "\n%s%*s %4d%% %12s",
          name, MAX(0, 25 - (int)get_internal_string_length(name)), "",
          level->power_fact,
          /* e.g. "-    ", "+ 1/3", "+ 1    ", "+ 2 2/3" */
          move_points_text(level->move_bonus, "+ ", "-", TRUE));
    }
    return TRUE;
  }
}

/****************************************************************************
  Insert generated text for the helpdata "name".
  Returns TRUE if anything was added.
****************************************************************************/
static bool insert_generated_text(char *outbuf, size_t outlen, const char *name)
{
  if (!game.client.ruleset_init) {
    return FALSE;
  }

  if (0 == strcmp(name, "TerrainAlterations")) {
    int clean_pollution_time = -1, clean_fallout_time = -1;
    int buildable_bases = 0;

    CATLSTR(outbuf, outlen,
            _("Terrain     Road   Irrigation     Mining         Transform\n"));
    CATLSTR(outbuf, outlen,
            "---------------------------------------------------------------\n");
    terrain_type_iterate(pterrain) {
      if (0 != strlen(terrain_rule_name(pterrain))) {
        char road_time[4], irrigation_time[4],
             mining_time[4], transform_time[4];
        const char *terrain, *irrigation_result,
                   *mining_result,*transform_result;

        fc_snprintf(road_time, sizeof(road_time), "%d", pterrain->road_time);
        fc_snprintf(irrigation_time, sizeof(irrigation_time),
                    "%d", pterrain->irrigation_time);
        fc_snprintf(mining_time, sizeof(mining_time),
                    "%d", pterrain->mining_time);
        fc_snprintf(transform_time, sizeof(transform_time),
                    "%d", pterrain->transform_time);
        terrain = terrain_name_translation(pterrain);
        irrigation_result = 
          (pterrain->irrigation_result == pterrain
           || pterrain->irrigation_result == T_NONE) ? ""
          : terrain_name_translation(pterrain->irrigation_result);
        mining_result =
          (pterrain->mining_result == pterrain
           || pterrain->mining_result == T_NONE) ? ""
          : terrain_name_translation(pterrain->mining_result);
        transform_result =
          (pterrain->transform_result == pterrain
           || pterrain->transform_result == T_NONE) ? ""
          : terrain_name_translation(pterrain->transform_result);
        /* Use get_internal_string_length() for correct alignment with
         * multibyte character encodings */
        cat_snprintf(outbuf, outlen,
            "%s%*s %3s    %3s %s%*s %3s %s%*s %3s %s\n",
            terrain,
            MAX(0, 10 - (int)get_internal_string_length(terrain)), "",
            (pterrain->road_time == 0) ? "-" : road_time,
            (pterrain->irrigation_result == T_NONE) ? "-" : irrigation_time,
            irrigation_result,
            MAX(0, 10 - (int)get_internal_string_length(irrigation_result)), "",
            (pterrain->mining_result == T_NONE) ? "-" : mining_time,
            mining_result,
            MAX(0, 10 - (int)get_internal_string_length(mining_result)), "",
            (pterrain->transform_result == T_NONE) ? "-" : transform_time,
            transform_result);

	if (clean_pollution_time != 0 &&
	    !terrain_has_flag(pterrain, TER_NO_POLLUTION)) {
	  if (clean_pollution_time < 0)
	    clean_pollution_time = pterrain->clean_pollution_time;
	  else
	    if (clean_pollution_time != pterrain->clean_pollution_time)
	      clean_pollution_time = 0; /* give up */
	}
	if (clean_fallout_time != 0 &&
	    !terrain_has_flag(pterrain, TER_NO_POLLUTION)) {
	  if (clean_fallout_time < 0)
	    clean_fallout_time = pterrain->clean_fallout_time;
	  else
	    if (clean_fallout_time != pterrain->clean_fallout_time)
	      clean_fallout_time = 0; /* give up */
	}
      }
    } terrain_type_iterate_end;

    base_type_iterate(b) {
      if (base_extra_get(b)->buildable)
	buildable_bases++;
    } base_type_iterate_end;

    if (clean_pollution_time > 0 || clean_fallout_time > 0 ||
	buildable_bases > 0) {
      CATLSTR(outbuf, outlen, "\n");
      CATLSTR(outbuf, outlen,
	      _("Time taken for the following activities is independent of terrain:\n"));
      CATLSTR(outbuf, outlen, "\n");
      CATLSTR(outbuf, outlen,
	      _("Activity            Time\n"));
      CATLSTR(outbuf, outlen,
	      "---------------------------");
      if (clean_pollution_time > 0)
	cat_snprintf(outbuf, outlen,
		     _("\nClean pollution    %3d"), clean_pollution_time);
      if (clean_fallout_time > 0)
	cat_snprintf(outbuf, outlen,
		     _("\nClean fallout      %3d"), clean_fallout_time);
      base_type_iterate(b) {
	if (base_extra_get(b)->buildable) {
          const char *name = base_name_translation(b);

          cat_snprintf(outbuf, outlen,
                       "\n%s%*s %3d",
                       name,
                       MAX(0, 18 - (int)get_internal_string_length(name)), "",
                       b->build_time);
        }
      } base_type_iterate_end;
    }
    return TRUE;
  } else if (0 == strcmp (name, "VeteranLevels")) {
    return insert_veteran_help(outbuf, outlen, game.veteran,
        _("In this ruleset, the following veteran levels are defined:"),
        _("This ruleset has no default veteran levels defined."));
  } else if (0 == strcmp (name, "FreecivVersion")) {
    const char *ver = freeciv_name_version();

    cat_snprintf(outbuf, outlen,
                 /* TRANS: First %s is version string, e.g.,
                  * "Freeciv version 2.3.0-beta1 (beta version)" (translated).
                  * Second %s is client_string, e.g., "gui-gtk-2.0". */
                 _("This is %s, %s client."), ver, client_string);
    insert_client_build_info(outbuf, outlen);

    return TRUE;
  }
  log_error("Unknown directive '$%s' in help", name);
  return FALSE;
}

/****************************************************************
  Append text for the requirement.  Something like

    "Requires the Communism technology.\n"

  pplayer may be NULL.  Note that it must be updated everytime
  a new requirement type or range is defined.
*****************************************************************/
static bool insert_requirement(char *buf, size_t bufsz,
                               struct player *pplayer,
                               const struct requirement *preq)
{
  switch (preq->source.kind) {
  case VUT_NONE:
    return FALSE;

  case VUT_ADVANCE:
    switch (preq->range) {
    case REQ_RANGE_PLAYER:
      if (preq->present) {
        cat_snprintf(buf, bufsz,
                     _("Requires you to have researched the %s technology.\n"),
                     advance_name_for_player(pplayer, advance_number
                                             (preq->source.value.advance)));
      } else {
        cat_snprintf(buf, bufsz,
                     _("Prevented if you have researched the %s technology.\n"),
                     advance_name_for_player(pplayer, advance_number
                                             (preq->source.value.advance)));
      }
      return TRUE;
    case REQ_RANGE_ALLIANCE:
      if (preq->present) {
        cat_snprintf(buf, bufsz,
                     _("Requires that player allied to you has researched the %s technology.\n"),
                     advance_name_for_player(pplayer, advance_number
                                             (preq->source.value.advance)));
      } else {
        cat_snprintf(buf, bufsz,
                     _("Prevented if player allied to you has researched the %s technology.\n"),
                     advance_name_for_player(pplayer, advance_number
                                             (preq->source.value.advance)));
      }
      return TRUE;
    case REQ_RANGE_WORLD:
      if (preq->present) {
        cat_snprintf(buf, bufsz,
                     _("Requires that any player has researched "
                       "the %s technology.\n"),
                     advance_name_for_player(pplayer, advance_number
                                             (preq->source.value.advance)));
      } else {
        cat_snprintf(buf, bufsz,
                     _("Requires that no player has yet researched "
                       "the %s technology.\n"),
                     advance_name_for_player(pplayer, advance_number
                                             (preq->source.value.advance)));
      }
      return TRUE;
    case REQ_RANGE_LOCAL:
    case REQ_RANGE_CADJACENT:
    case REQ_RANGE_ADJACENT:
    case REQ_RANGE_CITY:
    case REQ_RANGE_CONTINENT:
    case REQ_RANGE_COUNT:
      /* Not supported. */
      break;
    }
    break;

  case VUT_TECHFLAG:
    switch (preq->range) {
    case REQ_RANGE_PLAYER:
      if (preq->present) {
        cat_snprintf(buf, bufsz,
                     _("Requires you to have researched technology with \%s\" flag.\n"),
                     tech_flag_id_name(preq->source.value.techflag));
      } else {
        cat_snprintf(buf, bufsz,
                     _("Prevented if you to have researched technology with \%s\" flag.\n"),
                     tech_flag_id_name(preq->source.value.techflag));
      }
      return TRUE;
    case REQ_RANGE_ALLIANCE:
      if (preq->present) {
        cat_snprintf(buf, bufsz,
                     _("Requires that player allied to you has researched technology with \%s\" flag.\n"),
                     tech_flag_id_name(preq->source.value.techflag));
      } else {
        cat_snprintf(buf, bufsz,
                     _("Prevented if player allied to you has researched technology with \%s\" flag.\n"),
                     tech_flag_id_name(preq->source.value.techflag));
      }
      return TRUE;
    case REQ_RANGE_WORLD:
      if (preq->present) {
        cat_snprintf(buf, bufsz,
                     _("Requires that any player has researched technology with \"%s\" flag.\n"),
                     tech_flag_id_name(preq->source.value.techflag));
      } else {
        cat_snprintf(buf, bufsz,
                     _("Requires that no player has researched technology with \"%s\" flag.\n"),
                     tech_flag_id_name(preq->source.value.techflag));
      }
      return TRUE;
    case REQ_RANGE_LOCAL:
    case REQ_RANGE_CADJACENT:
    case REQ_RANGE_ADJACENT:
    case REQ_RANGE_CITY:
    case REQ_RANGE_CONTINENT:
    case REQ_RANGE_COUNT:
      /* Not supported. */
      break;
    }
    break;

  case VUT_GOVERNMENT:
    if (preq->present) {
      cat_snprintf(buf, bufsz, _("Requires the %s government.\n"),
                   government_name_translation(preq->source.value.govern));
    } else {
      cat_snprintf(buf, bufsz, _("Not available under the %s government.\n"),
                   government_name_translation(preq->source.value.govern));
    }
    return TRUE;

  case VUT_ACHIEVEMENT:
    if (preq->present) {
      cat_snprintf(buf, bufsz, _("Requires the %s achievement.\n"),
                   achievement_name_translation(preq->source.value.achievement));
    } else {
      cat_snprintf(buf, bufsz, _("Not available under the %s achievement.\n"),
                   achievement_name_translation(preq->source.value.achievement));
    }
    return TRUE;

  case VUT_IMPROVEMENT:
    switch (preq->range) {
    case REQ_RANGE_WORLD:
      if (is_great_wonder(preq->source.value.building)) {
        if (preq->present) {
          cat_snprintf(buf, bufsz,
                       _("Requires that the %s wonder has been built by "
                         "any player.\n"),
                       improvement_name_translation
                       (preq->source.value.building));
        } else {
          cat_snprintf(buf, bufsz,
                       _("Requires that the %s wonder has not yet been built "
                         "by any player.\n"),
                       improvement_name_translation
                       (preq->source.value.building));
        }
        return TRUE;
      }
      break;
    case REQ_RANGE_ALLIANCE:
      if (is_wonder(preq->source.value.building)) {
        if (preq->present) {
          cat_snprintf(buf, bufsz, _("Requires someone allied to you to own the %s wonder.\n"),
                       improvement_name_translation
                       (preq->source.value.building));
        } else {
          cat_snprintf(buf, bufsz, _("Prevented if someone allied to you own the %s wonder.\n"),
                       improvement_name_translation
                       (preq->source.value.building));
        }
        return TRUE;
      }
      break;
    case REQ_RANGE_PLAYER:
      if (is_wonder(preq->source.value.building)) {
        if (preq->present) {
          cat_snprintf(buf, bufsz, _("Requires you to own the %s wonder.\n"),
                       improvement_name_translation
                       (preq->source.value.building));
        } else {
          cat_snprintf(buf, bufsz, _("Prevented if you own the %s wonder.\n"),
                       improvement_name_translation
                       (preq->source.value.building));
        }
        return TRUE;
      }
      break;
    case REQ_RANGE_CONTINENT:
      if (is_wonder(preq->source.value.building)) {
        if (preq->present) {
          cat_snprintf(buf, bufsz,
                       _("Requires the %s wonder to be owned by you and on "
                         "the same continent.\n"),
                       improvement_name_translation
                       (preq->source.value.building));
        } else {
          cat_snprintf(buf, bufsz,
                       _("Requires the %s wonder to either not be owned by "
                         "you or be on a different continent.\n"),
                       improvement_name_translation
                       (preq->source.value.building));
        }
        return TRUE;
      }
      break;
    case REQ_RANGE_CITY:
      if (preq->present) {
        cat_snprintf(buf, bufsz,
                     _("Requires the %s building in the city.\n"),
                     improvement_name_translation
                     (preq->source.value.building));
      } else {
        cat_snprintf(buf, bufsz,
                     _("Prevented by the %s building in the city.\n"),
                     improvement_name_translation
                     (preq->source.value.building));
      }
      return TRUE;
    case REQ_RANGE_LOCAL:
      if (preq->present) {
        cat_snprintf(buf, bufsz,
                     _("Only applies to \"%s\" buildings.\n"),
                     improvement_name_translation
                     (preq->source.value.building));
      } else {
        cat_snprintf(buf, bufsz,
                     _("Does not apply to \"%s\" buildings.\n"),
                     improvement_name_translation
                     (preq->source.value.building));
      }
      return TRUE;
    case REQ_RANGE_CADJACENT:
    case REQ_RANGE_ADJACENT:
    case REQ_RANGE_COUNT:
      /* Not supported. */
      break;
    }
    break;

  case VUT_EXTRA:
    switch (preq->range) {
    case REQ_RANGE_LOCAL:
      if (preq->present) {
        cat_snprintf(buf, bufsz,
                     _("Requires the %s extra on the tile.\n"),
                     extra_name_translation(preq->source.value.extra));
      } else {
        cat_snprintf(buf, bufsz,
                     _("Prevented by the %s extra on the tile.\n"),
                     extra_name_translation(preq->source.value.extra));
      }
      return TRUE;
    case REQ_RANGE_CADJACENT:
      if (preq->present) {
        cat_snprintf(buf, bufsz,
                     _("Requires the %s extra on the tile or "
                       "a cardinally adjacent tile.\n"),
                     extra_name_translation(preq->source.value.extra));
        } else {
        cat_snprintf(buf, bufsz,
                     _("Prevented by the %s extra on the tile or "
                       "a cardinally adjacent tile.\n"),
                     extra_name_translation(preq->source.value.extra));
        }
      return TRUE;
    case REQ_RANGE_ADJACENT:
      if (preq->present) {
        cat_snprintf(buf, bufsz,
                     _("Requires the %s extra on the tile or "
                       "an adjacent tile.\n"),
                     extra_name_translation(preq->source.value.extra));
      } else {
        cat_snprintf(buf, bufsz,
                     _("Prevented by the %s extra on the tile or "
                       "an adjacent tile.\n"),
                     extra_name_translation(preq->source.value.extra));
      }
      return TRUE;
    case REQ_RANGE_CITY:
      if (preq->present) {
        cat_snprintf(buf, bufsz,
                     _("Requires the %s extra on a tile within "
                       "the city radius.\n"),
                     extra_name_translation(preq->source.value.extra));
      } else {
        cat_snprintf(buf, bufsz,
                     _("Prevented by the %s extra on a tile within "
                       "the city radius.\n"),
                     extra_name_translation(preq->source.value.extra));
      }
      return TRUE;
    case REQ_RANGE_CONTINENT:
    case REQ_RANGE_PLAYER:
    case REQ_RANGE_ALLIANCE:
    case REQ_RANGE_WORLD:
    case REQ_RANGE_COUNT:
      /* Not supported. */
      break;
    }
    break;

  case VUT_TERRAIN:
    switch (preq->range) {
    case REQ_RANGE_LOCAL:
      if (preq->present) {
        cat_snprintf(buf, bufsz, _("Requires the %s terrain on the tile.\n"),
                     terrain_name_translation(preq->source.value.terrain));
      } else {
        cat_snprintf(buf, bufsz, _("Prevented by the %s terrain on the tile.\n"),
                     terrain_name_translation(preq->source.value.terrain));
      }
      return TRUE;
    case REQ_RANGE_CADJACENT:
      if (preq->present) {
        cat_snprintf(buf, bufsz,_("Requires the %s terrain on the tile or "
                                  "a cardinally adjacent tile.\n"),
                     terrain_name_translation(preq->source.value.terrain));
      } else {
        cat_snprintf(buf, bufsz,_("Prevented by the %s terrain on the tile or "
                                  "a cardinally adjacent tile.\n"),
                     terrain_name_translation(preq->source.value.terrain));
      }
      return TRUE;
    case REQ_RANGE_ADJACENT:
      if (preq->present) {
        cat_snprintf(buf, bufsz,_("Requires the %s terrain on the tile or "
                                  "an adjacent tile.\n"),
                     terrain_name_translation(preq->source.value.terrain));
      } else {
        cat_snprintf(buf, bufsz,_("Prevented by the %s terrain on the tile or "
                                  "an adjacent tile.\n"),
                     terrain_name_translation(preq->source.value.terrain));
      }
      return TRUE;
    case REQ_RANGE_CITY:
      if (preq->present) {
        cat_snprintf(buf, bufsz,
                     _("Requires the %s terrain on a tile within the city "
                       "radius.\n"),
                     terrain_name_translation(preq->source.value.terrain));
      } else {
        cat_snprintf(buf, bufsz,
                     _("Prevented by the %s terrain on a tile within the city "
                       "radius.\n"),
                     terrain_name_translation(preq->source.value.terrain));
      }
      return TRUE;
    case REQ_RANGE_CONTINENT:
    case REQ_RANGE_PLAYER:
    case REQ_RANGE_ALLIANCE:
    case REQ_RANGE_WORLD:
    case REQ_RANGE_COUNT:
      /* Not supported. */
      break;
    }
    break;

  case VUT_RESOURCE:
    switch (preq->range) {
    case REQ_RANGE_LOCAL:
      if (preq->present) {
        cat_snprintf(buf, bufsz, _("Requires the %s resource on the tile.\n"),
                     resource_name_translation(preq->source.value.resource));
      } else {
        cat_snprintf(buf, bufsz, _("Prevented by the %s resource on the tile.\n"),
                     resource_name_translation(preq->source.value.resource));
      }
      return TRUE;
    case REQ_RANGE_CADJACENT:
      if (preq->present) {
        cat_snprintf(buf, bufsz,_("Requires the %s resource on the tile or "
                                  "a cardinally adjacent tile.\n"),
                     resource_name_translation(preq->source.value.resource));
      } else {
        cat_snprintf(buf, bufsz,_("Prevented by the %s resource on the tile or "
                                  "a cardinally adjacent tile.\n"),
                     resource_name_translation(preq->source.value.resource));
      }
      return TRUE;
    case REQ_RANGE_ADJACENT:
      if (preq->present) {
        cat_snprintf(buf, bufsz,_("Requires the %s resource on the tile or "
                                  "an adjacent tile.\n"),
                     resource_name_translation(preq->source.value.resource));
      } else {
        cat_snprintf(buf, bufsz,_("Prevented by the %s resource on the tile or "
                                  "an adjacent tile.\n"),
                     resource_name_translation(preq->source.value.resource));
      }
      return TRUE;
    case REQ_RANGE_CITY:
      if (preq->present) {
        cat_snprintf(buf, bufsz,
                     _("Requires the %s resource on a tile within the city "
                       "radius.\n"),
                     resource_name_translation(preq->source.value.resource));
      } else {
        cat_snprintf(buf, bufsz,
                     _("Prevented by the %s resource on a tile within the city "
                       "radius.\n"),
                     resource_name_translation(preq->source.value.resource));
      }
      return TRUE;
    case REQ_RANGE_CONTINENT:
    case REQ_RANGE_PLAYER:
    case REQ_RANGE_ALLIANCE:
    case REQ_RANGE_WORLD:
    case REQ_RANGE_COUNT:
      /* Not supported. */
      break;
    }
    break;

  case VUT_NATION:
    switch (preq->range) {
    case REQ_RANGE_PLAYER:
      if (preq->present) {
        cat_snprintf(buf, bufsz, _("Requires that you are playing the %s nation.\n"),
                     nation_adjective_translation(preq->source.value.nation));
      } else {
        cat_snprintf(buf, bufsz, _("Requires that you are not playing the %s nation.\n"),
                     nation_adjective_translation(preq->source.value.nation));
      }
      return TRUE;
    case REQ_RANGE_ALLIANCE:
      if (preq->present) {
        cat_snprintf(buf, bufsz, _("Requires that you are allied with %s nation.\n"),
                     nation_adjective_translation(preq->source.value.nation));
      } else {
        cat_snprintf(buf, bufsz, _("Requires that you are not allied with %s nation.\n"),
                     nation_adjective_translation(preq->source.value.nation));
      }
    case REQ_RANGE_WORLD:
      if (preq->present) {
        cat_snprintf(buf, bufsz, _("Requires the %s nation in the game.\n"),
                     nation_adjective_translation(preq->source.value.nation));
      } else {
        cat_snprintf(buf, bufsz, _("Requires that the %s nation is not in the game.\n"),
                     nation_adjective_translation(preq->source.value.nation));
      }
      return TRUE;
    case REQ_RANGE_LOCAL:
    case REQ_RANGE_CADJACENT:
    case REQ_RANGE_ADJACENT:
    case REQ_RANGE_CITY:
    case REQ_RANGE_CONTINENT:
    case REQ_RANGE_COUNT:
      /* Not supported. */
      break;
    }
    break;

  case VUT_STYLE:

    fc_assert(preq->range == REQ_RANGE_PLAYER);

    if (preq->present) {
      cat_snprintf(buf, bufsz, _("Requires %s style nation.\n"),
                   style_name_translation(preq->source.value.style));
    } else {
      cat_snprintf(buf, bufsz, _("Requires that nation is not %s style.\n"),
                   style_name_translation(preq->source.value.style));
    }
    break;

  case VUT_NATIONALITY:
    switch (preq->range) {
    case REQ_RANGE_CITY:
      if (preq->present) {
        cat_snprintf(buf, bufsz, _("Requires %s citizen in city.\n"),
                     nation_adjective_translation(preq->source.value.nationality));
      } else {
        cat_snprintf(buf, bufsz, _("Requires no %s citizens in the city.\n"),
                     nation_adjective_translation(preq->source.value.nationality));
      }
      return TRUE;
    case REQ_RANGE_WORLD:
    case REQ_RANGE_ALLIANCE:
    case REQ_RANGE_PLAYER:
    case REQ_RANGE_LOCAL:
    case REQ_RANGE_CADJACENT:
    case REQ_RANGE_ADJACENT:
    case REQ_RANGE_CONTINENT:
    case REQ_RANGE_COUNT:
      /* Not supported. */
      break;
    }
    break;

  case VUT_DIPLREL:
    switch (preq->range) {
    case REQ_RANGE_PLAYER:
      if (preq->present) {
        cat_snprintf(buf, bufsz,
                     _("Requires that your diplomatic relationship to at"
                       " least one other living player is %s.\n"),
                     diplrel_name_translation(preq->source.value.diplrel));
      } else {
        cat_snprintf(buf, bufsz,
                     _("Requires that no diplomatic relationship you have"
                       " to any living player is %s.\n"),
                     diplrel_name_translation(preq->source.value.diplrel));
      }
      return TRUE;
    case REQ_RANGE_ALLIANCE:
      if (preq->present) {
        cat_snprintf(buf, bufsz,
                     _("Requires that somebody in your alliance has %s "
                       "diplomatic relationship to another living player.\n"),
                     diplrel_name_translation(preq->source.value.diplrel));
      } else {
        cat_snprintf(buf, bufsz,
                     _("Requires that nobody in your alliance has %s "
                       "diplomatic relationship to another living player.\n"),
                     diplrel_name_translation(preq->source.value.diplrel));
      }
    case REQ_RANGE_WORLD:
      if (preq->present) {
        cat_snprintf(buf, bufsz,
                     _("Requires the diplomatic relationship %s between two"
                       " living players.\n"),
                     diplrel_name_translation(preq->source.value.diplrel));
      } else {
        cat_snprintf(buf, bufsz,
                     _("Requires the abstinence of the diplomatic"
                       " relationship %s between any living players.\n"),
                     diplrel_name_translation(preq->source.value.diplrel));
      }
      return TRUE;
    case REQ_RANGE_LOCAL:
      if (preq->present) {
        cat_snprintf(buf, bufsz,
                     _("Requires that your diplomatic relationship to the"
                       " other player is %s.\n"),
                     diplrel_name_translation(preq->source.value.diplrel));
      } else {
        cat_snprintf(buf, bufsz,
                     _("Requires that your diplomatic relationship to the"
                       " other player isn't %s.\n"),
                     diplrel_name_translation(preq->source.value.diplrel));
      }
      return TRUE;
    case REQ_RANGE_CADJACENT:
    case REQ_RANGE_ADJACENT:
    case REQ_RANGE_CITY:
    case REQ_RANGE_CONTINENT:
    case REQ_RANGE_COUNT:
      /* Not supported. */
      break;
    }
    break;

  case VUT_UTYPE:
    switch (preq->range) {
    case REQ_RANGE_LOCAL:
      if (preq->present) {
        /* TRANS: %s is a single kind of unit (e.g., "Settlers"). */
        cat_snprintf(buf, bufsz, Q_("?unit:Requires %s.\n"),
                     utype_name_translation(preq->source.value.utype));
      } else {
        /* TRANS: %s is a single kind of unit (e.g., "Settlers"). */
        cat_snprintf(buf, bufsz, Q_("?unit:Does not apply to %s.\n"),
                     utype_name_translation(preq->source.value.utype));
      }
      return TRUE;
    case REQ_RANGE_CADJACENT:
    case REQ_RANGE_ADJACENT:
    case REQ_RANGE_CITY:
    case REQ_RANGE_CONTINENT:
    case REQ_RANGE_PLAYER:
    case REQ_RANGE_ALLIANCE:
    case REQ_RANGE_WORLD:
    case REQ_RANGE_COUNT:
      /* Not supported. */
      break;
    }
    break;

  case VUT_UTFLAG:
    switch (preq->range) {
    case REQ_RANGE_LOCAL:
      {
        struct astring astr = ASTRING_INIT;

         /* Unit type flags mean nothing to users. Explicitly list the unit
         * types with those flags. */
        if (role_units_translations(&astr, preq->source.value.unitflag,
                                    TRUE)) {
          if (preq->present) {
            /* TRANS: %s is a list of unit types separated by "or". */
            cat_snprintf(buf, bufsz, Q_("?ulist:Requires %s.\n"),
                         astr_str(&astr));
          } else {
            /* TRANS: %s is a list of unit types separated by "or". */
            cat_snprintf(buf, bufsz, Q_("?ulist:Does not apply to %s.\n"),
                         astr_str(&astr));
          }
          astr_free(&astr);
          return TRUE;
        }
      }
      break;
    case REQ_RANGE_CADJACENT:
    case REQ_RANGE_ADJACENT:
    case REQ_RANGE_CITY:
    case REQ_RANGE_CONTINENT:
    case REQ_RANGE_PLAYER:
    case REQ_RANGE_ALLIANCE:
    case REQ_RANGE_WORLD:
    case REQ_RANGE_COUNT:
      /* Not supported. */
      break;
    }
    break;

  case VUT_UCLASS:
    switch (preq->range) {
    case REQ_RANGE_LOCAL:
      if (preq->present) {
        /* TRANS: %s is a single unit class (e.g., "Air"). */
        cat_snprintf(buf, bufsz, Q_("?uclass:Requires %s units.\n"),
                     uclass_name_translation(preq->source.value.uclass));
      } else {
        /* TRANS: %s is a single unit class (e.g., "Air"). */
        cat_snprintf(buf, bufsz, Q_("?uclass:Does not apply to %s units.\n"),
                     uclass_name_translation(preq->source.value.uclass));
      }
      return TRUE;
    case REQ_RANGE_CADJACENT:
    case REQ_RANGE_ADJACENT:
    case REQ_RANGE_CITY:
    case REQ_RANGE_CONTINENT:
    case REQ_RANGE_PLAYER:
    case REQ_RANGE_ALLIANCE:
    case REQ_RANGE_WORLD:
    case REQ_RANGE_COUNT:
      /* Not supported. */
      break;
    }
    break;

  case VUT_UCFLAG:
    {
      const char *classes[uclass_count()];
      int i = 0;
      bool done = FALSE;
      struct astring list = ASTRING_INIT;

      unit_class_iterate(uclass) {
        if (uclass_has_flag(uclass, preq->source.value.unitclassflag)) {
          classes[i++] = uclass_name_translation(uclass);
        }
      } unit_class_iterate_end;
      astr_build_or_list(&list, classes, i);

      switch (preq->range) {
      case REQ_RANGE_LOCAL:
        if (preq->present) {
          /* TRANS: %s is a list of unit classes separated by "or". */
          cat_snprintf(buf, bufsz, Q_("?uclasslist:Requires %s units.\n"),
                       astr_str(&list));
        } else { 
          /* TRANS: %s is a list of unit classes separated by "or". */
          cat_snprintf(buf, bufsz, Q_("?uclasslist:Does not apply to %s units.\n"),
                       astr_str(&list));
        }
        done = TRUE;
        break;
      case REQ_RANGE_CADJACENT:
      case REQ_RANGE_ADJACENT:
      case REQ_RANGE_CITY:
      case REQ_RANGE_CONTINENT:
      case REQ_RANGE_PLAYER:
      case REQ_RANGE_ALLIANCE:
      case REQ_RANGE_WORLD:
      case REQ_RANGE_COUNT:
        /* Not supported. */
        break;
      }
      astr_free(&list);
      if (done) {
        return TRUE;
      }
    }
    break;

  case VUT_OTYPE:
    if (preq->present) {
      /* TRANS: Applies only to food. */
      cat_snprintf(buf, bufsz, Q_("?output:Applies only to %s.\n"),
                   get_output_name(preq->source.value.outputtype));
    } else {
      /* TRANS: Does not apply to food. */
      cat_snprintf(buf, bufsz, Q_("?output:Does not apply to %s.\n"),
                   get_output_name(preq->source.value.outputtype));
    }
    return TRUE;

  case VUT_SPECIALIST:
    if (preq->present) {
      /* TRANS: Applies only to scientist */
      cat_snprintf(buf, bufsz, Q_("?specialist:Applies only to %s.\n"),
                   specialist_plural_translation(preq->source.value.specialist));
    } else {
      /* TRANS: Does not apply to scientist */
      cat_snprintf(buf, bufsz, Q_("?specialist:Does not apply to %s.\n"),
                   specialist_plural_translation(preq->source.value.specialist));
    }
    return TRUE;

  case VUT_MINSIZE:
    if (preq->present) {
      cat_snprintf(buf, bufsz, _("Requires a minimum size of %d.\n"),
                   preq->source.value.minsize);
    } else {
      cat_snprintf(buf, bufsz, _("Requires city size less than %d.\n"),
                   preq->source.value.minsize);
    }
    return TRUE;

  case VUT_MAXTILEUNITS:
    switch (preq->range) {
    case REQ_RANGE_LOCAL:
      if (preq->present) {
        cat_snprintf(buf, bufsz,
                     _("Max %d unit(s) can be present on the tile.\n"),
                     preq->source.value.maxTileUnits);
      } else {
        cat_snprintf(buf, bufsz,
                     _("More than %d unit(s) must be present"
                       " on the tile.\n"),
                     preq->source.value.maxTileUnits);
      }
      return TRUE;
    case REQ_RANGE_CADJACENT:
      if (preq->present) {
        cat_snprintf(buf, bufsz,
                     _("A tile with max %d unit(s)"
                       " must be cardinally adjacent.\n"),
                     preq->source.value.maxTileUnits);
      } else {
        cat_snprintf(buf, bufsz,
                     _("A tile with more than than %d unit(s)"
                       " must be cardinally adjacent.\n"),
                     preq->source.value.maxTileUnits);
      }
      return TRUE;
    case REQ_RANGE_ADJACENT:
      if (preq->present) {
        cat_snprintf(buf, bufsz,
                     _("A tile with max %d unit(s) must be adjacent.\n"),
                     preq->source.value.maxTileUnits);
      } else {
        cat_snprintf(buf, bufsz,
                     _("A tile with more than than %d unit(s)"
                       " must be adjacent.\n"),
                     preq->source.value.maxTileUnits);
      }
      return TRUE;
    case REQ_RANGE_CITY:
    case REQ_RANGE_CONTINENT:
    case REQ_RANGE_PLAYER:
    case REQ_RANGE_ALLIANCE:
    case REQ_RANGE_WORLD:
    case REQ_RANGE_COUNT:
      /* Not supported. */
      break;
    }

  case VUT_AI_LEVEL:
    if (preq->present) {
      cat_snprintf(buf, bufsz, _("Requires AI player of level %s.\n"),
                   ai_level_translated_name(preq->source.value.ai_level));
    } else {
      cat_snprintf(buf, bufsz, _("Does not apply to AI player of level %s.\n"),
                   ai_level_translated_name(preq->source.value.ai_level));
    }
    return TRUE;

  case VUT_TERRAINCLASS:
    switch (preq->range) {
    case REQ_RANGE_LOCAL:
      if (preq->present) {
        cat_snprintf(buf, bufsz, _("Requires %s terrain class on the tile.\n"),
                     terrain_class_name_translation
                     (preq->source.value.terrainclass));
      } else {
        cat_snprintf(buf, bufsz, _("Prevented by %s terrain class on the tile.\n"),
                     terrain_class_name_translation
                     (preq->source.value.terrainclass));
      }
      return TRUE;
    case REQ_RANGE_CADJACENT:
      if (preq->present) {
        cat_snprintf(buf, bufsz,
                     _("Requires %s terrain class on a cardinally adjacent tile.\n"),
                     terrain_class_name_translation
                     (preq->source.value.terrainclass));
      } else {
        cat_snprintf(buf, bufsz,
                     _("Prevented by %s terrain class on a cardinally adjacent tile.\n"),
                     terrain_class_name_translation
                     (preq->source.value.terrainclass));
      }
      return TRUE;
    case REQ_RANGE_ADJACENT:
      if (preq->present) {
        cat_snprintf(buf, bufsz,
                     _("Requires %s terrain class on an adjacent tile.\n"),
                     terrain_class_name_translation
                     (preq->source.value.terrainclass));
      } else {
        cat_snprintf(buf, bufsz,
                     _("Prevented by %s terrain class on an adjacent tile.\n"),
                     terrain_class_name_translation
                     (preq->source.value.terrainclass));
      }
      return TRUE;
    case REQ_RANGE_CITY:
      if (preq->present) {
        cat_snprintf(buf, bufsz,
                     _("Requires %s terrain class on a tile within the city "
                       "radius.\n"),
                     terrain_class_name_translation
                     (preq->source.value.terrainclass));
      } else {
        cat_snprintf(buf, bufsz,
                     _("Prevented by %s terrain class on a tile within the "
                       "city radius.\n"),
                     terrain_class_name_translation
                     (preq->source.value.terrainclass));
      }
      return TRUE;
    case REQ_RANGE_CONTINENT:
    case REQ_RANGE_PLAYER:
    case REQ_RANGE_ALLIANCE:
    case REQ_RANGE_WORLD:
    case REQ_RANGE_COUNT:
      /* Not supported. */
      break;
    }
    break;

  case VUT_TERRFLAG:
    switch (preq->range) {
    case REQ_RANGE_LOCAL:
      if (preq->present) {
        cat_snprintf(buf, bufsz, _("Requires \"%s\" terrain flag on the tile.\n"),
                     terrain_flag_id_name(preq->source.value.terrainflag));
      } else {
        cat_snprintf(buf, bufsz, _("Prevented by \"%s\" terrain flag on the tile.\n"),
                     terrain_flag_id_name(preq->source.value.terrainflag));
      }
      return TRUE;
    case REQ_RANGE_CADJACENT:
      if (preq->present) {
        cat_snprintf(buf, bufsz,
                     _("Requires \"%s\" terrain flag on a cardinally adjacent tile.\n"),
                     terrain_flag_id_name(preq->source.value.terrainflag));
      } else {
        cat_snprintf(buf, bufsz,
                     _("Prevented by \"%s\" terrain flag on a cardinally adjacent tile.\n"),
                     terrain_flag_id_name(preq->source.value.terrainflag));
      }
      return TRUE;
    case REQ_RANGE_ADJACENT:
      if (preq->present) {
        cat_snprintf(buf, bufsz,
                     _("Requires \"%s\" terrain flag on an adjacent tile.\n"),
                     terrain_flag_id_name(preq->source.value.terrainflag));
      } else {
        cat_snprintf(buf, bufsz,
                     _("Prevented by \"%s\" terrain flag on an adjacent tile.\n"),
                     terrain_flag_id_name(preq->source.value.terrainflag));
      }
      return TRUE;
    case REQ_RANGE_CITY:
      if (preq->present) {
        cat_snprintf(buf, bufsz,
                     _("Requires \"%s\" terrain flag on a tile within the "
                       "city radius.\n"),
                     terrain_flag_id_name(preq->source.value.terrainflag));
      } else {
        cat_snprintf(buf, bufsz,
                     _("Prevented by \"%s\" terrain flag on a tile within the "
                       "city radius.\n"),
                     terrain_flag_id_name(preq->source.value.terrainflag));
      }
      return TRUE;
    case REQ_RANGE_CONTINENT:
    case REQ_RANGE_PLAYER:
    case REQ_RANGE_ALLIANCE:
    case REQ_RANGE_WORLD:
    case REQ_RANGE_COUNT:
      /* Not supported. */
      break;
    }
    break;

  case VUT_BASEFLAG:
    switch (preq->range) {
    case REQ_RANGE_LOCAL:
      if (preq->present) {
        cat_snprintf(buf, bufsz, _("Requires \"%s\" base flag on the tile.\n"),
                     base_flag_id_name(preq->source.value.baseflag));
      } else {
        cat_snprintf(buf, bufsz, _("Prevented by \"%s\" base flag on the tile.\n"),
                     base_flag_id_name(preq->source.value.baseflag));
      }
      return TRUE;
    case REQ_RANGE_CADJACENT:
      if (preq->present) {
        cat_snprintf(buf, bufsz,
                     _("Requires \"%s\" base flag on a cardinally adjacent tile.\n"),
                     base_flag_id_name(preq->source.value.baseflag));
      } else {
        cat_snprintf(buf, bufsz,
                     _("Prevented by \"%s\" base flag on a cardinally adjacent tile.\n"),
                     base_flag_id_name(preq->source.value.baseflag));
      }
      return TRUE;
    case REQ_RANGE_ADJACENT:
      if (preq->present) {
        cat_snprintf(buf, bufsz,
                     _("Requires \"%s\" base flag on an adjacent tile.\n"),
                     base_flag_id_name(preq->source.value.baseflag));
      } else {
        cat_snprintf(buf, bufsz,
                     _("Prevented by \"%s\" base flag on an adjacent tile.\n"),
                     base_flag_id_name(preq->source.value.baseflag));
      }
      return TRUE;
    case REQ_RANGE_CITY:
      if (preq->present) {
        cat_snprintf(buf, bufsz,
                     _("Requires \"%s\" base flag on a tile in the city.\n"),
                     base_flag_id_name(preq->source.value.baseflag));
      } else {
        cat_snprintf(buf, bufsz,
                     _("Prevented by \"%s\" base flag on a tile in the city.\n"),
                     base_flag_id_name(preq->source.value.baseflag));
      }
      return TRUE;
    case REQ_RANGE_CONTINENT:
    case REQ_RANGE_PLAYER:
    case REQ_RANGE_ALLIANCE:
    case REQ_RANGE_WORLD:
    case REQ_RANGE_COUNT:
      /* Not supported. */
      break;
    }
    break;

  case VUT_ROADFLAG:
    switch (preq->range) {
    case REQ_RANGE_LOCAL:
      if (preq->present) {
        cat_snprintf(buf, bufsz, _("Requires \"%s\" road flag on the tile.\n"),
                     road_flag_id_name(preq->source.value.roadflag));
      } else {
        cat_snprintf(buf, bufsz, _("Prevented by \"%s\" road flag on the tile.\n"),
                     road_flag_id_name(preq->source.value.roadflag));
      }
      return TRUE;
    case REQ_RANGE_CADJACENT:
      if (preq->present) {
        cat_snprintf(buf, bufsz,
                     _("Requires \"%s\" road flag on a cardinally adjacent tile.\n"),
                     road_flag_id_name(preq->source.value.roadflag));
      } else {
        cat_snprintf(buf, bufsz,
                     _("Prevented by \"%s\" road flag on a cardinally adjacent tile.\n"),
                     road_flag_id_name(preq->source.value.roadflag));
      }
      return TRUE;
    case REQ_RANGE_ADJACENT:
      if (preq->present) {
        cat_snprintf(buf, bufsz,
                     _("Requires \"%s\" road flag on an adjacent tile.\n"),
                     road_flag_id_name(preq->source.value.roadflag));
      } else {
        cat_snprintf(buf, bufsz,
                     _("Prevented by \"%s\" road flag on an adjacent tile.\n"),
                     road_flag_id_name(preq->source.value.roadflag));
      }
      return TRUE;
    case REQ_RANGE_CITY:
      if (preq->present) {
        cat_snprintf(buf, bufsz,
                     _("Requires \"%s\" road flag on a tile in the city.\n"),
                     road_flag_id_name(preq->source.value.roadflag));
      } else {
        cat_snprintf(buf, bufsz,
                     _("Prevented by \"%s\" road flag on a tile in the city.\n"),
                     road_flag_id_name(preq->source.value.roadflag));
      }
      return TRUE;
    case REQ_RANGE_CONTINENT:
    case REQ_RANGE_PLAYER:
    case REQ_RANGE_ALLIANCE:
    case REQ_RANGE_WORLD:
    case REQ_RANGE_COUNT:
      /* Not supported. */
      break;
    }
    break;

  case VUT_MINYEAR:
    if (preq->present) {
      cat_snprintf(buf, bufsz, _("Requires we reached the year %d.\n"),
                   preq->source.value.minyear);
    } else {
      cat_snprintf(buf, bufsz, _("Requires we have not yet reached the year %d.\n"),
                   preq->source.value.minyear);
    }
    return TRUE;

  case VUT_TERRAINALTER:
    switch (preq->range) {
    case REQ_RANGE_LOCAL:
      if (preq->present) {
        cat_snprintf(buf, bufsz,
                     _("Requires terrain on which alteration %s is possible.\n"),
                     Q_(terrain_alteration_name(preq->source.value.terrainalter)));
      } else {
        cat_snprintf(buf, bufsz,
                     _("Prevented by terrain on which alteration %s cannot be made.\n"),
                     Q_(terrain_alteration_name(preq->source.value.terrainalter)));
      }
      return TRUE;
    case REQ_RANGE_CADJACENT:
    case REQ_RANGE_ADJACENT:
    case REQ_RANGE_CITY:
    case REQ_RANGE_CONTINENT:
    case REQ_RANGE_PLAYER:
    case REQ_RANGE_ALLIANCE:
    case REQ_RANGE_WORLD:
    case REQ_RANGE_COUNT:
      /* Not supported. */
      break;
    }
    break;

  case VUT_CITYTILE:
    if (preq->present) {
      cat_snprintf(buf, bufsz, _("Applies only to city centers.\n"));
    } else {
      cat_snprintf(buf, bufsz, _("Does not apply to city centers.\n"));
    }
    return TRUE;

  case VUT_COUNT:
    break;
  }

  {
    char text[256];

    log_error("Requirement %s in range %d is not supported in helpdata.c.",
              universal_name_translation(&preq->source, text, sizeof(text)),
              preq->range);
  }

  return FALSE;
}

/****************************************************************************
  Generate text for what this requirement source allows.  Something like

    "Allows Communism (with University).\n"
    "Allows Mfg. Plant (with Factory).\n"
    "Allows Library (absent Fundamentalism).\n"
    "Prevents Harbor.\n"

  This should be called to generate helptext for every possible source
  type.  Note this doesn't handle effects but rather production
  requirements (currently only building reqs).

  NB: This function overwrites any existing buffer contents by writing the
  generated text to the start of the given 'buf' pointer (i.e. it does
  NOT append like cat_snprintf).
****************************************************************************/
static void insert_allows(struct universal *psource,
			  char *buf, size_t bufsz)
{
  buf[0] = '\0';

  /* FIXME: show other data like range and survives. */

  improvement_iterate(pimprove) {
    requirement_vector_iterate(&pimprove->reqs, req) {
      if (are_universals_equal(psource, &req->source)) {
        if (req->present) {
          /* This source enables a building, but other sources may
           * also be required (or required to be absent). */
          char coreq_buf[512] = "", conoreq_buf[512] = "";

          requirement_vector_iterate(&pimprove->reqs, coreq) {
            if (!are_universals_equal(psource, &coreq->source)) {
              char buf2[512] = "";
              char *rbuf = coreq->present ? coreq_buf : conoreq_buf;

              universal_name_translation(&coreq->source,
                                         buf2, sizeof(buf2));
              fc_assert_action(sizeof(coreq_buf) == sizeof(conoreq_buf), break);
              if (rbuf[0] == '\0') {
                fc_strlcpy(rbuf, buf2, sizeof(coreq_buf));
              } else {
                cat_snprintf(rbuf, sizeof(coreq_buf),
                             Q_("?clistmore:, %s"), buf2);
              }
            }
          } requirement_vector_iterate_end;

          if (coreq_buf[0] != '\0') {
            if (conoreq_buf[0] != '\0') {
              cat_snprintf(buf, bufsz, _("Allows %s (with %s but no %s)."),
                           improvement_name_translation(pimprove),
                           coreq_buf, conoreq_buf);
            } else {
              cat_snprintf(buf, bufsz, _("Allows %s (with %s)."),
                           improvement_name_translation(pimprove),
                           coreq_buf);
            }
          } else {
            if (conoreq_buf[0] != '\0') {
              cat_snprintf(buf, bufsz, _("Allows %s (absent %s)."),
                           improvement_name_translation(pimprove),
                           conoreq_buf);
            } else {
              cat_snprintf(buf, bufsz, _("Allows %s."),
                           improvement_name_translation(pimprove));
            }
          }
        } else {
          /* This source can, on its own, prevent a building. */
          cat_snprintf(buf, bufsz, _("Prevents %s."),
                       improvement_name_translation(pimprove));
        }
        cat_snprintf(buf, bufsz, "\n");
      }
    } requirement_vector_iterate_end;
  } improvement_iterate_end;
}

/****************************************************************
  Allocate and initialize new help item
*****************************************************************/
static struct help_item *new_help_item(int type)
{
  struct help_item *pitem;
  
  pitem = fc_malloc(sizeof(struct help_item));
  pitem->topic = NULL;
  pitem->text = NULL;
  pitem->type = type;
  return pitem;
}

/****************************************************************
 for help_list_sort(); sort by topic via compare_strings()
 (sort topics with more leading spaces after those with fewer)
*****************************************************************/
static int help_item_compar(const struct help_item *const *ppa,
                            const struct help_item *const *ppb)
{
  const struct help_item *ha, *hb;
  char *ta, *tb;
  ha = *ppa;
  hb = *ppb;
  for (ta = ha->topic, tb = hb->topic; *ta != '\0' && *tb != '\0'; ta++, tb++) {
    if (*ta != ' ') {
      if (*tb == ' ') return -1;
      break;
    } else if (*tb != ' ') {
      if (*ta == ' ') return 1;
      break;
    }
  }
  return compare_strings(ta, tb);
}

/****************************************************************
  pplayer may be NULL.
*****************************************************************/
void boot_help_texts(struct player *pplayer)
{
  static bool booted = FALSE;

  struct section_file *sf;
  const char *filename;
  struct help_item *pitem;
  int i;
  struct section_list *sec;
  const char **paras;
  size_t npara;
  char long_buffer[64000]; /* HACK: this may be overrun. */

  check_help_nodes_init();

  /* need to do something like this or bad things happen */
  popdown_help_dialog();

  if(!booted) {
    log_verbose("Booting help texts");
  } else {
    /* free memory allocated last time booted */
    free_help_texts();
    log_verbose("Rebooting help texts");
  }

  filename = fileinfoname(get_data_dirs(), "helpdata.txt");
  if (!filename) {
    log_error("Did not read help texts");
    return;
  }
  /* after following call filename may be clobbered; use sf->filename instead */
  if (!(sf = secfile_load(filename, FALSE))) {
    /* this is now unlikely to happen */
    log_error("failed reading help-texts from '%s':\n%s", filename,
              secfile_error());
    return;
  }

  sec = secfile_sections_by_name_prefix(sf, "help_");

  if (NULL != sec) {
    section_list_iterate(sec, psection) {
      const char *sec_name = section_name(psection);
      const char *gen_str = secfile_lookup_str(sf, "%s.generate", sec_name);
      
      if (gen_str) {
        enum help_page_type current_type = HELP_ANY;
        int level = strspn(gen_str, " ");
        gen_str += level;
        if (!booted) {
          continue; /* on initial boot data tables are empty */
        }
        for(i=2; help_type_names[i]; i++) {
          if(strcmp(gen_str, help_type_names[i])==0) {
            current_type = i;
            break;
          }
        }
        if (current_type == HELP_ANY) {
          log_error("bad help-generate category \"%s\"", gen_str);
          continue;
        }
        {
          /* Note these should really fill in pitem->text from auto-gen
             data instead of doing it later on the fly, but I don't want
             to change that now.  --dwp
          */
          char name[2048];
          struct help_list *category_nodes = help_list_new();

          switch (current_type) {
          case HELP_UNIT:
            unit_type_iterate(punittype) {
              pitem = new_help_item(current_type);
              fc_snprintf(name, sizeof(name), "%*s%s", level, "",
                          utype_name_translation(punittype));
              pitem->topic = fc_strdup(name);
              pitem->text = fc_strdup("");
              help_list_append(category_nodes, pitem);
            } unit_type_iterate_end;
            break;
          case HELP_TECH:
            advance_index_iterate(A_FIRST, i) {
              if (valid_advance_by_number(i)) {
                pitem = new_help_item(current_type);
                fc_snprintf(name, sizeof(name), "%*s%s", level, "",
                            advance_name_for_player(pplayer, i));
                pitem->topic = fc_strdup(name);
                pitem->text = fc_strdup("");
                help_list_append(category_nodes, pitem);
              }
            } advance_index_iterate_end;
            break;
          case HELP_TERRAIN:
            terrain_type_iterate(pterrain) {
              if (0 != strlen(terrain_rule_name(pterrain))) {
                pitem = new_help_item(current_type);
                fc_snprintf(name, sizeof(name), "%*s%s", level, "",
                            terrain_name_translation(pterrain));
                pitem->topic = fc_strdup(name);
                pitem->text = fc_strdup("");
                help_list_append(category_nodes, pitem);
              }
            } terrain_type_iterate_end;
            break;
          case HELP_BASE:
            base_type_iterate(pbase) {
              pitem = new_help_item(current_type);
              fc_snprintf(name, sizeof(name), "%*s%s", level, "",
                          base_name_translation(pbase));
              pitem->topic = fc_strdup(name);
              pitem->text = fc_strdup("");
              help_list_append(category_nodes, pitem);
            } base_type_iterate_end;
            break;
          case HELP_ROAD:
            road_type_iterate(proad) {
              pitem = new_help_item(current_type);
              fc_snprintf(name, sizeof(name), "%*s%s", level, "",
                          road_name_translation(proad));
              pitem->topic = fc_strdup(name);
              pitem->text = fc_strdup("");
              help_list_append(category_nodes, pitem);
            } road_type_iterate_end;
            break;
          case HELP_SPECIALIST:
            specialist_type_iterate(sp) {
              struct specialist *pspec = specialist_by_number(sp);
              pitem = new_help_item(current_type);
              fc_snprintf(name, sizeof(name), "%*s%s", level, "",
                          specialist_plural_translation(pspec));
              pitem->topic = fc_strdup(name);
              pitem->text = fc_strdup("");
              help_list_append(category_nodes, pitem);
            } specialist_type_iterate_end;
            break;
          case HELP_GOVERNMENT:
            governments_iterate(gov) {
              pitem = new_help_item(current_type);
              fc_snprintf(name, sizeof(name), "%*s%s", level, "",
                          government_name_translation(gov));
              pitem->topic = fc_strdup(name);
              pitem->text = fc_strdup("");
              help_list_append(category_nodes, pitem);
            } governments_iterate_end;
            break;
          case HELP_IMPROVEMENT:
            improvement_iterate(pimprove) {
              if (valid_improvement(pimprove) && !is_great_wonder(pimprove)) {
                pitem = new_help_item(current_type);
                fc_snprintf(name, sizeof(name), "%*s%s", level, "",
                            improvement_name_translation(pimprove));
                pitem->topic = fc_strdup(name);
                pitem->text = fc_strdup("");
                help_list_append(category_nodes, pitem);
              }
            } improvement_iterate_end;
            break;
          case HELP_WONDER:
            improvement_iterate(pimprove) {
              if (valid_improvement(pimprove) && is_great_wonder(pimprove)) {
                pitem = new_help_item(current_type);
                fc_snprintf(name, sizeof(name), "%*s%s", level, "",
                            improvement_name_translation(pimprove));
                pitem->topic = fc_strdup(name);
                pitem->text = fc_strdup("");
                help_list_append(category_nodes, pitem);
              }
            } improvement_iterate_end;
            break;
          case HELP_RULESET:
            pitem = new_help_item(HELP_RULESET);
            /*           pitem->topic = fc_strdup(_(game.control.name)); */
            fc_snprintf(name, sizeof(name), "%*s%s", level, "",
                        Q_(HELP_RULESET_ITEM));
            pitem->topic = fc_strdup(name);
            if (game.control.description[0] != '\0') {
              pitem->text = fc_strdup(_(game.control.description));
            } else {
              pitem->text =
                  fc_strdup(_("Current ruleset contains no description."));
            }
            help_list_append(help_nodes, pitem);
            break;
          case HELP_NATIONS:
            nations_iterate(pnation) {
              if (client_state() < C_S_RUNNING
                  || is_nation_pickable(pnation)) {
                pitem = new_help_item(current_type);
                fc_snprintf(name, sizeof(name), "%*s%s", level, "",
                            _(nation_rule_name(pnation)));
                pitem->topic = fc_strdup(name);
                pitem->text = fc_strdup("");
                help_list_append(category_nodes, pitem);
              }
            } nations_iterate_end;
            break;
          default:
            log_error("Bad current_type: %d.", current_type);
            break;
          }
          help_list_sort(category_nodes, help_item_compar);
          help_list_iterate(category_nodes, ptmp) {
            help_list_append(help_nodes, ptmp);
          } help_list_iterate_end;
          help_list_destroy(category_nodes);
          continue;
        }
      }
      
      /* It wasn't a "generate" node: */
      
      pitem = new_help_item(HELP_TEXT);
      pitem->topic = fc_strdup(Q_(secfile_lookup_str(sf, "%s.name",
                                                     sec_name)));

      paras = secfile_lookup_str_vec(sf, &npara, "%s.text", sec_name);

      long_buffer[0] = '\0';
      for (i=0; i<npara; i++) {
        bool inserted;
        const char *para = paras[i];
        if(strncmp(para, "$", 1)==0) {
          inserted =
            insert_generated_text(long_buffer, sizeof(long_buffer), para+1);
        } else {
          sz_strlcat(long_buffer, _(para));
          inserted = TRUE;
        }
        if (inserted && i!=npara-1) {
          sz_strlcat(long_buffer, "\n\n");
        }
      }
      free(paras);
      paras = NULL;
      pitem->text=fc_strdup(long_buffer);
      help_list_append(help_nodes, pitem);
    } section_list_iterate_end;

    section_list_destroy(sec);
  }

  secfile_check_unused(sf);
  secfile_destroy(sf);
  booted = TRUE;
  log_verbose("Booted help texts ok");
}

/****************************************************************
  The following few functions are essentially wrappers for the
  help_nodes help_list.  This allows us to avoid exporting the
  help_list, and instead only access it through a controlled
  interface.
*****************************************************************/

/****************************************************************
  Number of help items.
*****************************************************************/
int num_help_items(void)
{
  check_help_nodes_init();
  return help_list_size(help_nodes);
}

/****************************************************************
  Return pointer to given help_item.
  Returns NULL for 1 past end.
  Returns NULL and prints error message for other out-of bounds.
*****************************************************************/
const struct help_item *get_help_item(int pos)
{
  int size;
  
  check_help_nodes_init();
  size = help_list_size(help_nodes);
  if (pos < 0 || pos > size) {
    log_error("Bad index %d to get_help_item (size %d)", pos, size);
    return NULL;
  }
  if (pos == size) {
    return NULL;
  }
  return help_list_get(help_nodes, pos);
}

/****************************************************************
  Find help item by name and type.
  Returns help item, and sets (*pos) to position in list.
  If no item, returns pointer to static internal item with
  some faked data, and sets (*pos) to -1.
*****************************************************************/
const struct help_item*
get_help_item_spec(const char *name, enum help_page_type htype, int *pos)
{
  int idx;
  const struct help_item *pitem = NULL;
  static struct help_item vitem; /* v = virtual */
  static char vtopic[128];
  static char vtext[256];

  check_help_nodes_init();
  idx = 0;
  help_list_iterate(help_nodes, ptmp) {
    char *p=ptmp->topic;
    while (*p == ' ') {
      p++;
    }
    if(strcmp(name, p)==0 && (htype==HELP_ANY || htype==ptmp->type)) {
      pitem = ptmp;
      break;
    }
    idx++;
  }
  help_list_iterate_end;
  
  if(!pitem) {
    idx = -1;
    vitem.topic = vtopic;
    sz_strlcpy(vtopic, name);
    vitem.text = vtext;
    if(htype==HELP_ANY || htype==HELP_TEXT) {
      fc_snprintf(vtext, sizeof(vtext),
		  _("Sorry, no help topic for %s.\n"), vitem.topic);
      vitem.type = HELP_TEXT;
    } else {
      fc_snprintf(vtext, sizeof(vtext),
		  _("Sorry, no help topic for %s.\n"
		    "This page was auto-generated.\n\n"),
		  vitem.topic);
      vitem.type = htype;
    }
    pitem = &vitem;
  }
  *pos = idx;
  return pitem;
}

/****************************************************************
  Start iterating through help items;
  that is, reset iterator to start position.
  (Could iterate using get_help_item(), but that would be
  less efficient due to scanning to find pos.)
*****************************************************************/
void help_iter_start(void)
{
  check_help_nodes_init();
  help_nodes_iterator = help_list_head(help_nodes);
}

/****************************************************************
  Returns next help item; after help_iter_start(), this is
  the first item.  At end, returns NULL.
*****************************************************************/
const struct help_item *help_iter_next(void)
{
  const struct help_item *pitem;
  
  check_help_nodes_init();
  pitem = help_list_link_data(help_nodes_iterator);
  if (pitem) {
    help_nodes_iterator = help_list_link_next(help_nodes_iterator);
  }

  return pitem;
}


/****************************************************************
  FIXME:
  Also, in principle these could be auto-generated once, inserted
  into pitem->text, and then don't need to keep re-generating them.
  Only thing to be careful of would be changeable data, but don't
  have that here (for ruleset change or spacerace change must
  re-boot helptexts anyway).  Eg, genuinely dynamic information
  which could be useful would be if help system said which wonders
  have been built (or are being built and by who/where?)
*****************************************************************/

/**************************************************************************
  Write dynamic text for buildings (including wonders).  This includes
  the ruleset helptext as well as any automatically generated text.

  pplayer may be NULL.
  user_text, if non-NULL, will be appended to the text.
**************************************************************************/
char *helptext_building(char *buf, size_t bufsz, struct player *pplayer,
                        const char *user_text, struct impr_type *pimprove)
{
  bool reqs = FALSE;
  struct universal source = {
    .kind = VUT_IMPROVEMENT,
    .value = {.building = pimprove}
  };

  fc_assert_ret_val(NULL != buf && 0 < bufsz, NULL);
  buf[0] = '\0';

  if (NULL == pimprove) {
    return buf;
  }

  if (NULL != pimprove->helptext) {
    strvec_iterate(pimprove->helptext, text) {
      cat_snprintf(buf, bufsz, "%s\n\n", _(text));
    } strvec_iterate_end;
  }

  /* Add requirement text for improvement itself */
  requirement_vector_iterate(&pimprove->reqs, preq) {
    if (insert_requirement(buf, bufsz, pplayer, preq)) {
      reqs = TRUE;
    }
  } requirement_vector_iterate_end;
  if (reqs) {
    fc_strlcat(buf, "\n", bufsz);
  }

  requirement_vector_iterate(&pimprove->obsolete_by, pobs) {
    if (VUT_ADVANCE == pobs->source.kind) {
      cat_snprintf(buf, bufsz,
                   _("* The discovery of %s will make %s obsolete.\n"),
                   advance_name_for_player(pplayer,
                                           advance_number(pobs->source.value.advance)),
                   improvement_name_translation(pimprove));
    }
  } requirement_vector_iterate_end;

  if (is_small_wonder(pimprove)) {
    cat_snprintf(buf, bufsz,
                 _("* A 'small wonder': at most one of your cities may "
                   "possess this improvement.\n"));
  }
  /* (Great wonders are in their own help section explaining their
   * uniqueness, so we don't mention it here.) */

  if (building_has_effect(pimprove, EFT_ENABLE_NUKE)
      && num_role_units(UTYF_NUCLEAR) > 0) {
    struct unit_type *u = get_role_unit(UTYF_NUCLEAR, 0);

    cat_snprintf(buf, bufsz,
		 /* TRANS: 'Allows all players with knowledge of atomic
		  * power to build nuclear units.' */
		 _("* Allows all players with knowledge of %s "
		   "to build %s units.\n"),
		 advance_name_for_player(pplayer,
					 advance_number(u->require_advance)),
		 utype_name_translation(u));
    cat_snprintf(buf, bufsz, "  ");
  }

  insert_allows(&source, buf + strlen(buf), bufsz - strlen(buf));

  unit_type_iterate(u) {
    if (u->need_improvement == pimprove) {
      if (A_NEVER != u->require_advance) {
	cat_snprintf(buf, bufsz, _("* Allows %s (with %s).\n"),
		     utype_name_translation(u),
		     advance_name_for_player(pplayer,
					     advance_number(u->require_advance)));
      } else {
	cat_snprintf(buf, bufsz, _("* Allows %s.\n"),
		     utype_name_translation(u));
      }
    }
  } unit_type_iterate_end;

  if (improvement_has_flag(pimprove, IF_SAVE_SMALL_WONDER)) {
    cat_snprintf(buf, bufsz,
                 /* TRANS: don't translate 'savepalace' */
                 _("* If you lose the city containing this improvement, "
                   "it will be rebuilt for free in another of your cities "
                   "(if the 'savepalace' server setting is enabled).\n"));
  }

  if (user_text && user_text[0] != '\0') {
    cat_snprintf(buf, bufsz, "\n\n%s", user_text);
  }
  return buf;
}

#define techs_with_flag_iterate(flag, tech_id)				    \
{									    \
  Tech_type_id tech_id = 0;						    \
									    \
  while ((tech_id = advance_by_flag(tech_id, (flag))) != A_LAST) {

#define techs_with_flag_iterate_end		\
    tech_id++;					\
  }						\
}

/****************************************************************************
  Will the specified unit type fullfill the requirements in the list?
****************************************************************************/
static bool
unit_type_fulfills_requirement(struct unit_type *utype,
                               struct requirement_vector *reqs)
{
  fc_assert(utype);
  fc_assert(reqs);

  requirement_vector_iterate(reqs, preq) {
    bool found = FALSE;

    switch (preq->source.kind) {
    case VUT_UTYPE:
      found = utype == preq->source.value.utype;
      break;
    case VUT_UCLASS:
      found = utype_class(utype) == preq->source.value.uclass;
      break;
    case VUT_UTFLAG:
      found = utype_has_flag(utype, preq->source.value.unitflag);
      break;
    case VUT_UCFLAG:
      found = uclass_has_flag(utype_class(utype),
                              preq->source.value.unitclassflag);
      break;
    default:
      /* Not found and not relevant. */
      continue;
    };

    if (found) {
      if (!preq->present) {
        return FALSE;
      }
    } else {
      if (preq->present) {
        return FALSE;
      }
    }
  } requirement_vector_iterate_end;

  return TRUE;
}

/****************************************************************
  Append misc dynamic text for units.
  Transport capacity, unit flags, fuel.

  pplayer may be NULL.
*****************************************************************/
char *helptext_unit(char *buf, size_t bufsz, struct player *pplayer,
		    const char *user_text, struct unit_type *utype)
{
  bool can_be_veteran;
  int flagid;

  fc_assert_ret_val(NULL != buf && 0 < bufsz && NULL != user_text, NULL);

  if (!utype) {
    log_error("Unknown unit!");
    fc_strlcpy(buf, user_text, bufsz);
    return buf;
  }

  can_be_veteran = !utype_has_flag(utype, UTYF_NO_VETERAN)
    && utype_veteran_levels(utype) > 1;

  buf[0] = '\0';

  cat_snprintf(buf, bufsz,
               _("* Belongs to %s unit class.\n"),
               uclass_name_translation(utype_class(utype)));
  if (uclass_has_flag(utype_class(utype), UCF_CAN_OCCUPY_CITY)
      && !utype_has_flag(utype, UTYF_CIVILIAN)) {
    CATLSTR(buf, bufsz, _("  * Can occupy empty enemy cities.\n"));
  }
  if (!uclass_has_flag(utype_class(utype), UCF_TERRAIN_SPEED)) {
    CATLSTR(buf, bufsz, _("  * Speed is not affected by terrain.\n"));
  }
  if (!uclass_has_flag(utype_class(utype), UCF_TERRAIN_DEFENSE)) {
    CATLSTR(buf, bufsz, _("  * Does not get defense bonuses from terrain.\n"));
  }
  if (!uclass_has_flag(utype_class(utype), UCF_ZOC)) {
    CATLSTR(buf, bufsz, _("  * Not subject to zones of control.\n"));
  }
  if (uclass_has_flag(utype_class(utype), UCF_DAMAGE_SLOWS)) {
    CATLSTR(buf, bufsz, _("  * Slowed down while damaged.\n"));
  }
  if (uclass_has_flag(utype_class(utype), UCF_MISSILE)) {
    CATLSTR(buf, bufsz, _("  * Gets used up in making an attack.\n"));
  }
  if (uclass_has_flag(utype_class(utype), UCF_CAN_FORTIFY)
      && !utype_has_flag(utype, UTYF_SETTLERS)) {
    if (utype->defense_strength > 0) {
      CATLSTR(buf, bufsz,
              /* xgettext:no-c-format */
              _("  * Gets a 50% defensive bonus while in cities.\n"));
      CATLSTR(buf, bufsz,
              /* xgettext:no-c-format */
              _("  * May fortify, granting a 50% defensive bonus when not in "
                "a city.\n"));
    } else {
      CATLSTR(buf, bufsz,
              /* xgettext:no-c-format */
              _("  * May fortify to stay put.\n"));
    }
  }
  if (uclass_has_flag(utype_class(utype), UCF_UNREACHABLE)) {
    CATLSTR(buf, bufsz,
	    _("  * Is unreachable. Most units cannot attack this one.\n"));
  }
  if (uclass_has_flag(utype_class(utype), UCF_CAN_PILLAGE)) {
    CATLSTR(buf, bufsz,
	    _("  * Can pillage tile improvements.\n"));
  }
  if (uclass_has_flag(utype_class(utype), UCF_DOESNT_OCCUPY_TILE)
      && !utype_has_flag(utype, UTYF_CIVILIAN)) {
    CATLSTR(buf, bufsz,
	    _("  * Doesn't prevent enemy cities from working the tile it's on.\n"));
  }
  if (can_attack_non_native(utype)) {
    CATLSTR(buf, bufsz,
	    _("  * Can attack units on non-native tiles.\n"));
  }
  /* Must use flag to distinguish from UTYF_MARINES text. */
  if (uclass_has_flag(utype_class(utype), UCF_ATT_FROM_NON_NATIVE)) {
    CATLSTR(buf, bufsz,
            _("  * Can launch attack from non-native tiles.\n"));
  }

  if (utype->need_improvement) {
    cat_snprintf(buf, bufsz,
                 _("* Can only be built if there is %s in the city.\n"),
                 improvement_name_translation(utype->need_improvement));
  }

  if (utype->need_government) {
    cat_snprintf(buf, bufsz,
                 _("* Can only be built with %s as government.\n"),
                 government_name_translation(utype->need_government));
  }
  
  if (utype_has_flag(utype, UTYF_NOBUILD)) {
    CATLSTR(buf, bufsz, _("* May not be built in cities.\n"));
  }
  if (utype_has_flag(utype, UTYF_BARBARIAN_ONLY)) {
    CATLSTR(buf, bufsz, _("* Only barbarians may build this.\n"));
  }
  {
    const char *types[utype_count()];
    int i = 0;
    unit_type_iterate(utype2) {
      if (utype2->converted_to == utype) {
        types[i++] = utype_name_translation(utype2);
      }
    } unit_type_iterate_end;
    if (i > 0) {
      struct astring list = ASTRING_INIT;
      astr_build_or_list(&list, types, i);
      cat_snprintf(buf, bufsz,
                   /* TRANS: %s is a list of unit types separated by "or". */
                   _("* May be obtained by conversion of %s.\n"),
                   astr_str(&list));
      astr_free(&list);
    }
  }
  if (NULL != utype->converted_to) {
    cat_snprintf(buf, bufsz,
                 /* TRANS: %s is a unit type. */
                 _("* May be converted into %s.\n"),
                 utype_name_translation(utype->converted_to));
  }
  if (utype_has_flag(utype, UTYF_NOHOME)) {
    CATLSTR(buf, bufsz, _("* Never has a home city.\n"));
  }
  if (utype_has_flag(utype, UTYF_GAMELOSS)) {
    CATLSTR(buf, bufsz, _("* Losing this unit will lose you the game!\n"));
  }
  if (utype_has_flag(utype, UTYF_UNIQUE)) {
    CATLSTR(buf, bufsz,
	    _("* Each player may only have one of this type of unit.\n"));
  }
  for (flagid = UTYF_USER_FLAG_1 ; flagid <= UTYF_LAST_USER_FLAG; flagid++) {
    if (utype_has_flag(utype, flagid)) {
      const char *helptxt = unit_type_flag_helptxt(flagid);

      if (helptxt != NULL) {
        CATLSTR(buf, bufsz, "* ");
        CATLSTR(buf, bufsz, _(helptxt));
        CATLSTR(buf, bufsz, "\n");
      }
    }
  }
  if (utype->pop_cost > 0) {
    cat_snprintf(buf, bufsz,
                 PL_("* Requires %d population to build.\n",
                     "* Requires %d population to build.\n", utype->pop_cost),
                 utype->pop_cost);
  }
  if (0 < utype->transport_capacity) {
    const char *classes[uclass_count()];
    int i = 0;
    struct astring list = ASTRING_INIT;

    unit_class_iterate(uclass) {
      if (can_unit_type_transport(utype, uclass)) {
        classes[i++] = uclass_name_translation(uclass);
      }
    } unit_class_iterate_end;
    astr_build_or_list(&list, classes, i);

    cat_snprintf(buf, bufsz,
                 /* TRANS: %s is a list of unit classes separated by "or". */
                 PL_("* Can carry and refuel %d %s unit.\n",
                     "* Can carry and refuel up to %d %s units.\n",
                     utype->transport_capacity),
                 utype->transport_capacity, astr_str(&list));
    astr_free(&list);
  }
  if (utype_has_flag(utype, UTYF_TRIREME)) {
    CATLSTR(buf, bufsz, _("* Must stay next to coast.\n"));
  }
  if (utype_has_flag(utype, UTYF_TRADE_ROUTE)) {
    cat_snprintf(buf, bufsz,
                 /* TRANS: "Manhattan" distance is the distance along
                  * gridlines, with no diagonals allowed. */
                 _("* Can establish trade routes (must travel to target"
                   " city and must be at least %d tiles [in Manhattan"
                   " distance] from this unit's home city).\n"),
                 game.info.trademindist);
  }
  if (utype_has_flag(utype, UTYF_HELP_WONDER)) {
    cat_snprintf(buf, bufsz,
		 _("* Can help build wonders (adds %d production).\n"),
		 utype_build_shield_cost(utype));
  }
  if (utype_has_flag(utype, UTYF_UNDISBANDABLE)) {
    CATLSTR(buf, bufsz, _("* May not be disbanded.\n"));
  } else {
    CATLSTR(buf, bufsz,
	    /* xgettext:no-c-format */
	    _("* May be disbanded in a city to recover 50% of the"
	      " production cost.\n"));
  }
  if (utype_has_flag(utype, UTYF_CITIES)) {
    CATLSTR(buf, bufsz, _("* Can build new cities.\n"));
  }
  if (utype_has_flag(utype, UTYF_ADD_TO_CITY)) {
    cat_snprintf(buf, bufsz,
                 /* TRANS: Plural in "%d population", not "size %d". */
		 PL_("* Can add on %d population to cities of no more than"
                     " size %d.\n",
		     "* Can add on %d population to cities of no more than"
                     " size %d.\n", utype_pop_value(utype)),
		 utype_pop_value(utype),
		 game.info.add_to_size_limit - utype_pop_value(utype));
  }
  if (utype_has_flag(utype, UTYF_SETTLERS)) {
    /* Roads, rail, mines, irrigation. */
    CATLSTR(buf, bufsz, _("* Can build roads and railroads.\n"));
    CATLSTR(buf, bufsz, _("* Can build mines on tiles.\n"));
    CATLSTR(buf, bufsz, _("* Can build irrigation and farmland on tiles.\n"));

    /* Pollution, fallout. */
    CATLSTR(buf, bufsz, _("* Can clean pollution from tiles.\n"));
    CATLSTR(buf, bufsz, _("* Can clean nuclear fallout from tiles.\n"));
  }
  /* FIXME: bases -- but there is no good way to find out which bases a unit
   * can conceivably build currently, so we have to remain silent. */
  if (utype_has_flag(utype, UTYF_SPY)) {
    CATLSTR(buf, bufsz, _("* Performs better diplomatic actions.\n"));
  }
  if (utype_has_flag(utype, UTYF_DIPLOMAT)
      || utype_has_flag(utype, UTYF_SUPERSPY)) {
    CATLSTR(buf, bufsz, _("* Defends cities against diplomatic actions.\n"));
  }
  if (utype_has_flag(utype, UTYF_SUPERSPY)) {
    CATLSTR(buf, bufsz, _("* Will never lose a diplomat-versus-diplomat fight.\n"));
  }
  if (utype_has_flag(utype, UTYF_SPY)
      && utype_has_flag(utype, UTYF_SUPERSPY)) {
    CATLSTR(buf, bufsz, _("* Will always survive a spy mission.\n"));
  }
  if (utype_has_flag(utype, UTYF_PARTIAL_INVIS)) {
    CATLSTR(buf, bufsz,
            _("* Is invisible except when next to an enemy unit or city.\n"));
  }
  if (utype_has_flag(utype, UTYF_ONLY_NATIVE_ATTACK)) {
    CATLSTR(buf, bufsz,
            _("* Can only attack units on native tiles.\n"));
  }
  /* Must use flag to distinguish from UCF_ATT_FROM_NON_NATIVE text. */
  if (utype_has_flag(utype, UTYF_MARINES)) {
    CATLSTR(buf, bufsz,
            _("* Can launch attack from non-native tiles.\n"));
  }
  if (utype_has_flag(utype, UTYF_PARATROOPERS)) {
    cat_snprintf(buf, bufsz,
		 _("* Can be paradropped from a friendly city or suitable base"
		   " (range: %d tiles).\n"),
		 utype->paratroopers_range);
  }
  if (!uclass_has_flag(utype_class(utype), UCF_MISSILE)
      && utype_has_flag(utype, UTYF_ONEATTACK)) {
    CATLSTR(buf, bufsz,
	    _("* Making an attack ends this unit's turn.\n"));
  }
  if (utype_has_flag(utype, UTYF_NUCLEAR)) {
    CATLSTR(buf, bufsz,
	    _("* This unit's attack causes a nuclear explosion!\n"));
  }
  if (utype_has_flag(utype, UTYF_CITYBUSTER)) {
    CATLSTR(buf, bufsz,
	    _("* Gets double firepower when attacking cities.\n"));
  }
  if (utype_has_flag(utype, UTYF_IGWALL)) {
    CATLSTR(buf, bufsz, _("* Ignores the effects of city walls.\n"));
  }
  if (utype_has_flag(utype, UTYF_BOMBARDER)) {
    cat_snprintf(buf, bufsz,
		 _("* Does bombard attacks (%d per turn).  These attacks will"
		   " only damage (never kill) the defender, but have no risk"
		   " for the attacker.\n"),
		 utype->bombard_rate);
  }
  if (utype_has_flag(utype, UTYF_IGTER)) {
    CATLSTR(buf, bufsz,
	    _("* Ignores terrain effects (treats all tiles as roads).\n"));
  }
  if (utype_has_flag(utype, UTYF_IGZOC)) {
    CATLSTR(buf, bufsz, _("* Ignores zones of control.\n"));
  }
  if (utype_has_flag(utype, UTYF_NOZOC)) {
    CATLSTR(buf, bufsz, _("* This unit imposes no zone of control on its"
                          " adjacent tiles, thus it will not restrict"
                          " movement of other units.\n"));
  }
  if (utype_has_flag(utype, UTYF_CIVILIAN)) {
    CATLSTR(buf, bufsz,
            _("* A non-military unit:\n"));
    CATLSTR(buf, bufsz,
            _("  * Cannot attack.\n"));
    CATLSTR(buf, bufsz,
            _("  * Doesn't impose martial law.\n"));
    CATLSTR(buf, bufsz,
            _("  * Can enter foreign territory regardless of peace treaty.\n"));
    CATLSTR(buf, bufsz,
            _("  * Doesn't prevent enemy cities from working the tile it's on.\n"));
  }
  if (utype_has_flag(utype, UTYF_FIELDUNIT)) {
    CATLSTR(buf, bufsz,
            _("* A field unit: one unhappiness applies even when non-aggressive.\n"));
  }
  if (utype_has_flag(utype, UTYF_CAPTURER)) {
    CATLSTR(buf, bufsz, _("* Can capture some enemy units.\n"));
  }
  if (utype_has_flag(utype, UTYF_CAPTURABLE)) {
    CATLSTR(buf, bufsz, _("* Can be captured by some enemy units.\n"));
  }
  if (utype_has_flag(utype, UTYF_SHIELD2GOLD)) {
    /* FIXME: the conversion shield => gold is activated if
     *        EFT_SHIELD2GOLD_FACTOR is not equal null; how to determine
     *        possible sources? */
    CATLSTR(buf, bufsz,
            _("* Under certain conditions the shield upkeep of this unit can "
              "be converted to gold upkeep.\n"));
  }

  unit_class_iterate(pclass) {
    if (uclass_has_flag(pclass, UCF_UNREACHABLE)
        && BV_ISSET(utype->targets, uclass_index(pclass))) {
      cat_snprintf(buf, bufsz,
                   _("* Can attack against %s units, which are usually not "
                     "reachable.\n"),
                   uclass_name_translation(pclass));
    }
  } unit_class_iterate_end;
  if (utype_fuel(utype)) {
    const char *types[utype_count()];
    int i = 0;

    unit_type_iterate(transport) {
      if (can_unit_type_transport(transport, utype_class(utype))) {
        types[i++] = utype_name_translation(transport);
      }
    } unit_type_iterate_end;

    if (0 == i) {
     cat_snprintf(buf, bufsz,
                   PL_("* Unit has to be in a city or a base"
                       " after %d turn.\n",
                       "* Unit has to be in a city or a base"
                       " after %d turns.\n",
                       utype_fuel(utype)),
                  utype_fuel(utype));
    } else {
      struct astring list = ASTRING_INIT;

      cat_snprintf(buf, bufsz,
                   /* TRANS: %s is a list of unit types separated by "or" */
                   PL_("* Unit has to be in a city, a base, or on a %s"
                       " after %d turn.\n",
                       "* Unit has to be in a city, a base, or on a %s"
                       " after %d turns.\n",
                       utype_fuel(utype)),
                   astr_build_or_list(&list, types, i), utype_fuel(utype));
      astr_free(&list);
    }
  }
  action_iterate(act) {
    if (action_get_actor_kind(act) != AAK_UNIT) {
      continue;
    }

    action_enabler_list_iterate(action_enablers_for_action(act), enabler) {
      if (unit_type_fulfills_requirement(utype, &(enabler->actor_reqs))) {
        char *target_kind
            = _(action_target_kind_name(action_get_target_kind(act)));

        cat_snprintf(buf, bufsz,
                     _("* Can do the action \'%s\' to some %s.\n"),
                     action_prepare_ui_name(act, "", ""),
                     target_kind);
        break;
      }
    } action_enabler_list_iterate_end;
  } action_iterate_end;
  action_iterate(act) {
    bool vulnerable;

    /* Not relevant */
    if (action_get_target_kind(act) != ATK_UNIT) {
      continue;
    }

    /* All units are immune to this since its not enabled */
    if (action_enabler_list_size(action_enablers_for_action(act)) == 0) {
      continue;
    }

    /* Must be immune in all cases */
    vulnerable = FALSE;
    action_enabler_list_iterate(action_enablers_for_action(act), enabler) {
      if (unit_type_fulfills_requirement(utype, &(enabler->target_reqs))) {
        vulnerable = TRUE;
        break;
      }
    } action_enabler_list_iterate_end;

    if (!vulnerable) {
      cat_snprintf(buf, bufsz,
                   _("* Immune to the action \'%s\'.\n"),
                   action_prepare_ui_name(act, "", ""));
    }
  } action_iterate_end;
  if (!can_be_veteran) {
    /* Only mention this if the game generally has veteran levels. */
    if (game.veteran->levels > 1) {
      CATLSTR(buf, bufsz, _("* Will never achieve veteran status.\n"));
    }
  } else {
    /* Not useful currently: */
#if 0
    /* Some units can never become veteran through combat in practice. */
    bool veteran_through_combat =
      !((utype->attack_strength == 0
         || uclass_has_flag(utype_class(utype), UCF_MISSILE))
        && utype->defense_strength == 0);
#endif
    /* FIXME: if we knew the raise chances on the client, we could be
     * more specific here about whether veteran status can be acquired
     * through combat/missions/work. */
    CATLSTR(buf, bufsz, _("* May acquire veteran status.\n"));
    if (utype_veteran_has_power_bonus(utype)) {
      if (utype->attack_strength > 0
          || utype->defense_strength > 0) {
        CATLSTR(buf, bufsz,
                _("  * Veterans have increased strength in combat.\n"));
      }
      /* SUPERSPY always wins/escapes */
      if ((utype_has_flag(utype, UTYF_DIPLOMAT)
           || utype_has_flag(utype, UTYF_SPY))
          && !utype_has_flag(utype, UTYF_SUPERSPY)) {
        CATLSTR(buf, bufsz,
                _("  * Veterans have improved chances in diplomatic "
                  "contests.\n"));
        if (utype_has_flag(utype, UTYF_SPY)) {
          CATLSTR(buf, bufsz,
                  _("  * Veterans are more likely to survive missions.\n"));
        }
      }
      if (utype_has_flag(utype, UTYF_SETTLERS)) {
        CATLSTR(buf, bufsz,
                _("  * Veterans work faster.\n"));
      }
    }
  }
  if (strlen(buf) > 0) {
    CATLSTR(buf, bufsz, "\n");
  }
  if (can_be_veteran && utype->veteran) {
    /* The case where the unit has only a single veteran level has already
     * been handled above, so keep quiet here if that happens */
    if (insert_veteran_help(buf, bufsz, utype->veteran,
            _("This type of unit has its own veteran levels:"), NULL)) {
      CATLSTR(buf, bufsz, "\n\n");
    }
  }
  if (NULL != utype->helptext) {
    strvec_iterate(utype->helptext, text) {
      cat_snprintf(buf, bufsz, "%s\n\n", _(text));
    } strvec_iterate_end;
  }
  CATLSTR(buf, bufsz, user_text);
  return buf;
}

/****************************************************************************
  Append misc dynamic text for advance/technology.

  pplayer may be NULL.
****************************************************************************/
void helptext_advance(char *buf, size_t bufsz, struct player *pplayer,
                      const char *user_text, int i)
{
  struct astring astr = ASTRING_INIT;
  struct advance *vap = valid_advance_by_number(i);
  struct universal source = {
    .kind = VUT_ADVANCE,
    .value = {.advance = vap}
  };
  int flagid;

  fc_assert_ret(NULL != buf && 0 < bufsz && NULL != user_text);
  fc_strlcpy(buf, user_text, bufsz);

  if (NULL == vap) {
    log_error("Unknown tech %d.", i);
    return;
  }

  if (player_invention_state(pplayer, i) != TECH_KNOWN) {
    if (player_invention_state(pplayer, i) == TECH_PREREQS_KNOWN) {
      cat_snprintf(buf, bufsz,
		   _("Starting now, researching %s would need %d bulbs."),
		   advance_name_for_player(pplayer, i),
		   base_total_bulbs_required(pplayer, i, FALSE));
    } else if (player_invention_reachable(pplayer, i)) {
      cat_snprintf(buf, bufsz,
                   PL_("To reach %s you need to obtain %d other"
                       " technology first. The whole project"
                       " will require %d bulbs to complete.",
                       "To reach %s you need to obtain %d other"
                       " technologies first. The whole project"
                       " will require %d bulbs to complete.",
                       num_unknown_techs_for_goal(pplayer, i) - 1),
		   advance_name_for_player(pplayer, i),
		   num_unknown_techs_for_goal(pplayer, i) - 1,
		   total_bulbs_required_for_goal(pplayer, i));
    } else {
      CATLSTR(buf, bufsz,
	      _("You cannot research this technology."));
    }
    if (!techs_have_fixed_costs()
     && player_invention_reachable(pplayer, i)) {
      CATLSTR(buf, bufsz,
	      _(" This number may vary depending on what "
		"other players research.\n"));
    } else {
      CATLSTR(buf, bufsz, "\n");
    }
  }

  CATLSTR(buf, bufsz, "\n");
  insert_allows(&source, buf + strlen(buf), bufsz - strlen(buf));

  if (advance_has_flag(i, TF_BONUS_TECH)) {
    cat_snprintf(buf, bufsz,
		 _("* The first player to research %s gets"
		   " an immediate advance.\n"),
		 advance_name_for_player(pplayer, i));
  }
  if (advance_has_flag(i, TF_POPULATION_POLLUTION_INC))
    CATLSTR(buf, bufsz,
            _("* Increases the pollution generated by the population.\n"));

  if (advance_has_flag(i, TF_BRIDGE)
      && role_units_translations(&astr, UTYF_SETTLERS, FALSE)) {
    cat_snprintf(buf, bufsz,
                 /* TRANS: %s is list of units separated by "and". */
                 _("* Allows %s to build roads on river tiles.\n"),
                 astr_str(&astr));
  }

  for (flagid = TECH_USER_1 ; flagid <= TECH_USER_LAST; flagid++) {
    if (advance_has_flag(i, flagid)) {
      const char *helptxt = tech_flag_helptxt(flagid);

      if (helptxt != NULL) {
        CATLSTR(buf, bufsz, "* ");
        CATLSTR(buf, bufsz, _(helptxt));
        CATLSTR(buf, bufsz, "\n");
      }
    }
  }

  /* FIXME: bases -- but there is no good way to find out which bases a tech
   * can enable currently, so we have to remain silent. */

  if (game.info.tech_upkeep_style != TECH_UPKEEP_NONE) {
    CATLSTR(buf, bufsz,
            _("* To preserve this technology for our nation some bulbs "
              "are needed each turn.\n"));
  }

  if (NULL != vap->helptext) {
    if (strlen(buf) > 0) {
      CATLSTR(buf, bufsz, "\n");
    }
    strvec_iterate(vap->helptext, text) {
      cat_snprintf(buf, bufsz, "%s\n\n", _(text));
    } strvec_iterate_end;
  }

  astr_free(&astr);
}

/****************************************************************
  Append text for terrain.
*****************************************************************/
void helptext_terrain(char *buf, size_t bufsz, struct player *pplayer,
		      const char *user_text, struct terrain *pterrain)
{
  struct universal source = {
    .kind = VUT_TERRAIN,
    .value = {.terrain = pterrain}
  };
  int flagid;

  fc_assert_ret(NULL != buf && 0 < bufsz);
  buf[0] = '\0';

  if (!pterrain) {
    log_error("Unknown terrain!");
    return;
  }

  insert_allows(&source, buf + strlen(buf), bufsz - strlen(buf));
  if (terrain_has_flag(pterrain, TER_NO_POLLUTION)) {
    CATLSTR(buf, bufsz,
	    _("* Pollution cannot be generated on this terrain."));
    CATLSTR(buf, bufsz, "\n");
    CATLSTR(buf, bufsz,
	    _("* Fallout cannot be generated on this terrain."));
    CATLSTR(buf, bufsz, "\n");
  }
  if (terrain_has_flag(pterrain, TER_NO_CITIES)) {
    CATLSTR(buf, bufsz,
	    _("* You cannot build cities on this terrain."));
    CATLSTR(buf, bufsz, "\n");
  }
  if (terrain_has_flag(pterrain, TER_UNSAFE_COAST)
      && terrain_type_terrain_class(pterrain) != TC_OCEAN) {
    CATLSTR(buf, bufsz,
	    _("* The coastline of this terrain is unsafe."));
    CATLSTR(buf, bufsz, "\n");
  }
  {
    const char *classes[uclass_count()];
    int i = 0;
    bv_extras extras;

    BV_CLR_ALL(extras);

    unit_class_iterate(uclass) {
      if (is_native_to_class(uclass, pterrain, extras)) {
        classes[i++] = uclass_name_translation(uclass);
      }
    } unit_class_iterate_end;

    if (0 < i) {
      struct astring list = ASTRING_INIT;

      /* TRANS: %s is a list of unit classes separated by "and". */
      cat_snprintf(buf, bufsz, _("* Can be traveled by %s units.\n"),
                   astr_build_and_list(&list, classes, i));
      astr_free(&list);
    }
  }
  for (flagid = TER_USER_1 ; flagid <= TER_USER_LAST; flagid++) {
    if (terrain_has_flag(pterrain, flagid)) {
      const char *helptxt = terrain_flag_helptxt(flagid);

      if (helptxt != NULL) {
        CATLSTR(buf, bufsz, "* ");
        CATLSTR(buf, bufsz, _(helptxt));
        CATLSTR(buf, bufsz, "\n");
      }
    }
  }

  if (NULL != pterrain->helptext) {
    if (buf[0] != '\0') {
      CATLSTR(buf, bufsz, "\n");
    }
    strvec_iterate(pterrain->helptext, text) {
      CATLSTR(buf, bufsz, _(text));
    } strvec_iterate_end;
  }
  if (user_text && user_text[0] != '\0') {
    CATLSTR(buf, bufsz, "\n\n");
    CATLSTR(buf, bufsz, user_text);
  }
}

/****************************************************************************
  Append misc dynamic text for extras.
  Assumes build time and conflicts are handled in the GUI front-end.

  pplayer may be NULL.
****************************************************************************/
void helptext_extra(char *buf, size_t bufsz, struct player *pplayer,
                    const char *user_text, struct extra_type *pextra)
{
  struct base_type *pbase;
  struct road_type *proad;

  fc_assert_ret(NULL != buf && 0 < bufsz);
  buf[0] = '\0';

  if (!pextra) {
    log_error("Unknown extra!");
    return;
  }

  if (is_extra_caused_by(pextra, EC_BASE)) {
    pbase = pextra->data.base;
  } else {
    pbase = NULL;
  }

  if (is_extra_caused_by(pextra, EC_ROAD)) {
    proad = pextra->data.road;
  } else {
    proad = NULL;
  }

  if (pbase != NULL && pbase->helptext != NULL) {
    strvec_iterate(pbase->helptext, text) {
      cat_snprintf(buf, bufsz, "%s\n\n", _(text));
    } strvec_iterate_end;
  }

  if (proad != NULL && proad->helptext != NULL) {
    strvec_iterate(proad->helptext, text) {
      cat_snprintf(buf, bufsz, "%s\n\n", _(text));
    } strvec_iterate_end;
  }

  /* XXX Non-zero requirement vector is not a good test of whether
   * insert_requirement() will give any output. */
  if (requirement_vector_size(&pextra->reqs) > 0) {
    if (pextra->buildable) {
      CATLSTR(buf, bufsz, _("Requirements to build:\n"));
    }
    requirement_vector_iterate(&pextra->reqs, preq) {
      (void) insert_requirement(buf, bufsz, pplayer, preq);
    } requirement_vector_iterate_end;
    CATLSTR(buf, bufsz, "\n");
  }

  {
    const char *classes[uclass_count()];
    int i = 0;

    unit_class_iterate(uclass) {
      if (is_native_extra_to_uclass(pextra, uclass)) {
        classes[i++] = uclass_name_translation(uclass);
      }
    } unit_class_iterate_end;

    if (0 < i) {
      struct astring list = ASTRING_INIT;

      if (proad != NULL) {
        /* TRANS: %s is a list of unit classes separated by "and". */
        cat_snprintf(buf, bufsz, _("* Can be traveled by %s units.\n"),
                     astr_build_and_list(&list, classes, i));
      } else {
        /* TRANS: %s is a list of unit classes separated by "and". */
        cat_snprintf(buf, bufsz, _("* Native to %s units.\n"),
                     astr_build_and_list(&list, classes, i));
      }
      astr_free(&list);

      if (extra_has_flag(pextra, EF_NATIVE_TILE)) {
        CATLSTR(buf, bufsz,
                _("  * Such units can move onto this tile even if it would "
                  "not normally be suitable terrain.\n"));
      }
      if (pbase != NULL) {
        if (BORDERS_DISABLED != game.info.borders
            && game.info.happyborders
            && base_has_flag(pbase, BF_NOT_AGGRESSIVE)) {
          /* "3 tiles" is hardcoded in is_friendly_city_near() */
          CATLSTR(buf, bufsz,
                  _("  * Such units situated here are not considered aggressive "
                    "if this tile is within 3 tiles of a friendly city.\n"));
        }
        if (territory_claiming_base(pbase)) {
          CATLSTR(buf, bufsz,
                  _("  * Can be captured by such units if at war with the "
                    "nation that currently owns it.\n"));
        }
        if (base_has_flag(pbase, BF_DIPLOMAT_DEFENSE)) {
          CATLSTR(buf, bufsz,
                  /* xgettext:no-c-format */
                  _("  * Diplomatic units get a 25% defense bonus in "
                    "diplomatic fights.\n"));
        }
      }
      if (pextra->defense_bonus) {
        cat_snprintf(buf, bufsz,
                     _("  * Such units get a %d%% defense bonus on this "
                       "tile.\n"),
                     pextra->defense_bonus);
      }
    }
  }

  if (proad != NULL) {
    if (proad->move_cost == 0) {
      CATLSTR(buf, bufsz, _("* Allows infinite movement.\n"));
    } else {
      cat_snprintf(buf, bufsz,
                   _("* Movement cost along this road is %d/%d movement points.\n"),
                   proad->move_cost, SINGLE_MOVE);
    }
  }

  if (!pextra->buildable) {
    CATLSTR(buf, bufsz,
            _("* Cannot be built.\n"));
  }
  if (is_extra_removed_by(pextra, ERM_PILLAGE)) {
    CATLSTR(buf, bufsz,
            _("* Can be pillaged by units.\n"));
  }
  if (is_extra_removed_by(pextra, ERM_CLEANPOLLUTION) || is_extra_removed_by(pextra, ERM_CLEANFALLOUT)) {
    CATLSTR(buf, bufsz,
            _("* Can be cleaned by units.\n"));
  }
  if (pbase != NULL) {
    if (game.info.killstack
        && base_has_flag(pbase, BF_NO_STACK_DEATH)) {
      CATLSTR(buf, bufsz,
              _("* Defeat of one unit does not cause death of all other units "
                "on this tile.\n"));
    }
    if (base_has_flag(pbase, BF_PARADROP_FROM)) {
      CATLSTR(buf, bufsz,
              _("* Units can paradrop from this tile.\n"));
    }
    if (territory_claiming_base(pbase)) {
      CATLSTR(buf, bufsz,
              _("* Extends national borders of the building nation.\n"));
    }
    if (pbase->vision_main_sq >= 0) {
      CATLSTR(buf, bufsz,
              _("* Grants permanent vision of an area around the tile to "
                "its owner.\n"));
    }
    if (pbase->vision_invis_sq >= 0) {
      CATLSTR(buf, bufsz,
              _("* Allows the owner to see normally invisible units in an "
                "area around the tile.\n"));
    }
  }

  if (proad != NULL) {
    if (road_has_flag(proad, RF_REQUIRES_BRIDGE)) {
      /* TODO: List actual technologies. */
      CATLSTR(buf, bufsz,
              _("* Cannot be built to river tiles unless some technology "
                "allowing bridge building is knowns.\n"));
    }
  }

  if (user_text && user_text[0] != '\0') {
    CATLSTR(buf, bufsz, "\n\n");
    CATLSTR(buf, bufsz, user_text);
  }
}

/****************************************************************************
  Append misc dynamic text for specialists.
  Assumes effects are described in the help text.

  pplayer may be NULL.
****************************************************************************/
void helptext_specialist(char *buf, size_t bufsz, struct player *pplayer,
                         const char *user_text, struct specialist *pspec)
{
  bool reqs = FALSE;

  fc_assert_ret(NULL != buf && 0 < bufsz);
  buf[0] = '\0';

  if (NULL != pspec->helptext) {
    strvec_iterate(pspec->helptext, text) {
      cat_snprintf(buf, bufsz, "%s\n\n", _(text));
    } strvec_iterate_end;
  }

  /* Requirements for this specialist. */
  requirement_vector_iterate(&pspec->reqs, preq) {
    if (insert_requirement(buf, bufsz, pplayer, preq)) {
      reqs = TRUE;
    }
  } requirement_vector_iterate_end;
  if (reqs) {
    fc_strlcat(buf, "\n", bufsz);
  }

  CATLSTR(buf, bufsz, user_text);
}

/****************************************************************
  Append text for government.

  pplayer may be NULL.

  TODO: Generalize the effects code for use elsewhere. Add
  other requirements.
*****************************************************************/
void helptext_government(char *buf, size_t bufsz, struct player *pplayer,
                         const char *user_text, struct government *gov)
{
  bool reqs = FALSE;
  struct universal source = {
    .kind = VUT_GOVERNMENT,
    .value = {.govern = gov}
  };

  fc_assert_ret(NULL != buf && 0 < bufsz);
  buf[0] = '\0';

  if (NULL != gov->helptext) {
    strvec_iterate(gov->helptext, text) {
      cat_snprintf(buf, bufsz, "%s\n\n", _(text));
    } strvec_iterate_end;
  }

  /* Add requirement text for government itself */
  requirement_vector_iterate(&gov->reqs, preq) {
    if (insert_requirement(buf, bufsz, pplayer, preq)) {
      reqs = TRUE;
    }
  } requirement_vector_iterate_end;
  if (reqs) {
    fc_strlcat(buf, "\n", bufsz);
  }

  /* Effects */
  CATLSTR(buf, bufsz, _("Features:\n"));
  insert_allows(&source, buf + strlen(buf), bufsz - strlen(buf));
  effect_list_iterate(get_req_source_effects(&source), peffect) {
    Output_type_id output_type = O_LAST;
    struct unit_class *unitclass = NULL;
    struct unit_type *unittype = NULL;
    enum unit_type_flag_id unitflag = unit_type_flag_id_invalid();
    struct strvec *outputs = strvec_new();
    struct astring outputs_or = ASTRING_INIT;
    struct astring outputs_and = ASTRING_INIT;
    bool extra_reqs = FALSE;
    bool world_value_valid = TRUE;

    /* Grab output type, if there is one */
    requirement_list_iterate(peffect->reqs, preq) {
      /* FIXME: perhaps we should treat any effect with negated requirements
       * or nreqs as too complex for us to explain here? */
      if (!preq->present) {
        continue;
      }
      switch (preq->source.kind) {
       case VUT_OTYPE:
         /* We should never have multiple outputtype requirements
          * in one list in the first place (it simply makes no sense,
          * output cannot be of multiple types)
          * Ruleset loading code should check against that. */
         fc_assert(output_type == O_LAST);
         output_type = preq->source.value.outputtype;
         strvec_append(outputs, get_output_name(output_type));
         break;
       case VUT_UCLASS:
         fc_assert(unitclass == NULL);
         unitclass = preq->source.value.uclass;
         /* FIXME: can't easily get world bonus for unit class */
         world_value_valid = FALSE;
         break;
       case VUT_UTYPE:
         fc_assert(unittype == NULL);
         unittype = preq->source.value.utype;
         break;
       case VUT_UTFLAG:
         if (!unit_type_flag_id_is_valid(unitflag)) {
           unitflag = preq->source.value.unitflag;
           /* FIXME: can't easily get world bonus for unit type flag */
           world_value_valid = FALSE;
         } else {
           /* Already have a unit flag requirement. More than one is too
            * complex for us to explain, so say nothing. */
           /* FIXME: we could handle this */
           extra_reqs = TRUE;
         }
         break;
       case VUT_GOVERNMENT:
         /* This is government we are generating helptext for.
          * ...or if not, it's ruleset bug that should never make it
          * this far. Fix ruleset loading code. */
         fc_assert(preq->source.value.govern == gov);
         break;
       default:
         extra_reqs = TRUE;
         world_value_valid = FALSE;
         break;
      };
    } requirement_list_iterate_end;

    if (!extra_reqs) {
      /* Only list effects that don't have extra requirements too complex
       * for us to handle.
       * Anything more complicated will have to be documented by hand by the
       * ruleset author. */

      /* Guard condition for simple player-wide effects descriptions.
       * (FIXME: in many cases, e.g. EFT_MAKE_CONTENT, additional requirements
       * like unittype will be ignored for gameplay, but will affect our
       * help here.) */
      const bool playerwide
        = world_value_valid && !unittype && (output_type == O_LAST);
      /* In some cases we give absolute values (world bonus + gov bonus).
       * We assume the fact that there's an effect with a gov requirement
       * is sufficient reason to list it in that gov's help.
       * Guard accesses to these with 'playerwide' or 'world_value_valid'. */
      int world_value = -999, net_value = -999;
      if (world_value_valid) {
        /* Get government-independent world value of effect if the extra
         * requirements were simple enough. */
        struct output_type *potype =
          output_type != O_LAST ? get_output_type(output_type) : NULL;
        world_value = 
          get_target_bonus_effects(NULL, NULL, NULL, NULL, NULL, NULL,
                                   unittype, potype, NULL, peffect->type);
        net_value = peffect->value + world_value;
      }

      if (output_type == O_LAST) {
        /* There was no outputtype requirement. Effect is active for all
         * output types. Generate lists for that. */
        bool harvested_only = TRUE; /* Consider only output types from fields */

        if (peffect->type == EFT_UPKEEP_FACTOR
            || peffect->type == EFT_UNIT_UPKEEP_FREE_PER_CITY
            || peffect->type == EFT_OUTPUT_BONUS
            || peffect->type == EFT_OUTPUT_BONUS_2) {
          /* Effect can use or require any kind of output */
          harvested_only = FALSE;
        }

        output_type_iterate(ot) {
          struct output_type *pot = get_output_type(ot);

          if (!harvested_only || pot->harvested) {
            strvec_append(outputs, _(pot->name));
          }
        } output_type_iterate_end;
      }

      if (0 == strvec_size(outputs)) {
         /* TRANS: Empty output type list, should never happen. */
        astr_set(&outputs_or, "%s", Q_("?outputlist: Nothing "));
        astr_set(&outputs_and, "%s", Q_("?outputlist: Nothing "));
      } else {
        strvec_to_or_list(outputs, &outputs_or);
        strvec_to_and_list(outputs, &outputs_and);
      }

      switch (peffect->type) {
      case EFT_UNHAPPY_FACTOR:
        if (playerwide) {
          /* FIXME: EFT_MAKE_CONTENT_MIL_PER would cancel this out. We assume
           * no-one will set both, so we don't bother handling it. */
          cat_snprintf(buf, bufsz,
                       PL_("* Military units away from home and field units"
                           " will each cause %d citizen to become unhappy.\n",
                           "* Military units away from home and field units"
                           " will each cause %d citizens to become unhappy.\n",
                           net_value),
                       net_value);
        } /* else too complicated or silly ruleset */
        break;
      case EFT_MAKE_CONTENT_MIL:
        if (playerwide) {
          cat_snprintf(buf, bufsz,
                       PL_("* Each of your cities will avoid %d unhappiness"
                           " caused by units.\n",
                           "* Each of your cities will avoid %d unhappiness"
                           " caused by units.\n",
                           peffect->value),
                       peffect->value);
        }
        break;
      case EFT_MAKE_CONTENT:
        if (playerwide) {
          cat_snprintf(buf, bufsz,
                       PL_("* Each of your cities will avoid %d unhappiness,"
                           " not including that caused by units.\n",
                           "* Each of your cities will avoid %d unhappiness,"
                           " not including that caused by units.\n",
                           peffect->value),
                       peffect->value);
        }
        break;
      case EFT_FORCE_CONTENT:
        if (playerwide) {
          cat_snprintf(buf, bufsz,
                       PL_("* Each of your cities will avoid %d unhappiness,"
                           " including unhappiness caused by units.\n",
                           "* Each of your cities will avoid %d unhappiness,"
                           " including unhappiness caused by units.\n",
                           peffect->value),
                       peffect->value);
        }
        break;
      case EFT_UPKEEP_FACTOR:
        if (world_value_valid && !unittype) {
          if (net_value == 0) {
            if (output_type != O_LAST) {
              cat_snprintf(buf, bufsz,
                           /* TRANS: %s is the output type, like 'shield'
                            * or 'gold'. */
                           _("* You pay no %s upkeep for your units.\n"),
                           astr_str(&outputs_or));
            } else {
              CATLSTR(buf, bufsz,
                      _("* You pay no upkeep for your units.\n"));
            }
          } else if (net_value != world_value) {
            double ratio = (double)net_value / world_value;
            if (output_type != O_LAST) {
              cat_snprintf(buf, bufsz,
                           /* TRANS: %s is the output type, like 'shield'
                            * or 'gold'. */
                           _("* You pay %.2g times normal %s upkeep for your "
                             "units.\n"),
                           ratio, astr_str(&outputs_and));
            } else {
              cat_snprintf(buf, bufsz,
                           _("* You pay %.2g times normal upkeep for your "
                             "units.\n"),
                           ratio);
            }
          } /* else this effect somehow has no effect; keep quiet */
        } /* else there was some extra condition making it complicated */
        break;
      case EFT_UNIT_UPKEEP_FREE_PER_CITY:
        if (!unittype) {
          if (output_type != O_LAST) {
            cat_snprintf(buf, bufsz,
                         /* TRANS: %s is the output type, like 'shield' or
                          * 'gold'; pluralised in %d but there is currently
                          * no way to control the singular/plural name of the
                          * output type; sorry */
                         PL_("* Each of your cities will avoid paying %d %s"
                             " upkeep for your units.\n",
                             "* Each of your cities will avoid paying %d %s"
                             " upkeep for your units.\n", peffect->value),
                         peffect->value, astr_str(&outputs_and));
          } else {
            cat_snprintf(buf, bufsz,
                         /* TRANS: Amount is subtracted from upkeep cost
                          * for each upkeep type. */
                         PL_("* Each of your cities will avoid paying %d"
                             " upkeep for your units.\n",
                             "* Each of your cities will avoid paying %d"
                             " upkeep for your units.\n", peffect->value),
                         peffect->value);
          }
        } /* else too complicated */
        break;
      case EFT_CIVIL_WAR_CHANCE:
        if (playerwide) {
          cat_snprintf(buf, bufsz,
                       _("* If you lose your capital,"
                         " the chance of civil war is %d%%.\n"),
                       net_value);
        }
        break;
      case EFT_EMPIRE_SIZE_BASE:
        if (playerwide) {
          cat_snprintf(buf, bufsz,
                       /* TRANS: %d should always be greater than 2. */
                       PL_("* When you have %d city, the first unhappy "
                           "citizen will appear in each city due to "
                           "civilization size.\n",
                           "* When you have %d cities, the first unhappy "
                           "citizen will appear in each city due to "
                           "civilization size.\n", net_value),
                       net_value);
        }
        break;
      case EFT_EMPIRE_SIZE_STEP:
        if (playerwide) {
          cat_snprintf(buf, bufsz,
                       /* TRANS: %d should always be greater than 2. */
                       PL_("* After the first unhappy citizen due to"
                           " civilization size, for each %d additional city"
                           " another unhappy citizen will appear.\n",
                           "* After the first unhappy citizen due to"
                           " civilization size, for each %d additional cities"
                           " another unhappy citizen will appear.\n",
                           net_value),
                       net_value);
        }
        break;
      case EFT_MAX_RATES:
        if (playerwide && game.info.changable_tax) {
          if (net_value < 100) {
            cat_snprintf(buf, bufsz,
                         _("* The maximum rate you can set for science,"
                            " gold, or luxuries is %d%%.\n"),
                         net_value);
          } else {
            CATLSTR(buf, bufsz,
                    _("* Has unlimited science/gold/luxuries rates.\n"));
          }
        }
        break;
      case EFT_MARTIAL_LAW_EACH:
        if (playerwide) {
          cat_snprintf(buf, bufsz,
                       PL_("* Your units may impose martial law."
                           " Each military unit inside a city will force %d"
                           " unhappy citizen to become content.\n",
                           "* Your units may impose martial law."
                           " Each military unit inside a city will force %d"
                           " unhappy citizens to become content.\n",
                           peffect->value),
                       peffect->value);
        }
        break;
      case EFT_MARTIAL_LAW_MAX:
        if (playerwide && net_value < 100) {
          cat_snprintf(buf, bufsz,
                       PL_("* A maximum of %d unit in each city can enforce"
                           " martial law.\n",
                           "* A maximum of %d units in each city can enforce"
                           " martial law.\n",
                           net_value),
                       net_value);
        }
        break;
      case EFT_RAPTURE_GROW:
        if (playerwide && net_value > 0) {
          cat_snprintf(buf, bufsz,
                       _("* You may grow your cities by means of "
                         "celebrations."));
          if (game.info.celebratesize > 1) {
            cat_snprintf(buf, bufsz,
                         /* TRANS: Preserve leading space. %d should always be
                          * 2 or greater. */
                         _(" (Cities below size %d cannot grow in this way.)"),
                         game.info.celebratesize);
          }
          cat_snprintf(buf, bufsz, "\n");
        }
        break;
      case EFT_NO_INCITE:
        if (playerwide && net_value > 0) {
          CATLSTR(buf, bufsz, _("* Your cities cannot be incited to "
                                "revolt.\n"));
        }
        break;
      case EFT_REVOLUTION_UNHAPPINESS:
        if (playerwide) {
          cat_snprintf(buf, bufsz,
                       PL_("* If a city is in disorder for more than %d turn "
                           "in a row, government will fall into anarchy.\n",
                           "* If a city is in disorder for more than %d turns "
                           "in a row, government will fall into anarchy.\n",
                           net_value),
                       net_value);
        }
        break;
      case EFT_HAS_SENATE:
        if (playerwide && net_value > 0) {
          CATLSTR(buf, bufsz,
                  _("* Has a senate that may prevent declaration of war.\n"));
        }
        break;
      case EFT_INSPIRE_PARTISANS:
        if (playerwide && net_value > 0) {
          CATLSTR(buf, bufsz,
                  _("* Allows partisans when cities are taken by the "
                    "enemy.\n"));
        }
        break;
      case EFT_HAPPINESS_TO_GOLD:
        if (playerwide && net_value > 0) {
          CATLSTR(buf, bufsz,
                  _("* Buildings that normally confer bonuses against"
                    " unhappiness will instead give gold.\n"));
        }
        break;
      case EFT_FANATICS:
        if (playerwide && net_value > 0) {
          CATLSTR(buf, bufsz, _("* Pays no upkeep for fanatics.\n"));
        }
        break;
      case EFT_NO_UNHAPPY:
        if (playerwide && net_value > 0) {
          CATLSTR(buf, bufsz, _("* Has no unhappy citizens.\n"));
        }
        break;
      case EFT_VETERAN_BUILD:
        {
          int conditions = 0;
          if (unitclass) {
            conditions++;
          }
          if (unittype) {
            conditions++;
          }
          if (unit_type_flag_id_is_valid(unitflag)) {
            conditions++;
          }
          if (conditions > 1) {
            /* More than one requirement on units, too complicated for us
             * to describe. */
            break;
          }
          if (unitclass) {
            /* FIXME: account for multiple veteran levels, or negative
             * values. This might lie for complicated rulesets! */
            cat_snprintf(buf, bufsz,
                         /* TRANS: %s is a unit class */
                         Q_("?unitclass:* New %s units will be veteran.\n"),
                         uclass_name_translation(unitclass));
          } else if (unit_type_flag_id_is_valid(unitflag)) {
            /* FIXME: same problems as unitclass */
            cat_snprintf(buf, bufsz,
                         /* TRANS: %s is a unit type flag */
                         Q_("?unitflag:* New %s units will be veteran.\n"),
                         unit_type_flag_id_name(unitflag));
          } else if (unittype != NULL) {
            if (world_value_valid && net_value > 0) {
              /* Here we can be specific about veteran level, and get
               * net value correct. */
              int maxlvl = utype_veteran_system(unittype)->levels - 1;
              const struct veteran_level *vlevel =
                utype_veteran_level(unittype, MIN(net_value, maxlvl));
              cat_snprintf(buf, bufsz,
                           /* TRANS: "* New Partisan units will have the rank
                            * of elite." */
                           Q_("?unittype:* New %s units will have the rank "
                              "of %s.\n"),
                           utype_name_translation(unittype),
                           name_translation(&vlevel->name));
            } /* else complicated */
          } else {
            /* No extra criteria. */
            /* FIXME: same problems as above */
            cat_snprintf(buf, bufsz,
                         _("* New units will be veteran.\n"));
          }
        }
        break;
      case EFT_OUTPUT_PENALTY_TILE:
        if (world_value_valid) {
          cat_snprintf(buf, bufsz,
                       /* TRANS: %s is list of output types, with 'or';
                        * pluralised in %d but of course the output types
                        * can't be pluralised; sorry */
                       PL_("* Each worked tile that gives more than %d %s will"
                           " suffer a -1 penalty, unless the city working it"
                           " is celebrating.",
                           "* Each worked tile that gives more than %d %s will"
                           " suffer a -1 penalty, unless the city working it"
                           " is celebrating.", net_value),
                       net_value, astr_str(&outputs_or));
          if (game.info.celebratesize > 1) {
            cat_snprintf(buf, bufsz,
                         /* TRANS: Preserve leading space. %d should always be
                          * 2 or greater. */
                         _(" (Cities below size %d will not celebrate.)"),
                         game.info.celebratesize);
          }
          cat_snprintf(buf, bufsz, "\n");
        }
        break;
      case EFT_OUTPUT_INC_TILE_CELEBRATE:
        cat_snprintf(buf, bufsz,
                     /* TRANS: %s is list of output types, with 'or' */
                     PL_("* Each worked tile with at least 1 %s will yield"
                         " %d more of it while the city working it is"
                         " celebrating.",
                         "* Each worked tile with at least 1 %s will yield"
                         " %d more of it while the city working it is"
                         " celebrating.", peffect->value),
                     astr_str(&outputs_or), peffect->value);
        if (game.info.celebratesize > 1) {
          cat_snprintf(buf, bufsz,
                       /* TRANS: Preserve leading space. %d should always be
                        * 2 or greater. */
                       _(" (Cities below size %d will not celebrate.)"),
                       game.info.celebratesize);
        }
        cat_snprintf(buf, bufsz, "\n");
        break;
      case EFT_OUTPUT_INC_TILE:
        cat_snprintf(buf, bufsz,
                     /* TRANS: %s is list of output types, with 'or' */
                     PL_("* Each worked tile with at least 1 %s will yield"
                         " %d more of it.\n",
                         "* Each worked tile with at least 1 %s will yield"
                         " %d more of it.\n", peffect->value),
                     astr_str(&outputs_or), peffect->value);
        break;
      case EFT_OUTPUT_BONUS:
      case EFT_OUTPUT_BONUS_2:
        /* FIXME: makes most sense iff world_value == 0 */
        cat_snprintf(buf, bufsz,
                     /* TRANS: %s is list of output types, with 'and' */
                     _("* %s production is increased %d%%.\n"),
                     astr_str(&outputs_and), peffect->value);
        break;
      case EFT_OUTPUT_WASTE:
        if (world_value_valid) {
          if (net_value > 30) {
            cat_snprintf(buf, bufsz,
                         /* TRANS: %s is list of output types, with 'and' */
                         _("* %s production will suffer massive losses.\n"),
                         astr_str(&outputs_and));
          } else if (net_value >= 15) {
            cat_snprintf(buf, bufsz,
                         /* TRANS: %s is list of output types, with 'and' */
                         _("* %s production will suffer some losses.\n"),
                         astr_str(&outputs_and));
          } else if (net_value > 0) {
            cat_snprintf(buf, bufsz,
                         /* TRANS: %s is list of output types, with 'and' */
                         _("* %s production will suffer a small amount "
                           "of losses.\n"),
                         astr_str(&outputs_and));
          }
        }
        break;
      case EFT_HEALTH_PCT:
        if (playerwide) {
          if (peffect->value > 0) {
            CATLSTR(buf, bufsz, _("* Increases the chance of plague"
                                  " within your cities.\n"));
          } else if (peffect->value < 0) {
            CATLSTR(buf, bufsz, _("* Decreases the chance of plague"
                                  " within your cities.\n"));
          }
        }
        break;
      case EFT_OUTPUT_WASTE_BY_DISTANCE:
        if (world_value_valid) {
          if (net_value >= 3) {
            cat_snprintf(buf, bufsz,
                         /* TRANS: %s is list of output types, with 'and' */
                         _("* %s losses will increase quickly"
                           " with distance from capital.\n"),
                         astr_str(&outputs_and));
          } else if (net_value == 2) {
            cat_snprintf(buf, bufsz,
                         /* TRANS: %s is list of output types, with 'and' */
                         _("* %s losses will increase"
                           " with distance from capital.\n"),
                         astr_str(&outputs_and));
          } else if (net_value > 0) {
            cat_snprintf(buf, bufsz,
                         /* TRANS: %s is list of output types, with 'and' */
                         _("* %s losses will increase slowly"
                           " with distance from capital.\n"),
                         astr_str(&outputs_and));
          }
        }
        break;
      case EFT_MIGRATION_PCT:
        if (playerwide) {
          if (peffect->value > 0) {
            CATLSTR(buf, bufsz, _("* Increases the chance of migration"
                                  " into your cities.\n"));
          } else if (peffect->value < 0) {
            CATLSTR(buf, bufsz, _("* Decreases the chance of migration"
                                  " into your cities.\n"));
          }
        }
        break;
      default:
        break;
      };
    }

    strvec_destroy(outputs);
    astr_free(&outputs_or);
    astr_free(&outputs_and);

  } effect_list_iterate_end;

  unit_type_iterate(utype) {
    if (utype->need_government == gov) {
      cat_snprintf(buf, bufsz,
                   _("* Allows you to build %s.\n"),
                   utype_name_translation(utype));
    }
  } unit_type_iterate_end;

  /* Action immunity */
  action_iterate(act) {
    if (action_immune_government(gov, act)) {
      cat_snprintf(buf, bufsz,
                   _("* Makes your %s immune to the action \'%s\'.\n"),
                   _(action_target_kind_name(action_get_target_kind(act))),
                   action_prepare_ui_name(act, "", ""));
    }
  } action_iterate_end;

  if (user_text && user_text[0] != '\0') {
    cat_snprintf(buf, bufsz, "\n%s", user_text);
  }
}

/****************************************************************
  Returns pointer to static string with eg: "1 shield, 1 unhappy"
*****************************************************************/
char *helptext_unit_upkeep_str(struct unit_type *utype)
{
  static char buf[128];
  int any = 0;

  if (!utype) {
    log_error("Unknown unit!");
    return "";
  }


  buf[0] = '\0';
  output_type_iterate(o) {
    if (utype->upkeep[o] > 0) {
      /* TRANS: "2 Food" or ", 1 Shield" */
      cat_snprintf(buf, sizeof(buf), _("%s%d %s"),
	      (any > 0 ? Q_("?blistmore:, ") : ""), utype->upkeep[o],
	      get_output_name(o));
      any++;
    }
  } output_type_iterate_end;
  if (utype->happy_cost > 0) {
    /* TRANS: "2 Unhappy" or ", 1 Unhappy" */
    cat_snprintf(buf, sizeof(buf), _("%s%d Unhappy"),
	    (any > 0 ? Q_("?blistmore:, ") : ""), utype->happy_cost);
    any++;
  }

  if (any == 0) {
    /* strcpy(buf, _("None")); */
    fc_snprintf(buf, sizeof(buf), "%d", 0);
  }
  return buf;
}

/****************************************************************************
  Returns nation legend and characteristics
****************************************************************************/
void helptext_nation(char *buf, size_t bufsz, struct nation_type *pnation,
		     const char *user_text)
{
  fc_assert_ret(NULL != buf && 0 < bufsz);
  buf[0] = '\0';

  if (pnation->legend[0] != '\0') {
    /* Client side legend is stored already translated */
    sprintf(buf, "%s\n\n", pnation->legend);
  }
  
  cat_snprintf(buf, bufsz,
               _("Initial government is %s.\n"),
               government_name_translation(pnation->init_government));
  if (pnation->init_techs[0] != A_LAST) {
    const char *tech_names[MAX_NUM_TECH_LIST];
    int i;
    struct astring list = ASTRING_INIT;
    for (i = 0; i < MAX_NUM_TECH_LIST; i++) {
      if (pnation->init_techs[i] == A_LAST) {
        break;
      }
      tech_names[i] =
        advance_name_translation(advance_by_number(pnation->init_techs[i]));
    }
    astr_build_and_list(&list, tech_names, i);
    if (game.rgame.global_init_techs[0] != A_LAST) {
      cat_snprintf(buf, bufsz,
                   /* TRANS: %s is an and-separated list of techs */
                   _("Starts with knowledge of %s in addition to the standard "
                     "starting technologies.\n"), astr_str(&list));
    } else {
      cat_snprintf(buf, bufsz,
                   /* TRANS: %s is an and-separated list of techs */
                   _("Starts with knowledge of %s.\n"), astr_str(&list));
    }
    astr_free(&list);
  }
  if (pnation->init_units[0]) {
    const struct unit_type *utypes[MAX_NUM_UNIT_LIST];
    int count[MAX_NUM_UNIT_LIST];
    int i, j, n = 0, total = 0;
    /* Count how many of each type there is. */
    for (i = 0; i < MAX_NUM_UNIT_LIST; i++) {
      if (!pnation->init_units[i]) {
        break;
      }
      for (j = 0; j < n; j++) {
        if (pnation->init_units[i] == utypes[j]) {
          count[j]++;
          total++;
          break;
        }
      }
      if (j == n) {
        utypes[n] = pnation->init_units[i];
        count[n] = 1;
        total++;
        n++;
      }
    }
    {
      /* Construct the list of unit types and counts. */
      struct astring utype_names[MAX_NUM_UNIT_LIST];
      struct astring list = ASTRING_INIT;
      for (i = 0; i < n; i++) {
        astr_init(&utype_names[i]);
        if (count[i] > 1) {
          /* TRANS: a unit type followed by a count. For instance,
           * "Fighter (2)" means two Fighters. Count is never 1. 
           * Used in a list. */
          astr_set(&utype_names[i], _("%s (%d)"),
                   utype_name_translation(utypes[i]), count[i]);
        } else {
          astr_set(&utype_names[i], "%s", utype_name_translation(utypes[i]));
        }
      }
      {
        const char *utype_name_strs[MAX_NUM_UNIT_LIST];
        for (i = 0; i < n; i++) {
          utype_name_strs[i] = astr_str(&utype_names[i]);
        }
        astr_build_and_list(&list, utype_name_strs, n);
      }
      for (i = 0; i < n; i++) {
        astr_free(&utype_names[i]);
      }
      cat_snprintf(buf, bufsz,
                   /* TRANS: %s is an and-separated list of unit types
                    * possibly with counts. Plurality is in total number of
                    * units represented. */
                   PL_("Starts with the following additional unit: %s.\n",
                       "Starts with the following additional units: %s.\n",
                      total), astr_str(&list));
      astr_free(&list);
    }
  }
  if (pnation->init_buildings[0] != B_LAST) {
    const char *impr_names[MAX_NUM_BUILDING_LIST];
    int i;
    struct astring list = ASTRING_INIT;
    for (i = 0; i < MAX_NUM_BUILDING_LIST; i++) {
      if (pnation->init_buildings[i] == B_LAST) {
        break;
      }
      impr_names[i] =
        improvement_name_translation(
          improvement_by_number(pnation->init_buildings[i]));
    }
    astr_build_and_list(&list, impr_names, i);
    if (game.rgame.global_init_buildings[0] != B_LAST) {
      cat_snprintf(buf, bufsz,
                   /* TRANS: %s is an and-separated list of improvements */
                   _("First city will get %s for free in addition to the "
                     "standard improvements.\n"), astr_str(&list));
    } else {
      cat_snprintf(buf, bufsz,
                   /* TRANS: %s is an and-separated list of improvements */
                   _("First city will get %s for free.\n"), astr_str(&list));
    }
    astr_free(&list);
  }

  if (user_text && user_text[0] != '\0') {
    CATLSTR(buf, bufsz, "\n");
    CATLSTR(buf, bufsz, user_text);
  }
}
