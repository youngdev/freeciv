/****************************************************************************
 Freeciv - Copyright (C) 2004 - The Freeciv Team
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
****************************************************************************/

#ifdef HAVE_CONFIG_H
#include <fc_config.h>
#endif

/* utility */
#include "fcintl.h"
#include "string_vector.h"

/* common */
#include "extras.h"
#include "fc_types.h"
#include "game.h"
#include "movement.h"
#include "name_translation.h"
#include "unittype.h"

#include "road.h"

/**************************************************************************
  Return the road id.
**************************************************************************/
Road_type_id road_number(const struct road_type *proad)
{
  fc_assert_ret_val(NULL != proad, -1);

  return proad->id;
}

/**************************************************************************
  Return the road index.

  Currently same as road_number(), paired with road_count()
  indicates use as an array index.
**************************************************************************/
Road_type_id road_index(const struct road_type *proad)
{
  fc_assert_ret_val(NULL != proad, -1);

  /* FIXME: */
  /* return proad - roads; */
  return road_number(proad);
}

/**************************************************************************
  Return extra that road is.
**************************************************************************/
struct extra_type *road_extra_get(const struct road_type *proad)
{
  return proad->self;
}

/**************************************************************************
  Return the number of road_types.
**************************************************************************/
Road_type_id road_count(void)
{
  return game.control.num_road_types;
}

/****************************************************************************
  Return road type of given id.
****************************************************************************/
struct road_type *road_by_number(Road_type_id id)
{
  struct extra_type_list *roads;

  roads = extra_type_list_by_cause(EC_ROAD);

  if (roads == NULL || id < 0 || id >= extra_type_list_size(roads)) {
    return NULL;
  }

  return extra_road_get(extra_type_list_get(roads, id));
}

/****************************************************************************
  Initialize road_type structures.
****************************************************************************/
void road_type_init(struct extra_type *pextra, int idx)
{
  struct road_type *proad;

  proad = fc_malloc(sizeof(*proad));

  pextra->data.road = proad;

  proad->id = idx;
  proad->helptext = NULL;
  proad->self = pextra;
}

/****************************************************************************
  Free the memory associated with road types
****************************************************************************/
void road_types_free(void)
{
  road_type_iterate(proad) {
    if (NULL != proad->helptext) {
      strvec_destroy(proad->helptext);
      proad->helptext = NULL;
    }
  } road_type_iterate_end;
}

/****************************************************************************
  Return tile special that used to represent this road type.
****************************************************************************/
enum road_compat road_compat_special(const struct road_type *proad)
{
  return proad->compat;
}

/****************************************************************************
  Return road type represented by given compatibility special, or NULL if
  special does not represent road type at all.
****************************************************************************/
struct road_type *road_by_compat_special(enum road_compat compat)
{
  if (compat == ROCO_NONE) {
    return NULL;
  }

  road_type_iterate(proad) {
    if (road_compat_special(proad) == compat) {
      return proad;
    }
  } road_type_iterate_end;

  return NULL;
}

/****************************************************************************
  Return translated name of this road type.
****************************************************************************/
const char *road_name_translation(struct road_type *proad)
{
  struct extra_type *pextra = road_extra_get(proad);

  if (pextra == NULL) {
    return NULL;
  }

  return extra_name_translation(pextra);
}

/****************************************************************************
  Return untranslated name of this road type.
****************************************************************************/
const char *road_rule_name(const struct road_type *proad)
{
  struct extra_type *pextra = road_extra_get(proad);

  if (pextra == NULL) {
    return NULL;
  }

  return extra_rule_name(pextra);
}

/**************************************************************************
  Returns road type matching rule name or NULL if there is no road type
  with such name.
**************************************************************************/
struct road_type *road_type_by_rule_name(const char *name)
{
  struct extra_type *pextra = extra_type_by_rule_name(name);

  if (pextra == NULL) {
    return NULL;
  }

  return extra_road_get(pextra);
}

/**************************************************************************
  Returns road type matching the translated name, or NULL if there is no
  road type with that name.
**************************************************************************/
struct road_type *road_type_by_translated_name(const char *name)
{
  struct extra_type *pextra = extra_type_by_translated_name(name);

  if (pextra == NULL) {
    return NULL;
  }

  return extra_road_get(pextra);
}

/****************************************************************************
  Tells if road can build to tile if all other requirements are met.
****************************************************************************/
bool road_can_be_built(const struct road_type *proad, const struct tile *ptile)
{

  if (!(road_extra_get(proad)->buildable)) {
    /* Road type not buildable. */
    return FALSE;
  }

  if (tile_has_road(ptile, proad)) {
    /* Road exist already */
    return FALSE;
  }

  if (tile_terrain(ptile)->road_time == 0) {
    return FALSE;
  }

  return TRUE;
}

/****************************************************************************
  Tells if player can build road to tile with suitable unit.
****************************************************************************/
bool can_build_road_base(const struct road_type *proad,
                         const struct player *pplayer,
                         const struct tile *ptile)
{
  if (!road_can_be_built(proad, ptile)) {
    return FALSE;
  }

  if (road_has_flag(proad, RF_REQUIRES_BRIDGE)
      && !player_knows_techs_with_flag(pplayer, TF_BRIDGE)) {
    /* TODO: Cache list of road types with RF_PREVENTS_OTHER_ROADS
     *       after ruleset loading and use that list here instead
     *       of always iterating through all road types. */
    road_type_iterate(old) {
      if (road_has_flag(old, RF_PREVENTS_OTHER_ROADS)
          && tile_has_road(ptile, old)) {
        return FALSE;
      }
    } road_type_iterate_end;
  }

  return TRUE;
}

