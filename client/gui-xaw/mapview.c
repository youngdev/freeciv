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
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/Scrollbar.h>

#include "canvas.h"
#include "pixcomm.h"

#include "fcintl.h"
#include "game.h"
#include "government.h"		/* government_graphic() */
#include "map.h"
#include "player.h"
#include "rand.h"
#include "support.h"
#include "timing.h"
#include "unit.h"


#include "civclient.h"
#include "climisc.h"
#include "colors.h"
#include "control.h" /* set_unit_focus_no_center and get_unit_in_focus */
#include "graphics.h"
#include "gui_stuff.h"
#include "mapctrl.h"
#include "options.h"
#include "tilespec.h"

#include "mapview.h"

/*
The bottom row of the map was sometimes hidden.

As of now the top left corner is always aligned with the tiles. This is what
causes the problem in the first place. The ideal solution would be to align the
window with the bottom left tiles if you tried to center the window on a tile
closer than (screen_tiles_height/2 -1) to the south pole.

But, for now, I just grepped for occurences where the ysize (or the values
derived from it) were used, and those places that had relevance to drawing the
map, and I added 1 (using the EXTRA_BOTTOM_ROW constant).

-Thue
*/
#define EXTRA_BOTTOM_ROW 1


extern Display	*display;
extern GC civ_gc, font_gc, prod_font_gc;
extern GC fill_bg_gc;
extern GC fill_tile_gc;
extern XFontStruct *main_font_struct;
extern XFontStruct *prod_font_struct;
extern Window root_window;

extern Widget map_canvas, map_form, info_command, turn_done_button;
extern Widget map_vertical_scrollbar, map_horizontal_scrollbar;
extern Widget overview_canvas, main_form, left_column_form;
extern Widget menu_form, below_menu_form, bottom_form;
extern Widget econ_label[10];
extern Widget bulb_label, sun_label, government_label, timeout_label;
extern Widget unit_info_label;
extern Widget unit_pix_canvas, unit_below_canvas[MAX_NUM_UNITS_BELOW];
extern Widget more_arrow_label;
extern int display_depth;
extern Pixmap single_tile_pixmap;
extern Pixmap gray50,gray25;
extern int city_workers_color;

extern int seconds_to_turndone;

extern struct Sprite *intro_gfx_sprite;
extern struct Sprite *radar_gfx_sprite;
extern Cursor goto_cursor;
extern Cursor drop_cursor;
extern Cursor nuke_cursor;

extern Pixmap map_canvas_store;
extern int map_canvas_store_twidth, map_canvas_store_theight;
extern Pixmap overview_canvas_store;
extern int overview_canvas_store_width, overview_canvas_store_height;

/* contains the x0, y0 coordinates of the upper left corner block */
int map_view_x0, map_view_y0;

/* used by map_canvas expose func */ 
int force_full_repaint;

/* adjusted depending on tile size: */
int num_units_below = MAX_NUM_UNITS_BELOW;

int unit_ids[MAX_NUM_UNITS_BELOW];

extern int goto_state;
extern int paradrop_state;
extern int nuke_state;

static void pixmap_put_overlay_tile(Pixmap pixmap, int x, int y,
 				    struct Sprite *ssprite);
static void show_city_descriptions(void);

/* the intro picture is held in this pixmap, which is scaled to
   the screen size */
Pixmap scaled_intro_pixmap;
int scaled_intro_pixmap_width, scaled_intro_pixmap_height;


/**************************************************************************
...
**************************************************************************/
void decrease_unit_hp_smooth(struct unit *punit0, int hp0, 
			     struct unit *punit1, int hp1)
{
  static struct timer *anim_timer = NULL; 
  struct unit *losing_unit = (hp0 == 0 ? punit0 : punit1);
  int i;

  if (!do_combat_animation) {
    punit0->hp = hp0;
    punit1->hp = hp1;

    set_units_in_combat(NULL, NULL);
    refresh_tile_mapcanvas(punit0->x, punit0->y, 1);
    refresh_tile_mapcanvas(punit1->x, punit1->y, 1);

    return;
  }

  set_units_in_combat(punit0, punit1);

  do {
    anim_timer = renew_timer_start(anim_timer, TIMER_USER, TIMER_ACTIVE);

    if (punit0->hp > hp0
	&& myrand((punit0->hp - hp0) + (punit1->hp - hp1)) < punit0->hp - hp0)
      punit0->hp--;
    else if (punit1->hp > hp1)
      punit1->hp--;
    else
      punit0->hp--;

    refresh_tile_mapcanvas(punit0->x, punit0->y, 1);
    refresh_tile_mapcanvas(punit1->x, punit1->y, 1);

    XSync(display, 0);
    usleep_since_timer_start(anim_timer, 10000);

  } while (punit0->hp > hp0 || punit1->hp > hp1);

  for (i = 0; i < num_tiles_explode_unit; i++) {
    anim_timer = renew_timer_start(anim_timer, TIMER_USER, TIMER_ACTIVE);

    pixmap_put_tile(single_tile_pixmap, 0, 0,
		    losing_unit->x, losing_unit->y, 0);
    put_unit_pixmap(losing_unit, single_tile_pixmap, 0, 0);
    pixmap_put_overlay_tile(single_tile_pixmap, 0, 0, sprites.explode.unit[i]);

    XCopyArea(display, single_tile_pixmap, XtWindow(map_canvas), civ_gc,
	      0, 0,
	      NORMAL_TILE_WIDTH, NORMAL_TILE_HEIGHT,
	      map_canvas_adjust_x(losing_unit->x) * NORMAL_TILE_WIDTH,
	      map_canvas_adjust_y(losing_unit->y) * NORMAL_TILE_HEIGHT );

    XSync(display, 0);
    usleep_since_timer_start(anim_timer, 20000);
  }

  set_units_in_combat(NULL, NULL);
  refresh_tile_mapcanvas(punit0->x, punit0->y, 1);
  refresh_tile_mapcanvas(punit1->x, punit1->y, 1);
}

