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
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdarg.h>

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/Form.h>
#include <X11/Xaw/Label.h>
#include <X11/Xaw/SimpleMenu.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/AsciiText.h>  
#include <X11/Xaw/List.h>
#include <X11/Xaw/MenuButton.h>
#include <X11/Xaw/SimpleMenu.h>
#include <X11/Xaw/SmeBSB.h>
#include <X11/Xaw/SmeLine.h>
#include <X11/Xaw/Toggle.h>     

#include <player.h>
#include <packets.h>
#include <game.h>
#include <mapctrl.h>
#include <tech.h>
#include <repodlgs.h>
#include <shared.h>
#include <xstuff.h>
#include <mapview.h>
#include <citydlg.h>
#include <city.h>
#include <civclient.h>
#include <helpdlg.h>
#include <chatline.h>
#include <dialogs.h>

#define MAX_CITIES_SHOWN 256

extern Widget toplevel, main_form, map_canvas;

extern struct connection aconnection;
extern Display	*display;
extern int display_depth;
extern struct advance advances[];

extern int did_advance_tech_this_year;

void create_science_dialog(int make_modal);
void science_close_callback(Widget w, XtPointer client_data, 
			    XtPointer call_data);
void science_help_callback(Widget w, XtPointer client_data, 
			   XtPointer call_data);
void science_change_callback(Widget w, XtPointer client_data, 
			     XtPointer call_data);


/******************************************************************/
Widget science_dialog_shell;
Widget science_label;
Widget science_current_label;
Widget science_change_menu_button, science_list, science_help_toggle;
int science_dialog_shell_is_modal;
Widget popupmenu;


/******************************************************************/
void create_city_report_dialog(int make_modal);
void city_close_callback(Widget w, XtPointer client_data, 
			 XtPointer call_data);
void city_center_callback(Widget w, XtPointer client_data, 
			  XtPointer call_data);
void city_popup_callback(Widget w, XtPointer client_data, 
			 XtPointer call_data);
void city_buy_callback(Widget w, XtPointer client_data, 
		       XtPointer call_data);
void city_change_callback(Widget w, XtPointer client_data, 
			  XtPointer call_data);
void city_list_callback(Widget w, XtPointer client_data, 
			XtPointer call_data);

Widget city_dialog_shell;
Widget city_label;
Widget city_list, city_list_label;
Widget city_center_command, city_popup_command, city_buy_command;
Widget city_change_command, city_popupmenu;

int city_dialog_shell_is_modal;
int cities_in_list[MAX_CITIES_SHOWN];


/******************************************************************/
void create_trade_report_dialog(int make_modal);
void trade_close_callback(Widget w, XtPointer client_data, 
			 XtPointer call_data);

Widget trade_dialog_shell;
Widget trade_label, trade_label2;
Widget trade_list, trade_list_label;
int trade_dialog_shell_is_modal;

/******************************************************************/
void create_activeunits_report_dialog(int make_modal);
void activeunits_close_callback(Widget w, XtPointer client_data, 
			 XtPointer call_data);
void activeunits_upgrade_callback(Widget w, XtPointer client_data, 
			 XtPointer call_data);
void activeunits_list_callback(Widget w, XtPointer client_data, 
                           XtPointer call_data);
int activeunits_type[U_LAST];

Widget activeunits_dialog_shell;
Widget activeunits_label, activeunits_label2;
Widget activeunits_list, activeunits_list_label;
Widget upgrade_command;

int activeunits_dialog_shell_is_modal;

/******************************************************************
...
*******************************************************************/
void update_report_dialogs(void)
{
  activeunits_report_dialog_update();
  trade_report_dialog_update();
  city_report_dialog_update(); 
  science_dialog_update();
}

/****************************************************************
...
****************************************************************/
char *get_report_title(char *report_name)
{
  char buf[512];
  
  sprintf(buf, "%s\n%s of the %s\n%s %s: %s",
	  report_name,
	  get_government_name(game.player_ptr->government),
	  get_race_name_plural(game.player_ptr->race),
	  get_ruler_title(game.player_ptr->government),
	  game.player_ptr->name,
	  textyear(game.year));

  return create_centered_string(buf);
}


