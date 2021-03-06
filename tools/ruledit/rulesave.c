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

#ifdef HAVE_CONFIG_H
#include <fc_config.h>
#endif

/* utility */
#include "registry.h"
#include "string_vector.h"

/* common */
#include "achievements.h"
#include "game.h"
#include "government.h"
#include "movement.h"
#include "specialist.h"
#include "style.h"
#include "unittype.h"

/* server */
#include "ruleset.h"

#include "rulesave.h"

/**************************************************************************
  Create new ruleset section file with common header.
**************************************************************************/
static struct section_file *create_ruleset_file(const char *rsname,
                                                const char *rstype)
{
  struct section_file *sfile = secfile_new(TRUE);
  char buf[500];

  if (rsname != NULL) {
    fc_snprintf(buf, sizeof(buf), "Template %s %s data for Freeciv", rsname, rstype);
  } else {
    fc_snprintf(buf, sizeof(buf), "Template %s data for Freeciv", rstype);
  }

  secfile_insert_str(sfile, buf, "datafile.description");
  secfile_insert_str(sfile, RULESET_CAPABILITIES, "datafile.options");

  return sfile;
}

/**************************************************************************
  Save int value that has default applied upon loading.
**************************************************************************/
static bool save_default_int(struct section_file *sfile, int value,
                             int default_value, const char *path, const char *entry)
{
  if (value != default_value) {
    if (entry != NULL) {
      secfile_insert_int(sfile, value,
                         "%s.%s", path, entry);
    } else {
      secfile_insert_int(sfile, value,
                         "%s", path);
    }
  }

  return TRUE;
}

/**************************************************************************
  Save bool value that has default applied upon loading.
**************************************************************************/
static bool save_default_bool(struct section_file *sfile, bool value,
                              bool default_value, const char *path, const char *entry)
{
  if ((value && !default_value)
      || (!value && default_value)) {
    if (entry != NULL) {
      secfile_insert_bool(sfile, value,
                          "%s.%s", path, entry);
    } else {
      secfile_insert_bool(sfile, value,
                          "%s", path);
    }
  }

  return TRUE;
}

/**************************************************************************
  Save name of the object.
**************************************************************************/
static bool save_name_translation(struct section_file *sfile,
                                  struct name_translation *name,
                                  const char *path)
{
  secfile_insert_str(sfile,
                     untranslated_name(name),
                     "%s.name", path);
  if (strcmp(skip_intl_qualifier_prefix(untranslated_name(name)),
             rule_name(name))) {
    secfile_insert_str(sfile,
                       rule_name(name),
                       "%s.rule_name", path);
  }

  return TRUE;
}

/**************************************************************************
  Save vector of requirements
**************************************************************************/
static bool save_reqs_vector(struct section_file *sfile,
                             struct requirement_vector *reqs,
                             const char *path, const char *entry)
{
  int i;
  bool includes_negated = FALSE;
  bool includes_surviving = FALSE;

  requirement_vector_iterate(reqs, preq) {
    if (!preq->present) {
      includes_negated = TRUE;
    }
    if (preq->survives) {
      includes_surviving = TRUE;
    }
  } requirement_vector_iterate_end;

  i = 0;
  requirement_vector_iterate(reqs, preq) {
    secfile_insert_str(sfile,
                       universals_n_name(preq->source.kind),
                       "%s.%s%d.type", path, entry, i);
    secfile_insert_str(sfile,
                       universal_rule_name(&(preq->source)),
                       "%s.%s%d.name", path, entry, i);
    secfile_insert_str(sfile,
                       req_range_name(preq->range),
                       "%s.%s%d.range", path, entry, i);

    if (includes_surviving) {
      secfile_insert_bool(sfile,
                          preq->survives,
                          "%s.%s%d.survives", path, entry, i);
    }

    if (includes_negated) {
      secfile_insert_bool(sfile,
                          preq->present,
                          "%s.%s%d.present", path, entry, i);
    }

    i++;
  } requirement_vector_iterate_end;

  return TRUE;
}

/**************************************************************************
  Save list of requirements
**************************************************************************/
static bool save_reqs_list(struct section_file *sfile,
                           const struct requirement_list *reqs,
                           const char *path, const char *entry)
{
  int i;
  bool includes_negated = FALSE;
  bool includes_surviving = FALSE;

  requirement_list_iterate(reqs, preq) {
    if (!preq->present) {
      includes_negated = TRUE;
    }
    if (preq->survives) {
      includes_surviving = TRUE;
    }
  } requirement_list_iterate_end;

  i = 0;
  requirement_list_iterate(reqs, preq) {
    secfile_insert_str(sfile,
                       universals_n_name(preq->source.kind),
                       "%s.%s%d.type", path, entry, i);
    secfile_insert_str(sfile,
                       universal_rule_name(&(preq->source)),
                       "%s.%s%d.name", path, entry, i);
    secfile_insert_str(sfile,
                       req_range_name(preq->range),
                       "%s.%s%d.range", path, entry, i);

    if (includes_surviving) {
      secfile_insert_bool(sfile,
                          preq->survives,
                          "%s.%s%d.survives", path, entry, i);
    }

    if (includes_negated) {
      secfile_insert_bool(sfile,
                          preq->present,
                          "%s.%s%d.present", path, entry, i);
    }

    i++;
  } requirement_list_iterate_end;

  return TRUE;
}

/**************************************************************************
  Save techs vector. Input is A_LAST terminated array of techs to save.
**************************************************************************/
static bool save_tech_list(struct section_file *sfile, int *input,
                           const char *path, const char *entry)
{
  const char *tech_names[MAX_NUM_TECH_LIST];
  int set_count;
  int i;

  set_count = 0;
  for (i = 0; input[i] != A_LAST && i < MAX_NUM_TECH_LIST; i++) {
    tech_names[set_count++] = advance_rule_name(advance_by_number(input[i]));
  }

  if (set_count > 0) {
    secfile_insert_str_vec(sfile, tech_names, set_count,
                           "%s.%s", path, entry);
  }

  return TRUE;
}

/**************************************************************************
  Save tech reference
**************************************************************************/
static bool save_tech_ref(struct section_file *sfile,
                          const struct advance *padv,
                          const char *path, const char *entry)
{
   if (padv == A_NEVER) {
     secfile_insert_str(sfile, "Never", "%s.%s", path, entry);
   } else {
     secfile_insert_str(sfile, advance_rule_name(padv),
                        "%s.%s", path, entry);
   }

   return TRUE;
}

/**************************************************************************
  Save terrain reference
**************************************************************************/
static bool save_terrain_ref(struct section_file *sfile,
                             const struct terrain *save,
                             const struct terrain *pthis,
                             const char *path, const char *entry)
{
  if (save == NULL) {
    secfile_insert_str(sfile, "none", "%s.%s", path, entry);
  } else if (save == pthis) {
    secfile_insert_str(sfile, "yes", "%s.%s", path, entry);   
  } else {
    secfile_insert_str(sfile, terrain_rule_name(save),
                        "%s.%s", path, entry);
  }

  return TRUE;
}

/**************************************************************************
  Save government reference
**************************************************************************/
static bool save_gov_ref(struct section_file *sfile,
                         const struct government *gov,
                         const char *path, const char *entry)
{
  secfile_insert_str(sfile, government_rule_name(gov), "%s.%s", path, entry);

  return TRUE;
}

/**************************************************************************
  Save buildings vector. Input is B_LAST terminated array of buildings
  to save.
**************************************************************************/
static bool save_building_list(struct section_file *sfile, int *input,
                               const char *path, const char *entry)
{
  const char *building_names[MAX_NUM_BUILDING_LIST];
  int set_count;
  int i;

  set_count = 0;
  for (i = 0; input[i] != B_LAST && i < MAX_NUM_BUILDING_LIST; i++) {
    building_names[set_count++] = improvement_rule_name(improvement_by_number(input[i]));
  }

  if (set_count > 0) {
    secfile_insert_str_vec(sfile, building_names, set_count,
                           "%s.%s", path, entry);
  }

  return TRUE;
}

/**************************************************************************
  Save building reference
**************************************************************************/
static bool save_building_ref(struct section_file *sfile,
                              const struct impr_type *impr,
                              const char *path, const char *entry)
{
  if (impr == B_NEVER) {
    secfile_insert_str(sfile, "None", "%s.%s", path, entry);
  } else {
    secfile_insert_str(sfile, improvement_rule_name(impr), "%s.%s",
                       path, entry);
  }

  return TRUE;
}

