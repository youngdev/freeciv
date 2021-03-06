/********************************************************************** 
 Freeciv - Copyright (C) 2005 - The Freeciv Team
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* utility */
#include "connection.h"
#include "log.h"

/* client */
#include "audio.h"
#include "client_main.h"
#include "options.h"

#include "music.h"

/**********************************************************************
  Start music suitable for current game situation
***********************************************************************/
void start_style_music(void)
{
  if (client_state() != C_S_RUNNING) {
    /* Style music plays in running game only */
    return;
  }

  if (client.conn.playing == NULL) {
    /* Detached connections and global observers currently not
     * supported */
    return;
  }

  if (sound_enable_game_music) {
    struct music_style *pms;

    stop_style_music();

    pms = music_style_by_number(client.conn.playing->music_style);

    if (pms != NULL) {
      const char *tag = NULL;

      switch (client.conn.playing->client.mood) {
      case MOOD_COUNT:
        fc_assert(client.conn.playing->client.mood != MOOD_COUNT);
        /* No break but use default tag */
      case MOOD_PEACEFUL:
        tag = pms->music_peaceful;
        break;
      case MOOD_COMBAT:
        tag = pms->music_combat;
        break;
      }

      if (tag != NULL && tag[0] != '\0') {
        log_debug("Play %s", tag);
        audio_play_music(tag, NULL);
      }
    }
  }
}

/**********************************************************************
  Stop style music completely.
***********************************************************************/
void stop_style_music(void)
{
  audio_stop();
}

/**********************************************************************
  Start menu music.
***********************************************************************/
void start_menu_music(const char *const tag, char *const alt_tag)
{
  if (sound_enable_menu_music) {
    audio_play_music(tag, alt_tag);
  }
}

/**********************************************************************
  Stop menu music completely.
***********************************************************************/
void stop_menu_music(void)
{
  audio_stop();
}