/**************************************************************************
...
**************************************************************************/
void set_overview_dimensions(int x, int y)
{
  Dimension h, w;

  XtVaSetValues(overview_canvas, XtNwidth, 2*x, XtNheight, 2*y, NULL);

  XtVaGetValues(left_column_form, XtNheight, &h, NULL);
  XtVaSetValues(map_form, XtNheight, h, NULL);

  XtVaGetValues(below_menu_form, XtNwidth, &w, NULL);
  XtVaSetValues(menu_form, XtNwidth, w, NULL);
  XtVaSetValues(bottom_form, XtNwidth, w, NULL);

  overview_canvas_store_width=2*x;
  overview_canvas_store_height=2*y;

  if(overview_canvas_store)
    XFreePixmap(display, overview_canvas_store);
  
  overview_canvas_store=XCreatePixmap(display, XtWindow(overview_canvas), 
				      overview_canvas_store_width,
				      overview_canvas_store_height,
				      display_depth);
}


/**************************************************************************
...
**************************************************************************/
void update_turn_done_button(int do_restore)
{
   static int flip;
   Pixel fore, back;
 
   if(game.player_ptr->ai.control && !ai_manual_turn_done)
     return;
   if((do_restore && flip) || !do_restore) { 
   
      XtVaGetValues(turn_done_button, 
		    XtNforeground, &fore,
		    XtNbackground, &back, NULL);
      
      
      XtVaSetValues(turn_done_button, 
		    XtNforeground, back,
		    XtNbackground, fore, NULL);
      
      flip=!flip;
   }
}


/**************************************************************************
...
**************************************************************************/
void update_timeout_label(void)
{
  char buffer[512];

  format_duration(buffer, sizeof(buffer), seconds_to_turndone);
  xaw_set_label(timeout_label, buffer);
}


/**************************************************************************
...
**************************************************************************/
void update_info_label(void)
{
  char buffer[512]; int d;
  
  my_snprintf(buffer, sizeof(buffer),
	      _("%s People\n"
		"Year: %s\n"
		"Gold: %d\n"
		"Tax:%d Lux:%d Sci:%d"),
	  int_to_text(civ_population(game.player_ptr)),
	  textyear(game.year),
	  game.player_ptr->economic.gold,
	  game.player_ptr->economic.tax,
	  game.player_ptr->economic.luxury,
	  game.player_ptr->economic.science);
  xaw_set_label(info_command, buffer);

  set_bulb_sol_government(client_research_sprite(),
			  client_pollution_sprite(),
			  game.player_ptr->government);

  d=0;
  for(;d<(game.player_ptr->economic.luxury)/10;d++)
    xaw_set_bitmap(econ_label[d], get_citizen_pixmap(0) ); /* elvis tile */
 
  for(;d<(game.player_ptr->economic.science+game.player_ptr->economic.luxury)/10;d++)
    xaw_set_bitmap(econ_label[d], get_citizen_pixmap(1) ); /* scientist tile */
 
   for(;d<10;d++)
    xaw_set_bitmap(econ_label[d], get_citizen_pixmap(2) ); /* taxman tile */
 
  update_timeout_label();
}


/**************************************************************************
...
**************************************************************************/
void update_unit_info_label(struct unit *punit)
{
  if(punit) {
    char buffer[512];
    struct city *pcity;
    pcity=player_find_city_by_id(game.player_ptr, punit->homecity);
    my_snprintf(buffer, sizeof(buffer), "%s %s\n%s\n%s\n%s", 
	    get_unit_type(punit->type)->name,
	    (punit->veteran) ? _("(veteran)") : "",
	    (goto_state==punit->id) ? 
	    _("Select destination") : unit_activity_text(punit), 
	    map_get_tile_info_text(punit->x, punit->y),
	    pcity ? pcity->name : "");
    xaw_set_label(unit_info_label, buffer);

    if ((goto_cursor != (Cursor) None) && (drop_cursor != (Cursor) None)) {
      if (goto_state == punit->id) {
	if (paradrop_state)
	  XDefineCursor(display, XtWindow(map_canvas), drop_cursor);
	else if (nuke_state)
	  XDefineCursor(display, XtWindow(map_canvas), nuke_cursor);
	else
	  XDefineCursor(display, XtWindow(map_canvas), goto_cursor);
      } else {
	XUndefineCursor(display, XtWindow(map_canvas));
      }
    }
  }
  else
    xaw_set_label(unit_info_label, "");

  update_unit_pix_label(punit);
}

/**************************************************************************
...
**************************************************************************/
void update_unit_pix_label(struct unit *punit)
{
  int i;
  /* what initialises these statics? */
  static enum unit_activity uactivity = ACTIVITY_UNKNOWN;
  static int utemplate = U_LAST;
  static int uhp = -1;
  static int showing_arrow=0;
  struct genlist_iterator myiter;
  
  if(punit) {
    if(punit->type!=utemplate || punit->activity!=uactivity || punit->hp!=uhp) {
      if (flags_are_transparent)
        XawPixcommClear(unit_pix_canvas); /* STG */
      put_unit_pixmap(punit, XawPixcommPixmap(unit_pix_canvas), 0, 0);
      xaw_expose_now(unit_pix_canvas);
      utemplate=punit->type;
      uactivity=punit->activity;
      uhp=punit->hp;
    }
    genlist_iterator_init(&myiter, 
			  &(map_get_tile(punit->x, punit->y)->units.list), 0);
    
    for(i=0; i<num_units_below && ITERATOR_PTR(myiter); i++) {
      int id;
      id=ITERATOR_PTR(myiter) ? ((struct unit *)ITERATOR_PTR(myiter))->id : 0;
      if(id==punit->id) {
	ITERATOR_NEXT(myiter);
	i--;
	continue;
      }
      
      /* IS THIS INTENTIONAL?? - mjd */
      if(1 || unit_ids[i]!=id) {
	if(id) {
	  if (flags_are_transparent)
	    XawPixcommClear(unit_below_canvas[i]); /* STG */
	  put_unit_pixmap((struct unit *)ITERATOR_PTR(myiter),
			  XawPixcommPixmap(unit_below_canvas[i]),
			  0, 0);
	  xaw_expose_now(unit_below_canvas[i]);
	}
	else
	  XawPixcommClear(unit_below_canvas[i]);
	  
	unit_ids[i]=id;
      }
      ITERATOR_NEXT(myiter);
    }

    
    for(; i<num_units_below; i++) {
      XawPixcommClear(unit_below_canvas[i]);
      unit_ids[i]=0;
    }

    
    if(ITERATOR_PTR(myiter)) {
      if(!showing_arrow) {
	xaw_set_bitmap(more_arrow_label, sprites.right_arrow->pixmap);
	showing_arrow=1;
      }
    }
    else {
      if(showing_arrow) {
	xaw_set_bitmap(more_arrow_label, None);
	showing_arrow=0;
      }
    }
  }
  else {
    XawPixcommClear(unit_pix_canvas);
    utemplate=U_LAST;
    uactivity=ACTIVITY_UNKNOWN;
    uhp = -1;
    for(i=0; i<num_units_below; i++) {
      XawPixcommClear(unit_below_canvas[i]);
      unit_ids[i]=0;
    }
    xaw_set_bitmap(more_arrow_label, None);
    showing_arrow=0;
  }
}