/****************************************************************
...
************************ ***************************************/
void popup_science_dialog(int make_modal)
{

  if(!science_dialog_shell) {
    Position x, y;
    Dimension width, height;
    
    science_dialog_shell_is_modal=make_modal;
    
    if(make_modal)
      XtSetSensitive(main_form, FALSE);
    
    create_science_dialog(make_modal);
    
    XtVaGetValues(toplevel, XtNwidth, &width, XtNheight, &height, NULL);
    
    XtTranslateCoords(toplevel, (Position) width/10, (Position) height/10,
		      &x, &y);
    XtVaSetValues(science_dialog_shell, XtNx, x, XtNy, y, NULL);
    
    XtPopup(science_dialog_shell, XtGrabNone);
  }

}


/****************************************************************
...
*****************************************************************/
void create_science_dialog(int make_modal)
{
  Widget science_form;
  Widget  close_command;
  static char *tech_list_names_ptrs[A_LAST+1];
  static char tech_list_names[A_LAST+1][200];
  int i, j, flag;
  Dimension width;
  char current_text[512];
  char *report_title;
  
  sprintf(current_text, "Researching %s: %d/%d",
	  advances[game.player_ptr->research.researching].name,
	  game.player_ptr->research.researched,
	  research_time(game.player_ptr));
  
  for(i=1, j=0; i<A_LAST; i++)
    if(get_invention(game.player_ptr, i)==TECH_KNOWN) {
      strcpy(tech_list_names[j], advances[i].name);
      tech_list_names_ptrs[j]=tech_list_names[j];
      j++;
    }
  tech_list_names_ptrs[j]=0;
  
  science_dialog_shell = XtVaCreatePopupShell("sciencepopup", 
					      make_modal ? 
					      transientShellWidgetClass :
					      topLevelShellWidgetClass,
					      toplevel, 
					      0);

  science_form = XtVaCreateManagedWidget("scienceform", 
					 formWidgetClass,
					 science_dialog_shell,
					 NULL);   

  report_title=get_report_title("Science Advisor");
  science_label = XtVaCreateManagedWidget("sciencelabel", 
					  labelWidgetClass, 
					  science_form,
					  XtNlabel, 
					  report_title,
					  NULL);
  free(report_title);

  science_current_label = XtVaCreateManagedWidget("sciencecurrentlabel", 
						  labelWidgetClass, 
						  science_form,
						  XtNlabel, 
						  current_text,
						  NULL);

  science_change_menu_button = XtVaCreateManagedWidget(
				       "sciencechangemenubutton", 
				       menuButtonWidgetClass,
				       science_form,
				       NULL);

  science_help_toggle = XtVaCreateManagedWidget("sciencehelptoggle", 
						toggleWidgetClass, 
						science_form,
						NULL);
  
  science_list = XtVaCreateManagedWidget("sciencelist", 
					 listWidgetClass,
					 science_form,
					 XtNlist, tech_list_names_ptrs,
					 NULL);

  close_command = XtVaCreateManagedWidget("scienceclosecommand", 
					  commandWidgetClass,
					  science_form,
					  NULL);
  
  
  popupmenu=XtVaCreatePopupShell("menu", 
				 simpleMenuWidgetClass, 
				 science_change_menu_button, 
				 NULL);

  
  for(i=1, flag=0; i<A_LAST; i++)
    if(get_invention(game.player_ptr, i)==TECH_REACHABLE) {
      Widget entry=
      XtVaCreateManagedWidget(advances[i].name, smeBSBObjectClass, 
			      popupmenu, NULL);
      XtAddCallback(entry, XtNcallback, science_change_callback, 
		    (XtPointer) i); 
      flag=1;
    }
  
  if(!flag)
    XtSetSensitive(science_change_menu_button, FALSE);
  
  XtAddCallback(close_command, XtNcallback, science_close_callback, NULL);
  XtAddCallback(science_list, XtNcallback, science_help_callback, NULL);

  XtRealizeWidget(science_dialog_shell);

  width=500;
  XtVaSetValues(science_label, XtNwidth, &width, NULL);
}


/****************************************************************
...
*****************************************************************/
void science_change_callback(Widget w, XtPointer client_data, 
			     XtPointer call_data)
{
  char current_text[512];
  struct packet_player_request packet;
  int to;
  Boolean b;

  to=(int)client_data;

  XtVaGetValues(science_help_toggle, XtNstate, &b, NULL);
  if (b == TRUE)
    popup_help_dialog_string(advances[to].name);
  else
    {  
      sprintf(current_text, "Researching %s: %d/%d",
	      advances[to].name, game.player_ptr->research.researched, 
	      research_time(game.player_ptr));
  
      XtVaSetValues(science_current_label, XtNlabel, current_text, NULL);
  
      packet.tech=to;
      send_packet_player_request(&aconnection, &packet, PACKET_PLAYER_RESEARCH);
    }
}