/**************************************************************************
  Save vector of unit class names based on bitvector bits
**************************************************************************/
static bool save_uclass_vec(struct section_file *sfile,
                            bv_unit_classes *bits,
                            const char *path, const char *entry,
                            bool unreachable_only)
{
  const char *class_names[UCL_LAST];
  int classes = 0;

  unit_class_iterate(pcargo) {
    if (BV_ISSET(*(bits), uclass_index(pcargo))
        && (uclass_has_flag(pcargo, UCF_UNREACHABLE)
            || !unreachable_only)) {
      class_names[classes++] = uclass_rule_name(pcargo);
    }
  } unit_class_iterate_end;

  if (classes > 0) {
    secfile_insert_str_vec(sfile, class_names, classes,
                           "%s.%s", path, entry);
  }

  return TRUE;
}

/**************************************************************************
  Save strvec as ruleset vector of strings
**************************************************************************/
static bool save_strvec(struct section_file *sfile,
                        struct strvec *to_save,
                        const char *path, const char *entry)
{
  if (to_save != NULL) {
    int sect_count = strvec_size(to_save);
    const char *sections[sect_count];
    int i;

    for (i = 0; i < sect_count; i++) {
      sections[i] = strvec_get(to_save, i);
    }

    secfile_insert_str_vec(sfile, sections, sect_count, "%s.%s", path, entry);
  }

  return TRUE;
}

/**************************************************************************
  Save ruleset file.
**************************************************************************/
static bool save_ruleset_file(struct section_file *sfile, const char *filename)
{
  return secfile_save(sfile, filename, 0, FZ_PLAIN);
}

/**************************************************************************
  Save buildings.ruleset
**************************************************************************/
static bool save_buildings_ruleset(const char *filename, const char *name)
{
  struct section_file *sfile = create_ruleset_file(name, "building");
  int sect_idx;

  if (sfile == NULL) {
    return FALSE;
  }

  sect_idx = 0;
  improvement_iterate(pb) {
    char path[512];
    const char *flag_names[IF_COUNT];
    int set_count;
    int flagi;

    fc_snprintf(path, sizeof(path), "building_%d", sect_idx++);

    save_name_translation(sfile, &(pb->name), path);

    secfile_insert_str(sfile, impr_genus_id_name(pb->genus),
                       "%s.genus", path);

    if (strcmp(pb->graphic_str, "-")) {
      secfile_insert_str(sfile, pb->graphic_str, "%s.graphic", path);
    }
    if (strcmp(pb->graphic_alt, "-")) {
      secfile_insert_str(sfile, pb->graphic_alt, "%s.graphic_alt", path);
    }
    if (strcmp(pb->soundtag, "-")) {
      secfile_insert_str(sfile, pb->soundtag, "%s.sound", path);
    }
    if (strcmp(pb->soundtag_alt, "-")) {
      secfile_insert_str(sfile, pb->soundtag_alt, "%s.sound_alt", path);
    }

    save_reqs_vector(sfile, &(pb->reqs), path, "reqs");
    save_reqs_vector(sfile, &(pb->obsolete_by), path, "obsolete_by");

    save_building_ref(sfile, pb->replaced_by, path, "replaced_by");

    secfile_insert_int(sfile, pb->build_cost, "%s.build_cost", path);
    secfile_insert_int(sfile, pb->upkeep, "%s.upkeep", path);
    secfile_insert_int(sfile, pb->sabotage, "%s.sabotage", path);

    set_count = 0;
    for (flagi = 0; flagi < IF_COUNT; flagi++) {
      if (improvement_has_flag(pb, flagi)) {
        flag_names[set_count++] = impr_flag_id_name(flagi);
      }
    }

    if (set_count > 0) {
      secfile_insert_str_vec(sfile, flag_names, set_count,
                             "%s.flags", path);
    }

   save_strvec(sfile, pb->helptext, path, "helptext");

  } improvement_iterate_end;

  return save_ruleset_file(sfile, filename);
}

/**************************************************************************
  Save styles.ruleset
**************************************************************************/
static bool save_styles_ruleset(const char *filename, const char *name)
{
  struct section_file *sfile = create_ruleset_file(name, "styles");
  int sect_idx;

  if (sfile == NULL) {
    return FALSE;
  }

  sect_idx = 0;
  styles_iterate(pstyle) {
    char path[512];

    fc_snprintf(path, sizeof(path), "style_%d", sect_idx++);

    save_name_translation(sfile, &(pstyle->name), path);
  } styles_iterate_end;

  sect_idx = 0;
  music_styles_iterate(pmus) {
    char path[512];

    fc_snprintf(path, sizeof(path), "musicstyle_%d", sect_idx++);

    secfile_insert_str(sfile, pmus->music_peaceful, "%s.music_peaceful", path);
    secfile_insert_str(sfile, pmus->music_combat, "%s.music_combat", path);

    save_reqs_vector(sfile, &(pmus->reqs), path, "reqs");
  } music_styles_iterate_end;

  return save_ruleset_file(sfile, filename);
}

/**************************************************************************
  Save cities.ruleset
**************************************************************************/
static bool save_cities_ruleset(const char *filename, const char *name)
{
  struct section_file *sfile = create_ruleset_file(name, "cities");
  int sect_idx;
  int i;

  if (sfile == NULL) {
    return FALSE;
  }

  sect_idx = 0;
  specialist_type_iterate(sp) {
    struct specialist *s = specialist_by_number(sp);
    char path[512];

    fc_snprintf(path, sizeof(path), "specialist_%d", sect_idx++);

    save_name_translation(sfile, &(s->name), path);

    if (strcmp(rule_name(&s->name), rule_name(&s->abbreviation))) {
      secfile_insert_str(sfile, rule_name(&s->abbreviation),
                         "%s.short_name", path);
    }

    save_reqs_vector(sfile, &(s->reqs), path, "reqs");

    if (strcmp(s->graphic_alt, "-")) {
      secfile_insert_str(sfile, s->graphic_alt, "%s.graphic_alt", path);
    }

    save_strvec(sfile, s->helptext, path, "helptext");

  } specialist_type_iterate_end;

  if (game.info.celebratesize != GAME_DEFAULT_CELEBRATESIZE) {
    secfile_insert_int(sfile, game.info.celebratesize,
                       "parameters.celebrate_size_limit");
  }
  if (game.info.add_to_size_limit != GAME_DEFAULT_ADDTOSIZE) {
    secfile_insert_int(sfile, game.info.add_to_size_limit,
                       "parameters.add_to_size_limit");
  }
  if (game.info.angrycitizen != GAME_DEFAULT_ANGRYCITIZEN) {
    secfile_insert_bool(sfile, game.info.angrycitizen,
                       "parameters.angry_citizens");
  }
  if (game.info.changable_tax != GAME_DEFAULT_CHANGABLE_TAX) {
    secfile_insert_bool(sfile, game.info.changable_tax,
                       "parameters.changable_tax");
  }
  if (game.info.forced_science != 0) {
    secfile_insert_int(sfile, game.info.forced_science,
                       "parameters.forced_science");
  }
  if (game.info.forced_luxury != 100) {
    secfile_insert_int(sfile, game.info.forced_luxury,
                       "parameters.forced_luxury");
  }
  if (game.info.forced_gold != 0) {
    secfile_insert_int(sfile, game.info.forced_gold,
                       "parameters.forced_gold");
  }
  if (game.server.vision_reveal_tiles != GAME_DEFAULT_VISION_REVEAL_TILES) {
    secfile_insert_bool(sfile, game.server.vision_reveal_tiles,
                       "parameters.vision_reveal_tiles");
  }
  if (game.info.pop_report_zeroes != 1) {
    secfile_insert_int(sfile, game.info.pop_report_zeroes,
                       "parameters.pop_report_zeroes");
  }
  if (game.info.citizen_nationality != GAME_DEFAULT_NATIONALITY) {
    secfile_insert_bool(sfile, game.info.citizen_nationality,
                       "citizen.nationality");
  }
  if (game.info.citizen_convert_speed != GAME_DEFAULT_CONVERT_SPEED) {
    secfile_insert_int(sfile, game.info.citizen_convert_speed,
                       "citizen.convert_speed");
  }
  if (game.info.citizen_partisans_pct != 0) {
    secfile_insert_int(sfile, game.info.citizen_partisans_pct,
                       "citizen.partisans_pct");
  }

  sect_idx = 0;
  for (i = 0; i < game.control.styles_count; i++) {
    char path[512];

    fc_snprintf(path, sizeof(path), "citystyle_%d", sect_idx++);

    save_name_translation(sfile, &(city_styles[i].name), path);

    secfile_insert_str(sfile, city_styles[i].graphic, "%s.graphic", path);
    secfile_insert_str(sfile, city_styles[i].graphic_alt, "%s.graphic_alt", path);
    if (strcmp(city_styles[i].oceanic_graphic, "")) {
      secfile_insert_str(sfile, city_styles[i].oceanic_graphic, "%s.oceanic_graphic", path);
    }
    if (strcmp(city_styles[i].oceanic_graphic_alt, "")) {
      secfile_insert_str(sfile, city_styles[i].oceanic_graphic_alt,
                         "%s.oceanic_graphic_alt", path);
    }
    if (strcmp(city_styles[i].citizens_graphic, "-")) {
      secfile_insert_str(sfile, city_styles[i].citizens_graphic,
                         "%s.citizens_graphic", path);
    }
    if (strcmp(city_styles[i].citizens_graphic_alt, "generic")) {
      secfile_insert_str(sfile, city_styles[i].citizens_graphic_alt,
                         "%s.citizens_graphic_alt", path);
    }

    save_reqs_vector(sfile, &(city_styles[i].reqs), path, "reqs");

    if (city_styles[i].replaced_by < 0) {
      secfile_insert_str(sfile, "-", "%s.replaced_by", path);
    } else {
      secfile_insert_str(sfile, city_style_rule_name(city_styles[i].replaced_by),
                         "%s.replaced_by", path);
    }

  }

  return save_ruleset_file(sfile, filename);
}