/**************************************************************************
...
**************************************************************************/
Pixmap get_thumb_pixmap(int onoff)
{
  return sprites.treaty_thumb[BOOL_VAL(onoff)]->pixmap;
}

/**************************************************************************
...
**************************************************************************/
Pixmap get_citizen_pixmap(int frame)
{
  frame = CLIP(0, frame, NUM_TILES_CITIZEN-1);
  return sprites.citizen[frame]->pixmap;
}


/**************************************************************************
...
**************************************************************************/
void set_bulb_sol_government(int bulb, int sol, int government)
{
  struct Sprite *gov_sprite;
  
  bulb = CLIP(0, bulb, NUM_TILES_PROGRESS-1);
  sol  = CLIP(0, sol, NUM_TILES_PROGRESS-1);
  
  xaw_set_bitmap(bulb_label, sprites.bulb[bulb]->pixmap);
  xaw_set_bitmap(sun_label, sprites.warming[sol]->pixmap);

  if (game.government_count==0) {
    /* not sure what to do here */
    gov_sprite = sprites.citizen[7]; 
  } else {
    gov_sprite = get_government(government)->sprite;
  }

  if (gov_sprite)
    xaw_set_bitmap(government_label, gov_sprite->pixmap);
}

/**************************************************************************
...
**************************************************************************/
int map_canvas_adjust_x(int x)
{
  if(map_view_x0+map_canvas_store_twidth<=map.xsize)
     return x-map_view_x0;
  else if(x>=map_view_x0)
     return x-map_view_x0;
  else if(x<map_adjust_x(map_view_x0+map_canvas_store_twidth))
     return x+map.xsize-map_view_x0;

  return -1;
}

/**************************************************************************
...
**************************************************************************/
int map_canvas_adjust_y(int y)
{
  return y-map_view_y0;
}

/**************************************************************************
...
**************************************************************************/
void refresh_tile_mapcanvas(int x, int y, int write_to_screen)
{
  x=map_adjust_x(x);
  y=map_adjust_y(y);

  if(tile_visible_mapcanvas(x, y)) {
    update_map_canvas(map_canvas_adjust_x(x), 
		      map_canvas_adjust_y(y), 1, 1, write_to_screen);
  }
  overview_update_tile(x, y);
}

/**************************************************************************
...
**************************************************************************/
int tile_visible_mapcanvas(int x, int y)
{
  return (y>=map_view_y0 && y<map_view_y0+map_canvas_store_theight &&
	  ((x>=map_view_x0 && x<map_view_x0+map_canvas_store_twidth) ||
	   (x+map.xsize>=map_view_x0 && 
	    x+map.xsize<map_view_x0+map_canvas_store_twidth)));
}

/**************************************************************************
...
**************************************************************************/
int tile_visible_and_not_on_border_mapcanvas(int x, int y)
{
  return ((y>=map_view_y0+2 || (y >= map_view_y0 && map_view_y0 == 0))
	  && (y<map_view_y0+map_canvas_store_theight-2 ||
	      (y<map_view_y0+map_canvas_store_theight &&
	       map_view_y0 + map_canvas_store_theight-EXTRA_BOTTOM_ROW == map.ysize))
	  && ((x>=map_view_x0+2 && x<map_view_x0+map_canvas_store_twidth-2) ||
	      (x+map.xsize>=map_view_x0+2
	       && x+map.xsize<map_view_x0+map_canvas_store_twidth-2)));
}

/**************************************************************************
Animates punit's "smooth" move from (x0,y0) to (x0+dx,y0+dy).
Note: Works only for adjacent-square moves.
(Tiles need not be square.)
**************************************************************************/
void move_unit_map_canvas(struct unit *punit, int x0, int y0, int dx, int dy)
{
  static struct timer *anim_timer = NULL; 
  int dest_x, dest_y;

  /* only works for adjacent-square moves */
  if ((dx < -1) || (dx > 1) || (dy < -1) || (dy > 1) ||
      ((dx == 0) && (dy == 0))) {
    return;
  }

  dest_x = map_adjust_x(x0+dx);
  dest_y = map_adjust_y(y0+dy);

  if (player_can_see_unit(game.player_ptr, punit) &&
      (tile_visible_mapcanvas(x0, y0) ||
       tile_visible_mapcanvas(dest_x, dest_y))) {
    int i, steps;
    int start_x, start_y;
    int this_x, this_y;

    if (smooth_move_unit_steps < 2) {
      steps = 2;
    } else if (smooth_move_unit_steps >
	       MIN(NORMAL_TILE_WIDTH, NORMAL_TILE_HEIGHT)) {
      steps = MIN(NORMAL_TILE_WIDTH, NORMAL_TILE_HEIGHT);
    } else {
      steps = smooth_move_unit_steps;
    }

    if(x0 >= map_view_x0) {
      start_x = (x0 - map_view_x0) * NORMAL_TILE_WIDTH;
    } else {
      start_x = (map.xsize - map_view_x0 + x0) * NORMAL_TILE_WIDTH;
    }
    start_y = (y0 - map_view_y0) * NORMAL_TILE_HEIGHT;

    this_x = start_x;
    this_y = start_y;

    for (i = 1; i <= steps; i++) {
      anim_timer = renew_timer_start(anim_timer, TIMER_USER, TIMER_ACTIVE);

      XCopyArea(display, map_canvas_store, XtWindow(map_canvas), civ_gc,
		this_x, this_y, NORMAL_TILE_WIDTH, NORMAL_TILE_HEIGHT,
		this_x, this_y);

      this_x = start_x + (dx * ((i * NORMAL_TILE_WIDTH) / steps));
      this_y = start_y + (dy * ((i * NORMAL_TILE_HEIGHT) / steps));

      XCopyArea(display, map_canvas_store, single_tile_pixmap, civ_gc,
		this_x, this_y, NORMAL_TILE_WIDTH, NORMAL_TILE_HEIGHT,
		0, 0);
      put_unit_pixmap(punit, single_tile_pixmap, 0, 0);

      XCopyArea(display, single_tile_pixmap, XtWindow(map_canvas), civ_gc,
		0, 0, NORMAL_TILE_WIDTH, NORMAL_TILE_HEIGHT,
		this_x, this_y);

      XSync(display, 0);
      if (i < steps) {
	usleep_since_timer_start(anim_timer, 10000);
      }
    }
  }
}

