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

#ifndef FC__RULEDIT_QT_H
#define FC__RULEDIT_QT_H

// Qt
#include <QApplication>
#include <QObject>
#include <QLabel>
#include <QTabWidget>

class QLineEdit;
class QStackedLayout;

class tab_misc;
class tab_tech;

class ruledit_gui : public QObject
{
  Q_OBJECT

  public:
  void setup(QApplication *qapp, QWidget *central_in);
    void display_msg(const char *msg);
    int run();
    void close();

  private:
    QApplication *app;
    QLabel *msg_dspl;
    QTabWidget *stack;
    QLineEdit *ruleset_select;
    QWidget *central;
    QStackedLayout *main_layout;

    tab_misc *misc;
    tab_tech *tech;

  private slots:
    void launch_now();
};

bool ruledit_qt_setup(int argc, char **argv);
int ruledit_qt_run();
void ruledit_qt_close();

#endif // FC__RULEDIT_QT_H
