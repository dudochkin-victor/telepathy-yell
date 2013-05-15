/*
 * call-channel.c - Source for TpyCallChannel
 * Copyright (C) 2011 Collabora Ltd. <http://www.collabora.co.uk/>
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

/**
 * SECTION:call-channel
 * @title: TpyCallChannel
 * @short_description: help class for a call channel
 *
 * #TpyCallChannel is a sub-class of #TpChannel providing convenient API
 * to make audio and video calls.
 *
 * Since:
 */

/**
 * TpyCallChannel:
 *
 * Data structure representing a #TpyCallChannel.
 *
 * Since:
 */

/**
 * TpyCallChannelClass:
 *
 * The class of a #TpyCallChannel.
 *
 * Since:
 */

#include <config.h>

#include "telepathy-yell/call-channel.h"
#include "telepathy-yell/call-stream.h"

#include <telepathy-glib/dbus.h>
#include <telepathy-glib/enums.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/proxy-subclass.h>
#include <telepathy-glib/util.h>

#include "extensions.h"
#include "interfaces.h"
#include "_gen/signals-marshal.h"

#define DEBUG_FLAG TPY_DEBUG_CALL
#include "debug.h"

G_DEFINE_TYPE (TpyCallChannel, tpy_call_channel, TP_TYPE_CHANNEL)

struct _TpyCallChannelPrivate
{
  TpyCallState state;
  TpyCallFlags flags;
  GHashTable *details;

  gboolean initial_audio;
  gboolean initial_video;

  /* Hash of Handle => CallMemberFlags */
  GHashTable *members;

  /* Array of TpyCallContents */
  GPtrArray *contents;

  GSimpleAsyncResult *result;

  gboolean properties_retrieved;
  gboolean ready;
};

enum /* props */
{
  PROP_CONTENTS = 1,
  PROP_STATE,
  PROP_STATE_DETAILS,
  PROP_STATE_REASON,
  PROP_FLAGS,
  PROP_HARDWARE_STREAMING,
  PROP_MEMBERS,
  PROP_INITIAL_TRANSPORT,
  PROP_INITIAL_AUDIO,
  PROP_INITIAL_AUDIO_NAME,
  PROP_INITIAL_VIDEO,
  PROP_INITIAL_VIDEO_NAME,
  PROP_MUTABLE_CONTENTS,
  PROP_READY
};

enum /* signals */
{
  CONTENT_ADDED,
  CONTENT_REMOVED,
  STATE_CHANGED,
  MEMBERS_CHANGED,
  LAST_SIGNAL
};

static guint _signals[LAST_SIGNAL] = { 0, };

static void
maybe_go_to_ready (TpyCallChannel *self)
{
  TpyCallChannelPrivate *priv = self->priv;
  guint i;

  if (priv->ready)
    return;

  if (!priv->properties_retrieved)
    return;

  for (i = 0 ; i < priv->contents->len; i++)
    {
      TpyCallContent *c = g_ptr_array_index (self->priv->contents, i);
      gboolean ready;

      g_object_get (c, "ready", &ready, NULL);

      if (!ready)
        return;
    }

  priv->ready = TRUE;
  g_object_notify (G_OBJECT (self), "ready");
}

static void
on_content_ready_cb (TpyCallContent *content,
  GParamSpec *spec,
  TpyCallChannel *self)
{
  maybe_go_to_ready (self);
}

static void
on_content_added_cb (TpProxy *proxy,
    const gchar *content_path,
    gpointer user_data,
    GObject *weak_object)
{
  TpyCallChannel *self = TPY_CALL_CHANNEL (proxy);
  TpyCallContent *content;

  DEBUG ("Content added: %s", content_path);

  content = g_object_new (TPY_TYPE_CALL_CONTENT,
          "bus-name", tp_proxy_get_bus_name (self),
          "dbus-daemon", tp_proxy_get_dbus_daemon (self),
          "dbus-connection", tp_proxy_get_dbus_connection (self),
          "object-path", content_path,
          NULL);

  if (content == NULL)
    {
      g_warning ("Could not create a CallContent for path %s", content_path);
      return;
    }

  g_ptr_array_add (self->priv->contents, content);
  tp_g_signal_connect_object (content, "notify::ready",
    G_CALLBACK (on_content_ready_cb), self, 0);

  g_signal_emit (self, _signals[CONTENT_ADDED], 0, content);
}

