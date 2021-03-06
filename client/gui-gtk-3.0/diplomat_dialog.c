/********************************************************************** 
 Freeciv - Copyright (C) 1996-2005 - Freeciv Development Team
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

#include <gtk/gtk.h>

/* utility */
#include "astring.h"
#include "support.h"

/* common */
#include "actions.h"
#include "game.h"
#include "unit.h"
#include "unitlist.h"

/* client */
#include "dialogs_g.h"
#include "chatline.h"
#include "choice_dialog.h"
#include "client_main.h"
#include "climisc.h"
#include "connectdlg_common.h"
#include "control.h"
#include "gui_main.h"
#include "gui_stuff.h"
#include "mapview.h"
#include "packhand.h"

/* client/gui-gtk-3.0 */
#include "citydlg.h"
#include "dialogs.h"
#include "wldlg.h"

static GtkWidget *diplomat_dialog;
static int diplomat_id;
static int diplomat_target_id[ATK_COUNT];

static GtkWidget  *spy_tech_shell;
static int         steal_advance;

static GtkWidget  *spy_sabotage_shell;
static int         sabotage_improvement;

/****************************************************************
  User responded to bribe dialog
*****************************************************************/
static void bribe_response(GtkWidget *w, gint response)
{
  if (response == GTK_RESPONSE_YES) {
    request_diplomat_action(DIPLOMAT_BRIBE, diplomat_id,
                            diplomat_target_id[ATK_UNIT], 0);
  }
  gtk_widget_destroy(w);
}

/****************************************************************
  Ask the server how much the bribe is
*****************************************************************/
static void diplomat_bribe_callback(GtkWidget *w, gpointer data)
{
  if (NULL != game_unit_by_number(diplomat_id)
      && NULL != game_unit_by_number(diplomat_target_id[ATK_UNIT])) {
    request_diplomat_answer(DIPLOMAT_BRIBE, diplomat_id,
                            diplomat_target_id[ATK_UNIT], 0);
  }
  gtk_widget_destroy(diplomat_dialog);
}

/*************************************************************************
  Popup unit bribe dialog
**************************************************************************/
void popup_bribe_dialog(struct unit *actor, struct unit *punit, int cost)
{
  GtkWidget *shell;
  char buf[1024];

  fc_snprintf(buf, ARRAY_SIZE(buf), PL_("Treasury contains %d gold.",
                                        "Treasury contains %d gold.",
                                        client_player()->economic.gold),
              client_player()->economic.gold);

  if (cost <= client_player()->economic.gold) {
    shell = gtk_message_dialog_new(NULL, 0,
      GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO,
      /* TRANS: %s is pre-pluralised "Treasury contains %d gold." */
      PL_("Bribe unit for %d gold?\n%s",
          "Bribe unit for %d gold?\n%s", cost), cost, buf);
    gtk_window_set_title(GTK_WINDOW(shell), _("Bribe Enemy Unit"));
    setup_dialog(shell, toplevel);
  } else {
    shell = gtk_message_dialog_new(NULL, 0,
      GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE,
      /* TRANS: %s is pre-pluralised "Treasury contains %d gold." */
      PL_("Bribing the unit costs %d gold.\n%s",
          "Bribing the unit costs %d gold.\n%s", cost), cost, buf);
    gtk_window_set_title(GTK_WINDOW(shell), _("Traitors Demand Too Much!"));
    setup_dialog(shell, toplevel);
  }
  gtk_window_present(GTK_WINDOW(shell));
  
  g_signal_connect(shell, "response", G_CALLBACK(bribe_response), NULL);
}

/****************************************************************
  User selected sabotaging from choice dialog
*****************************************************************/
static void diplomat_sabotage_callback(GtkWidget *w, gpointer data)
{
  if (NULL != game_unit_by_number(diplomat_id)
      && NULL != game_city_by_number(diplomat_target_id[ATK_CITY])) {
    request_diplomat_action(DIPLOMAT_SABOTAGE, diplomat_id,
                            diplomat_target_id[ATK_CITY], B_LAST + 1);
  }
  gtk_widget_destroy(diplomat_dialog);
}

/****************************************************************
  User selected investigating from choice dialog
*****************************************************************/
static void diplomat_investigate_callback(GtkWidget *w, gpointer data)
{
  if (NULL != game_city_by_number(diplomat_target_id[ATK_CITY])
      && NULL != game_unit_by_number(diplomat_id)) {
    request_diplomat_action(DIPLOMAT_INVESTIGATE, diplomat_id,
                            diplomat_target_id[ATK_CITY], 0);
  }
  gtk_widget_destroy(diplomat_dialog);
}

