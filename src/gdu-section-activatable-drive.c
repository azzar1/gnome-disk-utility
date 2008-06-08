/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/* gdu-section-activatable-drive.c
 *
 * Copyright (C) 2007 David Zeuthen
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <config.h>
#include <string.h>
#include <glib/gi18n.h>
#include <dbus/dbus-glib.h>
#include <stdlib.h>
#include <math.h>
#include <polkit-gnome/polkit-gnome.h>

#include <gdu/gdu.h>
#include "gdu-ui-util.h"
#include "gdu-tree.h"
#include "gdu-section-activatable-drive.h"

struct _GduSectionActivatableDrivePrivate
{
        GtkWidget *linux_md_name_label;
        GtkWidget *linux_md_type_label;
        GtkWidget *linux_md_size_label;
        GtkWidget *linux_md_components_label;
        GtkWidget *linux_md_state_label;
        GtkWidget *linux_md_tree_view;
        GtkTreeStore *linux_md_tree_store;
#if 0
        GtkWidget *linux_md_add_to_array_button;
        GtkWidget *linux_md_remove_from_array_button;
        GtkWidget *linux_md_add_new_to_array_button;
#endif

        PolKitAction *pk_linux_md_action;
        PolKitAction *pk_linux_md_system_internal_action;

        PolKitGnomeAction *attach_action;
        PolKitGnomeAction *detach_action;
        PolKitGnomeAction *add_action;
};

static GObjectClass *parent_class = NULL;

G_DEFINE_TYPE (GduSectionActivatableDrive, gdu_section_activatable_drive, GDU_TYPE_SECTION)

enum {
        MD_LINUX_ICON_COLUMN,
        MD_LINUX_NAME_COLUMN,
        MD_LINUX_STATE_STRING_COLUMN,
        MD_LINUX_STATE_COLUMN,
        MD_LINUX_OBJPATH_COLUMN,
        MD_LINUX_N_COLUMNS,
};

/* ---------------------------------------------------------------------------------------------------- */

static void
add_component_callback (GduDevice *device,
                        GError *error,
                        gpointer user_data)
{
        GduSection *section = GDU_SECTION (user_data);
        if (error != NULL) {
                gdu_shell_raise_error (gdu_section_get_shell (section),
                                       gdu_section_get_presentable (section),
                                       error,
                                       _("Error adding component"));
        }
        g_object_unref (section);
}

static void
add_action_callback (GtkAction *action, gpointer user_data)
{
        GduSectionActivatableDrive *section = GDU_SECTION_ACTIVATABLE_DRIVE (user_data);
        GduPresentable *presentable;
        GduPresentable *selected_presentable;
        GduDevice *device;
        GduDevice *selected_device;
        GduActivatableDrive *activatable_drive;
        GduPool *pool;
        GtkWidget *dialog;
        GtkWidget *vbox;
        int response;
        GtkWidget *tree_view;
        char *array_name;
        char *s;

        device = NULL;
        selected_device = NULL;
        pool = NULL;
        array_name = NULL;
        selected_presentable = NULL;

        presentable = gdu_section_get_presentable (GDU_SECTION (section));
        if (!GDU_IS_ACTIVATABLE_DRIVE (presentable)) {
                g_warning ("%s: is not an activatable drive", __FUNCTION__);
                goto out;
        }

        device = gdu_presentable_get_device (presentable);
        if (device == NULL) {
                g_warning ("%s: activatable drive not active", __FUNCTION__);
                goto out;
        }

        activatable_drive = GDU_ACTIVATABLE_DRIVE (presentable);

        pool = gdu_device_get_pool (device);

        dialog = gtk_dialog_new_with_buttons ("",
                                              GTK_WINDOW (gdu_shell_get_toplevel (gdu_section_get_shell (GDU_SECTION (section)))),
                                              GTK_DIALOG_MODAL|GTK_DIALOG_DESTROY_WITH_PARENT|GTK_DIALOG_NO_SEPARATOR,
                                              NULL);


	gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
	gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 2);
	gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (dialog)->action_area), 5);
	gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->action_area), 6);

        GtkWidget *hbox;

	hbox = gtk_hbox_new (FALSE, 12);
	gtk_container_set_border_width (GTK_CONTAINER (hbox), 5);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), hbox, TRUE, TRUE, 0);

        GtkWidget *image;

	image = gtk_image_new_from_pixbuf (gdu_util_get_pixbuf_for_presentable (presentable, GTK_ICON_SIZE_DIALOG));
	gtk_misc_set_alignment (GTK_MISC (image), 0.5, 0.0);
	gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);

	vbox = gtk_vbox_new (FALSE, 10);
	gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);

        array_name = gdu_presentable_get_name (presentable);

        GtkWidget *label;
        label = gtk_label_new (NULL);
        s = g_strdup_printf ( _("<big><b>Select a volume to use as component in the array \"%s\"</b></big>\n\n"
                                "Only volumes of acceptable sizes can be selected. You may "
                                "need to manually create new volumes of acceptable sizes."),
                              array_name);
        gtk_label_set_markup (GTK_LABEL (label), s);
        g_free (s);
        gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
        gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
        gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, TRUE, 0);


        GtkWidget *scrolled_window;
        scrolled_window = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
                                        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
        gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window),
                                             GTK_SHADOW_IN);
        tree_view = gdu_device_tree_new (pool);
        gtk_container_add (GTK_CONTAINER (scrolled_window), tree_view);

	gtk_box_pack_start (GTK_BOX (vbox), scrolled_window, TRUE, TRUE, 0);

        gtk_widget_grab_focus (gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL));
        gtk_dialog_add_button (GTK_DIALOG (dialog), _("Add _Volume"), 0);

        gtk_window_set_default_size (GTK_WINDOW (dialog), -1, 350);

        gtk_widget_show_all (dialog);
        response = gtk_dialog_run (GTK_DIALOG (dialog));

        selected_presentable = gdu_device_tree_get_selected_presentable (GTK_TREE_VIEW (tree_view));
        if (selected_presentable != NULL)
                g_object_ref (selected_presentable);
        gtk_widget_destroy (dialog);

        if (response < 0)
                goto out;

        if (selected_presentable == NULL)
                goto out;

        selected_device = gdu_presentable_get_device (selected_presentable);
        if (selected_device == NULL)
                goto out;

        g_warning ("got it: %s", gdu_device_get_object_path (selected_device));

        /* got it! */
        gdu_device_op_linux_md_add_component (device,
                                              gdu_device_get_object_path (selected_device),
                                              add_component_callback,
                                              g_object_ref (section));


