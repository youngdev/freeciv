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

#ifndef FC__DIALOGS_H
#define FC__DIALOGS_H

#ifdef HAVE_CONFIG_H
#include <fc_config.h>
#endif

extern "C" {
#include "dialogs_g.h"
}

// Qt
#include <QDialog>
#include <QVariant>

// gui-qt
#include "mapview.h"

class QComboBox;
class QGridLayout;
class QGroupBox;
class QItemSelection;
class QRadioButton;
class QSignalMapper;
class QTableView;
class QTableWidget;
class QTextEdit;
class QToolBox;
class QWidget;

typedef void (*pfcn_void)(QVariant, QVariant);


void popup_races_dialog(struct player *pplayer);
/***************************************************************************
 Dialog for selecting nation, style and leader leader
***************************************************************************/
class races_dialog:public QDialog
{
  Q_OBJECT
  QGridLayout *main_layout;
  QToolBox *nation_tabs;
  QList<QWidget*> *nations_tabs_list;
  QTableWidget *selected_nation_tabs;
  QComboBox *leader_name;
  QRadioButton *is_male;
  QRadioButton *is_female;
  QTableWidget *city_styles;
  QTextEdit *description;
  QPushButton *ok_button;
  QPushButton *random_button;

public:
  races_dialog(struct player *pplayer, QWidget *parent = 0);
  ~races_dialog();

private slots:
  void set_index(int index);
  void nation_selected(const QItemSelection &sl, const QItemSelection &ds);
  void style_selected(const QItemSelection &sl, const QItemSelection &ds);
  void leader_selected(int index);
  void ok_pressed();
  void cancel_pressed();
  void random_pressed();

private:
  int selected_nation;
  int selected_style;
  int selected_sex;
  struct player *tplayer;
};

/***************************************************************************
 Widget around map view to display informations like demographics report,
 top 5 cities, traveler's report.
***************************************************************************/
class notify_dialog:public fcwidget
{
  Q_OBJECT
public:
  notify_dialog(const char *caption, const char *headline,
                const char *lines, QWidget *parent = 0);
  virtual void update_menu();
  ~notify_dialog();
protected:
  void mousePressEvent(QMouseEvent *event);
  void mouseMoveEvent(QMouseEvent *event);
  void mouseReleaseEvent(QMouseEvent *event);
private:
  void paintEvent(QPaintEvent *paint_event);
  void calc_size(int &x, int&y);
  close_widget *cw;
  QLabel *label;
  QVBoxLayout *layout;
  QString qcaption;
  QString qheadline;
  QStringList qlist;
  QFont *small_font;
  QPoint cursor;
};

/***************************************************************************
 Transparent widget for selecting units
 TODO Add some simple scrollbars (just paint it during paint event,
 if 'more' is true->scroll visible and would depend on show_line
***************************************************************************/
class unit_select: public fcwidget
{
  Q_OBJECT
  QPixmap *pix;
  QPixmap *h_pix; /** pixmap for highlighting */
  QSize item_size; /** size of each pixmap of unit */
  QList<unit*> unit_list; /** storing units only for drawing, for rest units
                            * iterate utile->units */
  QFont *ufont;
  QFont *info_font;
  int row_count;
public:
  unit_select(struct tile *ptile, QWidget *parent);
  ~unit_select();
  void update_menu();
  void update_units();
  void create_pixmap();
  tile *utile;
protected:
  void paint(QPainter *painter, QPaintEvent *event);
  void paintEvent(QPaintEvent *event);
  void mousePressEvent(QMouseEvent *event);
  void mouseMoveEvent(QMouseEvent *event);
  void wheelEvent(QWheelEvent *event);
private:
  bool more;
  int show_line;
  int highligh_num;
};

/***************************************************************************
  Simple choice dialog, allowing choosing one of set actions
***************************************************************************/
class choice_dialog: public QWidget
{
  Q_OBJECT
  QVBoxLayout *layout;
  QList<QVariant> data1_list;
  QList<QVariant> data2_list;
  QSignalMapper *signal_mapper;
  void (*run_on_close)(void);
public:
  choice_dialog(const QString title, const QString text,
                QWidget *parent = NULL, void (*run_on_close)(void) = NULL);
  ~choice_dialog();
  void set_layout();
  void add_item(QString title, pfcn_void func, QVariant data1, 
                QVariant data2, bool warn);
  void show_me();
  QVBoxLayout *get_layout();
  QList<pfcn_void> func_list;
  int unit_id;
public slots:
  void execute_action(const int action);
};

void popup_revolution_dialog(struct government *government = NULL);
void revolution_response(struct government *government);
void popup_upgrade_dialog(struct unit_list *punits);
void popup_disband_dialog(struct unit_list *punits);

#endif /* FC__DIALOGS_H */