/**************************************************************************
  Effect saving callback data structure.
**************************************************************************/
typedef struct {
  int idx;
  struct section_file *sfile;
} effect_cb_data;

/**************************************************************************
  Save one effect. Callback called for each effect in cache.
**************************************************************************/
static bool effect_save(const struct effect *peffect, void *data)
{
  effect_cb_data *cbdata = (effect_cb_data *)data;
  char path[512];

  fc_snprintf(path, sizeof(path), "effect_%d", cbdata->idx++);

  secfile_insert_str(cbdata->sfile,
                     effect_type_name(peffect->type),
                     "%s.type", path);
  secfile_insert_int(cbdata->sfile, peffect->value, "%s.value", path);

  save_reqs_list(cbdata->sfile, peffect->reqs, path, "reqs");
  save_reqs_list(cbdata->sfile, peffect->nreqs, path, "nreqs");

  return TRUE;
}

/**************************************************************************
  Save effects.ruleset
**************************************************************************/
static bool save_effects_ruleset(const char *filename, const char *name)
{
  struct section_file *sfile = create_ruleset_file(name, "effect");
  effect_cb_data data;

  if (sfile == NULL) {
    return FALSE;
  }

  data.idx = 0;
  data.sfile = sfile;

  if (!iterate_effect_cache(effect_save, &data)) {
    return FALSE;
  }

  return save_ruleset_file(sfile, filename);
}

