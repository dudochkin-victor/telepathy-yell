/*
 * call-content.c - Source for TpyCallContent
 * Copyright © 2009–2011 Collabora Ltd.
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

#include <telepathy-glib/proxy-subclass.h>

#include "call-content.h"

#define DEBUG_FLAG TPY_DEBUG_CALL
#include "debug.h"

#include <telepathy-yell/call-stream.h>
#include <telepathy-yell/interfaces.h>
#include <telepathy-yell/gtypes.h>
#include <telepathy-yell/enums.h>
#include <telepathy-yell/cli-call.h>

G_DEFINE_TYPE (TpyCallContent, tpy_call_content, TP_TYPE_PROXY)

struct _TpyCallContentPrivate
{
  const gchar * const *extra_interfaces;
  gchar *name;
  TpMediaStreamType media_type;
  TpyCallContentDisposition disposition;
  GList *streams;
  gboolean ready;
  gboolean properties_retrieved;

  GSimpleAsyncResult *result;
};

enum
{
  PROP_NAME = 1,
  PROP_MEDIA_TYPE,
  PROP_DISPOSITION,
  PROP_STREAMS,
  PROP_READY
};

enum
{
  REMOVED,
  STREAMS_ADDED,
  STREAMS_REMOVED,
  LAST_SIGNAL
};

static guint _signals[LAST_SIGNAL] = { 0, };

static void on_call_content_get_all_properties_cb (TpProxy *proxy,
    GHashTable *properties,
    const GError *error,
    gpointer user_data,
    GObject *weak_object);

static gint
find_stream_for_object_path (gconstpointer a,
    gconstpointer b)
{
  return g_strcmp0 (tp_proxy_get_object_path ((gpointer) a), b);
}

static void
on_content_removed_cb (TpProxy *proxy,
    gpointer user_data,
    GObject *weak_object)
{
  TpyCallContent *self = TPY_CALL_CONTENT (proxy);

  g_signal_emit (self, _signals[REMOVED], 0);
}

static void
maybe_go_to_ready (TpyCallContent *self)
{
  TpyCallContentPrivate *priv = self->priv;
  GList *l;

  if (priv->ready)
    return;

  if (!priv->properties_retrieved)
    return;


  for (l = priv->streams; l != NULL ; l = g_list_next (l))
    {
      TpyCallStream *s = l->data;
      gboolean ready;

      g_object_get (s, "ready", &ready, NULL);

      if (!ready)
        return;
    }

  priv->ready = TRUE;
  g_object_notify (G_OBJECT (self), "ready");
}

static void
on_stream_ready_cb (TpyCallStream *stream,
  GParamSpec *spec,
  TpyCallContent *self)
{
  maybe_go_to_ready (self);
}

static void
add_streams (TpyCallContent *self, const GPtrArray *streams)
{
  GPtrArray *object_streams;
  guint i;

  object_streams = g_ptr_array_sized_new (streams->len);

  for (i = 0; i < streams->len; i++)
    {
      TpyCallStream *stream;
      const gchar *object_path;

      object_path = g_ptr_array_index (streams, i);

      stream = g_object_new (TPY_TYPE_CALL_STREAM,
          "bus-name", tp_proxy_get_bus_name (self),
          "dbus-daemon", tp_proxy_get_dbus_daemon (self),
          "dbus-connection", tp_proxy_get_dbus_connection (self),
          "object-path", object_path,
          NULL);

      if (stream == NULL)
        {
          g_warning ("Could not create a CallStream for path %s", object_path);
          continue;
        }

      tp_g_signal_connect_object (stream, "notify::ready",
        G_CALLBACK (on_stream_ready_cb), self, 0);

      self->priv->streams = g_list_prepend (self->priv->streams, stream);
      g_ptr_array_add (object_streams, stream);
    }

  g_signal_emit (self, _signals[STREAMS_ADDED], 0, object_streams);
  g_ptr_array_unref (object_streams);
}

static void
on_streams_added_cb (TpProxy *proxy,
    const GPtrArray *streams,
    gpointer user_data,
    GObject *weak_object)
{
  TpyCallContent *self = TPY_CALL_CONTENT (proxy);

  add_streams (self, streams);
}

static void
on_streams_removed_cb (TpProxy *proxy,
    const GPtrArray *streams,
    gpointer user_data,
    GObject *weak_object)
{
  TpyCallContent *self = TPY_CALL_CONTENT (proxy);
  GPtrArray *object_streams;
  guint i;

  object_streams = g_ptr_array_sized_new (streams->len);
  g_ptr_array_set_free_func (object_streams, g_object_unref);

  for (i = 0; i < streams->len; i++)
    {
      GList *s;
      const gchar *object_path;

      object_path = g_ptr_array_index (streams, i);

      s = g_list_find_custom (self->priv->streams,
          object_path,
          find_stream_for_object_path);

      if (s == NULL)
        {
          g_warning ("Could not find a CallStream for path %s", object_path);
          continue;
        }

      self->priv->streams = g_list_remove_link (self->priv->streams, s);
      g_ptr_array_add (object_streams, s->data);
    }

  g_signal_emit (self, _signals[STREAMS_REMOVED], 0, object_streams);

  g_ptr_array_unref (object_streams);
}

static void
tpy_call_content_constructed (GObject *obj)
{
  TpyCallContent *self = (TpyCallContent *) obj;
  GError *error = NULL;

  ((GObjectClass *) tpy_call_content_parent_class)->constructed (obj);

  tpy_cli_call_content_connect_to_removed (TP_PROXY (self),
      on_content_removed_cb, NULL, NULL, G_OBJECT (self), &error);

  if (error != NULL)
    {
      g_critical ("Failed to connect to Removed signal: %s",
          error->message);

      g_error_free (error);
      return;
    }

  tpy_cli_call_content_connect_to_streams_added (TP_PROXY (self),
      on_streams_added_cb, NULL, NULL, G_OBJECT (self), &error);

  if (error != NULL)
    {
      g_critical ("Failed to connect to StreamsAdded signal: %s",
          error->message);

      g_error_free (error);
      return;
    }

  tpy_cli_call_content_connect_to_streams_removed (TP_PROXY (self),
      on_streams_removed_cb, NULL, NULL, G_OBJECT (self), &error);

  if (error != NULL)
    {
      g_critical ("Failed to connect to StreamsRemoved signal: %s",
          error->message);

      g_error_free (error);
      return;
    }

  tp_cli_dbus_properties_call_get_all (self, -1,
      TPY_IFACE_CALL_CONTENT,
      on_call_content_get_all_properties_cb, NULL, NULL, G_OBJECT (self));
}

static void
tpy_call_content_init (TpyCallContent *self)
{
  TpyCallContentPrivate *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TPY_TYPE_CALL_CONTENT, TpyCallContentPrivate);

  self->priv = priv;
}

static void
tpy_call_content_dispose (GObject *object)
{
  TpyCallContent *self = TPY_CALL_CONTENT (object);

  tp_clear_pointer (&self->priv->name, g_free);
  tp_clear_object (&self->priv->result);

  g_list_free_full (self->priv->streams, g_object_unref);
  self->priv->streams = NULL;

  G_OBJECT_CLASS (tpy_call_content_parent_class)->dispose (object);
}

static void
tpy_call_content_get_property (
    GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  TpyCallContent *self = TPY_CALL_CONTENT (object);

  switch (property_id)
    {
      case PROP_NAME:
        g_value_set_string (value, self->priv->name);
        break;
      case PROP_MEDIA_TYPE:
        g_value_set_uint (value, self->priv->media_type);
        break;
      case PROP_DISPOSITION:
        g_value_set_uint (value, self->priv->disposition);
        break;
      case PROP_STREAMS:
        g_value_set_boxed (value, self->priv->streams);
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
tpy_call_content_set_property (
    GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  TpyCallContent *self = TPY_CALL_CONTENT (object);

  switch (property_id)
    {
      case PROP_NAME:
        g_assert (self->priv->name == NULL);
        self->priv->name = g_value_dup_string (value);
        break;
      case PROP_MEDIA_TYPE:
        self->priv->media_type = g_value_get_uint (value);
        break;
      case PROP_DISPOSITION:
        self->priv->disposition = g_value_get_uint (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
tpy_call_content_class_init (
    TpyCallContentClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  TpProxyClass *proxy_class = TP_PROXY_CLASS (klass);
  GParamSpec *param_spec;

  g_type_class_add_private (klass, sizeof (TpyCallContentPrivate));

  object_class->constructed = tpy_call_content_constructed;
  object_class->dispose = tpy_call_content_dispose;
  object_class->get_property = tpy_call_content_get_property;
  object_class->set_property = tpy_call_content_set_property;

  proxy_class->interface = TPY_IFACE_QUARK_CALL_CONTENT;

  param_spec = g_param_spec_string ("name", "Name",
      "The name of this content, if any",
      "",
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_NAME, param_spec);

  param_spec = g_param_spec_uint ("media-type", "Media Type",
      "The media type of this content",
      0, G_MAXUINT, 0,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_MEDIA_TYPE, param_spec);

  param_spec = g_param_spec_uint ("disposition", "Disposition",
      "The disposition of this content",
      0, G_MAXUINT, 0,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_DISPOSITION, param_spec);

  param_spec = g_param_spec_boxed ("streams", "Stream",
      "The streams of this content",
      TP_ARRAY_TYPE_OBJECT_PATH_LIST,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_STREAMS,
      param_spec);

  param_spec = g_param_spec_boolean ("ready",
      "Ready",
      "If true the content and all its streams have retrieved all"
      "all async information from the CM",
      FALSE,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_READY,
      param_spec);

  /**
   * TpyCallContent::removed
   * @self: the #TpyCallContent
   *
   * The ::removed signal is emitted when @self is removed from
   * a #TpyCallChannel.
   */
  _signals[REMOVED] = g_signal_new ("removed",
      G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST,
      0, NULL, NULL,
      g_cclosure_marshal_VOID__VOID,
      G_TYPE_NONE,
      0);

  /**
   * TpyCallContent::streams-added
   * @self: the #TpyCallContent
   * @stream: the newly added streams
   *
   * The ::content-removed signal is emitted whenever
   * #TpyCallContent are added from @self.
   */
  _signals[STREAMS_ADDED] = g_signal_new ("streams-added",
      G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST,
      0, NULL, NULL,
      g_cclosure_marshal_VOID__BOXED,
      G_TYPE_NONE,
      1, G_TYPE_PTR_ARRAY);

  /**
   * TpyCallContent::streams-removed
   * @self: the #TpyCallContent
   * @stream: the newly removed streams
   *
   * The ::content-removed signal is emitted whenever
   * #TpyCallContent are removed from @self.
   */
  _signals[STREAMS_REMOVED] = g_signal_new ("streams-removed",
      G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST,
      0, NULL, NULL,
      g_cclosure_marshal_VOID__BOXED,
      G_TYPE_NONE,
      1, G_TYPE_PTR_ARRAY);
}