out:
        g_free (array_name);
        if (selected_presentable != NULL)
                g_object_unref (selected_presentable);
        if (selected_device != NULL)
                g_object_unref (selected_device);
        if (device != NULL)
                g_object_unref (device);
        if (pool != NULL)
                g_object_unref (pool);
}

static void
attach_action_callback (GtkAction *action, gpointer user_data)
{
        GtkTreePath *path;
        GduSectionActivatableDrive *section = GDU_SECTION_ACTIVATABLE_DRIVE (user_data);
        GduDevice *device;
        GduPresentable *presentable;
        GduActivatableDrive *activatable_drive;
        GduDevice *slave_device;
        GduPool *pool;
        GduActivableDriveSlaveState slave_state;
        char *component_objpath;

        device = NULL;
        slave_device = NULL;
        pool = NULL;
        component_objpath = NULL;

        presentable = gdu_section_get_presentable (GDU_SECTION (section));
        if (!GDU_IS_ACTIVATABLE_DRIVE (presentable)) {
                g_warning ("%s: is not an activatable drive", __FUNCTION__);
                goto out;
        }

        device = gdu_presentable_get_device (presentable);
        if (device == NULL) {
                g_warning ("%s: activatable drive not active", __FUNCTION__);
                goto out;
        }

        activatable_drive = GDU_ACTIVATABLE_DRIVE (presentable);

        gtk_tree_view_get_cursor (GTK_TREE_VIEW (section->priv->linux_md_tree_view), &path, NULL);
        if (path != NULL) {
                GtkTreeIter iter;

                if (gtk_tree_model_get_iter (GTK_TREE_MODEL (section->priv->linux_md_tree_store), &iter, path)) {

                        gtk_tree_model_get (GTK_TREE_MODEL (section->priv->linux_md_tree_store), &iter,
                                            MD_LINUX_OBJPATH_COLUMN,
                                            &component_objpath,
                                            -1);
                }
                gtk_tree_path_free (path);
        }

        if (component_objpath == NULL) {
                g_warning ("%s: no component selected", __FUNCTION__);
                goto out;
        }

        pool = gdu_device_get_pool (device);

        slave_device = gdu_pool_get_by_object_path (pool, component_objpath);
        if (slave_device == NULL) {
                g_warning ("%s: no device for component objpath %s", __FUNCTION__, component_objpath);
                goto out;
        }

        slave_state = gdu_activatable_drive_get_slave_state (activatable_drive, slave_device);
        if (slave_state == GDU_ACTIVATABLE_DRIVE_SLAVE_STATE_NOT_FRESH) {
                /* yay, add this to the array */
                gdu_device_op_linux_md_add_component (device,
                                                      component_objpath,
                                                      add_component_callback,
                                                      g_object_ref (section));
        }


out:
        g_free (component_objpath);
        if (device != NULL)
                g_object_unref (device);
        if (pool != NULL)
                g_object_unref (pool);
        if (slave_device != NULL)
                g_object_unref (slave_device);
}

static void
remove_component_callback (GduDevice *device,
                           GError *error,
                           gpointer user_data)
{
        GduSection *section = GDU_SECTION (user_data);
        if (error != NULL) {
                gdu_shell_raise_error (gdu_section_get_shell (section),
                                       gdu_section_get_presentable (section),
                                       error,
                                       _("Error removing component"));
        }
        g_object_unref (section);
}