/**************************************************************************
  Save game.ruleset
**************************************************************************/
static bool save_game_ruleset(const char *filename, const char *name)
{
  struct section_file *sfile = create_ruleset_file(name, "game");
  int sect_idx;
  int col_idx;
  int set_count;
  enum gameloss_style gs;
  const char *style_names[32]; /* FIXME: Should determine max length automatically.
                                * currently it's 3 (bits 0,1, and 2) so there's plenty of
                                * safety margin here. */
  enum trade_route_type trt;

  if (sfile == NULL) {
    return FALSE;
  }

  if (game.control.prefered_tileset[0] != '\0') {
    secfile_insert_str(sfile, game.control.prefered_tileset,
                       "tileset.preferred");
  }
  if (game.control.prefered_soundset[0] != '\0') {
    secfile_insert_str(sfile, game.control.prefered_soundset,
                       "soundset.preferred");
  }

  secfile_insert_str(sfile, game.control.name, "about.name");

  if (game.control.description[0] != '\0') {
    secfile_insert_str(sfile, game.control.description,
                       "about.description");
  }

  save_tech_list(sfile, game.rgame.global_init_techs,
                 "options", "global_init_techs");
  save_building_list(sfile, game.rgame.global_init_buildings,
                     "options", "global_init_buildings");

  save_default_bool(sfile, game.control.popup_tech_help,
                    FALSE,
                    "options.popup_tech_help", NULL);
  save_default_int(sfile, game.info.base_pollution,
                   RS_DEFAULT_BASE_POLLUTION,
                   "civstyle.base_pollution", NULL);

  set_count = 0;
  for (gs = gameloss_style_begin(); gs != gameloss_style_end(); gs = gameloss_style_next(gs)) {
    if (game.info.gameloss_style & gs) {
      style_names[set_count++] = gameloss_style_name(gs);
    }
  }

  if (set_count > 0) {
    secfile_insert_str_vec(sfile, style_names, set_count,
                           "cvistyle.gameloss_style");
  }

  save_default_int(sfile, game.info.happy_cost,
                   RS_DEFAULT_HAPPY_COST,
                   "civstyle.happy_cost", NULL);
  save_default_int(sfile, game.info.food_cost,
                   RS_DEFAULT_FOOD_COST,
                   "civstyle.food_cost", NULL);
  save_default_bool(sfile, game.info.civil_war_enabled,
                    TRUE,
                    "civstyle.civil_war_enabled", NULL);
  save_default_bool(sfile, game.info.paradrop_to_transport,
                    FALSE,
                    "civstyle.paradrop_to_transport", NULL);
  save_default_int(sfile, game.info.base_bribe_cost,
                   RS_DEFAULT_BASE_BRIBE_COST,
                   "civstyle.base_bribe_cost", NULL);
  save_default_int(sfile, game.server.ransom_gold,
                   RS_DEFAULT_RANSOM_GOLD,
                   "civstyle.ransom_gold", NULL);
  save_default_bool(sfile, game.info.pillage_select,
                    RS_DEFAULT_PILLAGE_SELECT,
                    "civstyle.pillage_select", NULL);
  save_default_bool(sfile, game.info.tech_trade_allow_holes,
                    RS_DEFAULT_TECH_TRADE_HOLES,
                    "civstyle.tech_trade_allow_holes", NULL);
  save_default_bool(sfile, game.info.tech_parasite_allow_holes,
                    RS_DEFAULT_TECH_PARASITE_HOLES,
                    "civstyle.tech_parasite_allow_holes", NULL);
  save_default_bool(sfile, game.info.tech_steal_allow_holes,
                    RS_DEFAULT_TECH_STEAL_HOLES,
                    "civstyle.tech_steal_allow_holes", NULL);
  save_default_int(sfile, game.server.upgrade_veteran_loss,
                   RS_DEFAULT_UPGRADE_VETERAN_LOSS,
                   "civstyle.upgrade_veteran_loss", NULL);
  save_default_int(sfile, game.server.autoupgrade_veteran_loss,
                   RS_DEFAULT_UPGRADE_VETERAN_LOSS,
                   "civstyle.autoupgrade_veteran_loss", NULL);
  save_default_int(sfile, game.info.base_tech_cost,
                   RS_DEFAULT_BASE_TECH_COST,
                   "civstyle.base_tech_cost", NULL);

  secfile_insert_int_vec(sfile, game.info.granary_food_ini,
                         game.info.granary_num_inis,
                         "civstyle.granary_food_ini");

  save_default_int(sfile, game.info.granary_food_inc,
                   RS_DEFAULT_GRANARY_FOOD_INC,
                   "civstyle.granary_food_inc", NULL);

  output_type_iterate(o) {
    char buffer[256];

    fc_snprintf(buffer, sizeof(buffer),
                "civstyle.min_city_center_%s",
                get_output_identifier(o));

    save_default_int(sfile, game.info.min_city_center_output[o],
                     RS_DEFAULT_CITY_CENTER_OUTPUT,
                     buffer, NULL);
  } output_type_iterate_end;

  save_default_int(sfile, game.server.init_vis_radius_sq,
                   RS_DEFAULT_VIS_RADIUS_SQ,
                   "civstyle.init_vis_radius_sq", NULL);
  save_default_int(sfile, game.info.init_city_radius_sq,
                   RS_DEFAULT_CITY_RADIUS_SQ,
                   "civstyle.init_city_radius_sq", NULL);
  save_default_int(sfile, game.info.gold_upkeep_style,
                   RS_DEFAULT_GOLD_UPKEEP_STYLE,
                   "civstyle.gold_upkeep_style", NULL);
  save_default_int(sfile, game.info.tech_cost_style,
                   RS_DEFAULT_TECH_COST_STYLE,
                   "civstyle.tech_cost_style", NULL);
  save_default_int(sfile, game.info.tech_leakage,
                   RS_DEFAULT_TECH_LEAKAGE,
                   "civstyle.tech_leakage", NULL);
  save_default_bool(sfile, game.info.illness_on,
                    RS_DEFAULT_ILLNESS_ON,
                    "illness.illness_on", NULL);
  save_default_int(sfile, game.info.illness_base_factor,
                   RS_DEFAULT_ILLNESS_BASE_FACTOR,
                   "illness.illness_base_factor", NULL);
  save_default_int(sfile, game.info.illness_min_size,
                   RS_DEFAULT_ILLNESS_MIN_SIZE,
                   "illness.illness_min_size", NULL);
  save_default_int(sfile, game.info.illness_trade_infection,
                   RS_DEFAULT_ILLNESS_TRADE_INFECTION_PCT,
                   "illness.illness_trade_infection", NULL);
  save_default_int(sfile, game.info.illness_pollution_factor,
                   RS_DEFAULT_ILLNESS_POLLUTION_PCT,
                   "illness.illness_pollution_factor", NULL);
  save_default_int(sfile, game.server.base_incite_cost,
                   RS_DEFAULT_INCITE_BASE_COST,
                   "incite_cost.base_incite_cost", NULL);
  save_default_int(sfile, game.server.incite_improvement_factor,
                   RS_DEFAULT_INCITE_IMPROVEMENT_FCT,
                   "incite_cost.improvement_factor", NULL);
  save_default_int(sfile, game.server.incite_unit_factor,
                   RS_DEFAULT_INCITE_UNIT_FCT,
                   "incite_cost.unit_factor", NULL);
  save_default_int(sfile, game.server.incite_total_factor,
                   RS_DEFAULT_INCITE_TOTAL_FCT,
                   "incite_cost.total_factor", NULL);
  save_default_bool(sfile, game.info.slow_invasions,
                    RS_DEFAULT_SLOW_INVASIONS,
                    "global_unit_options.slow_invasions", NULL);

  sect_idx = 0;
  action_enablers_iterate(pae) {
    char path[512];

    fc_snprintf(path, sizeof(path), "actionenabler_%d", sect_idx++);

    secfile_insert_str(sfile, gen_action_name(pae->action),
                       "%s.action", path);

    save_reqs_vector(sfile, &(pae->actor_reqs), path, "actor_reqs");
    save_reqs_vector(sfile, &(pae->target_reqs), path, "target_reqs");
  } action_enablers_iterate_end;

  save_default_bool(sfile, game.info.tired_attack,
                    RS_DEFAULT_TIRED_ATTACK,
                    "combat_rules.tired_attack", NULL);
  save_default_int(sfile, game.info.border_city_radius_sq,
                   RS_DEFAULT_BORDER_RADIUS_SQ_CITY,
                   "borders.radius_sq_city", NULL);
  save_default_int(sfile, game.info.border_size_effect,
                   RS_DEFAULT_BORDER_SIZE_EFFECT,
                   "borders.size_effect", NULL);
  secfile_insert_str(sfile, tech_upkeep_style_name(game.info.tech_upkeep_style),
                     "research.tech_upkeep_style");
  save_default_int(sfile, game.info.tech_upkeep_divider,
                   RS_DEFAULT_TECH_UPKEEP_DIVIDER,
                   "research.tech_upkeep_divider", NULL);
  secfile_insert_str(sfile, free_tech_method_name(game.info.free_tech_method),
                     "research.free_tech_method");

  save_default_bool(sfile, game.info.calendar_skip_0,
                    RS_DEFAULT_CALENDAR_SKIP_0,
                    "calendar.skip_year_0", NULL);
  save_default_int(sfile, game.server.start_year,
                   GAME_START_YEAR,
                   "calendar.start_year", NULL);

  /* FIXME: These are stored in memory in translated form only -
   *        So we're currently saving them translated, though ruleset should have
   *        untranslated form. */
  if (strcmp(game.info.positive_year_label, RS_DEFAULT_POS_YEAR_LABEL)) {
    secfile_insert_str(sfile, game.info.positive_year_label,
                       "calendar.positive_label");
  }
  if (strcmp(game.info.negative_year_label, RS_DEFAULT_NEG_YEAR_LABEL)) {
    secfile_insert_str(sfile, game.info.negative_year_label,
                       "calendar.negative_label");
  }

  if (game.plr_bg_color != NULL) {
    rgbcolor_save(sfile, game.plr_bg_color, "playercolors.background");
  }

  col_idx = 0;
  rgbcolor_list_iterate(game.server.plr_colors, pcol) {
    rgbcolor_save(sfile, pcol, "playercolors.colorlist%d", col_idx++);
  } rgbcolor_list_iterate_end;

  /* TODO: Team names */

  sect_idx = 0;
  disaster_type_iterate(pd) {
    char path[512];
    enum disaster_effect_id de;
    const char *effect_names[DE_COUNT];

    fc_snprintf(path, sizeof(path), "disaster_%d", sect_idx++);

    save_name_translation(sfile, &(pd->name), path);
    save_reqs_vector(sfile, &(pd->reqs), path, "reqs");
    if (pd->frequency != GAME_DEFAULT_DISASTER_FREQ) {
      secfile_insert_int(sfile, pd->frequency,
                         "%s.frequency", path);
    }

    set_count = 0;
    for (de = disaster_effect_id_begin();
         de != disaster_effect_id_end();
         de = disaster_effect_id_next(de)) {
      if (BV_ISSET(pd->effects, de)) {
        effect_names[set_count++] = disaster_effect_id_name(de);
      }
    }

    if (set_count > 0) {
      secfile_insert_str_vec(sfile, effect_names, set_count,
                             "%s.effects", path);
    }
  } disaster_type_iterate_end;

  sect_idx = 0;
  achievements_iterate(pach) {
    char path[512];

    fc_snprintf(path, sizeof(path), "achievement_%d", sect_idx++);

    save_name_translation(sfile, &(pach->name), path);

    secfile_insert_str(sfile, achievement_type_name(pach->type),
                       "%s.type", path);

    save_default_bool(sfile, pach->unique,
                      GAME_DEFAULT_ACH_UNIQUE,
                      path, "unique");
    save_default_int(sfile, pach->value,
                     GAME_DEFAULT_ACH_VALUE,
                     path, "value");

  } achievements_iterate_end;

  set_count = 0;
  for (trt = 0; trt < TRT_LAST; trt++) {
    struct trade_route_settings *set = trade_route_settings_by_type(trt);
    const char *cancelling = traderoute_cancelling_type_name(set->cancelling);

    if (set->trade_pct != 100 || strcmp(cancelling, "Active")) {
      char path[256];

      fc_snprintf(path, sizeof(path),
                  "trade.settings%d", set_count++);

      secfile_insert_str(sfile, trade_route_type_name(trt),
                         "%s.name", path);
      secfile_insert_int(sfile, set->trade_pct,
                         "%s.pct", path);
      secfile_insert_str(sfile, cancelling,
                         "%s.cancelling", path);
    }
  }

  /* TODO: Settings */

  return save_ruleset_file(sfile, filename);
}

/**************************************************************************
  Save governments.ruleset
**************************************************************************/
static bool save_governments_ruleset(const char *filename, const char *name)
{
  struct section_file *sfile = create_ruleset_file(name, "government");
  int sect_idx;

  if (sfile == NULL) {
    return FALSE;
  }

  save_gov_ref(sfile, game.government_during_revolution, "governments",
               "during_revolution");

  sect_idx = 0;
  governments_iterate(pg) {
    char path[512];
    struct ruler_title *prtitle;

    fc_snprintf(path, sizeof(path), "government_%d", sect_idx++);

    save_name_translation(sfile, &(pg->name), path);

    secfile_insert_str(sfile, pg->graphic_str, "%s.graphic", path);
    secfile_insert_str(sfile, pg->graphic_alt, "%s.graphic_alt", path);

    save_reqs_vector(sfile, &(pg->reqs), path, "reqs");

    if (pg->ai.better != NULL) {
      save_gov_ref(sfile, pg->ai.better, path,
                   "ai_better");
    }

    ruler_title_hash_lookup(pg->ruler_titles, NULL,
                            &prtitle);
    if (prtitle != NULL) {
      const char *title;

      title = ruler_title_male_untranslated_name(prtitle);
      if (title != NULL) {
        secfile_insert_str(sfile, title,
                           "%s.ruler_male_title", path);
      }

      title = ruler_title_female_untranslated_name(prtitle);
      if (title != NULL) {
        secfile_insert_str(sfile, title,
                           "%s.ruler_female_title", path);
      }
    }

    save_strvec(sfile, pg->helptext, path, "helptext");

  } governments_iterate_end;

  return save_ruleset_file(sfile, filename);
}

