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
#ifndef FC__CITYDLG_H
#define FC__CITYDLG_H

#ifdef HAVE_CONFIG_H
#include <fc_config.h>
#endif

// Qt
#include <QItemDelegate>

class QComboBox;
class QDialog;
class QGridLayout;
class QLabel;
class QPushButton;
class QTabWidget;
class QTableView;
class QTableWidget;
class QProgressBar;
class QVariant;
class QVBoxLayout;

#define NUM_INFO_FIELDS 12

// client
#include "canvas.h"

// Qt
#include <QProgressBar>
#include <QTableWidget>

class city_dialog;
class QChecBox;

/****************************************************************************
  Subclassed QProgressBar to receive clicked signal
****************************************************************************/
class progress_bar: public QProgressBar
{
  Q_OBJECT
signals:
  void clicked();
public:
  progress_bar(QWidget *parent): QProgressBar(parent) {}
  void mousePressEvent(QMouseEvent *event) {
    emit clicked();
  }
};

/****************************************************************************
  Subclassed QLabel to receive clicked signal
****************************************************************************/
class fc_label: public QLabel
{
  Q_OBJECT
signals:
  void clicked();
public:
  fc_label(QWidget *parent): QLabel(parent) {}
  void mousePressEvent(QMouseEvent *event) {
    emit clicked();
  }
};


/****************************************************************************
  Single item on unit_info in city dialog representing one unit
****************************************************************************/
class unit_item: public QLabel
{

  Q_OBJECT

  QAction *disband_action;
  QAction *change_home;
  QAction *activate;
  QAction *activate_and_close;
  QAction *sentry;
  QAction *fortify;

public:

  unit_item(struct unit *punit);
  ~unit_item();
  void init_pix();
  void mouseMoveEvent(QMouseEvent *event);
  void mousePressEvent(QMouseEvent *event);
private:

  struct unit *qunit;
  struct canvas *unit_pixmap;
  void contextMenuEvent(QContextMenuEvent *ev);
  void create_actions();

private slots:

  void disband();
  void change_homecity();
  void activate_unit();
  void activate_and_close_dialog();
  void sentry_unit();
  void fortify_unit();

};

/****************************************************************************
  Shows list of units ( as labels - unit_info )
****************************************************************************/
class unit_info: public QWidget
{

  Q_OBJECT

public:

  unit_info();
  ~unit_info();
  void add_item(unit_item *item);
  void init_layout();
  void clear_layout();
  QHBoxLayout *layout;
  QList<unit_item *> unit_list;
  void mouseMoveEvent(QMouseEvent *event);

};


/****************************************************************************
  Class used for showing tiles and workers view in city dialog
****************************************************************************/
class city_map: public QWidget
{

  Q_OBJECT
  canvas *view;
  canvas *miniview;

public:

  city_map(QWidget *parent);
  ~city_map();
  void set_pixmap(struct city *pcity);

private:

  void mousePressEvent(QMouseEvent *event);
  void paintEvent(QPaintEvent *event);
  struct city *mcity;
  int radius;
  int wdth;
  int hight;
  int cutted_width;
  int cutted_height;
  int delta_x;
  int delta_y;
};

/****************************************************************************
  Item delegate for production popup
****************************************************************************/
class city_production_delegate: public QItemDelegate
{
  Q_OBJECT

public:
  city_production_delegate(QPoint sh, QObject *parent, struct city* city);
  ~city_production_delegate() {}
  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const;
  QSize sizeHint(const QStyleOptionViewItem &option,
                 const QModelIndex &index) const;
private:
  int item_height;
  QPoint pd;
  struct city *pcity;
protected:
  void drawFocus(QPainter *painter, const QStyleOptionViewItem &option,
                 const QRect &rect) const;
};

/****************************************************************************
  Single item in production popup
****************************************************************************/
class production_item: public QObject
{
  Q_OBJECT

public:
  production_item(struct universal *ptarget);
  ~production_item();
  inline int columnCount() const {
    return 1;
  }
  QVariant data() const;
  bool setData();
private:
  struct universal *target;
};

/***************************************************************************
  City production model
***************************************************************************/
class city_production_model : public QAbstractListModel
{
  Q_OBJECT
public:
  city_production_model(struct city *pcity, bool f, bool su,
                        QObject *parent = 0);
  ~city_production_model();
  inline int rowCount(const QModelIndex &index = QModelIndex()) const {
    Q_UNUSED(index);
    return (qCeil(static_cast<float>(city_target_list.size()) / 3));
  }
  int columnCount(const QModelIndex &parent = QModelIndex()) const {
    Q_UNUSED(parent);
    return 3;
  }
  QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
  bool setData(const QModelIndex &index, const QVariant &value,
               int role = Qt::DisplayRole);
  QPoint size_hint();
  void populate();
  QPoint sh;
private:
  QList<production_item *> city_target_list;
  struct city *mcity;
  bool future_t;
  bool show_units;
};

