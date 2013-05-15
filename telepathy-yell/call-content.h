/*
 * call-content.h - Header for TpyCallContent
 * Copyright © 2011 Collabora Ltd.
 * @author Emilio Pozuelo Monfort <emilio.pozuelo@collabora.co.uk>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef TPY_CALL_CONTENT_H
#define TPY_CALL_CONTENT_H

#include <glib-object.h>

#include <telepathy-glib/telepathy-glib.h>

#include "enums.h"

G_BEGIN_DECLS

typedef struct _TpyCallContent TpyCallContent;
typedef struct _TpyCallContentPrivate TpyCallContentPrivate;
typedef struct _TpyCallContentClass TpyCallContentClass;

struct _TpyCallContentClass {
    TpProxyClass parent_class;
};

struct _TpyCallContent {
    TpProxy parent;

    TpyCallContentPrivate *priv;
};

GType tpy_call_content_get_type (void);

/* TYPE MACROS */
#define TPY_TYPE_CALL_CONTENT \
  (tpy_call_content_get_type ())
#define TPY_CALL_CONTENT(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), \
      TPY_TYPE_CALL_CONTENT, TpyCallContent))
#define TPY_CALL_CONTENT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), \
    TPY_TYPE_CALL_CONTENT, TpyCallContentClass))
#define TPY_IS_CALL_CONTENT(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), TPY_TYPE_CALL_CONTENT))
#define TPY_IS_CALL_CONTENT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), TPY_TYPE_CALL_CONTENT))
#define TPY_CALL_CONTENT_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
    TPY_TYPE_CALL_CONTENT, TpyCallContentClass))

const gchar *tpy_call_content_get_name (TpyCallContent *self);

TpMediaStreamType tpy_call_content_get_media_type (
    TpyCallContent *self);

TpyCallContentDisposition tpy_call_content_get_disposition (
    TpyCallContent *self);

GList *tpy_call_content_get_streams (TpyCallContent *self);

void tpy_call_content_remove_async (TpyCallContent *self,
    TpyContentRemovalReason reason,
    const gchar *detailed_removal_reason,
    const gchar *message,
    GAsyncReadyCallback callback,
    gpointer user_data);
gboolean tpy_call_content_remove_finish (TpyCallContent *self,
    GAsyncResult *result,
    GError **error);

G_END_DECLS

#endif /* #ifndef __TPY_CALL_CONTENT_H__*/
