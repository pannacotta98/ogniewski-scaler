/* GIMP Plug-in Template
 * Copyright (C) 2000-2004  Michael Natterer <mitch@gimp.org> (the "Author").
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name of the Author of the
 * Software shall not be used in advertising or otherwise to promote the
 * sale, use or other dealings in this Software without prior written
 * authorization from the Author.
 */

#include "config.h"

#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include "interface.h"
#include "main.h"

#include "plugin-intl.h"

/*  Constants  */

#define SCALE_WIDTH 180
#define SPIN_BUTTON_WIDTH 75
#define RANDOM_SEED_WIDTH 100

/*  Local function prototypes  */

static gboolean dialog_image_constraint_func(gint32 image_id, gpointer data);

/*  Local variables  */

static PlugInUIVals* ui_state = NULL;

/*  Public functions  */

gboolean dialog(gint32 image_ID,
                GimpDrawable* drawable,
                PlugInVals* vals,
                PlugInImageVals* image_vals,
                PlugInDrawableVals* drawable_vals,
                PlugInUIVals* ui_vals) {
    GtkWidget* dlg;
    GtkWidget* main_vbox;
    GtkWidget* frame;
    GtkWidget* table;
    GtkWidget* hbox;
    GtkWidget* hbox2;
    GtkWidget* coordinates;
    GtkWidget* combo;
    GtkObject* adj;
    gint row;
    gboolean run = FALSE;
    GimpUnit unit;
    gdouble xres, yres;

    ui_state = ui_vals;

    gimp_ui_init(PLUGIN_NAME, TRUE);

    dlg = gimp_dialog_new(_("Ogniewski Scaler"), PLUGIN_NAME, NULL, 0, gimp_standard_help_func,
                          "plug-in-template", GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OK,
                          GTK_RESPONSE_OK, NULL);

    main_vbox = gtk_vbox_new(FALSE, 12);
    gtk_container_set_border_width(GTK_CONTAINER(main_vbox), 12);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dlg)->vbox), main_vbox);

    /*  gimp_coordinates_new() example  */

    frame = gimp_frame_new(_("Size"));
    gtk_box_pack_start(GTK_BOX(main_vbox), frame, FALSE, FALSE, 0);
    gtk_widget_show(frame);

    hbox = gtk_hbox_new(FALSE, 4);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 4);
    gtk_container_add(GTK_CONTAINER(frame), hbox);
    gtk_widget_show(hbox);

    unit = gimp_image_get_unit(image_ID);
    gimp_image_get_resolution(image_ID, &xres, &yres);

    // TODO Should we allow aspect ratio changes?
    coordinates =
        gimp_coordinates_new(unit, "%p", TRUE, TRUE, SPIN_BUTTON_WIDTH, GIMP_SIZE_ENTRY_UPDATE_SIZE,
                             ui_vals->chain_active, TRUE, _("Width:"), drawable->width, xres, 1,
                             GIMP_MAX_IMAGE_SIZE, 0, drawable->width, _("Height:"),
                             drawable->height, yres, 1, GIMP_MAX_IMAGE_SIZE, 0, drawable->height);
    gtk_box_pack_start(GTK_BOX(hbox), coordinates, FALSE, FALSE, 0);
    gtk_widget_show(coordinates);

    /*  Show the main containers  */

    gtk_widget_show(main_vbox);
    gtk_widget_show(dlg);

    run = (gimp_dialog_run(GIMP_DIALOG(dlg)) == GTK_RESPONSE_OK);

    if (run) {
        /*  Save ui values  */
        ui_state->chain_active =
            gimp_chain_button_get_active(GIMP_COORDINATES_CHAINBUTTON(coordinates));
    }

    gtk_widget_destroy(dlg);

    return run;
}

/*  Private functions  */

static gboolean dialog_image_constraint_func(gint32 image_id, gpointer data) {
    return (gimp_image_base_type(image_id) == GIMP_RGB);
}
