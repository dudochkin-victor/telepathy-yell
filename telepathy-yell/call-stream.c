/*
 * call-stream.c - Source for TpyCallStream
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

#include "call-stream.h"

#define DEBUG_FLAG TPY_DEBUG_CALL
#include "debug.h"

#include <telepathy-yell/interfaces.h>
#include <telepathy-yell/gtypes.h>
#include <telepathy-yell/cli-call.h>

G_DEFINE_TYPE(TpyCallStream, tpy_call_stream, TP_TYPE_PROXY)

enum
{
  PROP_REMOTE_MEMBERS = 1,
  PROP_LOCAL_SENDING_STATE,
  PROP_CAN_REQUEST_RECEIVING,
  PROP_READY,
};

struct _TpyCallStreamPrivate
{
  GHashTable *remote_members;
  TpySendingState local_sending_state;
  gboolean can_request_receiving;
  gboolean ready;

  GSimpleAsyncResult *result;
};

static void
on_call_stream_get_all_properties_cb (TpProxy *proxy,
    GHashTable *properties,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  TpyCallStream *self = TPY_CALL_STREAM (proxy);
  GHashTable *members;

  if (error != NULL)
    {
      g_warning ("Could not get the stream properties: %s", error->message);
      return;
    }

  self->priv->local_sending_state = tp_asv_get_uint32 (properties,
      "LocalSendingState", NULL);
  self->priv->can_request_receiving = tp_asv_get_boolean (properties,
      "CanRequestReceiving", NULL);

  tp_clear_pointer (&self->priv->remote_members, g_hash_table_unref);
  members = tp_asv_get_boxed (properties,
      "RemoteMembers", TPY_HASH_TYPE_CALL_MEMBER_MAP);
  if (members != NULL)
    self->priv->remote_members =
        g_boxed_copy (TPY_HASH_TYPE_CALL_MEMBER_MAP, members);

  self->priv->ready = TRUE;
  g_object_notify (G_OBJECT (self), "ready");
}

static void
on_remote_members_changed_cb (TpProxy *proxy,
    GHashTable *updates,
    const GArray *removed,
    gpointer user_data,
    GObject *weak_object)
{
  TpyCallStream *self = TPY_CALL_STREAM (proxy);
  GHashTableIter iter;
  gpointer key, value;
  guint i;

  for (i = 0; i < removed->len; i++)
    g_hash_table_remove (self->priv->remote_members,
        GUINT_TO_POINTER (g_array_index (removed, TpHandle, i)));

  g_hash_table_iter_init (&iter, updates);
  while (g_hash_table_iter_next (&iter, &key, &value))
    g_hash_table_insert (self->priv->remote_members, key, value);

  g_object_notify (G_OBJECT (self), "remote-members");
}

static void
on_local_sending_state_changed_cb (TpProxy *proxy,
    guint state,
    gpointer user_data,
    GObject *weak_object)
{
  TpyCallStream *self = TPY_CALL_STREAM (proxy);

  if (self->priv->local_sending_state == state)
    return;

  self->priv->local_sending_state = state;
  g_object_notify (G_OBJECT (self), "local-sending-state");
}

static void
tpy_call_stream_constructed (GObject *obj)
{
  TpyCallStream *self = (TpyCallStream *) obj;
  GError *err = NULL;

  ((GObjectClass *) tpy_call_stream_parent_class)->constructed (obj);

  tpy_cli_call_stream_connect_to_remote_members_changed (TP_PROXY (self),
      on_remote_members_changed_cb, NULL, NULL, G_OBJECT (self), &err);

  if (err != NULL)
    {
      g_critical ("Failed to connect to RemoteMembersChanged signal: %s",
          err->message);

      g_error_free (err);
      return;
    }

  tpy_cli_call_stream_connect_to_local_sending_state_changed (TP_PROXY (self),
      on_local_sending_state_changed_cb, NULL, NULL, G_OBJECT (self), &err);

  if (err != NULL)
    {
      g_critical ("Failed to connect to LocalSendingStateChanged signal: %s",
          err->message);

      g_error_free (err);
      return;
    }

  tp_cli_dbus_properties_call_get_all (self, -1,
      TPY_IFACE_CALL_STREAM,
      on_call_stream_get_all_properties_cb, NULL, NULL, G_OBJECT (self));
}

static void
tpy_call_stream_init (TpyCallStream *self)
{
  TpyCallStreamPrivate *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TPY_TYPE_CALL_STREAM, TpyCallStreamPrivate);

  self->priv = priv;
  priv->remote_members = g_hash_table_new (g_direct_hash, g_direct_equal);
}

static void
tpy_call_stream_dispose (GObject *object)
{
  TpyCallStream *self = TPY_CALL_STREAM (object);

  tp_clear_object (&self->priv->result);
  tp_clear_pointer (&self->priv->remote_members, g_hash_table_unref);

  G_OBJECT_CLASS (tpy_call_stream_parent_class)->dispose (object);
}

static void
tpy_call_stream_get_property (
    GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  TpyCallStream *self = TPY_CALL_STREAM (object);
  TpyCallStreamPrivate *priv = self->priv;

  switch (property_id)
    {
      case PROP_REMOTE_MEMBERS:
        g_value_set_boxed (value, priv->remote_members);
        break;
      case PROP_LOCAL_SENDING_STATE:
        g_value_set_uint (value, priv->local_sending_state);
        break;
      case PROP_CAN_REQUEST_RECEIVING:
        g_value_set_boolean (value, priv->can_request_receiving);
        break;
      case PROP_READY:
        g_value_set_boolean (value, priv->ready);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
tpy_call_stream_set_property (
    GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  TpyCallStream *self = TPY_CALL_STREAM (object);

  switch (property_id)
    {
      case PROP_LOCAL_SENDING_STATE:
        self->priv->local_sending_state = g_value_get_uint (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
tpy_call_stream_class_init (TpyCallStreamClass *bsc_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (bsc_class);
  TpProxyClass *proxy_class = (TpProxyClass *) bsc_class;
  GParamSpec *param_spec;

  g_type_class_add_private (bsc_class, sizeof (TpyCallStreamPrivate));

  object_class->dispose = tpy_call_stream_dispose;
  object_class->constructed = tpy_call_stream_constructed;
  object_class->set_property = tpy_call_stream_set_property;
  object_class->get_property = tpy_call_stream_get_property;

  proxy_class->interface = TPY_IFACE_QUARK_CALL_STREAM;

  param_spec = g_param_spec_boxed ("remote-members", "Remote members",
      "Remote member map",
      TPY_HASH_TYPE_CONTACT_SENDING_STATE_MAP,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_REMOTE_MEMBERS,
      param_spec);

  param_spec = g_param_spec_uint ("local-sending-state", "LocalSendingState",
      "Local sending state",
      0, NUM_TPY_SENDING_STATES, 0,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_LOCAL_SENDING_STATE,
      param_spec);

  param_spec = g_param_spec_boolean ("can-request-receiving",
      "CanRequestReceiving",
      "If true, the user can request that a remote contact starts sending on"
      "this stream.",
      FALSE,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_CAN_REQUEST_RECEIVING,
      param_spec);

  param_spec = g_param_spec_boolean ("ready",
      "Ready",
      "If true the stream has retrieved all async information from the CM",
      FALSE,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_READY,
      param_spec);
}

TpySendingState
tpy_call_stream_get_local_sending_state (
  TpyCallStream *self)
{
  return self->priv->local_sending_state;
}

static void
on_set_sending_cb (TpProxy *proxy,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  TpyCallStream *self = TPY_CALL_STREAM (proxy);

  if (error != NULL)
    {
      DEBUG ("Failed to set sending: %s", error->message);

      g_simple_async_result_set_from_error (self->priv->result, error);
    }

  g_simple_async_result_set_op_res_gboolean (self->priv->result, TRUE);
  g_simple_async_result_complete (self->priv->result);
  tp_clear_object (&self->priv->result);
}

void
tpy_call_stream_set_sending_async (TpyCallStream *self,
    gboolean send,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  g_return_if_fail (TPY_IS_CALL_STREAM (self));
  g_return_if_fail (self->priv->result == NULL);

  self->priv->result = g_simple_async_result_new (G_OBJECT (self), callback,
      user_data, tpy_call_stream_set_sending_async);

  tpy_cli_call_stream_call_set_sending (TP_PROXY (self), -1,
      send,
      on_set_sending_cb, NULL, NULL, G_OBJECT (self));
}

gboolean
tpy_call_stream_set_sending_finish (TpyCallStream *self,
    GAsyncResult *result,
    GError **error)
{
  if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result),
      error))
    return FALSE;

  g_return_val_if_fail (g_simple_async_result_is_valid (result,
    G_OBJECT (self), tpy_call_stream_set_sending_async),
    FALSE);

  return g_simple_async_result_get_op_res_gboolean (
    G_SIMPLE_ASYNC_RESULT (result));
}

static void
on_request_receiving_cb (TpProxy *proxy,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  TpyCallStream *self = TPY_CALL_STREAM (proxy);

  if (error != NULL)
    {
      DEBUG ("Failed to request receiving: %s", error->message);

      g_simple_async_result_set_from_error (self->priv->result, error);
    }

  g_simple_async_result_set_op_res_gboolean (self->priv->result, TRUE);
  g_simple_async_result_complete (self->priv->result);
  tp_clear_object (&self->priv->result);
}

void
tpy_call_stream_request_receiving_async (TpyCallStream *self,
    TpHandle handle,
    gboolean receiving,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  g_return_if_fail (TPY_IS_CALL_STREAM (self));
  g_return_if_fail (self->priv->result == NULL);

  self->priv->result = g_simple_async_result_new (G_OBJECT (self), callback,
      user_data, tpy_call_stream_request_receiving_async);

  tpy_cli_call_stream_call_request_receiving (TP_PROXY (self), -1,
      handle, receiving,
      on_request_receiving_cb, NULL, NULL, G_OBJECT (self));
}

gboolean
tpy_call_stream_request_receiving_finish (TpyCallStream *self,
    GAsyncResult *result,
    GError **error)
{
  if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result),
      error))
    return FALSE;

  g_return_val_if_fail (g_simple_async_result_is_valid (result,
    G_OBJECT (self), tpy_call_stream_request_receiving_async),
    FALSE);

  return g_simple_async_result_get_op_res_gboolean (
    G_SIMPLE_ASYNC_RESULT (result));
}