/****************************************************************
...
*****************************************************************/
void science_close_callback(Widget w, XtPointer client_data, 
			    XtPointer call_data)
{

  if(science_dialog_shell_is_modal)
    XtSetSensitive(main_form, TRUE);
  XtDestroyWidget(science_dialog_shell);
  science_dialog_shell=0;
}

/****************************************************************
...
*****************************************************************/

void science_help_callback(Widget w, XtPointer client_data, 
			    XtPointer call_data)
{
  XawListReturnStruct *ret=XawListShowCurrent(science_list);
  Boolean b;

  XtVaGetValues(science_help_toggle, XtNstate, &b, NULL);
  if (b == TRUE)
    {
      if(ret->list_index!=XAW_LIST_NONE)
	popup_help_dialog_string(ret->string);
      else
	popup_help_dialog_string("Technology research");
    }
}


/****************************************************************
...
*****************************************************************/
void science_dialog_update(void)
{
  if(science_dialog_shell) {
    char text[512];
    static char *tech_list_names_ptrs[A_LAST+1];
    static char tech_list_names[A_LAST+1][200];
    int i, j, flag;
    char *report_title;
    
    report_title=get_report_title("Science Advisor");
    xaw_set_label(science_label, report_title);
    free(report_title);

    
    sprintf(text, "Researching %s: %d/%d",
	    advances[game.player_ptr->research.researching].name,
	    game.player_ptr->research.researched,
	    research_time(game.player_ptr));
    
    xaw_set_label(science_current_label, text);

    for(i=1, j=0; i<A_LAST; i++)
      if(get_invention(game.player_ptr, i)==TECH_KNOWN) {
	strcpy(tech_list_names[j], advances[i].name);
	tech_list_names_ptrs[j]=tech_list_names[j];
	j++;
      }
    tech_list_names_ptrs[j]=0;

    XawListChange(science_list, tech_list_names_ptrs, 0/*j*/, 0, 1);

    XtDestroyWidget(popupmenu);
    
    popupmenu=XtVaCreatePopupShell("menu", 
				   simpleMenuWidgetClass, 
				   science_change_menu_button, 
				   NULL);
    
      for(i=1, flag=0; i<A_LAST; i++)
      if(get_invention(game.player_ptr, i)==TECH_REACHABLE) {
	Widget entry=
	  XtVaCreateManagedWidget(advances[i].name, smeBSBObjectClass, 
				  popupmenu, NULL);
	XtAddCallback(entry, XtNcallback, science_change_callback, 
		      (XtPointer) i); 
	flag=1;
      }
    
    if(!flag)
      XtSetSensitive(science_change_menu_button, FALSE);

  }
  
}


/****************************************************************

                      CITY REPORT DIALOG
 
****************************************************************/

/****************************************************************
...
****************************************************************/
void popup_city_report_dialog(int make_modal)
{
  if(!city_dialog_shell) {
      Position x, y;
      Dimension width, height;
      
      city_dialog_shell_is_modal=make_modal;
    
      if(make_modal)
	XtSetSensitive(main_form, FALSE);
      
      create_city_report_dialog(make_modal);
      
      XtVaGetValues(toplevel, XtNwidth, &width, XtNheight, &height, NULL);
      
      XtTranslateCoords(toplevel, (Position) width/10, (Position) height/10,
			&x, &y);
      XtVaSetValues(city_dialog_shell, XtNx, x, XtNy, y, NULL);
      
      XtPopup(city_dialog_shell, XtGrabNone);
   }
}