/****************************************************************
  User selected unit sabotaging from choice dialog
*****************************************************************/
static void spy_sabotage_unit_callback(GtkWidget *w, gpointer data)
{
  request_diplomat_action(SPY_SABOTAGE_UNIT, diplomat_id,
                          diplomat_target_id[ATK_UNIT], 0);
  gtk_widget_destroy(diplomat_dialog);
}

/****************************************************************
  User selected embassy establishing from choice dialog
*****************************************************************/
static void diplomat_embassy_callback(GtkWidget *w, gpointer data)
{
  if (NULL != game_unit_by_number(diplomat_id)
      && NULL != game_city_by_number(diplomat_target_id[ATK_CITY])) {
    request_diplomat_action(DIPLOMAT_EMBASSY, diplomat_id,
                            diplomat_target_id[ATK_CITY], 0);
  }
  gtk_widget_destroy(diplomat_dialog);
}

/****************************************************************
  User selected poisoning from choice dialog
*****************************************************************/
static void spy_poison_callback(GtkWidget *w, gpointer data)
{
  if (NULL != game_unit_by_number(diplomat_id)
      && NULL != game_city_by_number(diplomat_target_id[ATK_CITY])) {
    request_diplomat_action(SPY_POISON, diplomat_id,
                            diplomat_target_id[ATK_CITY], 0);
  }
  gtk_widget_destroy(diplomat_dialog);
}

/****************************************************************
  User selected stealing from choice dialog
*****************************************************************/
static void diplomat_steal_callback(GtkWidget *w, gpointer data)
{
  if (NULL != game_unit_by_number(diplomat_id)
      && NULL != game_city_by_number(diplomat_target_id[ATK_CITY])) {
    request_diplomat_action(DIPLOMAT_STEAL, diplomat_id,
                            diplomat_target_id[ATK_CITY], A_UNSET);
  }
  gtk_widget_destroy(diplomat_dialog);
}

/****************************************************************
  User responded to steal advances dialog
*****************************************************************/
static void spy_advances_response(GtkWidget *w, gint response, gpointer data)
{
  if (response == GTK_RESPONSE_ACCEPT && steal_advance > 0) {
    if (NULL != game_unit_by_number(diplomat_id)
        && NULL != game_city_by_number(diplomat_target_id[ATK_CITY])) {
      request_diplomat_action(DIPLOMAT_STEAL_TARGET, diplomat_id,
                              diplomat_target_id[ATK_CITY], steal_advance);
    }
  }
  gtk_widget_destroy(spy_tech_shell);
  spy_tech_shell = NULL;
}

/****************************************************************
  User selected entry in steal advances dialog
*****************************************************************/
static void spy_advances_callback(GtkTreeSelection *select, gpointer data)
{
  GtkTreeModel *model;
  GtkTreeIter it;

  if (gtk_tree_selection_get_selected(select, &model, &it)) {
    gtk_tree_model_get(model, &it, 1, &steal_advance, -1);
    
    gtk_dialog_set_response_sensitive(GTK_DIALOG(spy_tech_shell),
      GTK_RESPONSE_ACCEPT, TRUE);
  } else {
    steal_advance = 0;
	  
    gtk_dialog_set_response_sensitive(GTK_DIALOG(spy_tech_shell),
      GTK_RESPONSE_ACCEPT, FALSE);
  }
}

/****************************************************************
  Create spy's tech stealing dialog
*****************************************************************/
static void create_advances_list(struct player *pplayer,
				 struct player *pvictim)
{  
  GtkWidget *sw, *label, *vbox, *view;
  GtkListStore *store;
  GtkCellRenderer *rend;
  GtkTreeViewColumn *col;

  spy_tech_shell = gtk_dialog_new_with_buttons(_("Steal Technology"),
    NULL,
    0,
    GTK_STOCK_CANCEL,
    GTK_RESPONSE_CANCEL,
    _("_Steal"),
    GTK_RESPONSE_ACCEPT,
    NULL);
  setup_dialog(spy_tech_shell, toplevel);
  gtk_window_set_position(GTK_WINDOW(spy_tech_shell), GTK_WIN_POS_MOUSE);