/**************************************************************************
  Save list of AI traits
**************************************************************************/
static bool save_traits(int *traits, int *default_traits,
                        struct section_file *sfile,
                        const char *secname, const char *field_prefix)
{
  enum trait tr;

 /* FIXME: Use specenum trait names without duplicating them here.
   *        Just needs to take care of case. */
  const char *trait_names[] = {
    "expansionist",
    "trader",
    "aggressive",
    NULL
  };

  for (tr = trait_begin(); tr != trait_end() && trait_names[tr] != NULL;
       tr = trait_next(tr)) {
    if ((default_traits == NULL && traits[tr] != TRAIT_DEFAULT_VALUE)
        || (default_traits != NULL && traits[tr] != default_traits[tr])) {
      secfile_insert_int(sfile, traits[tr], "%s.%s%s", secname, field_prefix,
                         trait_names[tr]);
    }
  }

  return TRUE;
}

/**************************************************************************
  Save nations.ruleset
**************************************************************************/
static bool save_nations_ruleset(const char *filename, const char *name)
{
  struct section_file *sfile = create_ruleset_file(name, "nation");

  if (sfile == NULL) {
    return FALSE;
  }

  save_traits(game.server.default_traits, NULL, sfile,
              "default_traits", "");

  /* TODO: allowed_govs, allowed_terrains, allowed_cstyles */

  if (game.server.default_government != NULL) {
    secfile_insert_str(sfile, government_rule_name(game.server.default_government),
                       "compatibility.default_government");
  }

  secfile_insert_include(sfile, "default/nationlist.ruleset");

  return save_ruleset_file(sfile, filename);
}

/**************************************************************************
  Save techs.ruleset
**************************************************************************/
static bool save_techs_ruleset(const char *filename, const char *name)
{
  struct section_file *sfile = create_ruleset_file(name, "tech");
  int i;
  int sect_idx;
  struct advance *a_none = advance_by_number(A_NONE);


  if (sfile == NULL) {
    return FALSE;
  }

  for (i = 0; i < MAX_NUM_USER_TECH_FLAGS; i++) {
    const char *flagname = tech_flag_id_name(i + TECH_USER_1);
    const char *helptxt = tech_flag_helptxt(i + TECH_USER_1);

    if (flagname != NULL && helptxt != NULL) {
      secfile_insert_str(sfile, flagname, "control.flags%d.name", i);
      secfile_insert_str(sfile, helptxt, "control.flags%d.helptxt", i);
    }
  }

  sect_idx = 0;
  advance_iterate(A_FIRST, pa) {
    char path[512];
    const char *flag_names[TF_COUNT];
    int set_count;
    int flagi;

    fc_snprintf(path, sizeof(path), "advance_%d", sect_idx++);

    save_name_translation(sfile, &(pa->name), path);

    save_tech_ref(sfile, pa->require[AR_ONE], path, "req1");
    save_tech_ref(sfile, pa->require[AR_TWO], path, "req2");
    if (pa->require[AR_ROOT] != a_none) {
      save_tech_ref(sfile, pa->require[AR_ROOT], path, "root_req");
    }

    secfile_insert_str(sfile, pa->graphic_str, "%s.graphic", path);
    if (strcmp("-", pa->graphic_alt)) {
      secfile_insert_str(sfile, pa->graphic_alt, "%s.graphic_alt", path);
    }
    if (pa->bonus_message != NULL) {
      secfile_insert_str(sfile, pa->bonus_message, "%s.bonus_message", path);
    }

    set_count = 0;
    for (flagi = 0; flagi < TF_COUNT; flagi++) {
      if (advance_has_flag(advance_index(pa), flagi)) {
        flag_names[set_count++] = tech_flag_id_name(flagi);
      }
    }

    if (set_count > 0) {
      secfile_insert_str_vec(sfile, flag_names, set_count,
                             "%s.flags", path);
    }
    if (pa->preset_cost >= 0) {
      secfile_insert_int(sfile, pa->preset_cost, "%s.cost", path);
    }

    save_strvec(sfile, pa->helptext, path, "helptext");

  } advance_iterate_end;

  return save_ruleset_file(sfile, filename);
}