/****************************************************************************
  Tells if player can build road to tile with suitable unit.
****************************************************************************/
bool player_can_build_road(const struct road_type *proad,
                           const struct player *pplayer,
                           const struct tile *ptile)
{
  struct extra_type *pextra;

  if (!can_build_road_base(proad, pplayer, ptile)) {
    return FALSE;
  }

  pextra = road_extra_get(proad);

  return are_reqs_active(pplayer, tile_owner(ptile), NULL, NULL, ptile,
                         NULL, NULL, NULL, &pextra->reqs,
                         RPT_POSSIBLE);
}

/****************************************************************************
  Tells if unit can build road on tile.
****************************************************************************/
bool can_build_road(struct road_type *proad,
		    const struct unit *punit,
		    const struct tile *ptile)
{
  struct extra_type *pextra;
  struct player *pplayer = unit_owner(punit);

  if (!can_build_road_base(proad, pplayer, ptile)) {
    return FALSE;
  }

  pextra = road_extra_get(proad);

  return are_reqs_active(pplayer, tile_owner(ptile), NULL, NULL, ptile,
                         unit_type(punit), NULL, NULL, &pextra->reqs,
                         RPT_CERTAIN);
}

/****************************************************************************
  Count tiles with specified road near the tile. Can be called with NULL
  road.
****************************************************************************/
int count_road_near_tile(const struct tile *ptile, const struct road_type *proad)
{
  int count = 0;

  if (proad == NULL) {
    return 0;
  }

  adjc_iterate(ptile, adjc_tile) {
    if (tile_has_road(adjc_tile, proad)) {
      count++;
    }
  } adjc_iterate_end;

  return count;
}

/****************************************************************************
  Count tiles with any river near the tile.
****************************************************************************/
int count_river_near_tile(const struct tile *ptile,
                          const struct road_type *priver)
{
  int count = 0;

  cardinal_adjc_iterate(ptile, adjc_tile) {
    if (priver == NULL && tile_has_river(adjc_tile)) {
      /* Some river */
      count++;
    } else if (priver != NULL && tile_has_road(adjc_tile, priver)) {
      /* Specific river */
      count++;
    }
  } cardinal_adjc_iterate_end;

  return count;
}

/****************************************************************************
  Count tiles with river of specific type cardinally adjacent to the tile.
****************************************************************************/
int count_river_type_tile_card(const struct tile *ptile,
                               const struct road_type *priver,
                               bool percentage)
{
  int count = 0;
  int total = 0;

  fc_assert(priver != NULL);

  cardinal_adjc_iterate(ptile, adjc_tile) {
    if (tile_has_road(adjc_tile, priver)) {
      count++;
    }
    total++;
  } cardinal_adjc_iterate_end;

  if (percentage) {
    count = count * 100 / total;
  }
  return count;
}

/****************************************************************************
  Count tiles with river of specific type near the tile.
****************************************************************************/
int count_river_type_near_tile(const struct tile *ptile,
                               const struct road_type *priver,
                               bool percentage)
{
  int count = 0;
  int total = 0;

  fc_assert(priver != NULL);

  adjc_iterate(ptile, adjc_tile) {
    if (tile_has_road(adjc_tile, priver)) {
      count++;
    }
    total++;
  } adjc_iterate_end;

  if (percentage) {
    count = count * 100 / total;
  }
  return count;
}

/****************************************************************************
  Check if road provides effect
****************************************************************************/
bool road_has_flag(const struct road_type *proad, enum road_flag_id flag)
{
  return BV_ISSET(proad->flags, flag);
}

/****************************************************************************
  Returns TRUE iff any cardinally adjacent tile contains a road with
  the given flag
****************************************************************************/
bool is_road_flag_card_near(const struct tile *ptile, enum road_flag_id flag)
{
  cardinal_adjc_iterate(ptile, adjc_tile) {
    road_type_iterate(proad) {
      if (road_has_flag(proad, flag) && tile_has_road(adjc_tile, proad)) {
        return TRUE;
      }
    } road_type_iterate_end;
  } cardinal_adjc_iterate_end;

  return FALSE;
}

/****************************************************************************
  Returns TRUE iff any adjacent tile contains a road with the given flag
****************************************************************************/
bool is_road_flag_near_tile(const struct tile *ptile, enum road_flag_id flag)
{
  adjc_iterate(ptile, adjc_tile) {
    road_type_iterate(proad) {
      if (road_has_flag(proad, flag) && tile_has_road(adjc_tile, proad)) {
        return TRUE;
      }
    } road_type_iterate_end;
  } adjc_iterate_end;

  return FALSE;
}

/****************************************************************************
  Is tile native to road?
****************************************************************************/
bool is_native_tile_to_road(const struct road_type *proad,
                            const struct tile *ptile)
{
  struct extra_type *pextra;

  if (road_has_flag(proad, RF_RIVER)) {
    if (!terrain_has_flag(tile_terrain(ptile), TER_CAN_HAVE_RIVER)) {
      return FALSE;
    }
  } else if (tile_terrain(ptile)->road_time == 0) {
    return FALSE;
  }

  pextra = road_extra_get(proad);

  return are_reqs_active(NULL, NULL, NULL, NULL, ptile,
                         NULL, NULL, NULL, &pextra->reqs, RPT_POSSIBLE);
}

/****************************************************************************
  Is extra cardinal only road.
****************************************************************************/
bool is_cardinal_only_road(const struct extra_type *pextra)
{
  const struct road_type *proad;

  if (!is_extra_caused_by(pextra, EC_ROAD)) {
    return FALSE;
  }

  proad = extra_road_get_const(pextra);

  return proad->move_mode == RMM_CARDINAL || proad->move_mode == RMM_RELAXED;
}