  gtk_dialog_set_default_response(GTK_DIALOG(spy_tech_shell),
				  GTK_RESPONSE_ACCEPT);

  label = gtk_frame_new(_("Select Advance to Steal"));
  gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(spy_tech_shell))), label);

  vbox = gtk_grid_new();
  gtk_orientable_set_orientation(GTK_ORIENTABLE(vbox),
                                 GTK_ORIENTATION_VERTICAL);
  gtk_grid_set_row_spacing(GTK_GRID(vbox), 6);
  gtk_container_add(GTK_CONTAINER(label), vbox);
      
  store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_INT);

  view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
  gtk_widget_set_hexpand(view, TRUE);
  gtk_widget_set_vexpand(view, TRUE);
  g_object_unref(store);
  gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(view), FALSE);

  rend = gtk_cell_renderer_text_new();
  col = gtk_tree_view_column_new_with_attributes(NULL, rend,
						 "text", 0, NULL);
  gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);

  label = g_object_new(GTK_TYPE_LABEL,
    "use-underline", TRUE,
    "mnemonic-widget", view,
    "label", _("_Advances:"),
    "xalign", 0.0,
    "yalign", 0.5,
    NULL);
  gtk_container_add(GTK_CONTAINER(vbox), label);
  
  sw = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw),
				      GTK_SHADOW_ETCHED_IN);
  gtk_container_add(GTK_CONTAINER(sw), view);

  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
    GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
  gtk_widget_set_size_request(sw, -1, 200);
  
  gtk_container_add(GTK_CONTAINER(vbox), sw);

  /* Now populate the list */
  if (pvictim) { /* you don't want to know what lag can do -- Syela */
    GtkTreeIter it;
    GValue value = { 0, };

    advance_index_iterate(A_FIRST, i) {
      if(player_invention_state(pvictim, i)==TECH_KNOWN && 
	 (player_invention_state(pplayer, i)==TECH_UNKNOWN || 
	  player_invention_state(pplayer, i)==TECH_PREREQS_KNOWN)) {
	gtk_list_store_append(store, &it);

	g_value_init(&value, G_TYPE_STRING);
	g_value_set_static_string(&value,
				  advance_name_for_player(client.conn.playing, i));
	gtk_list_store_set_value(store, &it, 0, &value);
	g_value_unset(&value);
	gtk_list_store_set(store, &it, 1, i, -1);
      }
    } advance_index_iterate_end;

    gtk_list_store_append(store, &it);

    g_value_init(&value, G_TYPE_STRING);
    {
      struct astring str = ASTRING_INIT;
      /* TRANS: %s is a unit name, e.g., Spy */
      astr_set(&str, _("At %s's Discretion"),
               unit_name_translation(game_unit_by_number(diplomat_id)));
      g_value_set_string(&value, astr_str(&str));
      astr_free(&str);
    }
    gtk_list_store_set_value(store, &it, 0, &value);
    g_value_unset(&value);
    gtk_list_store_set(store, &it, 1, A_UNSET, -1);
  }

  gtk_dialog_set_response_sensitive(GTK_DIALOG(spy_tech_shell),
    GTK_RESPONSE_ACCEPT, FALSE);
  
  gtk_widget_show_all(gtk_dialog_get_content_area(GTK_DIALOG(spy_tech_shell)));

  g_signal_connect(gtk_tree_view_get_selection(GTK_TREE_VIEW(view)), "changed",
		   G_CALLBACK(spy_advances_callback), NULL);
  g_signal_connect(spy_tech_shell, "response",
		   G_CALLBACK(spy_advances_response), NULL);
  
  steal_advance = 0;

  gtk_tree_view_focus(GTK_TREE_VIEW(view));
}

/****************************************************************
  User has responded to spy's sabotage building dialog
*****************************************************************/
static void spy_improvements_response(GtkWidget *w, gint response, gpointer data)
{
  if (response == GTK_RESPONSE_ACCEPT && sabotage_improvement > -2) {
    if (NULL != game_unit_by_number(diplomat_id)
        && NULL != game_city_by_number(diplomat_target_id[ATK_CITY])) {
      request_diplomat_action(DIPLOMAT_SABOTAGE_TARGET, diplomat_id,
                              diplomat_target_id[ATK_CITY],
                              sabotage_improvement + 1);
    }
  }
  gtk_widget_destroy(spy_sabotage_shell);
  spy_sabotage_shell = NULL;
}

