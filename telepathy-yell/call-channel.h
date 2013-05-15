/*
 * call-channel.h - Header for TpyCallChannel
 * Copyright (C) 2010-2011 Collabora Ltd. <http://www.collabora.co.uk/>
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

#ifndef __TPY_CALL_CHANNEL_H__
#define __TPY_CALL_CHANNEL_H__

#include <telepathy-glib/channel.h>

#include "enums.h"
#include "call-content.h"

G_BEGIN_DECLS

#define TPY_TYPE_CALL_CHANNEL (tpy_call_channel_get_type ())
#define TPY_CALL_CHANNEL(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), TPY_TYPE_CALL_CHANNEL, TpyCallChannel))
#define TPY_CALL_CHANNEL_CLASS(obj) (G_TYPE_CHECK_CLASS_CAST ((obj), TPY_TYPE_CALL_CHANNEL, TpyCallChannelClass))
#define TPY_IS_CALL_CHANNEL(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TPY_TYPE_CALL_CHANNEL))
#define TPY_IS_CALL_CHANNEL_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE ((obj), TPY_TYPE_CALL_CHANNEL))
#define TPY_CALL_CHANNEL_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), TPY_TYPE_CALL_CHANNEL, TpyCallChannelClass))

typedef struct _TpyCallChannel TpyCallChannel;
typedef struct _TpyCallChannelClass TpyCallChannelClass;
typedef struct _TpyCallChannelPrivate TpyCallChannelPrivate;

struct _TpyCallChannel
{
  /*<private>*/
  TpChannel parent;
  TpyCallChannelPrivate *priv;
};

struct _TpyCallChannelClass
{
  /*<private>*/
  TpChannelClass parent_class;
  GCallback _padding[7];
};

GType tpy_call_channel_get_type (void);

TpyCallChannel *tpy_call_channel_new (TpConnection *conn,
    const gchar *object_path,
    const GHashTable *immutable_properties,
    GError **error);

void tpy_call_channel_accept_async (TpyCallChannel *self,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean tpy_call_channel_accept_finish (TpyCallChannel *self,
    GAsyncResult *result,
    GError **error);

void tpy_call_channel_hangup_async (TpyCallChannel *self,
    TpyCallStateChangeReason reason,
    gchar *detailed_reason,
    gchar *message,
    GAsyncReadyCallback callback,
    gpointer user_data);

void tpy_call_channel_hangup_finish (TpyCallChannel *self,
    GAsyncResult *result,
    GError **error);

void tpy_call_channel_set_ringing_async (TpyCallChannel *self,
    GAsyncReadyCallback callback,
    gpointer user_data);

void tpy_call_channel_set_ringing_finish (TpyCallChannel *self,
    GAsyncResult *result,
    GError **error);

void tpy_call_channel_add_content_async (TpyCallChannel *self,
    gchar *name,
    TpMediaStreamType type,
    GAsyncReadyCallback callback,
    gpointer user_data);

TpyCallContent *tpy_call_channel_add_content_finish (TpyCallChannel *self,
    GAsyncResult *result,
    GError **error);

void tpy_call_channel_send_video (TpyCallChannel *self,
    gboolean send);

TpySendingState tpy_call_channel_get_video_state (TpyCallChannel *self);

TpyCallState tpy_call_channel_get_state (TpyCallChannel *self,
    TpyCallFlags *flags, GHashTable **details);

gboolean tpy_call_channel_has_initial_audio (TpyCallChannel *self);
gboolean tpy_call_channel_has_initial_video (TpyCallChannel *self);

gboolean tpy_call_channel_has_dtmf (TpyCallChannel *self);

void tpy_call_channel_dtmf_start_tone (TpyCallChannel *self,
    TpDTMFEvent event);

void tpy_call_channel_dtmf_stop_tone (TpyCallChannel *self);

G_END_DECLS

#endif