/**************************************************************************
  Save terrain.ruleset
**************************************************************************/
static bool save_terrain_ruleset(const char *filename, const char *name)
{
  struct section_file *sfile = create_ruleset_file(name, "terrain");
  int sect_idx;
  int i;

  if (sfile == NULL) {
    return FALSE;
  }

  for (i = 0; i < MAX_NUM_USER_TER_FLAGS; i++) {
    const char *flagname = terrain_flag_id_name(i + TER_USER_1);
    const char *helptxt = terrain_flag_helptxt(i + TER_USER_1);

    if (flagname != NULL && helptxt != NULL) {
      secfile_insert_str(sfile, flagname, "control.flags%d.name", i);
      secfile_insert_str(sfile, helptxt, "control.flags%d.helptxt", i);
    }
  }

  if (terrain_control.ocean_reclaim_requirement_pct <= 100) {
    secfile_insert_int(sfile, terrain_control.ocean_reclaim_requirement_pct,
                       "parameters.ocean_reclaim_requirement");
  }
  if (terrain_control.land_channel_requirement_pct <= 100) {
    secfile_insert_int(sfile, terrain_control.land_channel_requirement_pct,
                       "parameters.land_channel_requirement");
  }
  if (terrain_control.lake_max_size != 0) {
    secfile_insert_int(sfile, terrain_control.lake_max_size,
                       "parameters.lake_max_size");
  }
  if (terrain_control.min_start_native_area != 0) {
    secfile_insert_int(sfile, terrain_control.min_start_native_area,
                       "parameters.min_start_native_area");
  }
  if (terrain_control.move_fragments != 3) {
    secfile_insert_int(sfile, terrain_control.move_fragments,
                       "parameters.move_fragments");
  }
  if (terrain_control.igter_cost != 1) {
    secfile_insert_int(sfile, terrain_control.igter_cost,
                       "parameters.igter_cost");
  }
  if (map.server.ocean_resources) {
    secfile_insert_bool(sfile, TRUE,
                       "parameters.ocean_resources");
  }

  sect_idx = 0;
  terrain_type_iterate(pterr) {
    char path[512];
    char identifier[2];
    int i;
    const char *flag_names[TER_USER_LAST];
    const char *puc_names[UCL_LAST];
    int flagi;
    int set_count;

    fc_snprintf(path, sizeof(path), "terrain_%d", sect_idx++);

    save_name_translation(sfile, &(pterr->name), path);

    secfile_insert_str(sfile, pterr->graphic_str, "%s.graphic", path);
    secfile_insert_str(sfile, pterr->graphic_alt, "%s.graphic_alt", path);
    identifier[0] = pterr->identifier;
    identifier[1] = '\0';
    secfile_insert_str(sfile, identifier, "%s.identifier", path);

    secfile_insert_str(sfile, terrain_class_name(pterr->tclass),
                       "%s.class", path);

    secfile_insert_int(sfile, pterr->movement_cost, "%s.movement_cost", path);
    secfile_insert_int(sfile, pterr->defense_bonus, "%s.defense_bonus", path);

    output_type_iterate(o) {
      if (pterr->output[o] != 0) {
        secfile_insert_int(sfile, pterr->output[o], "%s.%s", path,
                           get_output_identifier(o));
      }
    } output_type_iterate_end;

    /* Check resource count */
    for (i = 0; pterr->resources[i] != NULL; i++);

    {
      const char *resource_names[i];

      for (i = 0; pterr->resources[i] != NULL; i++) {
        resource_names[i] = resource_rule_name(pterr->resources[i]);
      }

      secfile_insert_str_vec(sfile, resource_names, i,
                             "%s.resources", path);
    }

    output_type_iterate(o) {
      if (pterr->road_output_incr_pct[o] != 0) {
        secfile_insert_int(sfile, pterr->road_output_incr_pct[o],
                           "%s.road_%s_incr_pct", path,
                           get_output_identifier(o));
      }
    } output_type_iterate_end;

    secfile_insert_int(sfile, pterr->base_time, "%s.base_time", path);
    secfile_insert_int(sfile, pterr->road_time, "%s.road_time", path);

    save_terrain_ref(sfile, pterr->irrigation_result, pterr, path,
                     "irrigation_result");
    secfile_insert_int(sfile, pterr->irrigation_food_incr,
                       "%s.irrigation_food_incr", path);
    secfile_insert_int(sfile, pterr->irrigation_time,
                       "%s.irrigation_time", path);

    save_terrain_ref(sfile, pterr->mining_result, pterr, path,
                     "mining_result");
    secfile_insert_int(sfile, pterr->mining_shield_incr,
                       "%s.mining_shield_incr", path);
    secfile_insert_int(sfile, pterr->mining_time,
                       "%s.mining_time", path);

    save_terrain_ref(sfile, pterr->transform_result, pterr, path,
                     "transorm_result");
    secfile_insert_int(sfile, pterr->transform_time,
                       "%s.transform_time", path);

    if (pterr->animal != NULL) {
      secfile_insert_str(sfile, utype_rule_name(pterr->animal),
                         "%s.animal", path);
    } else {
      secfile_insert_str(sfile, "None",
                         "%s.animal", path);
    }

    secfile_insert_int(sfile, pterr->clean_pollution_time,
                       "%s.clean_pollution_time", path);
    secfile_insert_int(sfile, pterr->clean_fallout_time,
                       "%s.clean_fallout_time", path);

    save_terrain_ref(sfile, pterr->warmer_wetter_result, pterr, path,
                     "warmer_wetter_result");
    save_terrain_ref(sfile, pterr->warmer_drier_result, pterr, path,
                     "warmer_drier_result");
    save_terrain_ref(sfile, pterr->cooler_wetter_result, pterr, path,
                     "cooler_wetter_result");
    save_terrain_ref(sfile, pterr->cooler_drier_result, pterr, path,
                     "cooler_drier_result");

    set_count = 0;
    for (flagi = 0; flagi < TER_USER_LAST; flagi++) {
      if (terrain_has_flag(pterr, flagi)) {
        flag_names[set_count++] = terrain_flag_id_name(flagi);
      }
    }

    if (set_count > 0) {
      secfile_insert_str_vec(sfile, flag_names, set_count,
                             "%s.flags", path);
    }

    {
      enum mapgen_terrain_property mtp;

      for (mtp = mapgen_terrain_property_begin();
           mtp != mapgen_terrain_property_end();
           mtp = mapgen_terrain_property_next(mtp)) {
        if (pterr->property[mtp] != 0) {
          secfile_insert_int(sfile, pterr->property[mtp],
                             "%s.property_%s", path,
                             mapgen_terrain_property_name(mtp));
        }
      }
    }

    set_count = 0;
    unit_class_iterate(puc) {
      if (BV_ISSET(pterr->native_to, uclass_index(puc))) {
        puc_names[set_count++] = uclass_rule_name(puc);
      }
    } unit_class_iterate_end;

    if (set_count > 0) {
      secfile_insert_str_vec(sfile, puc_names, set_count,
                             "%s.native_to", path);
    }

    rgbcolor_save(sfile, pterr->rgb, "%s.color", path);

    save_strvec(sfile, pterr->helptext, path, "helptext");

  } terrain_type_iterate_end;

  sect_idx = 0;
  resource_type_iterate(pres) {
    char path[512];
    char identifier[2];

    fc_snprintf(path, sizeof(path), "resource_%d", sect_idx++);

    save_name_translation(sfile, &(pres->name), path);

    output_type_iterate(o) {
      if (pres->output[o] != 0) {
        secfile_insert_int(sfile, pres->output[o], "%s.%s",
                           path, get_output_identifier(o));
      }
    } output_type_iterate_end;

    secfile_insert_str(sfile, pres->graphic_str, "%s.graphic", path);
    secfile_insert_str(sfile, pres->graphic_alt, "%s.graphic_alt", path);
    identifier[0] = pres->identifier;
    identifier[1] = '\0';
    secfile_insert_str(sfile, identifier, "%s.identifier", path);
  } resource_type_iterate_end;

  sect_idx = 0;
  extra_type_iterate(pextra) {
    char path[512];
    const char *flag_names[EF_COUNT];
    const char *cause_names[EC_COUNT];
    const char *puc_names[UCL_LAST];
    const char *extra_names[MAX_EXTRA_TYPES];
    int flagi;
    int causei;
    int set_count;

    fc_snprintf(path, sizeof(path), "extra_%d", sect_idx++);

    save_name_translation(sfile, &(pextra->name), path);

    secfile_insert_str(sfile, extra_category_name(pextra->category),
                       "%s.category", path);

    set_count = 0;
    for (causei = 0; causei < EC_COUNT; causei++) {
      if (is_extra_caused_by(pextra, causei)) {
        cause_names[set_count++] = extra_cause_name(causei);
      }
    }

    if (set_count > 0) {
      secfile_insert_str_vec(sfile, cause_names, set_count,
                             "%s.causes", path);
    }

    set_count = 0;
    for (causei = 0; causei < ERM_COUNT; causei++) {
      if (is_extra_removed_by(pextra, causei)) {
        cause_names[set_count++] = extra_rmcause_name(causei);
      }
    }

    if (set_count > 0) {
      secfile_insert_str_vec(sfile, cause_names, set_count,
                             "%s.rmcauses", path);
    }

    if (strcmp(pextra->graphic_str, "-")) {
      secfile_insert_str(sfile, pextra->graphic_str, "%s.graphic", path);
    }
    if (strcmp(pextra->graphic_alt, "-")) {
      secfile_insert_str(sfile, pextra->graphic_alt, "%s.graphic_alt", path);
    }
    if (strcmp(pextra->activity_gfx, "-")) {
      secfile_insert_str(sfile, pextra->activity_gfx, "%s.activity_gfx", path);
    }
    if (strcmp(pextra->act_gfx_alt, "-")) {
      secfile_insert_str(sfile, pextra->act_gfx_alt, "%s.act_gfx_alt", path);
    }
    if (strcmp(pextra->rmact_gfx, "-")) {
      secfile_insert_str(sfile, pextra->rmact_gfx, "%s.rmact_gfx", path);
    }
    if (strcmp(pextra->rmact_gfx_alt, "-")) {
      secfile_insert_str(sfile, pextra->rmact_gfx_alt, "%s.rmact_gfx_alt", path);
    }

    save_reqs_vector(sfile, &(pextra->reqs), path, "reqs");
    save_reqs_vector(sfile, &(pextra->rmreqs), path, "rmreqs");

    if (!pextra->buildable) {
      secfile_insert_bool(sfile, pextra->buildable, "%s.buildable", path);
    }
    if (pextra->defense_bonus != 0) {
      secfile_insert_int(sfile, pextra->defense_bonus, "%s.defense_bonus", path);
    }

    set_count = 0;
    unit_class_iterate(puc) {
      if (BV_ISSET(pextra->native_to, uclass_index(puc))) {
        puc_names[set_count++] = uclass_rule_name(puc);
      }
    } unit_class_iterate_end;

    if (set_count > 0) {
      secfile_insert_str_vec(sfile, puc_names, set_count,
                             "%s.native_to", path);
    }

    set_count = 0;
    for (flagi = 0; flagi < EF_COUNT; flagi++) {
      if (extra_has_flag(pextra, flagi)) {
        flag_names[set_count++] = extra_flag_id_name(flagi);
      }
    }

    if (set_count > 0) {
      secfile_insert_str_vec(sfile, flag_names, set_count,
                             "%s.flags", path);
    }

    set_count = 0;
    extra_type_iterate(confl) {
      if (!can_extras_coexist(pextra, confl)) {
        extra_names[set_count++] = extra_rule_name(confl);
      }
    } extra_type_iterate_end;

    if (set_count > 0) {
      secfile_insert_str_vec(sfile, extra_names, set_count,
                             "%s.conflicts", path);
    }

    set_count = 0;
    extra_type_iterate(top) {
      if (BV_ISSET(pextra->hidden_by, extra_index(top))) {
        extra_names[set_count++] = extra_rule_name(top);
      }
    } extra_type_iterate_end;

    if (set_count > 0) {
      secfile_insert_str_vec(sfile, extra_names, set_count,
                             "%s.hidden_by", path);
    }

  } extra_type_iterate_end;

  sect_idx = 0;
  base_type_iterate(pbase) {
    char path[512];
    const char *flag_names[BF_COUNT];
    int flagi;
    int set_count;

    fc_snprintf(path, sizeof(path), "base_%d", sect_idx++);

    secfile_insert_str(sfile, extra_rule_name(base_extra_get(pbase)),
                       "%s.name", path);

    secfile_insert_str(sfile, base_gui_type_name(pbase->gui_type),
                       "%s.gui_type", path);
    secfile_insert_int(sfile, pbase->build_time, "%s.build_time", path);

    if (pbase->border_sq >= 0) {
      secfile_insert_int(sfile, pbase->border_sq, "%s.border_sq", path);
    }
    if (pbase->vision_main_sq >= 0) {
      secfile_insert_int(sfile, pbase->vision_main_sq, "%s.vision_main_sq", path);
    }

    set_count = 0;
    for (flagi = 0; flagi < BF_COUNT; flagi++) {
      if (base_has_flag(pbase, flagi)) {
        flag_names[set_count++] = base_flag_id_name(flagi);
      }
    }

    if (set_count > 0) {
      secfile_insert_str_vec(sfile, flag_names, set_count,
                             "%s.flags", path);
    }

    save_strvec(sfile, pbase->helptext, path, "helptext");

  } base_type_iterate_end;

  sect_idx = 0;
  road_type_iterate(proad) {
    char path[512];
    const char *flag_names[RF_COUNT];
    int flagi;
    int set_count;

    fc_snprintf(path, sizeof(path), "road_%d", sect_idx++);

    secfile_insert_str(sfile, extra_rule_name(road_extra_get(proad)),
                       "%s.name", path);

    secfile_insert_int(sfile, proad->move_cost, "%s.move_cost", path);

    if (proad->move_mode != RMM_FAST_ALWAYS) {
      secfile_insert_str(sfile, road_move_mode_name(proad->move_mode),
                         "%s.move_mode", path);
    }

    secfile_insert_int(sfile, proad->build_time, "%s.build_time", path);

    output_type_iterate(o) {
      if (proad->tile_incr_const[o] != 0) {
        secfile_insert_int(sfile, proad->tile_incr_const[o],
                           "%s.%s_incr_const", path, get_output_identifier(o));
      }
      if (proad->tile_incr[o] != 0) {
        secfile_insert_int(sfile, proad->tile_incr[o],
                           "%s.%s_incr", path, get_output_identifier(o));
      }
      if (proad->tile_bonus[o] != 0) {
        secfile_insert_int(sfile, proad->tile_bonus[o],
                           "%s.%s_bonus", path, get_output_identifier(o));
      }
    } output_type_iterate_end;

    switch (proad->compat) {
    case ROCO_ROAD:
      secfile_insert_str(sfile, "Road", "%s.compat_special", path);
      break;
    case ROCO_RAILROAD:
      secfile_insert_str(sfile, "Railroad", "%s.compat_special", path);
      break;
    case ROCO_RIVER:
      secfile_insert_str(sfile, "River", "%s.compat_special", path);
      break;
    case ROCO_NONE:
      secfile_insert_str(sfile, "None", "%s.compat_special", path);
      break;
    }

    set_count = 0;
    for (flagi = 0; flagi < RF_COUNT; flagi++) {
      if (road_has_flag(proad, flagi)) {
        flag_names[set_count++] = road_flag_id_name(flagi);
      }
    }

    if (set_count > 0) {
      secfile_insert_str_vec(sfile, flag_names, set_count,
                             "%s.flags", path);
    }

    save_strvec(sfile, proad->helptext, path, "helptext");

  } road_type_iterate_end;

  return save_ruleset_file(sfile, filename);
}