static void
detach_action_callback (GtkAction *action, gpointer user_data)
{
        GtkTreePath *path;
        GduSectionActivatableDrive *section = GDU_SECTION_ACTIVATABLE_DRIVE (user_data);
        GduDevice *device;
        GduPresentable *presentable;
        GduActivatableDrive *activatable_drive;
        GduDevice *slave_device;
        GduPool *pool;
        GduActivableDriveSlaveState slave_state;
        char *component_objpath;
        GduPresentable *slave_presentable;

        device = NULL;
        slave_device = NULL;
        pool = NULL;
        component_objpath = NULL;
        slave_presentable = NULL;

        presentable = gdu_section_get_presentable (GDU_SECTION (section));
        if (!GDU_IS_ACTIVATABLE_DRIVE (presentable)) {
                g_warning ("%s: is not an activatable drive", __FUNCTION__);
                goto out;
        }

        device = gdu_presentable_get_device (presentable);
        if (device == NULL) {
                g_warning ("%s: activatable drive not active", __FUNCTION__);
                goto out;
        }

        activatable_drive = GDU_ACTIVATABLE_DRIVE (presentable);

        gtk_tree_view_get_cursor (GTK_TREE_VIEW (section->priv->linux_md_tree_view), &path, NULL);
        if (path != NULL) {
                GtkTreeIter iter;

                if (gtk_tree_model_get_iter (GTK_TREE_MODEL (section->priv->linux_md_tree_store), &iter, path)) {

                        gtk_tree_model_get (GTK_TREE_MODEL (section->priv->linux_md_tree_store), &iter,
                                            MD_LINUX_OBJPATH_COLUMN,
                                            &component_objpath,
                                            -1);
                }
                gtk_tree_path_free (path);
        }

        if (component_objpath == NULL) {
                g_warning ("%s: no component selected", __FUNCTION__);
                goto out;
        }

        pool = gdu_device_get_pool (device);

        slave_device = gdu_pool_get_by_object_path (pool, component_objpath);
        if (slave_device == NULL) {
                g_warning ("%s: no device for component objpath %s", __FUNCTION__, component_objpath);
                goto out;
        }

        slave_presentable = gdu_pool_get_volume_by_device (pool, slave_device);
        if (slave_presentable == NULL) {
                g_warning ("%s: no volume for component objpath %s", __FUNCTION__, component_objpath);
                goto out;
        }

        slave_state = gdu_activatable_drive_get_slave_state (activatable_drive, slave_device);
        if (slave_state == GDU_ACTIVATABLE_DRIVE_SLAVE_STATE_RUNNING ||
            slave_state == GDU_ACTIVATABLE_DRIVE_SLAVE_STATE_RUNNING_SYNCING ||
            slave_state == GDU_ACTIVATABLE_DRIVE_SLAVE_STATE_RUNNING_HOT_SPARE) {
                char *primary;
                char *secondary;
                char *secure_erase;
                char *array_name;
                char *component_name;

                array_name = gdu_presentable_get_name (presentable);
                component_name = gdu_presentable_get_name (slave_presentable);

                /* confirmation dialog */
                primary = g_strdup (_("<b><big>Are you sure you want to remove the component from the array?</big></b>"));

                secondary = g_strdup_printf (_("The data on component \"%s\" of the RAID Array \"%s\" will be "
                                               "irrecovably erased and the RAID Array might be degraded. "
                                               "Make sure important data is backed up. "
                                               "This action cannot be undone."),
                                             component_name,
                                             array_name);

                secure_erase = gdu_util_delete_confirmation_dialog (gdu_shell_get_toplevel (gdu_section_get_shell (GDU_SECTION (section))),
                                                                    "",
                                                                    primary,
                                                                    secondary,
                                                                    _("_Remove Component"));
                if (secure_erase != NULL) {
                        /* yay, remove this component from the array */
                        gdu_device_op_linux_md_remove_component (device,
                                                                 component_objpath,
                                                                 secure_erase,
                                                                 remove_component_callback,
                                                                 g_object_ref (section));
                }

                g_free (primary);
                g_free (secondary);
                g_free (secure_erase);
                g_free (array_name);
                g_free (component_name);
        }


out:
        g_free (component_objpath);
        if (device != NULL)
                g_object_unref (device);
        if (pool != NULL)
                g_object_unref (pool);
        if (slave_device != NULL)
                g_object_unref (slave_device);
        if (slave_presentable != NULL)
                g_object_unref (slave_presentable);
}

