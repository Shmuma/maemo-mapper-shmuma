/*
 * Copyright (C) 2006, 2007 John Costigan.
 *
 * POI and GPS-Info code originally written by Cezary Jackiewicz.
 *
 * Default map data provided by http://www.openstreetmap.org/
 *
 * This file is part of Maemo Mapper.
 *
 * Maemo Mapper is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Maemo Mapper is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Maemo Mapper.  If not, see <http://www.gnu.org/licenses/>.
 */


#define _GNU_SOURCE

#include "marshal.h"

#include    <glib-object.h>

G_BEGIN_DECLS

void
g_cclosure_user_marshal_VOID__STRING_STRING_POINTER_UCHAR_UINT (GClosure     *closure,
                                                                GValue       *return_value,
                                                                guint         n_param_values,
                                                                const GValue *param_values,
                                                                gpointer      invocation_hint,
                                                                gpointer      marshal_data)
{
  typedef void (*GMarshalFunc_VOID__STRING_STRING_POINTER_UCHAR_UINT) (gpointer     data1,
                                                                       gpointer     arg_1,
                                                                       gpointer     arg_2,
                                                                       gpointer     arg_3,
                                                                       guchar       arg_4,
                                                                       guint        arg_5,
                                                                       gpointer     data2);
  register GMarshalFunc_VOID__STRING_STRING_POINTER_UCHAR_UINT callback;
  register GCClosure *cc = (GCClosure*) closure;
  register gpointer data1, data2;

  g_return_if_fail (n_param_values == 6);

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      data1 = closure->data;
      data2 = g_value_peek_pointer (param_values + 0);
    }
  else
    {
      data1 = g_value_peek_pointer (param_values + 0);
      data2 = closure->data;
    }
  callback = (GMarshalFunc_VOID__STRING_STRING_POINTER_UCHAR_UINT) (marshal_data ? marshal_data : cc->callback);

  callback (data1,
            g_marshal_value_peek_string (param_values + 1),
            g_marshal_value_peek_string (param_values + 2),
            g_marshal_value_peek_pointer (param_values + 3),
            g_marshal_value_peek_uchar (param_values + 4),
            g_marshal_value_peek_uint (param_values + 5),
            data2);
}

G_END_DECLS