/**************************************************************************
  Save one veteran system.
**************************************************************************/
static bool save_veteran_system(struct section_file *sfile, const char *path,
                                struct veteran_system *vsystem)
{
  const char *vlist_name[vsystem->levels];
  int vlist_power[vsystem->levels];
  int vlist_raise[vsystem->levels];
  int vlist_wraise[vsystem->levels];
  int vlist_move[vsystem->levels];
  int i;

  for (i = 0; i < vsystem->levels; i++) {
    vlist_name[i] = rule_name(&(vsystem->definitions[i].name));
    vlist_power[i] = vsystem->definitions[i].power_fact;
    vlist_raise[i] = vsystem->definitions[i].raise_chance;
    vlist_wraise[i] = vsystem->definitions[i].work_raise_chance;
    vlist_move[i] = vsystem->definitions[i].move_bonus;
  }

  secfile_insert_str_vec(sfile, vlist_name, vsystem->levels,
                         "%s.veteran_names", path);
  secfile_insert_int_vec(sfile, vlist_power, vsystem->levels,
                         "%s.veteran_power_fact", path);
  secfile_insert_int_vec(sfile, vlist_raise, vsystem->levels,
                         "%s.veteran_raise_chance", path);
  secfile_insert_int_vec(sfile, vlist_wraise, vsystem->levels,
                         "%s.veteran_work_raise_chance", path);
  secfile_insert_int_vec(sfile, vlist_move, vsystem->levels,
                         "%s.veteran_move_bonus", path);

  return TRUE;
}

/**************************************************************************
  Save unit combat bonuses list.
**************************************************************************/
static bool save_combat_bonuses(struct section_file *sfile,
                                struct unit_type *put,
                                char *path)
{
  int i = 0;

  combat_bonus_list_iterate(put->bonuses, pbonus) {
    secfile_insert_str(sfile, unit_type_flag_id_name(pbonus->flag),
                       "%s.bonuses%d.flag", path, i);
    secfile_insert_str(sfile, combat_bonus_type_name(pbonus->type),
                       "%s.bonuses%d.type", path, i);
    secfile_insert_int(sfile, pbonus->value,
                       "%s.bonuses%d.value", path, i);
    i++;
  } combat_bonus_list_iterate_end;

  return TRUE;
}

