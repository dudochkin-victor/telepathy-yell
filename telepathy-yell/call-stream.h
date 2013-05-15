/*
 * call-stream.h - Header for TpyCallStream
 * Copyright © 2009–2011 Collabora Ltd.
 * @author Sjoerd Simons <sjoerd.simons@collabora.co.uk>
 * @author Will Thompson <will.thompson@collabora.co.uk>
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

#ifndef TPY_CALL_STREAM_H
#define TPY_CALL_STREAM_H

#include <glib-object.h>

#include <telepathy-glib/telepathy-glib.h>

#include <telepathy-yell/enums.h>

G_BEGIN_DECLS

typedef struct _TpyCallStream TpyCallStream;
typedef struct _TpyCallStreamPrivate TpyCallStreamPrivate;
typedef struct _TpyCallStreamClass TpyCallStreamClass;

struct _TpyCallStreamClass {
    TpProxyClass parent_class;
};

struct _TpyCallStream {
    TpProxy parent;

    TpyCallStreamPrivate *priv;
};

GType tpy_call_stream_get_type (void);

/* TYPE MACROS */
#define TPY_TYPE_CALL_STREAM \
  (tpy_call_stream_get_type ())
#define TPY_CALL_STREAM(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), TPY_TYPE_CALL_STREAM, TpyCallStream))
#define TPY_CALL_STREAM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), TPY_TYPE_CALL_STREAM, \
    TpyCallStreamClass))
#define TPY_IS_CALL_STREAM(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), TPY_TYPE_CALL_STREAM))
#define TPY_IS_CALL_STREAM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), TPY_TYPE_CALL_STREAM))
#define TPY_CALL_STREAM_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), TPY_TYPE_CALL_STREAM, \
    TpyCallStreamClass))

TpySendingState tpy_call_stream_get_local_sending_state (
    TpyCallStream *self);

void tpy_call_stream_set_sending_async (TpyCallStream *self,
    gboolean send,
    GAsyncReadyCallback callback,
    gpointer user_data);
gboolean tpy_call_stream_set_sending_finish (TpyCallStream *self,
    GAsyncResult *result,
    GError **error);

void tpy_call_stream_request_receiving_async (TpyCallStream *self,
    TpHandle handle,
    gboolean receiving,
    GAsyncReadyCallback callback,
    gpointer user_data);
gboolean tpy_call_stream_request_receiving_finish (TpyCallStream *self,
    GAsyncResult *result,
    GError **error);

G_END_DECLS

#endif
