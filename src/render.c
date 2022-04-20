/* GIMP Plug-in Template
 * Copyright (C) 2000  Michael Natterer <mitch@gimp.org> (the "Author").
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

#include <gtk/gtk.h>
#include <libgimp/gimp.h>

#include "main.h"
#include "render.h"

#include "plugin-intl.h"

static inline gint to_1d_index(gint x, gint y, gint channel, gint width);
static inline gdouble out_to_in_coord(gint out, gdouble factor);

// TODO How do we handle edge cases?

void render(gint32 image_ID,
            GimpDrawable* drawable,
            PlugInVals* vals,
            PlugInImageVals* image_vals,
            PlugInDrawableVals* drawable_vals) {
    gint x1, y1, x2, y2;
    gimp_drawable_mask_bounds(drawable->drawable_id, &x1, &y1, &x2, &y2);
    gint channels = gimp_drawable_bpp(drawable->drawable_id);
    gint in_width = x2 - x1;
    gint in_height = y2 - y1;

    // Makes everything a lot faster, see https://developer.gimp.org/writing-a-plug-in/3/index.html
    gimp_tile_cache_ntiles(2 * (drawable->width / gimp_tile_width() + 1));

    GimpPixelRgn rgn_in, rgn_out;
    gimp_pixel_rgn_init(&rgn_in, drawable, x1, y1, in_width, in_height, FALSE, FALSE);
    gimp_pixel_rgn_init(&rgn_out, drawable, x1, y1, in_width, in_height, TRUE, TRUE);

    // /* Initialise enough memory for row1, row2, row3, outrow */
    // guchar* row1 = g_new(guchar, channels * in_width);
    // guchar* row2 = g_new(guchar, channels * in_width);
    // guchar* row3 = g_new(guchar, channels * in_width);
    // guchar* outrow = g_new(guchar, channels * in_width);

    // for (gint i = y1; i < y2; i++) {
    //     /* Get row i-1, i, i+1 */
    //     gimp_pixel_rgn_get_row(&rgn_in, row1, x1, MAX(y1, i - 1), in_width);
    //     gimp_pixel_rgn_get_row(&rgn_in, row2, x1, i, in_width);
    //     gimp_pixel_rgn_get_row(&rgn_in, row3, x1, MIN(y2 - 1, i + 1), in_width);

    //     for (gint j = x1; j < x2; j++) {
    //         // TODO Faster to not declare every time?
    //         double vert_alpha = 0.0;
    //         double hori_alpha = 0.0;
    //         gboolean vert_changed = FALSE;
    //         gboolean hori_changed = FALSE;

    //         for (gint k = 0; k < channels; k++) {
    //             // Trying out some vertical alpha calculations
    //             double d0 =
    //                 (double)row2[channels * (j - x1) + k] - (double)row1[channels * (j - x1) +
    //                 k];
    //             double d1 =
    //                 (double)row3[channels * (j - x1) + k] - (double)row2[channels * (j - x1) +
    //                 k];

    //             if (d0 != 0.0 || d1 != 0.0) {
    //                 double maybe_alpha = 0.0;

    //                 if ((d0 > 0.0 && d1 > 0.0) || (d0 < 0.0 && d1 < 0.0)) {
    //                     double td = 0.5 * (d0 + d1);
    //                     maybe_alpha = 0.5;

    //                     if (fabs(d0) < fabs(td))
    //                         maybe_alpha = fabs(d0) / fabs(td) * 0.5;
    //                     if (fabs(d1) < fabs(td))
    //                         maybe_alpha = fabs(d1) / fabs(td) * 0.5;
    //                 }

    //                 if (!vert_changed || (vert_alpha > maybe_alpha)) {
    //                     vert_alpha = maybe_alpha;
    //                     vert_changed = TRUE;
    //                 }
    //             }

    //             // Trying out some horizontal alpha calculations
    //             d0 = (double)row2[channels * (j - x1) + k] -
    //                  (double)row2[channels * MAX((j - 1 - x1), 0) + k];
    //             d1 = (double)row2[channels * MIN((j + 1 - x1), in_width - 1) + k] -
    //                  (double)row2[channels * (j - x1) + k];

    //             if (d0 != 0.0 || d1 != 0.0) {
    //                 double maybe_alpha = 0.0;

    //                 if ((d0 > 0.0 && d1 > 0.0) || (d0 < 0.0 && d1 < 0.0)) {
    //                     double td = 0.5 * (d0 + d1);
    //                     maybe_alpha = 0.5;

    //                     if (fabs(d0) < fabs(td))
    //                         maybe_alpha = fabs(d0) / fabs(td) * 0.5;
    //                     if (fabs(d1) < fabs(td))
    //                         maybe_alpha = fabs(d1) / fabs(td) * 0.5;
    //                 }

    //                 if (!hori_changed || (hori_alpha > maybe_alpha)) {
    //                     hori_alpha = maybe_alpha;
    //                     hori_changed = TRUE;
    //                 }
    //             }
    //         }
    //         guchar debug1 = CLAMP(round(vert_alpha * 255.0 * 2.0), 0, 255);
    //         guchar debug2 = CLAMP(round(hori_alpha * 255.0 * 2.0), 0, 255);
    //         outrow[channels * (j - x1) + 0] = debug2;
    //         outrow[channels * (j - x1) + 1] = debug1;
    //         outrow[channels * (j - x1) + 2] = debug2;
    //     }

    //     gimp_pixel_rgn_set_row(&rgn_out, outrow, x1, i, in_width);

    //     if (i % 10 == 0)
    //         gimp_progress_update((gdouble)(i - y1) / (gdouble)(in_height));
    // }

    // g_free(row1);
    // g_free(row2);
    // g_free(row3);
    // g_free(outrow);

    // ===================

    guchar* in_img_array = g_new(guchar, in_width * in_height * 3);  // RGB is 3 bpp
    gimp_pixel_rgn_get_rect(&rgn_in, in_img_array, x1, y1, in_width, in_height);

    gint out_width = 4 * in_width;
    gint out_height = 2 * in_height;

    gimp_image_undo_group_start(gimp_item_get_image(drawable->drawable_id));

    // TESTING resizing the image
    if (gimp_image_resize(gimp_item_get_image(drawable->drawable_id), out_width, out_height, 0,
                          0)) {
        gimp_layer_resize_to_image_size(
            gimp_image_get_active_layer(gimp_item_get_image(drawable->drawable_id)));

        GimpDrawable* resized_drawable = gimp_drawable_get(
            gimp_image_get_active_drawable(gimp_item_get_image(drawable->drawable_id)));

        GimpPixelRgn dest_rgn;
        gimp_pixel_rgn_init(&dest_rgn, resized_drawable, 0, 0, out_width, out_height, TRUE, TRUE);

        const int out_img_array_size = out_width * out_height * 3;  // RGB is 3 bpp
        guchar* out_img_array = g_new(guchar, out_img_array_size);

        for (int ix = 0; ix < out_width; ++ix)
            for (int iy = 0; iy < out_height; ++iy) {
                // out_img_array[to_1d_index(ix, iy, 0, out_width)] =
                //     (guchar)CLAMP(255 * (double)ix / (double)out_width, 0, 255);
                // out_img_array[to_1d_index(ix, iy, 1, out_width)] =
                //     (guchar)CLAMP(255 * (double)iy / (double)out_height, 0, 255);
                // out_img_array[to_1d_index(ix, iy, 2, out_width)] =
                //     (guchar)CLAMP(255 * (double)ix / (double)out_width, 0, 255);

                // out_img_array[to_1d_index(ix, iy, ic, out_height)] = 128;
                // out_img_array[(ix * out_height * 3) + iy * 3 + ic] = 128;

                // out_img_array[to_1d_index(ix, iy, 1, out_width)] =
                //     (guchar)(255.0 * out_to_in_coord(ix, 1.5) / in_width);

                for (int ic = 0; ic < 3; ++ic) {
                    out_img_array[to_1d_index(ix, iy, ic, out_width)] =
                        in_img_array[to_1d_index(CLAMP(out_to_in_coord(ix, 4), 0, in_width),   // x
                                                 CLAMP(out_to_in_coord(iy, 2), 0, in_height),  // y
                                                 ic,  // channel
                                                 in_width)];
                }
            }

        gimp_pixel_rgn_set_rect(&dest_rgn, (guchar*)out_img_array, 0, 0, out_width, out_height);

        gimp_drawable_flush(resized_drawable);
        gimp_drawable_merge_shadow(resized_drawable->drawable_id, TRUE);
        gimp_drawable_update(resized_drawable->drawable_id, x1, y1, out_width, out_height);

        gimp_drawable_detach(resized_drawable);
    }

    g_free(in_img_array);
    gimp_image_undo_group_end(gimp_item_get_image(drawable->drawable_id));
    g_message(_("Lakrits3"));
}

// Not alpha as in alpha channel
static void calc_alpha() {
    // TODO
}

static inline gdouble out_to_in_coord(gint out, gdouble factor) {
    // TODO really not sure about this
    // seems offset, also dont forget to handle edges somewhere!
    return ((double)(out - 1.0) / factor) + 0.5 * (1.0 - 1.0 / factor);
}

static inline gint to_1d_index(gint x, gint y, gint channel, gint width) {
    return (y * width * 3) + x * 3 + channel;
}