/****************************************************************
  User has selected new building from spy's sabotage dialog
*****************************************************************/
static void spy_improvements_callback(GtkTreeSelection *select, gpointer data)
{
  GtkTreeModel *model;
  GtkTreeIter it;

  if (gtk_tree_selection_get_selected(select, &model, &it)) {
    gtk_tree_model_get(model, &it, 1, &sabotage_improvement, -1);
    
    gtk_dialog_set_response_sensitive(GTK_DIALOG(spy_sabotage_shell),
      GTK_RESPONSE_ACCEPT, TRUE);
  } else {
    sabotage_improvement = -2;
	  
    gtk_dialog_set_response_sensitive(GTK_DIALOG(spy_sabotage_shell),
      GTK_RESPONSE_ACCEPT, FALSE);
  }
}

/****************************************************************
  Creates spy's building sabotaging dialog
*****************************************************************/
static void create_improvements_list(struct player *pplayer,
				     struct city *pcity)
{  
  GtkWidget *sw, *label, *vbox, *view;
  GtkListStore *store;
  GtkCellRenderer *rend;
  GtkTreeViewColumn *col;
  GtkTreeIter it;
  
  spy_sabotage_shell = gtk_dialog_new_with_buttons(_("Sabotage Improvements"),
    NULL,
    0,
    GTK_STOCK_CANCEL,
    GTK_RESPONSE_CANCEL,
    _("_Sabotage"), 
    GTK_RESPONSE_ACCEPT,
    NULL);
  setup_dialog(spy_sabotage_shell, toplevel);
  gtk_window_set_position(GTK_WINDOW(spy_sabotage_shell), GTK_WIN_POS_MOUSE);

  gtk_dialog_set_default_response(GTK_DIALOG(spy_sabotage_shell),
				  GTK_RESPONSE_ACCEPT);

  label = gtk_frame_new(_("Select Improvement to Sabotage"));
  gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(spy_sabotage_shell))), label);

  vbox = gtk_grid_new();
  gtk_orientable_set_orientation(GTK_ORIENTABLE(vbox),
                                 GTK_ORIENTATION_VERTICAL);
  gtk_grid_set_row_spacing(GTK_GRID(vbox), 6);
  gtk_container_add(GTK_CONTAINER(label), vbox);
      
  store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_INT);

  view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
  gtk_widget_set_hexpand(view, TRUE);
  gtk_widget_set_vexpand(view, TRUE);
  g_object_unref(store);
  gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(view), FALSE);

  rend = gtk_cell_renderer_text_new();
  col = gtk_tree_view_column_new_with_attributes(NULL, rend,
						 "text", 0, NULL);
  gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);

  label = g_object_new(GTK_TYPE_LABEL,
    "use-underline", TRUE,
    "mnemonic-widget", view,
    "label", _("_Improvements:"),
    "xalign", 0.0,
    "yalign", 0.5,
    NULL);
  gtk_container_add(GTK_CONTAINER(vbox), label);
  
  sw = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw),
				      GTK_SHADOW_ETCHED_IN);
  gtk_container_add(GTK_CONTAINER(sw), view);

  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
    GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
  gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(sw), 200);
  
  gtk_container_add(GTK_CONTAINER(vbox), sw);

  /* Now populate the list */
  gtk_list_store_append(store, &it);
  gtk_list_store_set(store, &it, 0, _("City Production"), 1, -1, -1);

  city_built_iterate(pcity, pimprove) {
    if (pimprove->sabotage > 0) {
      gtk_list_store_append(store, &it);
      gtk_list_store_set(store, &it,
                         0, city_improvement_name_translation(pcity, pimprove),
                         1, improvement_number(pimprove),
                         -1);
    }  
  } city_built_iterate_end;

  gtk_list_store_append(store, &it);
  {
    struct astring str = ASTRING_INIT;
    /* TRANS: %s is a unit name, e.g., Spy */
    astr_set(&str, _("At %s's Discretion"),
             unit_name_translation(game_unit_by_number(diplomat_id)));
    gtk_list_store_set(store, &it, 0, astr_str(&str), 1, B_LAST, -1);
    astr_free(&str);
  }

  gtk_dialog_set_response_sensitive(GTK_DIALOG(spy_sabotage_shell),
    GTK_RESPONSE_ACCEPT, FALSE);
  
  gtk_widget_show_all(gtk_dialog_get_content_area(GTK_DIALOG(spy_sabotage_shell)));

  g_signal_connect(gtk_tree_view_get_selection(GTK_TREE_VIEW(view)), "changed",
		   G_CALLBACK(spy_improvements_callback), NULL);
  g_signal_connect(spy_sabotage_shell, "response",
		   G_CALLBACK(spy_improvements_response), NULL);

  sabotage_improvement = -2;
	  
  gtk_tree_view_focus(GTK_TREE_VIEW(view));
}