static void
linux_md_buttons_update (GduSectionActivatableDrive *section)
{
        GtkTreePath *path;
        char *component_objpath;
        gboolean show_add_to_array_button;
        gboolean show_add_new_to_array_button;
        gboolean show_remove_from_array_button;
        GduPresentable *presentable;
        GduActivatableDrive *activatable_drive;
        GduDevice *device;
        GduDevice *slave_device;
        GduPool *pool;
        GduActivableDriveSlaveState slave_state;

        component_objpath = NULL;
        device = NULL;
        slave_device = NULL;
        pool = NULL;
        show_add_to_array_button = FALSE;
        show_add_new_to_array_button = FALSE;
        show_remove_from_array_button = FALSE;

        gtk_tree_view_get_cursor (GTK_TREE_VIEW (section->priv->linux_md_tree_view), &path, NULL);
        if (path != NULL) {
                GtkTreeIter iter;

                if (gtk_tree_model_get_iter (GTK_TREE_MODEL (section->priv->linux_md_tree_store), &iter, path)) {

                        gtk_tree_model_get (GTK_TREE_MODEL (section->priv->linux_md_tree_store), &iter,
                                            MD_LINUX_OBJPATH_COLUMN,
                                            &component_objpath,
                                            -1);
                }
                gtk_tree_path_free (path);
        }

        presentable = gdu_section_get_presentable (GDU_SECTION (section));
        if (!GDU_IS_ACTIVATABLE_DRIVE (presentable)) {
                g_warning ("%s: is not an activatable drive", __FUNCTION__);
                goto out;
        }

        activatable_drive = GDU_ACTIVATABLE_DRIVE (presentable);

        /* can only add/remove components on a running drive */
        device = gdu_presentable_get_device (presentable);
        if (device == NULL) {
                goto out;
        }

        /* can always add a new component when the drive is running */
        show_add_new_to_array_button = TRUE;

        pool = gdu_device_get_pool (device);

        if (component_objpath != NULL) {

                slave_device = gdu_pool_get_by_object_path (pool, component_objpath);
                if (slave_device == NULL) {
                        g_warning ("%s: no device for component objpath %s", __FUNCTION__, component_objpath);
                        goto out;
                }

                slave_state = gdu_activatable_drive_get_slave_state (activatable_drive, slave_device);

                if (slave_state == GDU_ACTIVATABLE_DRIVE_SLAVE_STATE_NOT_FRESH) {
                        /* yay, we can add this to the array */
                        show_add_to_array_button = TRUE;
                }

                if (slave_state == GDU_ACTIVATABLE_DRIVE_SLAVE_STATE_RUNNING ||
                    slave_state == GDU_ACTIVATABLE_DRIVE_SLAVE_STATE_RUNNING_SYNCING ||
                    slave_state == GDU_ACTIVATABLE_DRIVE_SLAVE_STATE_RUNNING_HOT_SPARE) {
                        /* yay, we can remove this to the array */
                        show_remove_from_array_button = TRUE;
                }
        }

        g_object_set (section->priv->attach_action,
                      "polkit-action",
                      gdu_device_is_system_internal (device) ?
                      section->priv->pk_linux_md_system_internal_action :
                      section->priv->pk_linux_md_action,
                      NULL);

        g_object_set (section->priv->detach_action,
                      "polkit-action",
                      gdu_device_is_system_internal (device) ?
                      section->priv->pk_linux_md_system_internal_action :
                      section->priv->pk_linux_md_action,
                      NULL);

        g_object_set (section->priv->add_action,
                      "polkit-action",
                      gdu_device_is_system_internal (device) ?
                      section->priv->pk_linux_md_system_internal_action :
                      section->priv->pk_linux_md_action,
                      NULL);


out:
        polkit_gnome_action_set_sensitive (section->priv->attach_action, show_add_to_array_button);
        polkit_gnome_action_set_sensitive (section->priv->detach_action, show_remove_from_array_button);
        polkit_gnome_action_set_sensitive (section->priv->add_action, show_add_new_to_array_button);

        g_free (component_objpath);
        if (device != NULL)
                g_object_unref (device);
        if (pool != NULL)
                g_object_unref (pool);
        if (slave_device != NULL)
                g_object_unref (slave_device);
}

static void
linux_md_tree_changed (GtkTreeSelection *selection, gpointer user_data)
{
        GduSectionActivatableDrive *section = GDU_SECTION_ACTIVATABLE_DRIVE (user_data);
        linux_md_buttons_update (section);
}