const gchar *
tpy_call_content_get_name (TpyCallContent *self)
{
  g_return_val_if_fail (TPY_IS_CALL_CONTENT (self), NULL);

  return self->priv->name;
}

TpMediaStreamType
tpy_call_content_get_media_type (TpyCallContent *self)
{
  g_return_val_if_fail (TPY_IS_CALL_CONTENT (self),
      TP_MEDIA_STREAM_TYPE_AUDIO);

  return self->priv->media_type;
}

TpyCallContentDisposition
tpy_call_content_get_disposition (TpyCallContent *self)
{
  g_return_val_if_fail (TPY_IS_CALL_CONTENT (self),
      TPY_CALL_CONTENT_DISPOSITION_NONE);

  return self->priv->disposition;
}

static void
on_call_content_get_all_properties_cb (TpProxy *proxy,
    GHashTable *properties,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  TpyCallContent *self = TPY_CALL_CONTENT (proxy);
  GPtrArray *streams;

  if (error != NULL)
    {
      g_warning ("Could not get the content properties: %s", error->message);
      return;
    }

  self->priv->media_type = tp_asv_get_uint32 (properties, "Type", NULL);
  g_free (self->priv->name);
  self->priv->name = g_strdup (tp_asv_get_string (properties, "Name"));
  self->priv->disposition = tp_asv_get_uint32 (properties, "Disposition",
    NULL);

  streams = tp_asv_get_boxed (properties, "Streams",
    TP_ARRAY_TYPE_OBJECT_PATH_LIST);

  if (streams != NULL)
    add_streams (self, streams);

  self->priv->properties_retrieved = TRUE;
  maybe_go_to_ready (self);
}

