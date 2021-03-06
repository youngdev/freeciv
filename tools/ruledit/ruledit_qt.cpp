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

// Qt
#include <QApplication>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QPushButton>
#include <QStackedLayout>
#include <QVBoxLayout>

// utility
#include "fcintl.h"
#include "log.h"
#include "registry.h"

// common
#include "game.h"

// server
#include "ruleset.h"

// ruledit
#include "tab_misc.h"
#include "tab_tech.h"
#include "ruledit.h"

#include "ruledit_qt.h"

static ruledit_gui *gui;

/**************************************************************************
  Setup ruledit-qt gui.
**************************************************************************/
bool ruledit_qt_setup(int argc, char **argv)
{
  QApplication *qapp;
  QMainWindow *main_window;
  QWidget *central;

  qapp = new QApplication(argc, argv);
  main_window = new QMainWindow;
  main_window->setWindowTitle(R__("Freeciv ruleset editor"));
  central = new QWidget;

  gui = new ruledit_gui;
  gui->setup(qapp, central);
  main_window->setCentralWidget(central);
  main_window->setVisible(true);

  return true;
}

/**************************************************************************
  Execute ruledit-qt gui.
**************************************************************************/
int ruledit_qt_run()
{
  return gui->run();
}

/**************************************************************************
  Close ruledit-qt gui.
**************************************************************************/
void ruledit_qt_close()
{
  gui->close();

  delete gui;
}

/**************************************************************************
  Setup GUI object
**************************************************************************/
void ruledit_gui::setup(QApplication *qapp, QWidget *central_in)
{
  QVBoxLayout *full_layout = new QVBoxLayout();
  QVBoxLayout *preload_layout = new QVBoxLayout();
  QWidget *preload_widget = new QWidget();
  QVBoxLayout *edit_layout = new QVBoxLayout();
  QWidget *edit_widget = new QWidget();
  QPushButton *ruleset_accept;
  QLabel *rs_label;

  app = qapp;
  central = central_in;

  main_layout = new QStackedLayout();

  preload_layout->setSizeConstraint(QLayout::SetMaximumSize);
  rs_label = new QLabel(R__("Give ruleset to use as starting point."));
  rs_label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
  preload_layout->addWidget(rs_label);
  ruleset_select = new QLineEdit(central);
  ruleset_select->setText("classic");
  preload_layout->addWidget(ruleset_select);
  ruleset_accept = new QPushButton(R__("Start editing"));
  connect(ruleset_accept, SIGNAL(pressed()), this, SLOT(launch_now()));
  preload_layout->addWidget(ruleset_accept);

  preload_widget->setLayout(preload_layout);
  main_layout->addWidget(preload_widget);

  stack = new QTabWidget(central);

  misc = new tab_misc(this);
  stack->addTab(misc, R__("Misc"));
  tech = new tab_tech(this);
  stack->addTab(tech, R__("Tech"));

  edit_layout->addWidget(stack);

  edit_widget->setLayout(edit_layout);
  main_layout->addWidget(edit_widget);

  full_layout->addLayout(main_layout);

  msg_dspl = new QLabel(R__("Welcome to freeciv-ruledit"));
  msg_dspl->setParent(central);

  msg_dspl->setAlignment(Qt::AlignHCenter);

  full_layout->addWidget(msg_dspl);

  central->setLayout(full_layout);
}

/**************************************************************************
  User entered savedir
**************************************************************************/
void ruledit_gui::launch_now()
{
  sz_strlcpy(game.server.rulesetdir, ruleset_select->text().toUtf8().data());

  if (load_rulesets(NULL, FALSE, TRUE)) {

    log_debug("Terrains:     %d", game.control.terrain_count);
    log_debug("Resources:    %d", game.control.resource_count);
    log_debug("Techs:        %d", game.control.num_tech_types);
    log_debug("Unit classes: %d", game.control.num_unit_classes);
    log_debug("Unit types:   %d", game.control.num_unit_types);
    log_debug("Buildings:    %d", game.control.num_impr_types);
    log_debug("Nations:      %d", game.control.nation_count);
    log_debug("City Styles:  %d", game.control.styles_count);
    log_debug("Specialists:  %d", game.control.num_specialist_types);
    log_debug("Governments:  %d", game.control.government_count);
    log_debug("Disasters:    %d", game.control.num_disaster_types);
    log_debug("Achievements: %d", game.control.num_achievement_types);
    log_debug("Extras:       %d", game.control.num_extra_types);
    log_debug("Bases:        %d", game.control.num_base_types);
    log_debug("Roads:        %d", game.control.num_road_types);

    display_msg(R__("Ruleset loaded"));
    misc->refresh();
    tech->refresh();
    main_layout->setCurrentIndex(1);
  } else {
    display_msg(R__("Ruleset loading failed!"));
  }
}

/**************************************************************************
  Execute GUI object
**************************************************************************/
int ruledit_gui::run()
{
  app->exec();

  return EXIT_SUCCESS;
}

/**************************************************************************
  Close GUI object
**************************************************************************/
void ruledit_gui::close()
{
  delete app;
}

/**************************************************************************
  Display status message
**************************************************************************/
void ruledit_gui::display_msg(const char *msg)
{
  msg_dspl->setText(msg);
}