static void
update (GduSectionActivatableDrive *section)
{
        char *s;
        GduDevice *device;
        GduPresentable *presentable;
        GduActivatableDrive *activatable_drive;
        GList *l;
        GList *slaves;
        GduDevice *component;
        const char *uuid;
        const char *name;
        const char *level;
        int num_raid_devices;
        int num_slaves;
        char *level_str;
        guint64 component_size;
        guint64 raid_size;
        char *raid_size_str;
        char *components_str;
        char *state_str;

        slaves = NULL;
        device = NULL;
        level_str = NULL;
        raid_size_str = NULL;
        components_str = NULL;
        state_str = NULL;

        presentable = gdu_section_get_presentable (GDU_SECTION (section));
        activatable_drive = GDU_ACTIVATABLE_DRIVE (presentable);
        device = gdu_presentable_get_device (presentable);

        slaves = gdu_activatable_drive_get_slaves (activatable_drive);
        num_slaves = g_list_length (slaves);
        if (num_slaves == 0) {
                /* this fine; happens when the last component is yanked
                 * since remove_slave() emits "changed".
                 */
                /*g_warning ("%s: no slaves for activatable drive", __FUNCTION__);*/
                goto out;
        }
        component = GDU_DEVICE (slaves->data);

        if (!gdu_device_is_linux_md_component (component)) {
                g_warning ("%s: slave of activatable drive is not a linux md component", __FUNCTION__);
                goto out;
        }

        uuid = gdu_device_linux_md_component_get_uuid (component);
        name = gdu_device_linux_md_component_get_name (component);
        if (name == NULL || strlen (name) == 0) {
                name = _("-");
        }
        level = gdu_device_linux_md_component_get_level (component);
        num_raid_devices = gdu_device_linux_md_component_get_num_raid_devices (component);
        component_size = gdu_device_get_size (component);

        /*g_warning ("activatable drive:\n"
                   "uuid=%s\n"
                   "name=%s\n"
                   "level=%s\n"
                   "num_raid_devices=%d\n"
                   "num_slaves=%d\n"
                   "device=%p",
                   uuid, name, level, num_raid_devices, num_slaves, device);*/

        raid_size = gdu_presentable_get_size (presentable);

        if (strcmp (level, "raid0") == 0) {
                level_str = g_strdup (_("Striped (RAID-0)"));
        } else if (strcmp (level, "raid1") == 0) {
                level_str = g_strdup (_("Mirrored (RAID-1)"));
        } else if (strcmp (level, "raid4") == 0) {
                level_str = g_strdup (_("RAID-4"));
        } else if (strcmp (level, "raid5") == 0) {
                level_str = g_strdup (_("RAID-5"));
        } else if (strcmp (level, "raid6") == 0) {
                level_str = g_strdup (_("RAID-6"));
        } else if (strcmp (level, "linear") == 0) {
                level_str = g_strdup (_("Linear (Just a Bunch Of Disks)"));
        } else {
                level_str = g_strdup (level);
        }

        s = gdu_util_get_size_for_display (component_size, FALSE);
        if (strcmp (level, "linear") == 0) {
                components_str = g_strdup_printf (_("%d Components"), num_raid_devices);
        } else {
                components_str = g_strdup_printf (_("%d Components (%s each)"), num_raid_devices, s);
        }
        g_free (s);

        if (raid_size == 0) {
                raid_size_str = g_strdup_printf (_("-"));
        } else {
                raid_size_str = gdu_util_get_size_for_display (raid_size, TRUE);
        }

        if (device == NULL) {
                if (gdu_activatable_drive_can_activate (activatable_drive)) {
                        state_str = g_strdup (_("Not running"));
                } else if (gdu_activatable_drive_can_activate_degraded (activatable_drive)) {
                        state_str = g_strdup (_("Not running, can only start degraded"));
                } else {
                        state_str = g_strdup (_("Not running, not enough components to start"));
                }
        } else {
                gboolean is_degraded;
                const char *sync_action;
                double sync_percentage;
                guint64 sync_speed;
                char *sync_speed_str;
                GString *str;

                is_degraded = gdu_device_linux_md_is_degraded (device);
                sync_action = gdu_device_linux_md_get_sync_action (device);
                sync_percentage = gdu_device_linux_md_get_sync_percentage (device);
                sync_speed = gdu_device_linux_md_get_sync_speed (device);

                str = g_string_new (NULL);
                if (is_degraded)
                        g_string_append (str, _("<span foreground='red'><b>Degraded</b></span>"));
                else
                        g_string_append (str, _("Running"));

                if (strcmp (sync_action, "idle") != 0) {
                        if (strcmp (sync_action, "reshape") == 0)
                                g_string_append (str, _(", Reshaping"));
                        else if (strcmp (sync_action, "resync") == 0)
                                g_string_append (str, _(", Resyncing"));
                        else if (strcmp (sync_action, "repair") == 0)
                                g_string_append (str, _(", Repairing"));
                        else if (strcmp (sync_action, "recover") == 0)
                                g_string_append (str, _(", Recovering"));

                        sync_speed_str = gdu_util_get_speed_for_display (sync_speed);
                        g_string_append_printf (str, _(" @ %3.01f%% (%s)"), sync_percentage, sync_speed_str);
                        g_free (sync_speed_str);
                }

                state_str = g_string_free (str, FALSE);
        }

        gtk_label_set_text (GTK_LABEL (section->priv->linux_md_name_label), name);
        gtk_label_set_text (GTK_LABEL (section->priv->linux_md_type_label), level_str);
        gtk_label_set_text (GTK_LABEL (section->priv->linux_md_size_label), raid_size_str);
        gtk_label_set_text (GTK_LABEL (section->priv->linux_md_components_label), components_str);
        gtk_label_set_markup (GTK_LABEL (section->priv->linux_md_state_label), state_str);

        /* only build a new model if rebuilding the section */
        //if (reset_section) {
        {
                GtkTreeStore *store;

                if (section->priv->linux_md_tree_store != NULL)
                        g_object_unref (section->priv->linux_md_tree_store);

                store = gtk_tree_store_new (MD_LINUX_N_COLUMNS,
                                            GDK_TYPE_PIXBUF,
                                            G_TYPE_STRING,
                                            G_TYPE_STRING,
                                            G_TYPE_INT,
                                            G_TYPE_STRING);
                section->priv->linux_md_tree_store = store;

                gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store),
                                                      MD_LINUX_OBJPATH_COLUMN,
                                                      GTK_SORT_ASCENDING);

                /* add all slaves */
                for (l = slaves; l != NULL; l = l->next) {
                        GduDevice *sd = GDU_DEVICE (l->data);
                        GdkPixbuf *pixbuf;
                        char *name;
                        GtkTreeIter iter;
                        GduPool *pool;
                        GduPresentable *p;
                        GduActivableDriveSlaveState slave_state;
                        const char *slave_state_str;

                        pool = gdu_device_get_pool (sd);
                        p = gdu_pool_get_volume_by_device (pool, sd);
                        g_object_unref (pool);

                        if (p == NULL) {
                                g_warning ("Cannot find volume for device");
                                continue;
                        }

                        s = gdu_presentable_get_name (p);
                        name = g_strdup_printf ("%s (%s)", s, gdu_device_get_device_file (sd));
                        g_free (s);
                        pixbuf = gdu_util_get_pixbuf_for_presentable (p, GTK_ICON_SIZE_LARGE_TOOLBAR);

                        slave_state = gdu_activatable_drive_get_slave_state (activatable_drive, sd);

                        slave_state_str = _("Unknown");
                        switch (slave_state) {
                        case GDU_ACTIVATABLE_DRIVE_SLAVE_STATE_RUNNING:
                                slave_state_str = _("Running");
                                break;
                        case GDU_ACTIVATABLE_DRIVE_SLAVE_STATE_RUNNING_SYNCING:
                                slave_state_str = _("Running, Syncing to array");
                                break;
                        case GDU_ACTIVATABLE_DRIVE_SLAVE_STATE_RUNNING_HOT_SPARE:
                                slave_state_str = _("Running, Hot Spare");
                                break;
                        case GDU_ACTIVATABLE_DRIVE_SLAVE_STATE_READY:
                                slave_state_str = _("Ready");
                                break;
                        case GDU_ACTIVATABLE_DRIVE_SLAVE_STATE_NOT_FRESH:
                                if (device != NULL) {
                                        slave_state_str = _("Not Running, Stale");
                                } else {
                                        slave_state_str = _("Stale");
                                }
                                break;
                        default:
                                break;
                        }

                        gtk_tree_store_append (store, &iter, NULL);
                        gtk_tree_store_set (store,
                                            &iter,
                                            MD_LINUX_ICON_COLUMN, pixbuf,
                                            MD_LINUX_NAME_COLUMN, name,
                                            MD_LINUX_STATE_STRING_COLUMN, slave_state_str,
                                            MD_LINUX_STATE_COLUMN, slave_state,
                                            MD_LINUX_OBJPATH_COLUMN, gdu_device_get_object_path (sd),
                                            -1);

                        g_free (name);
                        if (pixbuf != NULL)
                                g_object_unref (pixbuf);

                        g_object_unref (p);
                }

                gtk_tree_view_set_model (GTK_TREE_VIEW (section->priv->linux_md_tree_view),
                                         GTK_TREE_MODEL (store));

        }

        linux_md_buttons_update (section);