/****************************************************************
...
*****************************************************************/
void create_city_report_dialog(int make_modal)
{
  Widget city_form;
  Widget close_command;
  char *report_title;
  
  city_dialog_shell = XtVaCreatePopupShell("reportcitypopup", 
					      make_modal ? 
					      transientShellWidgetClass :
					      topLevelShellWidgetClass,
					      toplevel, 
					      0);

  city_form = XtVaCreateManagedWidget("reportcityform", 
					 formWidgetClass,
					 city_dialog_shell,
					 NULL);   

  report_title=get_report_title("City Advisor");
  city_label = XtVaCreateManagedWidget("reportcitylabel", 
				       labelWidgetClass, 
				       city_form,
				       XtNlabel, 
				       report_title,
				       NULL);
  free(report_title);

  city_list_label = XtVaCreateManagedWidget("reportcitylistlabel", 
				       labelWidgetClass, 
				       city_form,
				       NULL);
  
  city_list = XtVaCreateManagedWidget("reportcitylist", 
				      listWidgetClass,
				      city_form,
				      NULL);

  close_command = XtVaCreateManagedWidget("reportcityclosecommand", 
					  commandWidgetClass,
					  city_form,
					  NULL);
  
  city_center_command = XtVaCreateManagedWidget("reportcitycentercommand", 
						commandWidgetClass,
						city_form,
						NULL);

  city_popup_command = XtVaCreateManagedWidget("reportcitypopupcommand", 
					       commandWidgetClass,
					       city_form,
					       NULL);

  city_popupmenu = 0;

  city_buy_command = XtVaCreateManagedWidget("reportcitybuycommand", 
					     commandWidgetClass,
					     city_form,
					     NULL);

  city_change_command = XtVaCreateManagedWidget("reportcitychangemenubutton", 
						menuButtonWidgetClass,
						city_form,
						NULL);

  XtAddCallback(close_command, XtNcallback, city_close_callback, NULL);
  XtAddCallback(city_center_command, XtNcallback, city_center_callback, NULL);
  XtAddCallback(city_popup_command, XtNcallback, city_popup_callback, NULL);
  XtAddCallback(city_buy_command, XtNcallback, city_buy_callback, NULL);
  XtAddCallback(city_list, XtNcallback, city_list_callback, NULL);
  
  XtRealizeWidget(city_dialog_shell);
  city_report_dialog_update();
}

/****************************************************************
...
*****************************************************************/
void city_list_callback(Widget w, XtPointer client_data, 
			 XtPointer call_data)
{
  XawListReturnStruct *ret=XawListShowCurrent(city_list);
  struct city *pcity;

  if(ret->list_index!=XAW_LIST_NONE && (pcity=find_city_by_id(cities_in_list[ret->list_index])))
    {
      int flag,i;
      char buf[512];

      XtSetSensitive(city_change_command, TRUE);
      XtSetSensitive(city_center_command, TRUE);
      XtSetSensitive(city_popup_command, TRUE);
      XtSetSensitive(city_buy_command, TRUE);
      if (city_popupmenu)
	XtDestroyWidget(city_popupmenu);

      city_popupmenu=XtVaCreatePopupShell("menu", 
				     simpleMenuWidgetClass, 
				     city_change_command,
				     NULL);
      flag = 0;
      for(i=0; i<B_LAST; i++)
	if(can_build_improvement(pcity, i)) 
	  {
	    Widget entry;
	    sprintf(buf,"%s (%d)", get_imp_name_ex(pcity, i),get_improvement_type(i)->build_cost);
	    entry = XtVaCreateManagedWidget(buf, smeBSBObjectClass, city_popupmenu, NULL);
	    XtAddCallback(entry, XtNcallback, city_change_callback, (XtPointer) i);
	    flag=1;
	  }

      for(i=0; i<U_LAST; i++)
	if(can_build_unit(pcity, i)) {
	    Widget entry;
	    sprintf(buf,"%s (%d)", 
		    get_unit_name(i),get_unit_type(i)->build_cost);
	    entry = XtVaCreateManagedWidget(buf, smeBSBObjectClass, 
					    city_popupmenu, NULL);
	    XtAddCallback(entry, XtNcallback, city_change_callback, 
			  (XtPointer) (i+B_LAST));
	    flag = 1;
	}

      if(!flag)
	XtSetSensitive(city_change_command, FALSE);

    }
  else
    {
      XtSetSensitive(city_change_command, FALSE);
      XtSetSensitive(city_center_command, FALSE);
      XtSetSensitive(city_popup_command, FALSE);
      XtSetSensitive(city_buy_command, FALSE);
    }
}


/****************************************************************
...
*****************************************************************/
void city_change_callback(Widget w, XtPointer client_data, 
			 XtPointer call_data)
{
  XawListReturnStruct *ret=XawListShowCurrent(city_list);
  struct city *pcity;



  if(ret->list_index!=XAW_LIST_NONE && (pcity=find_city_by_id(cities_in_list[ret->list_index])))
    {
      struct packet_city_request packet;
      int build_nr;
      Boolean unit;
      
      build_nr = (int) client_data;

      if (build_nr >= B_LAST)
	{
	  build_nr -= B_LAST;
	  unit = TRUE;
	}
      else
	unit = FALSE;

      packet.city_id=pcity->id;
      packet.name[0]='\0';
      packet.build_id=build_nr;
      packet.is_build_id_unit_id=unit;
      send_packet_city_request(&aconnection, &packet, PACKET_CITY_CHANGE);
  }
}