/**************************************************************************
...
**************************************************************************/
void get_center_tile_mapcanvas(int *x, int *y)
{
  *x=map_adjust_x(map_view_x0+map_canvas_store_twidth/2);
  *y=map_adjust_y(map_view_y0+map_canvas_store_theight/2);
}

/**************************************************************************
...
**************************************************************************/
void center_tile_mapcanvas(int x, int y)
{
  int new_map_view_x0, new_map_view_y0;

  new_map_view_x0=map_adjust_x(x-map_canvas_store_twidth/2);
  new_map_view_y0=map_adjust_y(y-map_canvas_store_theight/2);
  if(new_map_view_y0>map.ysize+EXTRA_BOTTOM_ROW-map_canvas_store_theight)
     new_map_view_y0=
       map_adjust_y(map.ysize+EXTRA_BOTTOM_ROW-map_canvas_store_theight);

  map_view_x0=new_map_view_x0;
  map_view_y0=new_map_view_y0;

  update_map_canvas(0, 0, map_canvas_store_twidth,map_canvas_store_theight, 1);
  update_map_canvas_scrollbars();
  
  refresh_overview_viewrect();
}

/**************************************************************************
...
**************************************************************************/
void overview_canvas_expose(Widget w, XEvent *event, Region exposed, 
			    void *client_data)
{
  Dimension height, width;
  
  if(get_client_state()!=CLIENT_GAME_RUNNING_STATE) {
    if(radar_gfx_sprite)
      XCopyArea(display, radar_gfx_sprite->pixmap, XtWindow(overview_canvas),
                 civ_gc,
                 event->xexpose.x, event->xexpose.y,
                 event->xexpose.width, event->xexpose.height,
                 event->xexpose.x, event->xexpose.y);
    return;
  }

  XtVaGetValues(w, XtNheight, &height, XtNwidth, &width, NULL);
  
  refresh_overview_viewrect();
}


/**************************************************************************
...
**************************************************************************/
static void set_overview_tile_foreground_color(int x, int y)
{
  XSetForeground(display, fill_bg_gc,
		 colors_standard[overview_tile_color(x, y)]);
}


/**************************************************************************
...
**************************************************************************/
void refresh_overview_canvas(void)
{
  int x, y;
  
  for(y=0; y<map.ysize; y++)
    for(x=0; x<map.xsize; x++) {
      set_overview_tile_foreground_color(x, y);
      XFillRectangle(display, overview_canvas_store, fill_bg_gc, x*2, y*2, 
		     2, 2);
    }

  XSetForeground(display, fill_bg_gc, 0);
}


/**************************************************************************
...
**************************************************************************/
void overview_update_tile(int x, int y)
{
  int pos = x + map.xsize/2 - (map_view_x0 + map_canvas_store_twidth/2);
  
  pos %= map.xsize;
  if (pos < 0)
    pos += map.xsize;
  
  set_overview_tile_foreground_color(x, y);
  XFillRectangle(display, overview_canvas_store, fill_bg_gc, x*2, y*2, 
                 2, 2);
  
  XFillRectangle(display, XtWindow(overview_canvas), fill_bg_gc, 
                 pos*2, y*2, 2, 2);
}

/**************************************************************************
...
**************************************************************************/
void refresh_overview_viewrect(void)
{
  int delta=map.xsize/2-(map_view_x0+map_canvas_store_twidth/2);

  if(delta>=0) {
    XCopyArea(display, overview_canvas_store, XtWindow(overview_canvas), 
	      civ_gc, 0, 0, 
	      overview_canvas_store_width-2*delta,
	      overview_canvas_store_height, 
	      2*delta, 0);
    XCopyArea(display, overview_canvas_store, XtWindow(overview_canvas), 
	      civ_gc, 
	      overview_canvas_store_width-2*delta, 0,
	      2*delta, overview_canvas_store_height, 
	      0, 0);
  }
  else {
    XCopyArea(display, overview_canvas_store, XtWindow(overview_canvas), 
	      civ_gc, 
	      -2*delta, 0, 
	      overview_canvas_store_width+2*delta,
	      overview_canvas_store_height, 
	      0, 0);

    XCopyArea(display, overview_canvas_store, XtWindow(overview_canvas), 
	      civ_gc, 
	      0, 0,
	      -2*delta, overview_canvas_store_height, 
	      overview_canvas_store_width+2*delta, 0);
  }

  XSetForeground(display, civ_gc, colors_standard[COLOR_STD_WHITE]);
  
  XDrawRectangle(display, XtWindow(overview_canvas), civ_gc, 
		 (overview_canvas_store_width-2*map_canvas_store_twidth)/2,
		 2*map_view_y0,
		 2*map_canvas_store_twidth, 2*map_canvas_store_theight-1);
}