out:
        g_free (state_str);
        g_free (level_str);
        g_free (raid_size_str);
        g_free (components_str);
        g_list_foreach (slaves, (GFunc) g_object_unref, NULL);
        g_list_free (slaves);
        if (device != NULL)
                g_object_unref (device);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
gdu_section_activatable_drive_finalize (GduSectionActivatableDrive *section)
{
        polkit_action_unref (section->priv->pk_linux_md_action);
        polkit_action_unref (section->priv->pk_linux_md_system_internal_action);
        g_object_unref (section->priv->attach_action);
        g_object_unref (section->priv->detach_action);
        g_object_unref (section->priv->add_action);
        if (G_OBJECT_CLASS (parent_class)->finalize)
                (* G_OBJECT_CLASS (parent_class)->finalize) (G_OBJECT (section));
}

static void
gdu_section_activatable_drive_class_init (GduSectionActivatableDriveClass *klass)
{
        GObjectClass *obj_class = (GObjectClass *) klass;
        GduSectionClass *section_class = (GduSectionClass *) klass;

        parent_class = g_type_class_peek_parent (klass);

        obj_class->finalize = (GObjectFinalizeFunc) gdu_section_activatable_drive_finalize;
        section_class->update = (gpointer) update;
}

static void
gdu_section_activatable_drive_init (GduSectionActivatableDrive *section)
{
        int row;
        GtkWidget *label;
        GtkWidget *table;
        GtkWidget *button_box;
        GtkWidget *scrolled_window;
        GtkWidget *tree_view;
        GtkTreeSelection *selection;
        GtkCellRenderer *renderer;
        GtkTreeViewColumn *column;

        section->priv = g_new0 (GduSectionActivatableDrivePrivate, 1);

        section->priv->pk_linux_md_action = polkit_action_new ();
        polkit_action_set_action_id (section->priv->pk_linux_md_action,
                                     "org.freedesktop.devicekit.disks.linux-md");

        section->priv->pk_linux_md_system_internal_action = polkit_action_new ();
        polkit_action_set_action_id (section->priv->pk_linux_md_system_internal_action,
                                     "org.freedesktop.devicekit.disks.linux-md-system-internal");

        gtk_box_set_spacing (GTK_BOX (section), 8);

        table = gtk_table_new (4, 2, FALSE);
        gtk_box_pack_start (GTK_BOX (section), table, FALSE, FALSE, 0);

        row = 0;

        /* name */
        label = gtk_label_new (NULL);
        gtk_misc_set_alignment (GTK_MISC (label), 1.0, 0.5);
        gtk_label_set_markup (GTK_LABEL (label), _("<b>Array Name:</b>"));
        gtk_table_attach (GTK_TABLE (table), label, 0, 1, row, row + 1,
                          GTK_FILL, GTK_EXPAND | GTK_FILL, 2, 2);
        label = gtk_label_new (NULL);
        gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
        gtk_table_attach (GTK_TABLE (table), label, 1, 2, row, row + 1,
                          GTK_FILL, GTK_EXPAND | GTK_FILL, 2, 2);
        section->priv->linux_md_name_label = label;

        row++;

        /* size */
        label = gtk_label_new (NULL);
        gtk_misc_set_alignment (GTK_MISC (label), 1.0, 0.5);
        gtk_label_set_markup (GTK_LABEL (label), _("<b>Array Size:</b>"));
        gtk_table_attach (GTK_TABLE (table), label, 0, 1, row, row + 1,
                          GTK_FILL, GTK_EXPAND | GTK_FILL, 2, 2);
        label = gtk_label_new (NULL);
        gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
        gtk_table_attach (GTK_TABLE (table), label, 1, 2, row, row + 1,
                          GTK_FILL, GTK_EXPAND | GTK_FILL, 2, 2);
        section->priv->linux_md_size_label = label;

        row++;

        /* type (level) */
        label = gtk_label_new (NULL);
        gtk_misc_set_alignment (GTK_MISC (label), 1.0, 0.5);
        gtk_label_set_markup (GTK_LABEL (label), _("<b>RAID Type:</b>"));
        gtk_table_attach (GTK_TABLE (table), label, 0, 1, row, row + 1,
                          GTK_FILL, GTK_EXPAND | GTK_FILL, 2, 2);
        label = gtk_label_new (NULL);
        gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
        gtk_table_attach (GTK_TABLE (table), label, 1, 2, row, row + 1,
                          GTK_FILL, GTK_EXPAND | GTK_FILL, 2, 2);
        section->priv->linux_md_type_label = label;

        row++;

        /* components */
        label = gtk_label_new (NULL);
        gtk_misc_set_alignment (GTK_MISC (label), 1.0, 0.5);
        gtk_label_set_markup (GTK_LABEL (label), _("<b>Components:</b>"));
        gtk_table_attach (GTK_TABLE (table), label, 0, 1, row, row + 1,
                          GTK_FILL, GTK_EXPAND | GTK_FILL, 2, 2);
        label = gtk_label_new (NULL);
        gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
        gtk_table_attach (GTK_TABLE (table), label, 1, 2, row, row + 1,
                          GTK_FILL, GTK_EXPAND | GTK_FILL, 2, 2);
        section->priv->linux_md_components_label = label;

        row++;

        /* components */
        label = gtk_label_new (NULL);
        gtk_misc_set_alignment (GTK_MISC (label), 1.0, 0.5);
        gtk_label_set_markup (GTK_LABEL (label), _("<b>State:</b>"));
        gtk_table_attach (GTK_TABLE (table), label, 0, 1, row, row + 1,
                          GTK_FILL, GTK_EXPAND | GTK_FILL, 2, 2);
        label = gtk_label_new (NULL);
        gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
        gtk_table_attach (GTK_TABLE (table), label, 1, 2, row, row + 1,
                          GTK_FILL, GTK_EXPAND | GTK_FILL, 2, 2);
        section->priv->linux_md_state_label = label;

        row++;

        tree_view = gtk_tree_view_new ();
        section->priv->linux_md_tree_view = tree_view;
        scrolled_window = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
        gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window), GTK_SHADOW_IN);
        gtk_container_add (GTK_CONTAINER (scrolled_window), tree_view);
        gtk_box_pack_start (GTK_BOX (section), scrolled_window, TRUE, TRUE, 0);

        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));
        gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
        g_signal_connect (selection, "changed", (GCallback) linux_md_tree_changed, section);

        button_box = gtk_hbutton_box_new ();
        gtk_button_box_set_layout (GTK_BUTTON_BOX (button_box), GTK_BUTTONBOX_START);
        gtk_box_set_spacing (GTK_BOX (button_box), 6);
        gtk_box_set_homogeneous (GTK_BOX (button_box), FALSE);
        gtk_box_pack_start (GTK_BOX (section), button_box, FALSE, FALSE, 0);

        section->priv->attach_action = polkit_gnome_action_new_default (
                "attach",
                section->priv->pk_linux_md_action,
                _("A_ttach"),
                _("Attaches the stale component to the RAID array. "
                  "After attachment, data from the array will be "
                  "synchronized on the component."));
        g_object_set (section->priv->attach_action,
                      "auth-label", _("A_ttach..."),
                      "yes-icon-name", GTK_STOCK_ADD,
                      "no-icon-name", GTK_STOCK_ADD,
                      "auth-icon-name", GTK_STOCK_ADD,
                      "self-blocked-icon-name", GTK_STOCK_ADD,
                      NULL);
        g_signal_connect (section->priv->attach_action, "activate", G_CALLBACK (attach_action_callback), section);
        gtk_container_add (GTK_CONTAINER (button_box),
                           polkit_gnome_action_create_button (section->priv->attach_action));

        section->priv->detach_action = polkit_gnome_action_new_default (
                "detach",
                section->priv->pk_linux_md_action,
                _("_Detach"),
                _("Detaches the running component from the RAID array. Data on "
                  "the component will be erased and the volume will be ready "
                  "for other use."));
        g_object_set (section->priv->detach_action,
                      "auth-label", _("_Detach..."),
                      "yes-icon-name", GTK_STOCK_REMOVE,
                      "no-icon-name", GTK_STOCK_REMOVE,
                      "auth-icon-name", GTK_STOCK_REMOVE,
                      "self-blocked-icon-name", GTK_STOCK_REMOVE,
                      NULL);
        g_signal_connect (section->priv->detach_action, "activate", G_CALLBACK (detach_action_callback), section);
        gtk_container_add (GTK_CONTAINER (button_box),
                           polkit_gnome_action_create_button (section->priv->detach_action));

        section->priv->add_action = polkit_gnome_action_new_default (
                "add",
                section->priv->pk_linux_md_action,
                _("_Add..."),
                _("Adds a new component to the running RAID array. Use this "
                  "when replacing a failed component or adding a hot spare."));
        g_object_set (section->priv->add_action,
                      "auth-label", _("_Add..."),
                      "yes-icon-name", GTK_STOCK_NEW,
                      "no-icon-name", GTK_STOCK_NEW,
                      "auth-icon-name", GTK_STOCK_NEW,
                      "self-blocked-icon-name", GTK_STOCK_NEW,
                      NULL);
        g_signal_connect (section->priv->add_action, "activate", G_CALLBACK (add_action_callback), section);
        gtk_container_add (GTK_CONTAINER (button_box),
                           polkit_gnome_action_create_button (section->priv->add_action));