/****************************************************************
...
*****************************************************************/
void city_buy_callback(Widget w, XtPointer client_data, 
			 XtPointer call_data)
{
  XawListReturnStruct *ret=XawListShowCurrent(city_list);

  if(ret->list_index!=XAW_LIST_NONE) {
    struct city *pcity;
    if((pcity=find_city_by_id(cities_in_list[ret->list_index]))) {
      int value;
      char *name;
      char buf[512];

      value=city_buy_cost(pcity);    
      if(pcity->is_building_unit)
	name=get_unit_type(pcity->currently_building)->name;
      else
	name=get_imp_name_ex(pcity, pcity->currently_building);

      if (game.player_ptr->economic.gold >= value)
	{
	  struct packet_city_request packet;
	  packet.city_id=pcity->id;
	  packet.name[0]='\0';
	  send_packet_city_request(&aconnection, &packet, PACKET_CITY_BUY);
	}
      else
	{
	  sprintf(buf, "Game: %s costs %d gold and you only have %d gold.",
		  name,value,game.player_ptr->economic.gold);
	  append_output_window(buf);
	}
    }
  }
}

/****************************************************************
...
*****************************************************************/
void city_close_callback(Widget w, XtPointer client_data, 
			 XtPointer call_data)
{

  if(city_dialog_shell_is_modal)
     XtSetSensitive(main_form, TRUE);
   XtDestroyWidget(city_dialog_shell);
   city_dialog_shell=0;
}

/****************************************************************
...
*****************************************************************/
void city_center_callback(Widget w, XtPointer client_data, 
			  XtPointer call_data)
{
  XawListReturnStruct *ret=XawListShowCurrent(city_list);

  if(ret->list_index!=XAW_LIST_NONE) {
    struct city *pcity;
    if((pcity=find_city_by_id(cities_in_list[ret->list_index])))
      center_tile_mapcanvas(pcity->x, pcity->y);
  }
}

/****************************************************************
...
*****************************************************************/
void city_popup_callback(Widget w, XtPointer client_data, 
			 XtPointer call_data)
{
  XawListReturnStruct *ret=XawListShowCurrent(city_list);

  if(ret->list_index!=XAW_LIST_NONE) {
    struct city *pcity;
    if((pcity=find_city_by_id(cities_in_list[ret->list_index]))) {
      center_tile_mapcanvas(pcity->x, pcity->y);
      popup_city_dialog(pcity, 0);
    }
  }
}


/****************************************************************
...
*****************************************************************/
void city_report_dialog_update(void)
{
  if(city_dialog_shell) {
    int i;
    Dimension width; 
    static char *city_list_names_ptrs[MAX_CITIES_SHOWN+1];
    static char city_list_names[MAX_CITIES_SHOWN][200];
    struct genlist_iterator myiter;
    char *report_title;
    char happytext[512];
    char statetext[512];
    report_title=get_report_title("City Advisor");
    xaw_set_label(city_label, report_title);
    free(report_title);
    if (city_popupmenu)
      {
	XtDestroyWidget(city_popupmenu);
	city_popupmenu = 0;
      }    
    genlist_iterator_init(&myiter, &game.player_ptr->cities.list, 0);
    
     for(i=0; ITERATOR_PTR(myiter) && i<MAX_CITIES_SHOWN; 
	 i++, ITERATOR_NEXT(myiter)) {
       char impro[64];
       struct city *pcity=(struct city *)ITERATOR_PTR(myiter);

       if(pcity->is_building_unit)
	 sprintf(impro, "%s(%d/%d/%d)", 
		 get_unit_type(pcity->currently_building)->name,
		 pcity->shield_stock,
		 get_unit_type(pcity->currently_building)->build_cost,
		 city_buy_cost(pcity));
       else
	 sprintf(impro, "%s(%d/%d/%d)", 
		 get_imp_name_ex(pcity, pcity->currently_building),
		 pcity->shield_stock,
		 get_improvement_type(pcity->currently_building)->build_cost,
		 city_buy_cost(pcity));
              
       sprintf(happytext, "%s(%d/%d/%d)",
	       city_unhappy(pcity) ? "Disorder" : "Peace",
	       pcity->ppl_happy[4],
	       pcity->ppl_content[4],
	       pcity->ppl_unhappy[4]);
 
       sprintf(statetext, "(%d/%d/%d)",
	       pcity->food_surplus, 
	       pcity->shield_surplus, 
	       pcity->trade_prod);

       sprintf(city_list_names[i], "%-15s %-16s%-12s%s", 
	       pcity->name,
	       happytext,
	       statetext,
	       impro);
       city_list_names_ptrs[i]=city_list_names[i];
       cities_in_list[i]=pcity->id;
     }
    if(i==0) {
      strcpy(city_list_names[0], 
	     "                                                             ");
      city_list_names_ptrs[0]=city_list_names[0];
      i=1;
      cities_in_list[0]=0;
    }
    city_list_names_ptrs[i]=0;

    XawListChange(city_list, city_list_names_ptrs, 0, 0, 1);

    XtVaGetValues(city_list, XtNwidth, &width, NULL);
    XtVaSetValues(city_list_label, XtNwidth, width, NULL); 
    XtSetSensitive(city_change_command, FALSE);
    XtSetSensitive(city_center_command, FALSE);
    XtSetSensitive(city_popup_command, FALSE);
    XtSetSensitive(city_buy_command, FALSE);
  }
  
}