/****************************************************************
  Popup tech stealing dialog with list of possible techs
*****************************************************************/
static void spy_steal_popup(GtkWidget *w, gpointer data)
{
  struct city *pvcity = game_city_by_number(diplomat_target_id[ATK_CITY]);
  struct player *pvictim = NULL;

  if(pvcity)
    pvictim = city_owner(pvcity);

/* it is concievable that pvcity will not be found, because something
has happened to the city during latency.  Therefore we must initialize
pvictim to NULL and account for !pvictim in create_advances_list. -- Syela */
  
  if(!spy_tech_shell){
    create_advances_list(client.conn.playing, pvictim);
    gtk_window_present(GTK_WINDOW(spy_tech_shell));
  }
  gtk_widget_destroy(diplomat_dialog);
}

/****************************************************************
 Requests up-to-date list of improvements, the return of
 which will trigger the popup_sabotage_dialog() function.
*****************************************************************/
static void spy_request_sabotage_list(GtkWidget *w, gpointer data)
{
  if (NULL != game_unit_by_number(diplomat_id)
      && NULL != game_city_by_number(diplomat_target_id[ATK_CITY])) {
    request_diplomat_answer(DIPLOMAT_SABOTAGE_TARGET, diplomat_id,
                            diplomat_target_id[ATK_CITY], 0);
  }
  gtk_widget_destroy(diplomat_dialog);
}

/*************************************************************************
 Pops-up the Spy sabotage dialog, upon return of list of
 available improvements requested by the above function.
**************************************************************************/
void popup_sabotage_dialog(struct unit *actor, struct city *pcity)
{
  if(!spy_sabotage_shell){
    create_improvements_list(client.conn.playing, pcity);
    gtk_window_present(GTK_WINDOW(spy_sabotage_shell));
  }
}

/****************************************************************
...  Ask the server how much the revolt is going to cost us
*****************************************************************/
static void diplomat_incite_callback(GtkWidget *w, gpointer data)
{
  if (NULL != game_unit_by_number(diplomat_id)
      && NULL != game_city_by_number(diplomat_target_id[ATK_CITY])) {
    request_diplomat_answer(DIPLOMAT_INCITE, diplomat_id,
                            diplomat_target_id[ATK_CITY], 0);
  }
  gtk_widget_destroy(diplomat_dialog);
}

/****************************************************************
  User has responded to incite dialog
*****************************************************************/
static void incite_response(GtkWidget *w, gint response)
{
  if (response == GTK_RESPONSE_YES) {
    request_diplomat_action(DIPLOMAT_INCITE, diplomat_id,
                            diplomat_target_id[ATK_CITY], 0);
  }
  gtk_widget_destroy(w);
}

/*************************************************************************
Popup the yes/no dialog for inciting, since we know the cost now
**************************************************************************/
void popup_incite_dialog(struct unit *actor, struct city *pcity, int cost)
{
  GtkWidget *shell;
  char buf[1024];

  fc_snprintf(buf, ARRAY_SIZE(buf), PL_("Treasury contains %d gold.",
                                        "Treasury contains %d gold.",
                                        client_player()->economic.gold),
              client_player()->economic.gold);

  if (INCITE_IMPOSSIBLE_COST == cost) {
    shell = gtk_message_dialog_new(NULL,
      0,
      GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE,
      _("You can't incite a revolt in %s."),
      city_name(pcity));
    gtk_window_set_title(GTK_WINDOW(shell), _("City can't be incited!"));
  setup_dialog(shell, toplevel);
  } else if (cost <= client_player()->economic.gold) {
    shell = gtk_message_dialog_new(NULL, 0,
      GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO,
      /* TRANS: %s is pre-pluralised "Treasury contains %d gold." */
      PL_("Incite a revolt for %d gold?\n%s",
          "Incite a revolt for %d gold?\n%s", cost), cost, buf);
    gtk_window_set_title(GTK_WINDOW(shell), _("Incite a Revolt!"));
    setup_dialog(shell, toplevel);
  } else {
    shell = gtk_message_dialog_new(NULL,
      0,
      GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE,
      /* TRANS: %s is pre-pluralised "Treasury contains %d gold." */
      PL_("Inciting a revolt costs %d gold.\n%s",
          "Inciting a revolt costs %d gold.\n%s", cost), cost, buf);
    gtk_window_set_title(GTK_WINDOW(shell), _("Traitors Demand Too Much!"));
    setup_dialog(shell, toplevel);
  }
  gtk_window_present(GTK_WINDOW(shell));
  
  g_signal_connect(shell, "response", G_CALLBACK(incite_response), NULL);
}