#if 0
        button = gtk_button_new_with_mnemonic (_("A_ttach"));
        gtk_button_set_image (GTK_BUTTON (button),
                              gtk_image_new_from_stock (GTK_STOCK_ADD, GTK_ICON_SIZE_BUTTON));
        gtk_container_add (GTK_CONTAINER (button_box), button);
        section->priv->linux_md_add_to_array_button = button;
        g_signal_connect (button, "clicked", G_CALLBACK (add_to_array_button_clicked), section);
        gtk_widget_set_tooltip_text (button, _("Attaches the stale component to the RAID array. "
                                               "After attachment, data from the array will be "
                                               "synchronized on the component."));

        button = gtk_button_new_with_mnemonic (_("_Detach"));
        gtk_button_set_image (GTK_BUTTON (button),
                              gtk_image_new_from_stock (GTK_STOCK_REMOVE, GTK_ICON_SIZE_BUTTON));
        gtk_container_add (GTK_CONTAINER (button_box), button);
        section->priv->linux_md_remove_from_array_button = button;
        g_signal_connect (button, "clicked", G_CALLBACK (remove_from_array_button_clicked), section);
        gtk_widget_set_tooltip_text (button, _("Detaches the running component from the RAID array. Data on "
                                               "the component will be erased and the volume will be ready "
                                               "for other use."));

        button = gtk_button_new_with_mnemonic (_("_Add..."));
        gtk_button_set_image (GTK_BUTTON (button),
                              gtk_image_new_from_stock (GTK_STOCK_NEW, GTK_ICON_SIZE_BUTTON));
        gtk_container_add (GTK_CONTAINER (button_box), button);
        section->priv->linux_md_add_new_to_array_button = button;
        g_signal_connect (button, "clicked", G_CALLBACK (add_new_to_array_button_clicked), section);
        gtk_widget_set_tooltip_text (button, _("Adds a new component to the running RAID array. Use this "
                                               "when replacing a failed component or adding a hot spare."));