/****************************************************************

                      TRADE REPORT DIALOG
 
****************************************************************/

/****************************************************************
...
****************************************************************/
void popup_trade_report_dialog(int make_modal)
{
  if(!trade_dialog_shell) {
      Position x, y;
      Dimension width, height;
      
      trade_dialog_shell_is_modal=make_modal;
    
      if(make_modal)
	XtSetSensitive(main_form, FALSE);
      
      create_trade_report_dialog(make_modal);
      
      XtVaGetValues(toplevel, XtNwidth, &width, XtNheight, &height, NULL);
      
      XtTranslateCoords(toplevel, (Position) width/10, (Position) height/10,
			&x, &y);
      XtVaSetValues(trade_dialog_shell, XtNx, x, XtNy, y, NULL);
      
      XtPopup(trade_dialog_shell, XtGrabNone);
   }
}


/****************************************************************
...
*****************************************************************/
void create_trade_report_dialog(int make_modal)
{
  Widget trade_form;
  Widget close_command;
  char *report_title;
  
  trade_dialog_shell = XtVaCreatePopupShell("reporttradepopup", 
					      make_modal ? 
					      transientShellWidgetClass :
					      topLevelShellWidgetClass,
					      toplevel, 
					      0);

  trade_form = XtVaCreateManagedWidget("reporttradeform", 
					 formWidgetClass,
					 trade_dialog_shell,
					 NULL);   

  report_title=get_report_title("Trade Advisor");
  trade_label = XtVaCreateManagedWidget("reporttradelabel", 
				       labelWidgetClass, 
				       trade_form,
				       XtNlabel, 
				       report_title,
				       NULL);
  free(report_title);

  trade_list_label = XtVaCreateManagedWidget("reporttradelistlabel", 
				       labelWidgetClass, 
				       trade_form,
				       NULL);
  
  trade_list = XtVaCreateManagedWidget("reporttradelist", 
				      listWidgetClass,
				      trade_form,
				      NULL);

  trade_label2 = XtVaCreateManagedWidget("reporttradelabel2", 
				       labelWidgetClass, 
				       trade_form,					 			      XtNlabel, 
				       "Total Cost:", 
				       NULL);

  close_command = XtVaCreateManagedWidget("reporttradeclosecommand", 
					  commandWidgetClass,
					  trade_form,
					  NULL);

  XtAddCallback(close_command, XtNcallback, trade_close_callback, NULL);
  XtRealizeWidget(trade_dialog_shell);
  trade_report_dialog_update();
}


/****************************************************************
...
*****************************************************************/
void trade_close_callback(Widget w, XtPointer client_data, 
			 XtPointer call_data)
{

  if(trade_dialog_shell_is_modal)
     XtSetSensitive(main_form, TRUE);
   XtDestroyWidget(trade_dialog_shell);
   trade_dialog_shell=0;
}