/****************************************************************
  Callback from diplomat/spy dialog for "keep moving".
  (This should only occur when entering allied city.)
*****************************************************************/
static void diplomat_keep_moving_city_callback(GtkWidget *w, gpointer data)
{
  struct unit *punit;
  struct city *pcity;

  if ((punit = game_unit_by_number(diplomat_id))
      && (pcity = game_city_by_number(diplomat_target_id[ATK_CITY]))
      && !same_pos(unit_tile(punit), city_tile(pcity))) {
    request_diplomat_action(DIPLOMAT_MOVE, diplomat_id,
                            diplomat_target_id[ATK_CITY], ATK_CITY);
  }
  gtk_widget_destroy(diplomat_dialog);
}

/*************************************************************************
  Callback from diplomat/spy dialog for "keep moving".
  (This should only occur when entering the tile of an allied unit.)
**************************************************************************/
static void diplomat_keep_moving_unit_callback(GtkWidget *w, gpointer data)
{
  struct unit *punit;
  struct unit *tunit;

  if ((punit = game_unit_by_number(diplomat_id))
      && (tunit = game_unit_by_number(diplomat_target_id[ATK_UNIT]))
      && !same_pos(unit_tile(punit), unit_tile(tunit))) {
    request_diplomat_action(DIPLOMAT_MOVE, diplomat_id,
                            diplomat_target_id[ATK_UNIT], ATK_UNIT);
  }
  gtk_widget_destroy(diplomat_dialog);
}

/****************************************************************
  Diplomat dialog has been destoryed
*****************************************************************/
static void diplomat_destroy_callback(GtkWidget *w, gpointer data)
{
  diplomat_dialog = NULL;
  choose_action_queue_next();
}

/****************************************************************
  Diplomat dialog has been canceled
*****************************************************************/
static void diplomat_cancel_callback(GtkWidget *w, gpointer data)
{
  gtk_widget_destroy(diplomat_dialog);
}

/******************************************************************
  Show the user the action if it is enabled.
*******************************************************************/
static void action_entry(GtkWidget *shl,
                         int action_id,
                         const action_probability *action_probabilities,
                         GCallback handler)
{
  const gchar *label;
  action_probability success_propability;
  gchar *chance_text;

  success_propability = action_probabilities[action_id];

  /* How to interpret action probabilities like success_propability is
   * documented in actions.h */
  switch (success_propability) {
  case ACTPROB_IMPOSSIBLE:
    /* Don't even show disabled actions */
    break;
  case ACTPROB_NOT_KNOWN:
    /* Unknown because the player don't have the required knowledge to
     * determine the probability of success for this action. */
    label = action_prepare_ui_name(action_id, "_", "");

    choice_dialog_add(shl, label, handler, NULL, TRUE);
    break;
  case ACTPROB_NOT_IMPLEMENTED:
    /* Unknown because of missing server support. */
    label = action_prepare_ui_name(action_id, "_", "");

    choice_dialog_add(shl, label, handler, NULL, FALSE);
    break;
  default:
    /* Should be in the range 1 (0.5%) to 200 (100%) */
    fc_assert_msg(success_propability < 201,
                  "Diplomat action probability out of range");

    /* TRANS: the probability that a diplomat action will succeed. */
    chance_text = g_strdup_printf(_(" (%.1f%% chance of success)"),
                                  (double)success_propability / 2);

    label = action_prepare_ui_name(action_id, "_", chance_text);

    /* The unit of success_propability is 0.5% chance of success. */
    choice_dialog_add(shl, label, handler, NULL, FALSE);

    free(chance_text);

    break;
  }
}