/**************************************************************************
...
**************************************************************************/
void map_canvas_expose(Widget w, XEvent *event, Region exposed, 
		       void *client_data)
{
  Dimension width, height;
  int tile_width, tile_height;

  XtVaGetValues(w, XtNwidth, &width, XtNheight, &height, NULL);
  tile_width=(width+NORMAL_TILE_WIDTH-1)/NORMAL_TILE_WIDTH;
  tile_height=(height+NORMAL_TILE_HEIGHT-1)/NORMAL_TILE_HEIGHT;

  if(get_client_state()!=CLIENT_GAME_RUNNING_STATE) {
    if(!intro_gfx_sprite)  load_intro_gfx();
    if(height!=scaled_intro_pixmap_height || width!=scaled_intro_pixmap_width) {
      if(scaled_intro_pixmap)
	XFreePixmap(display, scaled_intro_pixmap);

      scaled_intro_pixmap=x_scale_pixmap(intro_gfx_sprite->pixmap,
					 intro_gfx_sprite->width,
					 intro_gfx_sprite->height, 
					 width, height, root_window);
      scaled_intro_pixmap_width=width;
      scaled_intro_pixmap_height=height;
    }

    if(scaled_intro_pixmap)
       XCopyArea(display, scaled_intro_pixmap, XtWindow(map_canvas),
		 civ_gc,
		 event->xexpose.x, event->xexpose.y,
		 event->xexpose.width, event->xexpose.height,
		 event->xexpose.x, event->xexpose.y);

    if(map_canvas_store_twidth !=tile_width ||
       map_canvas_store_theight!=tile_height) { /* resized? */
      map_canvas_resize();
    }
    return;
  }
  if(scaled_intro_pixmap) {
    XFreePixmap(display, scaled_intro_pixmap);
    scaled_intro_pixmap=0; scaled_intro_pixmap_height=0;
  }

  if(map.xsize) { /* do we have a map at all */
    if(map_canvas_store_twidth !=tile_width ||
       map_canvas_store_theight!=tile_height) { /* resized? */
      map_canvas_resize();

      XFillRectangle(display, map_canvas_store, fill_bg_gc, 0, 0, 
		     NORMAL_TILE_WIDTH*map_canvas_store_twidth,
		     NORMAL_TILE_HEIGHT*map_canvas_store_theight);

      update_map_canvas(0, 0, map_canvas_store_twidth,
			map_canvas_store_theight, 1);

      update_map_canvas_scrollbars();
      refresh_overview_viewrect();
    } else {
      XCopyArea(display, map_canvas_store, XtWindow(map_canvas),
		civ_gc,
		event->xexpose.x, event->xexpose.y,
		event->xexpose.width, event->xexpose.height,
		event->xexpose.x, event->xexpose.y);
    }
  }
  refresh_overview_canvas();
}

/**************************************************************************
...
**************************************************************************/
void map_canvas_resize(void)
{
  Dimension width, height;

  if (map_canvas_store)
    XFreePixmap(display, map_canvas_store);

  XtVaGetValues(map_canvas, XtNwidth, &width, XtNheight, &height, NULL);
  map_canvas_store_twidth=((width-1)/NORMAL_TILE_WIDTH)+1;
  map_canvas_store_theight=((height-1)/NORMAL_TILE_HEIGHT)+1;

  map_canvas_store=XCreatePixmap(display, XtWindow(map_canvas),
				 map_canvas_store_twidth*NORMAL_TILE_WIDTH,
				 map_canvas_store_theight*NORMAL_TILE_HEIGHT,
				 display_depth);
}

/**************************************************************************
...
**************************************************************************/
void update_map_canvas(int tile_x, int tile_y, int width, int height, 
		       int write_to_screen)
{
  int x, y;

  for(y=tile_y; y<tile_y+height; y++)
    for(x=tile_x; x<tile_x+width; x++)
      pixmap_put_tile(map_canvas_store, x, y, 
		      (map_view_x0+x)%map.xsize, map_view_y0+y, 0);

  if(write_to_screen) {
    XCopyArea(display, map_canvas_store, XtWindow(map_canvas), 
	      civ_gc, 
	      tile_x*NORMAL_TILE_WIDTH, 
	      tile_y*NORMAL_TILE_HEIGHT, 
	      width*NORMAL_TILE_WIDTH,
	      height*NORMAL_TILE_HEIGHT,
	      tile_x*NORMAL_TILE_WIDTH, 
	      tile_y*NORMAL_TILE_HEIGHT);

    if(width==map_canvas_store_twidth && height==map_canvas_store_theight) {
      show_city_descriptions();
    }
    
  }
}

/**************************************************************************
 Update (only) the visible part of the map
**************************************************************************/
void update_map_canvas_visible(void)
{
  update_map_canvas(0,0, map_canvas_store_twidth,map_canvas_store_theight, 1);
}

/**************************************************************************
...
**************************************************************************/
void update_map_canvas_scrollbars(void)
{
  float shown_h=(float)map_canvas_store_twidth/(float)map.xsize;
  float top_h=(float)map_view_x0/(float)map.xsize;
  float shown_v=(float)map_canvas_store_theight/((float)map.ysize+EXTRA_BOTTOM_ROW);
  float top_v=(float)map_view_y0/((float)map.ysize+EXTRA_BOTTOM_ROW);

  XawScrollbarSetThumb(map_horizontal_scrollbar, top_h, shown_h);
  XawScrollbarSetThumb(map_vertical_scrollbar, top_v, shown_v);
}

/**************************************************************************
Update display of descriptions associated with cities on the main map.
**************************************************************************/
void update_city_descriptions(void)
{
  update_map_canvas(0, 0, map_canvas_store_twidth,
		    map_canvas_store_theight, 1);
}

/**************************************************************************
Draw at x = center of string, y = top of string.
**************************************************************************/
static void draw_shadowed_string(XFontStruct * font, GC font_gc,
					  int x, int y, const char *string)
{
  Window wndw=XtWindow(map_canvas);
  int len=strlen(string);
  int wth=XTextWidth(font, string, len);
  int xs=x-wth/2;
  int ys=y+font->ascent;

  XSetForeground(display, font_gc, colors_standard[COLOR_STD_BLACK]);
  XDrawString(display, wndw, font_gc, xs+1, ys+1, string, len);

  XSetForeground(display, font_gc, colors_standard[COLOR_STD_WHITE]);
  XDrawString(display, wndw, font_gc, xs, ys, string, len);
}