/****************************************************************
...
*****************************************************************/
void trade_report_dialog_update(void)
{
  if(trade_dialog_shell) {
    int j, k, count, cost, total;
    Dimension width; 
    static char *trade_list_names_ptrs[B_LAST+1];
    static char trade_list_names[B_LAST][200];
    struct genlist_iterator myiter;
    char *report_title;
    char trade_total[100];
    struct city *pcity;
    
    report_title=get_report_title("Trade Advisor");
    xaw_set_label(trade_label, report_title);
    free(report_title);
    total = 0;
    k = 0;
    for (j=0;j<B_LAST;j++) {
      count = 0;
      pcity = NULL;
      genlist_iterator_init(&myiter, &game.player_ptr->cities.list, 0);
      for(; ITERATOR_PTR(myiter);ITERATOR_NEXT(myiter)) {
	pcity=(struct city *)ITERATOR_PTR(myiter);
	if (city_got_building(pcity, j)) count++;
      }
      if (pcity) {
	cost = count * improvement_upkeep(pcity, j);
	if (cost) {
	  sprintf(trade_list_names[k], "%-20s%5d%5d%6d", get_improvement_name(j), count, improvement_upkeep(pcity, j), cost);
	  total+=cost;
	  trade_list_names_ptrs[k]=trade_list_names[k];
	  k++;
	}
      }
    }
    
    if(k==0) {
      strcpy(trade_list_names[0], "                                          ");
      trade_list_names_ptrs[0]=trade_list_names[0];
      k=1;
    }

    sprintf(trade_total, "Income:%6d    Total Costs: %6d", total+turn_gold_difference, total); 
     xaw_set_label(trade_label2, trade_total); 
    trade_list_names_ptrs[k]=0;
    
    XawListChange(trade_list, trade_list_names_ptrs, 0, 0, 1);

    XtVaGetValues(trade_list, XtNwidth, &width, NULL);
    XtVaSetValues(trade_list_label, XtNwidth, width, NULL); 

    XtVaSetValues(trade_label2, XtNwidth, width, NULL); 

    XtVaSetValues(trade_label, XtNwidth, width, NULL); 

  }
  
}

/****************************************************************

                      ACTIVE UNITS REPORT DIALOG
 
****************************************************************/

/****************************************************************
...
****************************************************************/
void popup_activeunits_report_dialog(int make_modal)
{
  if(!activeunits_dialog_shell) {
      Position x, y;
      Dimension width, height;
      
      activeunits_dialog_shell_is_modal=make_modal;
    
      if(make_modal)
	XtSetSensitive(main_form, FALSE);
      
      create_activeunits_report_dialog(make_modal);
      
      XtVaGetValues(toplevel, XtNwidth, &width, XtNheight, &height, NULL);
      
      XtTranslateCoords(toplevel, (Position) width/10, (Position) height/10,
			&x, &y);
      XtVaSetValues(activeunits_dialog_shell, XtNx, x, XtNy, y, NULL);
      
      XtPopup(activeunits_dialog_shell, XtGrabNone);
   }
}


/****************************************************************
...
*****************************************************************/
void create_activeunits_report_dialog(int make_modal)
{
  Widget activeunits_form;
  Widget close_command;
  char *report_title;
  
  activeunits_dialog_shell = XtVaCreatePopupShell("reportactiveunitspopup", 
					      make_modal ? 
					      transientShellWidgetClass :
					      topLevelShellWidgetClass,
					      toplevel, 
					      0);

  activeunits_form = XtVaCreateManagedWidget("reportactiveunitsform", 
					 formWidgetClass,
					 activeunits_dialog_shell,
					 NULL);   

  report_title=get_report_title("Active Units");
  activeunits_label = XtVaCreateManagedWidget("reportactiveunitslabel", 
				       labelWidgetClass, 
				       activeunits_form,
				       XtNlabel, 
				       report_title,
				       NULL);
  free(report_title);

  activeunits_list_label = XtVaCreateManagedWidget("reportactiveunitslistlabel", 
				       labelWidgetClass, 
				       activeunits_form,
				       NULL);
  
  activeunits_list = XtVaCreateManagedWidget("reportactiveunitslist", 
				      listWidgetClass,
				      activeunits_form,
				      NULL);

  activeunits_label2 = XtVaCreateManagedWidget("reportactiveunitslabel2", 
				       labelWidgetClass, 
				       activeunits_form,
                                       XtNlabel, 
				       "Total Cost:", 
				       NULL);

  close_command = XtVaCreateManagedWidget("reportactiveunitsclosecommand", 
					  commandWidgetClass,
					  activeunits_form,
					  NULL);

  upgrade_command = XtVaCreateManagedWidget("reportactiveunitsupgradecommand", 
					  commandWidgetClass,
					  activeunits_form,
					  XtNsensitive, False,
					  NULL);
  XtAddCallback(activeunits_list, XtNcallback, activeunits_list_callback, NULL);
  XtAddCallback(close_command, XtNcallback, activeunits_close_callback, NULL);
  XtAddCallback(upgrade_command, XtNcallback, activeunits_upgrade_callback, NULL);
  XtRealizeWidget(activeunits_dialog_shell);
  activeunits_report_dialog_update();
}