static void
on_content_removed_cb (TpProxy *proxy,
    const gchar *content_path,
    gpointer user_data,
    GObject *weak_object)
{
  TpyCallChannel *self = TPY_CALL_CHANNEL (proxy);
  TpyCallContent *content = NULL, *c;
  guint i;

  DEBUG ("Content removed: %s", content_path);

  for (i = 0; i < self->priv->contents->len; i++)
    {
      c = g_ptr_array_index (self->priv->contents, i);
      if (g_strcmp0 (tp_proxy_get_object_path (c), content_path) == 0)
        {
          content = c;
          break;
        }
    }

  if (content != NULL)
    {
      g_signal_emit (self, _signals[CONTENT_REMOVED], 0, content);
      g_ptr_array_remove (self->priv->contents, content);
    }
  else
    {
      g_warning ("The removed content '%s' isn't in the call!", content_path);
    }
}

static void
on_call_members_changed_cb (TpProxy *proxy,
    GHashTable *flags_changed,
    const GArray *removed,
    gpointer user_data,
    GObject *weak_object)
{
  TpyCallChannel *self = TPY_CALL_CHANNEL (proxy);

  DEBUG ("Call members changed: %d members",
      g_hash_table_size (flags_changed));

  g_hash_table_unref (self->priv->members);
  self->priv->members = g_hash_table_ref (flags_changed);

  g_signal_emit (self, _signals[MEMBERS_CHANGED], 0, self->priv->members);
}

static void
on_call_state_changed_cb (TpProxy *proxy,
    guint call_state,
    guint call_flags,
    const GValueArray *call_state_reason,
    GHashTable *call_state_details,
    gpointer user_data,
    GObject *weak_object)
{
  TpyCallChannel *self = TPY_CALL_CHANNEL (proxy);

  DEBUG ("Call state changed");

  self->priv->state = call_state;
  self->priv->flags = call_flags;

  tp_clear_pointer (&self->priv->details, g_hash_table_unref);
  self->priv->details = g_hash_table_ref (call_state_details);

  g_signal_emit (self, _signals[STATE_CHANGED], 0,
      call_state, call_flags, call_state_reason, call_state_details);
}

static void
on_call_channel_get_all_properties_cb (TpProxy *proxy,
    GHashTable *properties,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  TpyCallChannel *self = TPY_CALL_CHANNEL (proxy);
  GHashTable *hash_table;
  GPtrArray *contents;
  guint i;

  if (error != NULL)
    {
      g_warning ("Could not get the channel properties: %s", error->message);
      return;
    }

  self->priv->state = tp_asv_get_uint32 (properties,
      "CallState", NULL);
  self->priv->flags = tp_asv_get_uint32 (properties,
      "CallFlags", NULL);
  self->priv->initial_audio = tp_asv_get_boolean (properties,
      "InitialAudio", NULL);
  self->priv->initial_video = tp_asv_get_boolean (properties,
      "InitialVideo", NULL);

  hash_table = tp_asv_get_boxed (properties,
      "CallStateDetails", TP_HASH_TYPE_STRING_VARIANT_MAP);
  if (hash_table != NULL)
    self->priv->details = g_boxed_copy (TP_HASH_TYPE_STRING_VARIANT_MAP,
        hash_table);

  hash_table = tp_asv_get_boxed (properties,
      "CallMembers", TPY_HASH_TYPE_CALL_MEMBER_MAP);
  if (hash_table != NULL)
    self->priv->members = g_boxed_copy (TPY_HASH_TYPE_CALL_MEMBER_MAP,
        hash_table);

  contents = tp_asv_get_boxed (properties,
      "Contents", TP_ARRAY_TYPE_OBJECT_PATH_LIST);

  for (i = 0; i < contents->len; i++)
    {
      const gchar *content_path = g_ptr_array_index (contents, i);
      TpyCallContent *content;

      DEBUG ("Content added: %s", content_path);

      content = g_object_new (TPY_TYPE_CALL_CONTENT,
              "bus-name", tp_proxy_get_bus_name (self),
              "dbus-daemon", tp_proxy_get_dbus_daemon (self),
              "dbus-connection", tp_proxy_get_dbus_connection (self),
              "object-path", content_path,
              NULL);

      if (content == NULL)
        {
          g_warning ("Could not create a CallContent for path %s", content_path);
          return;
        }

      g_ptr_array_add (self->priv->contents, content);

      tp_g_signal_connect_object (content, "notify::ready",
        G_CALLBACK (on_content_ready_cb), self, 0);
    }

  g_signal_emit (self, _signals[MEMBERS_CHANGED], 0, self->priv->members);

  self->priv->properties_retrieved = TRUE;

  maybe_go_to_ready (self);
}