/****************************************************************************
  Class for popup avaialable production
****************************************************************************/
class production_widget: public QTableView
{
  Q_OBJECT
  city_production_model *list_model;
  city_production_delegate *c_p_d;
public:
  production_widget(struct city *pcity, bool future, int when, int curr,
                    bool show_units);
  ~production_widget();
public slots:
  void prod_selected(const QItemSelection &sl, const QItemSelection &ds);
private:
  void mousePressEvent(QMouseEvent *event);
  struct city *pw_city;
  int when_change;
  int curr_selection;
  bool sh_units;
};


/****************************************************************************
  city_label is used only for showing citizens icons
  and was created to catch mouse events
****************************************************************************/
class city_label: public QLabel
{

  Q_OBJECT

public:

  city_label(int type, QWidget *parent = 0);
  void set_city(struct city *pcity);

private:

  struct city *pcity;
  int type;

protected:

  void mousePressEvent(QMouseEvent *event);

};

/****************************************************************************
  City dialog
****************************************************************************/
class city_dialog: public QDialog
{

  Q_OBJECT

  QWidget *widget;
  QGridLayout *main_grid_layout;
  QGridLayout *overview_grid_layout;
  QGridLayout *production_grid_layout;
  QGridLayout *happiness_grid_layout;
  QGridLayout *cma_grid_layout;
  city_map *view;
  city_map *info_view;
  city_label *citizens_label;
  city_label *lab_table[6];
  QGridLayout *info_grid_layout;
  QGroupBox *info_labels_group;
  QWidget *info_widget;
  QLabel *qlt[NUM_INFO_FIELDS];
  QLabel *cma_info_text;
  QLabel *cma_result;
  QLabel *supp_units;
  QLabel *curr_units;
  progress_bar *production_combo;
  QTableWidget *production_table;
  progress_bar *production_combo_p;
  QTableWidget *p_table_p;
  QTableWidget *nationality_table;
  QTableWidget *cma_table;
  QPushButton *buy_button_p;
  QCheckBox *cma_celeb_checkbox;
  QCheckBox *future_targets_p;
  QCheckBox *show_units_p;
  QCheckBox *disband_at_one;
  QRadioButton *r1, *r2, *r3, *r4;
  QTabWidget *tab_widget;
  QWidget *overview_tab;
  QWidget *production_tab;
  QWidget *happiness_tab;
  QWidget *governor_tab;
  QPushButton *button;
  QPushButton *buy_button;
  QPushButton *item_button;
  QPushButton *item_button_p;
  QPushButton *cma_enable_but;
  QPushButton *next_city_but;
  QPushButton *prev_city_but;
  QPushButton *but_remove_item;
  QPushButton *but_clear_worklist;
  QPixmap *citizen_pixmap;
  unit_info *current_units;
  unit_info *supported_units;
  fc_label *lcity_name;
  fc_label *pcity_name;
  int selected_row_p;
  QSlider* slider_tab[2*O_LAST+2];

public:

  city_dialog(QWidget *parent = 0);
  ~city_dialog();
  void setup_ui(struct city *qcity);
  void refresh();
  struct city *pcity;

private:

  int current_building;
  void update_title();
  void update_building();
  void update_info_label();
  void update_buy_button();
  void update_citizens();
  void update_improvements();
  void update_units();
  void update_settings();
  void update_nation_table();
  void update_cma_tab();
  void update_disabled();

private slots:

  void next_city();
  void prev_city();
  void production_changed(int index);
  void show_targets();
  void show_targets_worklist();
  void buy();
  void dbl_click(QTableWidgetItem *item);
  void dbl_click_p(QTableWidgetItem *item);
  void delete_prod();;
  void item_selected(const QItemSelection &sl, const QItemSelection &ds);
  void clear_worklist();
  void save_worklist();
  void display_worklist_menu(const QPoint &p);
  void disband_state_changed(int state);
  void update_results_text();
  void cma_slider(int val);
  void cma_celebrate_changed(int val);
  void cma_remove();
  void cma_enable();
  void cma_changed();
  void cma_selected(const QItemSelection &sl, const QItemSelection &ds);
  void save_cma();
  void city_rename();
};

void destroy_city_dialog();

#endif                          /* FC__CITYDLG_H */