/**************************************************************************
...
**************************************************************************/
static void show_city_descriptions(void)
{
  int x, y;

  if (!draw_city_names && !draw_city_productions)
    return;

  for(y=0; y<map_canvas_store_theight; ++y) { 
    int ry=map_view_y0+y;
    for(x=0; x<map_canvas_store_twidth; ++x) { 
      int rx=(map_view_x0+x)%map.xsize;
      struct city *pcity;
      if((pcity=map_get_city(rx, ry))) {

	if (draw_city_names) {
	  draw_shadowed_string(main_font_struct, font_gc,
			       x*NORMAL_TILE_WIDTH+NORMAL_TILE_WIDTH/2,
			       (y+1)*NORMAL_TILE_HEIGHT,
			       pcity->name);
	}

	if (draw_city_productions && (pcity->owner==game.player_idx)) {
	  int turns;
	  struct unit_type *punit_type;
	  struct impr_type *pimpr_type;
	  char *name;
	  char buffer[512];

	  turns = city_turns_to_build(pcity, pcity->currently_building,
				      pcity->is_building_unit);

	  if (pcity->is_building_unit) {
	    punit_type = get_unit_type(pcity->currently_building);
	    name = punit_type->name;
	  } else {
	    pimpr_type = get_improvement_type(pcity->currently_building);
	    name = pimpr_type->name;
	  }
	  my_snprintf(buffer, sizeof(buffer), "%s %d", name, turns);

	  draw_shadowed_string(prod_font_struct, prod_font_gc,
			       x*NORMAL_TILE_WIDTH+NORMAL_TILE_WIDTH/2,
			       (y+1)*NORMAL_TILE_HEIGHT +
			         (draw_city_names ?
				   main_font_struct->ascent +
				     main_font_struct->descent :
				   0),
			       buffer);
	}

      }
    }
  }
}

#ifdef UNUSED
/**************************************************************************
...
**************************************************************************/
void put_city_pixmap(struct city *pcity, Pixmap pm, int xtile, int ytile)
{
  struct Sprite *mysprite;

  if(solid_color_behind_units) {
    XSetForeground(display, fill_bg_gc,
		   colors_standard[player_color(city_owner(pcity))]);
    XFillRectangle(display, pm, fill_bg_gc, 
		   xtile*NORMAL_TILE_WIDTH, ytile*NORMAL_TILE_HEIGHT, 
		   NORMAL_TILE_WIDTH, NORMAL_TILE_HEIGHT);
  }
  else if(!flags_are_transparent) {	/* observe transparency here, too! */
    mysprite=get_city_nation_flag_sprite(pcity);
    XCopyArea(display, mysprite->pixmap, pm, civ_gc, 0, 0,
	      mysprite->width, mysprite->height, 
	      xtile*NORMAL_TILE_WIDTH, ytile*NORMAL_TILE_HEIGHT);
  }
  else {
    pixmap_put_overlay_tile(pm,xtile,ytile, get_city_nation_flag_sprite(pcity));
  }

  pixmap_put_overlay_tile(pm, xtile, ytile,
			  (city_got_citywalls(pcity)
			   ? sprites.tx.city_walls
			   : sprites.tx.city));

  if(genlist_size(&((map_get_tile(pcity->x, pcity->y))->units.list)) > 0)
    pixmap_put_overlay_tile(pm, xtile, ytile, sprites.city.occupied);

  if(pcity->size>=10) {
    pixmap_put_overlay_tile(pm, xtile, ytile,
			    sprites.city.size_tens[pcity->size/10]);
  }
  pixmap_put_overlay_tile(pm, xtile, ytile, sprites.city.size[pcity->size%10]);

  if(map_get_special(pcity->x, pcity->y) & S_POLLUTION)
    pixmap_put_overlay_tile(pm, xtile, ytile, sprites.tx.pollution);

  if(city_unhappy(pcity))
    pixmap_put_overlay_tile(pm, xtile, ytile, sprites.city.disorder);
  
}
#endif

/**************************************************************************
...
**************************************************************************/
void put_city_tile_output(Pixmap pm, int x, int y, 
			  int food, int shield, int trade)
{
  food = CLIP(0, food, NUM_TILES_DIGITS-1);
  trade = CLIP(0, trade, NUM_TILES_DIGITS-1);
  shield = CLIP(0, shield, NUM_TILES_DIGITS-1);
  
  pixmap_put_overlay_tile(pm, x, y, sprites.city.tile_foodnum[food]);
  pixmap_put_overlay_tile(pm, x, y, sprites.city.tile_shieldnum[shield]);
  pixmap_put_overlay_tile(pm, x, y, sprites.city.tile_tradenum[trade]);
}


/**************************************************************************
...
**************************************************************************/
void put_unit_pixmap(struct unit *punit, Pixmap pm, int xtile, int ytile)
{
  struct Sprite *sprites[40];
  int count = fill_unit_sprite_array(sprites, punit);

  if(count)
  {
    int i;

    if(sprites[0])
    {
      if(flags_are_transparent)
      {
        pixmap_put_overlay_tile(pm, xtile, ytile, sprites[0]);
      } else
      {
        XCopyArea(display, sprites[0]->pixmap, pm, civ_gc, 0, 0,
                  sprites[0]->width, sprites[0]->height, 
                  xtile*NORMAL_TILE_WIDTH, ytile*NORMAL_TILE_HEIGHT);
      }
    } else
    {
      XSetForeground(display, fill_bg_gc,
		     colors_standard[player_color(get_player(punit->owner))]);
      XFillRectangle(display, pm, fill_bg_gc, 
                     xtile*NORMAL_TILE_WIDTH, ytile*NORMAL_TILE_HEIGHT, 
                     NORMAL_TILE_WIDTH, NORMAL_TILE_HEIGHT);
    }

    for(i=1;i<count;i++)
    {
      if(sprites[i])
        pixmap_put_overlay_tile(pm, xtile, ytile, sprites[i]);
    }
  }
}


