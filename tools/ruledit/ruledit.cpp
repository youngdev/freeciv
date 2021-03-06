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

/* ANSI */
#include <stdlib.h>

/* utility */
#include "fciconv.h"
#include "fcintl.h"
#include "log.h"
#include "registry.h"

/* common */
#include "fc_cmdhelp.h"
#include "version.h"

/* server */
#include "sernet.h"
#include "settings.h"

/* ruledit */
#include "ruledit_qt.h"

#include "ruledit.h"

static int re_parse_cmdline(int argc, char *argv[]);

/**************************************************************************
  Main entry point for freeciv-ruledit
**************************************************************************/
int main(int argc, char **argv)
{
  enum log_level loglevel = LOG_NORMAL;
  int ui_options;

  init_nls();

#ifdef ENABLE_NLS
  (void) bindtextdomain("freeciv-ruledit", LOCALEDIR);
#endif

  registry_module_init();
  init_character_encodings(FC_DEFAULT_DATA_ENCODING, FALSE);

  log_init(NULL, loglevel, NULL, NULL, -1);

  ui_options = re_parse_cmdline(argc, argv);

  if (ui_options != -1) {
    init_connections();

    settings_init(FALSE);

    /* Reset aifill to zero */
    game.info.aifill = 0;

    game_init();
    i_am_server();

    ruledit_qt_setup(ui_options, argv);
    ruledit_qt_run();
    ruledit_qt_close();
  }

  log_close();
  registry_module_close();

  return EXIT_SUCCESS;
}

/**************************************************************************
  Parse freeciv-ruledit commandline.
**************************************************************************/
static int re_parse_cmdline(int argc, char *argv[])
{
  int i = 1;
  bool ui_separator = FALSE;
  int ui_options = 0;

  while (i < argc) {
    if (ui_separator) {
      argv[1 + ui_options] = argv[i];
      ui_options++;
    } else if (is_option("--help", argv[i])) {
      struct cmdhelp *help = cmdhelp_new(argv[0]);

      cmdhelp_add(help, "h", "help",
                  R__("Print a summary of the options"));
      cmdhelp_add(help, "v", "version",
                  R__("Print the version number"));
      /* The function below prints a header and footer for the options.
       * Furthermore, the options are sorted. */
      cmdhelp_display(help, TRUE, TRUE, TRUE);
      cmdhelp_destroy(help);

      exit(EXIT_SUCCESS);
    } else if (is_option("--version", argv[i])) {
      fc_fprintf(stderr, "%s \n", freeciv_name_version());

      exit(EXIT_SUCCESS);
    } else if (is_option("--", argv[i])) {
      ui_separator = TRUE;
    } else {
      fc_fprintf(stderr, R__("Unrecognized option: \"%s\"\n"), argv[i]);
      exit(EXIT_FAILURE);
    }

    i++;
  }

  return ui_options;
}
