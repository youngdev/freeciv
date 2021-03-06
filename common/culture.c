/****************************************************************************
 Freeciv - Copyright (C) 2004 - The Freeciv Team
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
****************************************************************************/

#ifdef HAVE_CONFIG_H
#include <fc_config.h>
#endif

/* common */
#include "city.h"
#include "effects.h"

#include "culture.h"

/****************************************************************************
  Return current culture score of the city.
****************************************************************************/
int city_culture(struct city *pcity)
{
  return pcity->history + get_city_bonus(pcity, EFT_PERFORMANCE);
}

/****************************************************************************
  How much history city gains this turn.
****************************************************************************/
int city_history_gain(struct city *pcity)
{
  return get_city_bonus(pcity, EFT_HISTORY);
}