/****************************************************************
 Popup new diplomat dialog.
*****************************************************************/
void popup_diplomat_dialog(struct unit *punit, struct tile *dest_tile,
                           const action_probability *action_probabilities)
{
  struct city *pcity;
  struct unit *ptunit;
  GtkWidget *shl;
  struct astring title = ASTRING_INIT, text = ASTRING_INIT;

  diplomat_target_id[ATK_CITY] = -1;
  diplomat_target_id[ATK_UNIT] = -1;

  diplomat_id = punit->id;

  astr_set(&title,
           /* TRANS: %s is a unit name, e.g., Spy */
           _("Choose Your %s's Strategy"), unit_name_translation(punit));
  astr_set(&text,
           /* TRANS: %s is a unit name, e.g., Diplomat, Spy */
           _("Your %s is waiting for your command."),
           unit_name_translation(punit));

  shl = choice_dialog_start(GTK_WINDOW(toplevel), astr_str(&title),
                            astr_str(&text));

  if ((pcity = tile_city(dest_tile))) {
    /* Spy/Diplomat acting against a city */

    diplomat_target_id[ATK_CITY] = pcity->id;

    action_entry(shl,
                 ACTION_ESTABLISH_EMBASSY,
                 action_probabilities,
                 (GCallback)diplomat_embassy_callback);

    action_entry(shl,
                 ACTION_SPY_INVESTIGATE_CITY,
                 action_probabilities,
                 (GCallback)diplomat_investigate_callback);

    action_entry(shl,
                 ACTION_SPY_POISON,
                 action_probabilities,
                 (GCallback)spy_poison_callback);

    action_entry(shl,
                 ACTION_SPY_SABOTAGE_CITY,
                 action_probabilities,
                 (GCallback)diplomat_sabotage_callback);

    action_entry(shl,
                 ACTION_SPY_TARGETED_SABOTAGE_CITY,
                 action_probabilities,
                 (GCallback)spy_request_sabotage_list);

    action_entry(shl,
                 ACTION_SPY_STEAL_TECH,
                 action_probabilities,
                 (GCallback)diplomat_steal_callback);

    action_entry(shl,
                 ACTION_SPY_TARGETED_STEAL_TECH,
                 action_probabilities,
                 (GCallback)spy_steal_popup);

    action_entry(shl,
                 ACTION_SPY_INCITE_CITY,
                 action_probabilities,
                 (GCallback)diplomat_incite_callback);
  }

  if ((ptunit = unit_list_get(dest_tile->units, 0))){
    /* Spy/Diplomat acting against a unit */

    diplomat_target_id[ATK_UNIT] = ptunit->id;

    action_entry(shl,
                 ACTION_SPY_BRIBE_UNIT,
                 action_probabilities,
                 (GCallback)diplomat_bribe_callback);

    action_entry(shl,
                 ACTION_SPY_SABOTAGE_UNIT,
                 action_probabilities,
                 (GCallback)spy_sabotage_unit_callback);
  }

  if (diplomat_can_do_action(punit, DIPLOMAT_MOVE, dest_tile)) {
    if (pcity) {
      choice_dialog_add(shl, _("_Keep moving"),
                        (GCallback)diplomat_keep_moving_city_callback,
                        NULL, FALSE);
    } else {
      choice_dialog_add(shl, _("_Keep moving"),
                        (GCallback)diplomat_keep_moving_unit_callback,
                        NULL, FALSE);
    }
  }

  choice_dialog_add(shl, GTK_STOCK_CANCEL,
                    (GCallback)diplomat_cancel_callback, NULL, FALSE);

  choice_dialog_end(shl);

  diplomat_dialog = shl;

  choice_dialog_set_hide(shl, TRUE);
  g_signal_connect(shl, "destroy",
                   G_CALLBACK(diplomat_destroy_callback), NULL);
  g_signal_connect(shl, "delete_event",
                   G_CALLBACK(diplomat_cancel_callback), NULL);
  astr_free(&title);
  astr_free(&text);
}

/****************************************************************
  Returns id of a diplomat currently handled in diplomat dialog
*****************************************************************/
int diplomat_handled_in_diplomat_dialog(void)
{
  if (diplomat_dialog == NULL) {
    return -1;
  }
  return diplomat_id;
}

/****************************************************************
  Closes the diplomat dialog
****************************************************************/
void close_diplomat_dialog(void)
{
  if (diplomat_dialog != NULL) {
    gtk_widget_destroy(diplomat_dialog);
  }
}