static void
tpy_call_channel_dispose (GObject *obj)
{
  TpyCallChannel *self = (TpyCallChannel *) obj;

  tp_clear_pointer (&self->priv->contents, g_ptr_array_unref);
  tp_clear_pointer (&self->priv->details, g_hash_table_unref);

  tp_clear_object (&self->priv->result);

  G_OBJECT_CLASS (tpy_call_channel_parent_class)->dispose (obj);
}

static void
tpy_call_channel_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  TpyCallChannel *self = (TpyCallChannel *) object;

  switch (property_id)
    {
      case PROP_CONTENTS:
        g_value_set_boxed (value, self->priv->contents);
        break;

      case PROP_STATE:
        g_value_set_uint (value, self->priv->state);
        break;

      case PROP_FLAGS:
        g_value_set_uint (value, self->priv->flags);
        break;

      case PROP_STATE_DETAILS:
        g_value_set_boxed (value, self->priv->details);
        break;

      case PROP_MEMBERS:
        g_value_set_boxed (value, self->priv->members);
        break;

      case PROP_INITIAL_AUDIO:
        g_value_set_boolean (value, self->priv->initial_audio);
        break;

      case PROP_INITIAL_VIDEO:
        g_value_set_boolean (value, self->priv->initial_video);
        break;

      case PROP_READY:
        g_value_set_boolean (value, self->priv->ready);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
tpy_call_channel_constructed (GObject *obj)
{
  TpyCallChannel *self = (TpyCallChannel *) obj;
  TpChannel *chan = (TpChannel *) obj;
  GError *err = NULL;

  ((GObjectClass *) tpy_call_channel_parent_class)->constructed (obj);

  if (tp_channel_get_channel_type_id (chan) !=
      TPY_IFACE_QUARK_CHANNEL_TYPE_CALL)
    {
      GError error = { TP_DBUS_ERRORS, TP_DBUS_ERROR_INCONSISTENT,
          "Channel is not a Call" };

      DEBUG ("Channel is not a Call: %s", tp_channel_get_channel_type (chan));

      tp_proxy_invalidate (TP_PROXY (self), &error);
      return;
    }

  tpy_cli_channel_type_call_connect_to_content_added (TP_PROXY (self),
      on_content_added_cb, NULL, NULL, NULL, &err);

  if (err != NULL)
    {
      g_critical ("Failed to connect to ContentAdded signal: %s",
          err->message);

      g_error_free (err);
      return;
    }

  tpy_cli_channel_type_call_connect_to_content_removed (TP_PROXY (self),
      on_content_removed_cb, NULL, NULL, NULL, &err);

  if (err != NULL)
    {
      g_critical ("Failed to connect to ContentRemoved signal: %s",
          err->message);

      g_error_free (err);
      return;
    }

  tpy_cli_channel_type_call_connect_to_call_state_changed (TP_PROXY (self),
      on_call_state_changed_cb, NULL, NULL, NULL, &err);

  if (err != NULL)
    {
      g_critical ("Failed to connect to CallStateChanged signal: %s",
          err->message);

      g_error_free (err);
      return;
    }

  tpy_cli_channel_type_call_connect_to_call_members_changed (TP_PROXY (self),
      on_call_members_changed_cb, NULL, NULL, NULL, &err);

  if (err != NULL)
    {
      g_critical ("Failed to connect to CallMembersChanged signal: %s",
          err->message);

      g_error_free (err);
      return;
    }

  tp_cli_dbus_properties_call_get_all (self, -1,
      TPY_IFACE_CHANNEL_TYPE_CALL,
      on_call_channel_get_all_properties_cb, NULL, NULL, NULL);
}

static void
tpy_call_channel_class_init (TpyCallChannelClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GParamSpec *param_spec;

  gobject_class->constructed = tpy_call_channel_constructed;
  gobject_class->get_property = tpy_call_channel_get_property;
  gobject_class->dispose = tpy_call_channel_dispose;

  g_type_class_add_private (klass, sizeof (TpyCallChannelPrivate));

  /**
   * TpyCallChannel:contents:
   *
   * The list of content objects that are part of this call.
   *
   * Since:
   */
  param_spec = g_param_spec_boxed ("contents", "Contents",
      "The content objects of this call",
      G_TYPE_PTR_ARRAY,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (gobject_class, PROP_CONTENTS, param_spec);

  /**
   * TpyCallChannel:state:
   *
   * A #TpChannelCallState specifying the state of the call.
   *
   * Since:
   */
  param_spec = g_param_spec_uint ("state", "Call state",
      "The state of the call",
      0, G_MAXUINT, 0,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (gobject_class, PROP_STATE, param_spec);

  /**
   * TpyCallChannel:flags:
   *
   * A #TpChannelCallFlags specifying the flags of the call.
   *
   * Since:
   */
  param_spec = g_param_spec_uint ("flags", "Call flags",
      "The flags for the call",
      0, G_MAXUINT, 0,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (gobject_class, PROP_FLAGS, param_spec);

  /**
   * TpyCallChannel:state-details:
   *
   * The list of content objects that are part of this call.
   *
   * Since:
   */
  param_spec = g_param_spec_boxed ("state-details", "State details",
      "The details of the call",
      G_TYPE_HASH_TABLE,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (gobject_class,
      PROP_STATE_DETAILS, param_spec);

  /**
   * TpyCallChannel:members:
   *
   * The call participants, and their respective flags.
   *
   * Since:
   */
  param_spec = g_param_spec_boxed ("members", "Call members",
      "The call participants",
      G_TYPE_HASH_TABLE,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (gobject_class,
      PROP_MEMBERS, param_spec);

  /**
   * TpyCallChannel:initial-audio:
   *
   * Whether or not the Call was started with audio.
   *
   * Since:
   */
  param_spec = g_param_spec_boolean ("initial-audio", "Initial audio",
      "Initial audio",
      FALSE,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (gobject_class,
      PROP_INITIAL_AUDIO, param_spec);

  /**
   * TpyCallChannel:initial-video:
   *
   * Whether or not the Call was started with video.
   *
   * Since:
   */
  param_spec = g_param_spec_boolean ("initial-video", "Initial video",
      "Initial video",
      FALSE,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (gobject_class,
      PROP_INITIAL_VIDEO, param_spec);

  /**
   * TpyCallChannel:ready:
   *
   * Whether or call channel got all its async information
   *
   * Since:
   */
  param_spec = g_param_spec_boolean ("ready", "Ready",
      "If true the call channel and all its contents have retrieved all "
      "all async information from the CM",
      FALSE,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (gobject_class, PROP_READY,
      param_spec);

  /**
   * TpyCallChannel::content-added
   * @self: the #TpyCallChannel
   * @content: the newly added content
   *
   * The ::content-added signal is emitted whenever a
   * #TpyCallContent is added to @self.
   */
  _signals[CONTENT_ADDED] = g_signal_new ("content-added",
      G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST,
      0, NULL, NULL,
      g_cclosure_marshal_VOID__OBJECT,
      G_TYPE_NONE,
      1, G_TYPE_OBJECT);

  /**
   * TpyCallChannel::content-removed
   * @self: the #TpyCallChannel
   * @content: the newly removed content
   *
   * The ::content-removed signal is emitted whenever a
   * #TpyCallContent is removed from @self.
   */
  _signals[CONTENT_REMOVED] = g_signal_new ("content-removed",
      G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST,
      0, NULL, NULL,
      g_cclosure_marshal_VOID__OBJECT,
      G_TYPE_NONE,
      1, G_TYPE_OBJECT);

  /**
   * TpyCallChannel::state-changed
   * @self: the #TpyCallChannel
   * @state: the new #TpyCallState
   * @flags: the new #TpyCallFlags
   * @state_reason: the reason for the change
   * @state_details: additional details
   *
   * The ::state-changed signal is emitted whenever the
   * call state changes.
   */
  _signals[STATE_CHANGED] = g_signal_new ("state-changed",
      G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST,
      0, NULL, NULL,
      _tpy_marshal_VOID__UINT_UINT_BOXED_BOXED,
      G_TYPE_NONE,
      4, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_PTR_ARRAY, G_TYPE_HASH_TABLE);

  /**
   * TpyCallChannel::members-changed
   * @self: the #TpyCallChannel
   * @state: the new #TpyCallState
   * @flags: the new #TpyCallFlags
   *
   * The ::members-changed signal is emitted whenever participants
   * are added, removed, or their flags change.
   */
  _signals[MEMBERS_CHANGED] = g_signal_new ("members-changed",
      G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST,
      0, NULL, NULL,
      g_cclosure_marshal_VOID__BOXED,
      G_TYPE_NONE,
      1, G_TYPE_HASH_TABLE);

}

static void
tpy_call_channel_init (TpyCallChannel *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TPY_TYPE_CALL_CHANNEL, TpyCallChannelPrivate);

  self->priv->contents = g_ptr_array_new_with_free_func (g_object_unref);
}

/**
 * tpy_call_channel_new:
 * @conn: a #TpConnection; may not be %NULL
 * @object_path: the object path of the channel; may not be %NULL
 * @immutable_properties: (transfer none) (element-type utf8 GObject.Value):
 *  the immutable properties of the channel,
 *  as signalled by the NewChannel D-Bus signal or returned by the
 *  CreateChannel and EnsureChannel D-Bus methods: a mapping from
 *  strings (D-Bus interface name + "." + property name) to #GValue instances
 * @error: used to indicate the error if %NULL is returned
 *
 * Convenient function to create a new #TpyCallChannel
 *
 * Returns: (transfer full): a newly created #TpyCallChannel
 *
 * Since:
 */
TpyCallChannel *
tpy_call_channel_new (TpConnection *conn,
    const gchar *object_path,
    const GHashTable *immutable_properties,
    GError **error)
{
  TpProxy *conn_proxy = (TpProxy *) conn;

  g_return_val_if_fail (TP_IS_CONNECTION (conn), NULL);
  g_return_val_if_fail (object_path != NULL, NULL);
  g_return_val_if_fail (immutable_properties != NULL, NULL);

  if (!tp_dbus_check_valid_object_path (object_path, error))
    return NULL;

  return g_object_new (TPY_TYPE_CALL_CHANNEL,
      "connection", conn,
       "dbus-daemon", conn_proxy->dbus_daemon,
       "bus-name", conn_proxy->bus_name,
       "object-path", object_path,
       "handle-type", (guint) TP_UNKNOWN_HANDLE_TYPE,
       "channel-properties", immutable_properties,
       NULL);
}

static void
channel_accept_cb (TpProxy *proxy,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  TpyCallChannel *self = TPY_CALL_CHANNEL (proxy);

  if (error != NULL)
    {
      DEBUG ("Failed to accept call: %s", error->message);

      g_simple_async_result_set_from_error (self->priv->result, error);
    }

  g_simple_async_result_set_op_res_gboolean (self->priv->result, TRUE);
  g_simple_async_result_complete (self->priv->result);
  tp_clear_object (&self->priv->result);
}

/**
 * tpy_call_channel_accept_async:
 * @self: an incoming #TpyCallChannel
 * @callback: a callback to call
 * @user_data: data to pass to @callback
 *
 * Accept an incoming call. When the call has been accepted, @callback
 * will be called. You can then call tpy_call_channel_accept_finish()
 * to finish the operation.
 *
 * Since:
 */
void
tpy_call_channel_accept_async (TpyCallChannel *self,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  g_return_if_fail (TPY_IS_CALL_CHANNEL (self));
  g_return_if_fail (self->priv->result == NULL);

  self->priv->result = g_simple_async_result_new (G_OBJECT (self), callback,
      user_data, tpy_call_channel_accept_async);

  tpy_cli_channel_type_call_call_accept (TP_PROXY (self), -1,
      channel_accept_cb, NULL, NULL, G_OBJECT (self));
}

/**
 * tpy_call_channel_accept_finish:
 * @self: a #TpyCallChannel
 * @result: a #GAsyncResult
 * @error: a #GError to fill
 *
 * Finishes to accept a call.
 *
 * Since:
 */
gboolean
tpy_call_channel_accept_finish (TpyCallChannel *self,
    GAsyncResult *result,
    GError **error)
{
  if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result),
      error))
    return FALSE;

  g_return_val_if_fail (g_simple_async_result_is_valid (result,
    G_OBJECT (self), tpy_call_channel_accept_async),
    FALSE);

  return g_simple_async_result_get_op_res_gboolean (
    G_SIMPLE_ASYNC_RESULT (result));
}

static void
channel_hangup_cb (TpProxy *proxy,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  TpyCallChannel *self = TPY_CALL_CHANNEL (proxy);

  if (error != NULL)
    {
      DEBUG ("Failed to hang up: %s", error->message);

      g_simple_async_result_set_from_error (self->priv->result, error);
    }

  g_simple_async_result_complete (self->priv->result);
  tp_clear_object (&self->priv->result);
}

void
tpy_call_channel_hangup_async (TpyCallChannel *self,
    TpyCallStateChangeReason reason,
    gchar *detailed_reason,
    gchar *message,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  g_return_if_fail (TPY_IS_CALL_CHANNEL (self));
  g_return_if_fail (self->priv->result == NULL);

  self->priv->result = g_simple_async_result_new (G_OBJECT (self), callback,
      user_data, tpy_call_channel_accept_async);

  tpy_cli_channel_type_call_call_hangup (TP_PROXY (self), -1,
      reason, detailed_reason, message,
      channel_hangup_cb, NULL, NULL, G_OBJECT (self));
}

void
tpy_call_channel_hangup_finish (TpyCallChannel *self,
    GAsyncResult *result,
    GError **error)
{
}

TpyCallState
tpy_call_channel_get_state (TpyCallChannel *self,
    TpyCallFlags *flags, GHashTable **details)
{
  g_return_val_if_fail (TPY_IS_CALL_CHANNEL (self), TPY_CALL_STATE_UNKNOWN);

  if (flags != NULL)
    *flags = self->priv->flags;

  if (details != NULL)
    {
      if (self->priv->details != NULL)
        g_hash_table_ref (self->priv->details);

      *details = self->priv->details;
    }
  return self->priv->state;
}

gboolean
tpy_call_channel_has_initial_video (TpyCallChannel *self)
{
  g_return_val_if_fail (TPY_IS_CALL_CHANNEL (self), FALSE);

  return self->priv->initial_video;
}

gboolean
tpy_call_channel_has_initial_audio (TpyCallChannel *self)
{
  g_return_val_if_fail (TPY_IS_CALL_CHANNEL (self), FALSE);

  return self->priv->initial_audio;
}

void
tpy_call_channel_send_video (TpyCallChannel *self,
    gboolean send)
{
  gboolean found = FALSE;
  guint i;

  g_return_if_fail (TPY_IS_CALL_CHANNEL (self));

  /* Loop over all the contents, if some of them a video set all their
   * streams to sending, otherwise request a video channel in case we want to
   * sent */
  for (i = 0 ; i < self->priv->contents->len ; i++)
    {
      TpyCallContent *content = g_ptr_array_index (self->priv->contents, i);

      if (tpy_call_content_get_media_type (content)
          == TP_MEDIA_STREAM_TYPE_VIDEO)
        {
          GList *l;
          found = TRUE;

          for (l = tpy_call_content_get_streams (content);
              l != NULL ; l = g_list_next (l))
            {
              TpyCallStream *stream = TPY_CALL_STREAM (l->data);
              tpy_call_stream_set_sending_async (stream,
                send, NULL, NULL);
            }
        }
    }

  if (send && !found)
    tpy_cli_channel_type_call_call_add_content (TP_PROXY (self), -1,
        "video", TP_MEDIA_STREAM_TYPE_VIDEO,
        NULL, NULL, NULL, NULL);
}

TpySendingState
tpy_call_channel_get_video_state (TpyCallChannel *self)
{
  TpySendingState result = TPY_SENDING_STATE_NONE;
  guint i;

  g_return_val_if_fail (TPY_IS_CALL_CHANNEL (self), TPY_SENDING_STATE_NONE);

  for (i = 0 ; i < self->priv->contents->len ; i++)
    {
      TpyCallContent *content = g_ptr_array_index (self->priv->contents, i);

      if (tpy_call_content_get_media_type (content)
          == TP_MEDIA_STREAM_TYPE_VIDEO)
        {
          GList *l;

          for (l = tpy_call_content_get_streams (content);
              l != NULL ; l = g_list_next (l))
            {
              TpyCallStream *stream = TPY_CALL_STREAM (l->data);
              TpySendingState state;

              g_object_get (stream, "local-sending-state", &state, NULL);
              if (state != TPY_SENDING_STATE_PENDING_STOP_SENDING &&
                  state > result)
                result = state;
            }
        }
    }

  return result;
}


gboolean
tpy_call_channel_has_dtmf (TpyCallChannel *self)
{
  g_return_val_if_fail (TPY_IS_CALL_CHANNEL (self), FALSE);

  return tp_proxy_has_interface_by_id (self,
      TP_IFACE_QUARK_CHANNEL_INTERFACE_DTMF);
}

static void
on_dtmf_tone_cb (TpChannel *proxy,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  if (error)
    DEBUG ("Error %s: %s", (gchar *) user_data, error->message);
}

void
tpy_call_channel_dtmf_start_tone (TpyCallChannel *self,
    TpDTMFEvent event)
{
  g_return_if_fail (TPY_IS_CALL_CHANNEL (self));

  tp_cli_channel_interface_dtmf_call_start_tone (TP_CHANNEL (self), -1, 0,
      event,
      on_dtmf_tone_cb, "starting tone", NULL, G_OBJECT (self));
}

void
tpy_call_channel_dtmf_stop_tone (TpyCallChannel *self)
{
  g_return_if_fail (TPY_IS_CALL_CHANNEL (self));

  tp_cli_channel_interface_dtmf_call_stop_tone (TP_CHANNEL (self), -1, 0,
      on_dtmf_tone_cb, "stoping tone", NULL, G_OBJECT (self));
}