#endif

        /* add renderers for tree view */
        column = gtk_tree_view_column_new ();
        gtk_tree_view_column_set_title (column, _("RAID Component"));
        renderer = gtk_cell_renderer_pixbuf_new ();
        gtk_tree_view_column_pack_start (column, renderer, FALSE);
        gtk_tree_view_column_set_attributes (column, renderer,
                                             "pixbuf", MD_LINUX_ICON_COLUMN,
                                             NULL);
        renderer = gtk_cell_renderer_text_new ();
        gtk_tree_view_column_pack_start (column, renderer, TRUE);
        gtk_tree_view_column_set_attributes (column, renderer,
                                             "text", MD_LINUX_NAME_COLUMN,
                                             NULL);
        gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), column);

        column = gtk_tree_view_column_new ();
        gtk_tree_view_column_set_title (column, _("State"));
        renderer = gtk_cell_renderer_text_new ();
        gtk_tree_view_column_pack_start (column, renderer, FALSE);
        gtk_tree_view_column_set_attributes (column, renderer,
                                             "text", MD_LINUX_STATE_STRING_COLUMN,
                                             NULL);
        gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), column);

        gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (tree_view), TRUE);

}

GtkWidget *
gdu_section_activatable_drive_new (GduShell       *shell,
                                   GduPresentable *presentable)
{
        return GTK_WIDGET (g_object_new (GDU_TYPE_SECTION_ACTIVATABLE_DRIVE,
                                         "shell", shell,
                                         "presentable", presentable,
                                         NULL));
}