/**************************************************************************
  ...
  FIXME: 
  For now only two food, one shield and two masks can be drawn per unit,
  the proper way to do this is probably something like what Civ II does.
  (One food/shield/mask drawn N times, possibly one top of itself. -- SKi 
**************************************************************************/
void put_unit_pixmap_city_overlays(struct unit *punit, Pixmap pm)
{
  int upkeep_food = CLIP(0, punit->upkeep_food, 2);
  int unhappy = CLIP(0, punit->unhappiness, 2);
 
  /* wipe the slate clean */
  XSetForeground(display, fill_bg_gc, colors_standard[COLOR_STD_WHITE]);
  XFillRectangle(display, pm, fill_bg_gc, 0, NORMAL_TILE_WIDTH, 
		 NORMAL_TILE_HEIGHT, NORMAL_TILE_HEIGHT+SMALL_TILE_HEIGHT);

  /* draw overlay pixmaps */
  if (punit->upkeep > 0)
    pixmap_put_overlay_tile(pm, 0, 1, sprites.upkeep.shield);
  if (upkeep_food > 0)
    pixmap_put_overlay_tile(pm, 0, 1, sprites.upkeep.food[upkeep_food-1]);
  if (unhappy > 0)
    pixmap_put_overlay_tile(pm, 0, 1, sprites.upkeep.unhappy[unhappy-1]);
}


/**************************************************************************
...
**************************************************************************/
void put_nuke_mushroom_pixmaps(int abs_x0, int abs_y0)
{
  int x, y;
  
  for(y=0; y<3; y++)
    for(x=0; x<3; x++) {
      struct Sprite *mysprite = sprites.explode.nuke[y][x];
      XCopyArea(display, mysprite->pixmap, XtWindow(map_canvas), 
		civ_gc, 
		0, 0, 
		NORMAL_TILE_WIDTH,
		NORMAL_TILE_HEIGHT,
		map_canvas_adjust_x(x-1+abs_x0)*NORMAL_TILE_WIDTH, 
		map_canvas_adjust_y(y-1+abs_y0)*NORMAL_TILE_HEIGHT);
    }

  XSync(display, 0);
  sleep(1);

  update_map_canvas(map_canvas_adjust_x(abs_x0-1), 
                    map_canvas_adjust_y(abs_y0-1),
		    3, 3, 1);
}

/**************************************************************************
...
**************************************************************************/
void pixmap_put_black_tile(Pixmap pm, int x, int y)
{
  XSetForeground(display, fill_bg_gc, colors_standard[COLOR_STD_BLACK]);

  XFillRectangle(display, pm, fill_bg_gc,  
		 x*NORMAL_TILE_WIDTH, y*NORMAL_TILE_HEIGHT,
		 NORMAL_TILE_WIDTH, NORMAL_TILE_HEIGHT);
}
		     

/**************************************************************************
...
**************************************************************************/
void pixmap_frame_tile_red(Pixmap pm, int x, int y)
{
  XSetForeground(display, fill_bg_gc, colors_standard[COLOR_STD_RED]);

  XDrawRectangle(display, pm, fill_bg_gc,  
		 x*NORMAL_TILE_WIDTH, y*NORMAL_TILE_HEIGHT,
		 NORMAL_TILE_WIDTH-1, NORMAL_TILE_HEIGHT-1);
  XDrawRectangle(display, pm, fill_bg_gc,  
		 x*NORMAL_TILE_WIDTH+1, y*NORMAL_TILE_HEIGHT+1,
		 NORMAL_TILE_WIDTH-3, NORMAL_TILE_HEIGHT-3);
  XDrawRectangle(display, pm, fill_bg_gc,  
		 x*NORMAL_TILE_WIDTH+2, y*NORMAL_TILE_HEIGHT+2,
		 NORMAL_TILE_WIDTH-5, NORMAL_TILE_HEIGHT-5);
}



/**************************************************************************
...
**************************************************************************/
void pixmap_put_tile(Pixmap pm, int x, int y, int abs_x0, int abs_y0, 
		     int citymode)
{
  struct Sprite *sprites[80];
  int count = fill_tile_sprite_array(sprites, abs_x0, abs_y0, citymode);

  int x1 = x * NORMAL_TILE_WIDTH;
  int y1 = y * NORMAL_TILE_HEIGHT;

  if(count)
  {
    int i;

    if(sprites[0])
    {
      /* first tile without mask */
      XCopyArea(display, sprites[0]->pixmap, pm, 
              civ_gc, 0, 0,
              sprites[0]->width, sprites[0]->height, x1,y1);
    } else
    {
      /* normally when solid_color_behind_units */
      struct city *pcity;
      struct player *pplayer=NULL;

      if(count>1 && !sprites[1])
      {
        /* it's the unit */
        struct tile *ptile;
        struct unit *punit;
        ptile=map_get_tile(abs_x0, abs_y0);
        if ((punit=find_visible_unit(ptile)))
          pplayer = &game.players[punit->owner];

      } else
      {
        /* it's the city */
        if((pcity=map_get_city(abs_x0, abs_y0)))
          pplayer = &game.players[pcity->owner];
      }

      if(pplayer)
      {
        XSetForeground(display, fill_bg_gc,
		       colors_standard[player_color(pplayer)]);
        XFillRectangle(display, pm, fill_bg_gc, 
                       x1,y1,NORMAL_TILE_WIDTH, NORMAL_TILE_HEIGHT);
      }
    }

    for(i=1;i<count;i++)
    {
      if(sprites[i])
        pixmap_put_overlay_tile(pm, x, y, sprites[i]);
    }

    if(draw_map_grid && !citymode) {
      int here_in_radius =
	player_in_city_radius(game.player_ptr, abs_x0, abs_y0);
      /* left side... */
      if((map_get_tile(abs_x0-1, abs_y0))->known &&
	 (here_in_radius ||
	  player_in_city_radius(game.player_ptr, abs_x0-1, abs_y0))) {
	XSetForeground(display, civ_gc, colors_standard[COLOR_STD_WHITE]);
      } else {
	XSetForeground(display, civ_gc, colors_standard[COLOR_STD_BLACK]);
      }
      XDrawLine(display, pm, civ_gc,
		x*NORMAL_TILE_WIDTH, y*NORMAL_TILE_HEIGHT,
		x*NORMAL_TILE_WIDTH, (y+1)*NORMAL_TILE_HEIGHT);
      /* top side... */
      if((map_get_tile(abs_x0, abs_y0-1))->known &&
	 (here_in_radius ||
	  player_in_city_radius(game.player_ptr, abs_x0, abs_y0-1))) {
	XSetForeground(display, civ_gc, colors_standard[COLOR_STD_WHITE]);
      } else {
	XSetForeground(display, civ_gc, colors_standard[COLOR_STD_BLACK]);
      }
      XDrawLine(display, pm, civ_gc,
		x*NORMAL_TILE_WIDTH, y*NORMAL_TILE_HEIGHT,
		(x+1)*NORMAL_TILE_WIDTH, y*NORMAL_TILE_HEIGHT);
    }

  } else
  {
    /* tile is unknow */
    pixmap_put_black_tile(pm, x,y);
  }
}