/**************************************************************************
  Save units.ruleset
**************************************************************************/
static bool save_units_ruleset(const char *filename, const char *name)
{
  struct section_file *sfile = create_ruleset_file(name, "unit");
  int i;
  int sect_idx;

  if (sfile == NULL) {
    return FALSE;
  }

  for (i = 0; i < MAX_NUM_USER_UNIT_FLAGS; i++) {
    const char *flagname = unit_type_flag_id_name(i + UTYF_USER_FLAG_1);
    const char *helptxt = unit_type_flag_helptxt(i + UTYF_USER_FLAG_1);

    if (flagname != NULL && helptxt != NULL) {
      secfile_insert_str(sfile, flagname, "control.flags%d.name", i);
      secfile_insert_str(sfile, helptxt, "control.flags%d.helptxt", i);
    }
  }

  save_veteran_system(sfile, "veteran_system", game.veteran);

  sect_idx = 0;
  unit_class_iterate(puc) {
    char path[512];
    char *hut_str = NULL;
    const char *flag_names[UCF_COUNT];
    int flagi;
    int set_count;

    fc_snprintf(path, sizeof(path), "unitclass_%d", sect_idx++);

    save_name_translation(sfile, &(puc->name), path);

    secfile_insert_int(sfile, puc->min_speed / SINGLE_MOVE,
                       "%s.min_speed", path);
    secfile_insert_int(sfile, puc->hp_loss_pct, "%s.hp_loss_pct", path);
    if (puc->non_native_def_pct != 100) {
      secfile_insert_int(sfile, puc->non_native_def_pct,
                         "%s.non_native_def_pct", path);
    }
    if (puc->hut_behavior != HUT_NORMAL) {
      switch (puc->hut_behavior) {
      case HUT_NORMAL:
        hut_str = "Normal";
        break;
      case HUT_NOTHING:
        hut_str = "Nothing";
        break;
      case HUT_FRIGHTEN:
        hut_str = "Frighten";
        break;
      }
      fc_assert(hut_str != NULL);
      secfile_insert_str(sfile, hut_str, "%s.hut_behavior", path);
    }

    set_count = 0;
    for (flagi = 0; flagi < UCF_COUNT; flagi++) {
      if (uclass_has_flag(puc, flagi)) {
        flag_names[set_count++] = unit_class_flag_id_name(flagi);
      }
    }

    if (set_count > 0) {
      secfile_insert_str_vec(sfile, flag_names, set_count,
                             "%s.flags", path);
    }

  } unit_class_iterate_end;

  sect_idx = 0;
  unit_type_iterate(put) {
    char path[512];
    const char *flag_names[UTYF_LAST_USER_FLAG + 1];
    int flagi;
    int set_count;

    fc_snprintf(path, sizeof(path), "unit_%d", sect_idx++);

    save_name_translation(sfile, &(put->name), path);

    secfile_insert_str(sfile, uclass_rule_name(put->uclass),
                       "%s.class", path);

    save_tech_ref(sfile, put->require_advance, path, "tech_req");

    if (put->need_government != NULL) {
      secfile_insert_str(sfile, government_rule_name(put->need_government),
                         "%s.gov_req", path);
    }

    if (put->need_improvement != NULL) {
      secfile_insert_str(sfile, improvement_rule_name(put->need_improvement),
                         "%s.impr_req", path);
    }

    if (put->obsoleted_by != NULL) {
      secfile_insert_str(sfile, utype_rule_name(put->obsoleted_by),
                         "%s.obsolete_by", path);
    }

    secfile_insert_str(sfile, put->graphic_str, "%s.graphic", path);
    if (strcmp("-", put->graphic_alt)) {
      secfile_insert_str(sfile, put->graphic_alt, "%s.graphic_alt", path);
    }
    if (strcmp("-", put->sound_move)) {
      secfile_insert_str(sfile, put->sound_move, "%s.sound_move", path);
    }
    if (strcmp("-", put->sound_move_alt)) {
      secfile_insert_str(sfile, put->sound_move_alt, "%s.sound_move_alt", path);
    }
    if (strcmp("-", put->sound_fight)) {
      secfile_insert_str(sfile, put->sound_fight, "%s.sound_fight", path);
    }
    if (strcmp("-", put->sound_fight_alt)) {
      secfile_insert_str(sfile, put->sound_fight_alt, "%s.sound_fight_alt", path);
    }

    secfile_insert_int(sfile, put->build_cost, "%s.build_cost", path);
    secfile_insert_int(sfile, put->pop_cost, "%s.pop_cost", path);
    secfile_insert_int(sfile, put->attack_strength, "%s.attack", path);
    secfile_insert_int(sfile, put->defense_strength, "%s.defense", path);
    secfile_insert_int(sfile, put->move_rate / SINGLE_MOVE, "%s.move_rate", path);
    secfile_insert_int(sfile, put->vision_radius_sq, "%s.vision_radius_sq", path);
    secfile_insert_int(sfile, put->transport_capacity, "%s.transport_cap", path);

    save_uclass_vec(sfile, &(put->cargo), path, "cargo", FALSE);
    save_uclass_vec(sfile, &(put->embarks), path, "embarks", TRUE);
    save_uclass_vec(sfile, &(put->disembarks), path, "disembarks", TRUE);

    secfile_insert_int(sfile, put->hp, "%s.hitpoints", path);
    secfile_insert_int(sfile, put->firepower, "%s.firepower", path);
    secfile_insert_int(sfile, put->fuel, "%s.fuel", path);
    secfile_insert_int(sfile, put->happy_cost, "%s.uk_happy", path);

    output_type_iterate(o) {
      if (put->upkeep[o] != 0) {
        secfile_insert_int(sfile, put->upkeep[o], "%s.uk_%s",
                           path, get_output_identifier(o));
      }
    } output_type_iterate_end;

    if (put->converted_to != NULL) {
      secfile_insert_str(sfile, utype_rule_name(put->converted_to),
                         "%s.convert_to", path);
    }
    if (put->convert_time != 1) {
      secfile_insert_int(sfile, put->convert_time, "%s.convert_time", path);
    }

    save_combat_bonuses(sfile, put, path);
    save_uclass_vec(sfile, &(put->targets), path, "targets", TRUE);

    if (put->veteran != NULL) {
      save_veteran_system(sfile, path, put->veteran);
    }

    if (put->paratroopers_range != 0) {
      secfile_insert_int(sfile, put->paratroopers_range,
                         "%s.paratroopers_range", path);
     secfile_insert_int(sfile, put->paratroopers_mr_req / SINGLE_MOVE,
                         "%s.paratroopers_mr_req", path);
     secfile_insert_int(sfile, put->paratroopers_mr_sub / SINGLE_MOVE,
                         "%s.paratroopers_mr_sub", path);
    }
    if (put->bombard_rate != 0) {
      secfile_insert_int(sfile, put->bombard_rate,
                         "%s.bombard_rate", path);
    }
    if (put->city_size != 1) {
      secfile_insert_int(sfile, put->city_size,
                         "%s.city_size", path);
    }

    set_count = 0;
    for (flagi = 0; flagi <= UTYF_LAST_USER_FLAG; flagi++) {
      if (utype_has_flag(put, flagi)) {
        flag_names[set_count++] = unit_type_flag_id_name(flagi);
      }
    }

    if (set_count > 0) {
      secfile_insert_str_vec(sfile, flag_names, set_count,
                             "%s.flags", path);
    }

    set_count = 0;
    for (flagi = L_FIRST; flagi < L_LAST; flagi++) {
      if (utype_has_role(put, flagi)) {
        flag_names[set_count++] = unit_role_id_name(flagi);
      }
    }

    if (set_count > 0) {
      secfile_insert_str_vec(sfile, flag_names, set_count,
                             "%s.roles", path);
    }

    save_strvec(sfile, put->helptext, path, "helptext");

  } unit_type_iterate_end;

  return save_ruleset_file(sfile, filename);
}

/**************************************************************************
  Save script.lua
**************************************************************************/
static bool save_script_lua(const char *filename, const char *name)
{
  char *buffer = get_script_buffer();

  if (buffer != NULL) {
    FILE *ffile = fc_fopen(filename, "w");
    int full_len = strlen(buffer);
    int len;

    if (ffile != NULL) {
      len = fwrite(buffer, 1, full_len, ffile);

      if (len != full_len) {
        return FALSE;
      }

      fclose(ffile);
    } else {
      return FALSE;
    }
  }

  return TRUE;
}

/**************************************************************************
  Save ruleset to directory given.
**************************************************************************/
bool save_ruleset(const char *path, const char *name)
{
  if (make_dir(path)) {
    bool success = TRUE;
    char filename[500];

    if (success) {
      fc_snprintf(filename, sizeof(filename), "%s/buildings.ruleset", path);
      success = save_buildings_ruleset(filename, name);
    }

    if (success) {
      fc_snprintf(filename, sizeof(filename), "%s/styles.ruleset", path);
      success = save_styles_ruleset(filename, name);
    }

    if (success) {
      fc_snprintf(filename, sizeof(filename), "%s/cities.ruleset", path);
      success = save_cities_ruleset(filename, name);
    }

    if (success) {
      fc_snprintf(filename, sizeof(filename), "%s/effects.ruleset", path);
      success = save_effects_ruleset(filename, name);
    }

    if (success) {
      fc_snprintf(filename, sizeof(filename), "%s/game.ruleset", path);
      success = save_game_ruleset(filename, name);
    }

    if (success) {
      fc_snprintf(filename, sizeof(filename), "%s/governments.ruleset", path);
      success = save_governments_ruleset(filename, name);
    }

    if (success) {
      fc_snprintf(filename, sizeof(filename), "%s/nations.ruleset", path);
      success = save_nations_ruleset(filename, name);
    }

    if (success) {
      fc_snprintf(filename, sizeof(filename), "%s/techs.ruleset", path);
      success = save_techs_ruleset(filename, name);
    }

    if (success) {
      fc_snprintf(filename, sizeof(filename), "%s/terrain.ruleset", path);
      success = save_terrain_ruleset(filename, name);
    }

    if (success) {
      fc_snprintf(filename, sizeof(filename), "%s/units.ruleset", path);
      success = save_units_ruleset(filename, name);
    }

    if (success) {
      fc_snprintf(filename, sizeof(filename), "%s/script.lua", path);
      success = save_script_lua(filename, name);
    }

    return success;
  } else {
    log_error("Failed to create directory %s", path);
    return FALSE;
  }

  return TRUE;
}