/****************************************************************
...
*****************************************************************/
void activeunits_list_callback(Widget w, XtPointer client_data, 
			 XtPointer call_data)
{
  XawListReturnStruct *ret;
  ret=XawListShowCurrent(activeunits_list);

  if(ret->list_index!=XAW_LIST_NONE) {
    if (can_upgrade_unittype(game.player_ptr, activeunits_type[ret->list_index]) != -1) 
      XtSetSensitive(upgrade_command, TRUE);
    return;
  }
  XtSetSensitive(upgrade_command, FALSE);
}

/****************************************************************
...
*****************************************************************/
void upgrade_callback_yes(Widget w, XtPointer client_data, 
                                 XtPointer call_data)
{
  send_packet_unittype_info(&aconnection, (int)client_data, PACKET_UNIT_UPGRADE);
  destroy_message_dialog(w);
}

/****************************************************************
...
*****************************************************************/
void upgrade_callback_no(Widget w, XtPointer client_data, 
                                XtPointer call_data)
{
  destroy_message_dialog(w);
}

/****************************************************************
...
*****************************************************************/
void activeunits_upgrade_callback(Widget w, XtPointer client_data, 
			 XtPointer call_data)
{
  char buf[512];
  int ut1,ut2;

  XawListReturnStruct *ret;
  ret=XawListShowCurrent(activeunits_list);

  if(ret->list_index!=XAW_LIST_NONE) {
    ut1 = activeunits_type[ret->list_index];
    puts(unit_types[ut1].name);
    
    ut2 = can_upgrade_unittype(game.player_ptr, activeunits_type[ret->list_index]);
    
    sprintf(buf, "upgrade as many %s to %s as possible for %d gold each?\nTreasure %d gold.", unit_types[ut1].name, unit_types[ut2].name, unit_upgrade_price(game.player_ptr, ut1, ut2), game.player_ptr->economic.gold);
    popup_message_dialog(toplevel, "upgradedialog", buf,
			 upgrade_callback_yes, (XtPointer)(activeunits_type[ret->list_index]),
			 upgrade_callback_no, 0, 0);
  }
}

/****************************************************************
...
*****************************************************************/
void activeunits_close_callback(Widget w, XtPointer client_data, 
			 XtPointer call_data)
{

  if(activeunits_dialog_shell_is_modal)
     XtSetSensitive(main_form, TRUE);
   XtDestroyWidget(activeunits_dialog_shell);
   activeunits_dialog_shell=0;
}

/****************************************************************
...
*****************************************************************/
void activeunits_report_dialog_update(void)
{
  if(activeunits_dialog_shell) {
    int i, k, total;
    Dimension width; 
    static char *activeunits_list_names_ptrs[U_LAST+1];
    static char activeunits_list_names[U_LAST][200];
    int unit_count[U_LAST];
    char *report_title;
    char activeunits_total[100];
    
    report_title=get_report_title("Active Units");
    xaw_set_label(activeunits_label, report_title);
    free(report_title);
    for (i=0;i <U_LAST;i++) 
      unit_count[i]=0;
    unit_list_iterate(game.player_ptr->units, punit) 
      unit_count[punit->type]++;
    unit_list_iterate_end;
    k = 0;
    total = 0;
    for (i=0;i<U_LAST;i++) {
      if (unit_count[i] > 0) {
	sprintf(activeunits_list_names[k], "%-27s%c%5d", unit_name(i), can_upgrade_unittype(game.player_ptr, i) != -1 ? '*': '-', unit_count[i]);
	activeunits_list_names_ptrs[k]=activeunits_list_names[k];
	activeunits_type[k]=i;
	k++;
	total+=unit_count[i];
      }
    }
    if (k==0) {
      strcpy(activeunits_list_names[0], "                                ");
      activeunits_list_names_ptrs[0]=activeunits_list_names[0];
      k=1;
    }

    sprintf(activeunits_total, "Total units:%21d",total); 

    xaw_set_label(activeunits_label2, activeunits_total); 
    activeunits_list_names_ptrs[k]=0;
    
    XawListChange(activeunits_list, activeunits_list_names_ptrs, 0, 0, 1);

    XtVaGetValues(activeunits_list, XtNwidth, &width, NULL);
    XtVaSetValues(activeunits_list_label, XtNwidth, width, NULL); 

    XtVaSetValues(activeunits_label2, XtNwidth, width, NULL); 

    XtVaSetValues(activeunits_label, XtNwidth, width, NULL); 
  }
}