/**************************************************************************
...
**************************************************************************/
static void pixmap_put_overlay_tile(Pixmap pixmap, int x, int y,
 				    struct Sprite *ssprite)
{
  if (!ssprite) return;
      
  XSetClipOrigin(display, civ_gc, x*NORMAL_TILE_WIDTH, y*NORMAL_TILE_HEIGHT);
  XSetClipMask(display, civ_gc, ssprite->mask);
      
  XCopyArea(display, ssprite->pixmap, pixmap, 
	    civ_gc, 0, 0,
	    ssprite->width, ssprite->height, 
	    x*NORMAL_TILE_WIDTH, y*NORMAL_TILE_HEIGHT);
  XSetClipMask(display, civ_gc, None); 
}

/**************************************************************************
 Draws a cross-hair overlay on a tile
**************************************************************************/
void put_cross_overlay_tile(int x,int y)
{
  x=map_adjust_x(x);
  y=map_adjust_y(y);

  if(tile_visible_mapcanvas(x, y)) {
    pixmap_put_overlay_tile(XtWindow(map_canvas),map_canvas_adjust_x(x),
			    map_canvas_adjust_y(y), sprites.user.attention);
  }
}


/**************************************************************************
 Shade the tiles around a city to indicate the location of workers
**************************************************************************/
void put_city_workers(struct city *pcity, int color)
{
  int x,y;
  int i,j;
  static struct city *last_pcity=NULL;

  if(color==-1) {
    if(pcity!=last_pcity)  city_workers_color = (city_workers_color%3)+1;
    color=city_workers_color;
  }

  XSetForeground(display, fill_tile_gc, colors_standard[color]);
  x=map_canvas_adjust_x(pcity->x); y=map_canvas_adjust_y(pcity->y);
  city_map_iterate(i, j)  {
    enum city_tile_type t=get_worker_city(pcity, i, j);
    enum city_tile_type last_t=-1;
    if(!(i==2 && j==2)) {
      if(t==C_TILE_EMPTY) {
	if(last_t!=t) XSetStipple(display,fill_tile_gc,gray25);
      } else if(t==C_TILE_WORKER) {
	if(last_t!=t) XSetStipple(display,fill_tile_gc,gray50);
      } else continue;
      last_t=t;
      XCopyArea(display, map_canvas_store, XtWindow(map_canvas), civ_gc, 
		(x+i-2)*NORMAL_TILE_WIDTH, (y+j-2)*NORMAL_TILE_HEIGHT, 
		NORMAL_TILE_WIDTH, NORMAL_TILE_HEIGHT,
		(x+i-2)*NORMAL_TILE_WIDTH, (y+j-2)*NORMAL_TILE_HEIGHT);
      XFillRectangle(display, XtWindow(map_canvas), fill_tile_gc,
		     (x+i-2)*NORMAL_TILE_WIDTH, (y+j-2)*NORMAL_TILE_HEIGHT,
		     NORMAL_TILE_WIDTH, NORMAL_TILE_HEIGHT);
    }
    if(t==C_TILE_WORKER) {
      put_city_tile_output(XtWindow(map_canvas), x+i-2, y+j-2, 
			   get_food_tile(i, j, pcity),
			   get_shields_tile(i, j, pcity), 
			   get_trade_tile(i, j, pcity) );
    }
  }

  last_pcity=pcity;
}


/**************************************************************************
...
**************************************************************************/
void scrollbar_jump_callback(Widget w, XtPointer client_data,
			     XtPointer percent_ptr)
{
  float percent=*(float*)percent_ptr;

  if(get_client_state()!=CLIENT_GAME_RUNNING_STATE)
     return;

  if(w==map_horizontal_scrollbar)
    map_view_x0=percent*map.xsize;
  else {
    map_view_y0=percent*(map.ysize+EXTRA_BOTTOM_ROW);
    map_view_y0=(map_view_y0<0) ? 0 : map_view_y0;
    map_view_y0=
      (map_view_y0>map.ysize+EXTRA_BOTTOM_ROW-map_canvas_store_theight) ? 
	map.ysize+EXTRA_BOTTOM_ROW-map_canvas_store_theight :
	map_view_y0;
  }

  update_map_canvas(0, 0, map_canvas_store_twidth, map_canvas_store_theight, 1);
  /* The scrollbar tracks by itself, while calling the jumpProc,
     so there's no need to call update_map_canvas_scrollbars() here. */
  refresh_overview_viewrect();
}


/**************************************************************************
...
**************************************************************************/
void scrollbar_scroll_callback(Widget w, XtPointer client_data,
			     XtPointer position_val)
{
  int position=(int)position_val;


  if(get_client_state()!=CLIENT_GAME_RUNNING_STATE)
     return;

  if(w==map_horizontal_scrollbar) {
    if(position>0) 
      map_view_x0++;
    else
      map_view_x0--;
  }
  else {
    if(position>0 && map_view_y0<map.ysize+EXTRA_BOTTOM_ROW-map_canvas_store_theight)
      map_view_y0++;
    else if(position<0 && map_view_y0>0)
      map_view_y0--;
  }

  map_view_x0=map_adjust_x(map_view_x0);
  map_view_y0=map_adjust_y(map_view_y0);


  update_map_canvas(0, 0, map_canvas_store_twidth, map_canvas_store_theight, 1);
  update_map_canvas_scrollbars();
  refresh_overview_viewrect();
}