static void
on_content_remove_cb (TpProxy *proxy,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  TpyCallContent *self = TPY_CALL_CONTENT (proxy);

  if (error != NULL)
    {
      DEBUG ("Failed to remove content: %s", error->message);

      g_simple_async_result_set_from_error (self->priv->result, error);
    }

  g_simple_async_result_set_op_res_gboolean (self->priv->result, TRUE);
  g_simple_async_result_complete (self->priv->result);
  tp_clear_object (&self->priv->result);
}

void
tpy_call_content_remove_async (TpyCallContent *self,
    TpyContentRemovalReason reason,
    const gchar *detailed_removal_reason,
    const gchar *message,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  DEBUG ("removing content for reason %u, detailed reason: %s, message: %s",
      reason, detailed_removal_reason, message);

  tpy_cli_call_content_call_remove (TP_PROXY (self), -1,
      reason, detailed_removal_reason, message,
      on_content_remove_cb, NULL, NULL, G_OBJECT (self));
}

gboolean
tpy_call_content_remove_finish (TpyCallContent *self,
    GAsyncResult *result,
    GError **error)
{
  if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result),
      error))
    return FALSE;

  g_return_val_if_fail (g_simple_async_result_is_valid (result,
    G_OBJECT (self), tpy_call_content_remove_async),
    FALSE);

  return g_simple_async_result_get_op_res_gboolean (
    G_SIMPLE_ASYNC_RESULT (result));
}

GList *
tpy_call_content_get_streams (TpyCallContent *self)
{
  return self->priv->streams;
}
