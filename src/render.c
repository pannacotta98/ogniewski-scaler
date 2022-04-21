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

#include <assert.h>
#include <gtk/gtk.h>
#include <libgimp/gimp.h>

#include "main.h"
#include "render.h"

#include "plugin-intl.h"

static inline gint to_1d_index(gint x, gint y, gint channel, gint width, gint num_channels);
static inline gdouble out_to_in_coord(gint out, gdouble factor);
static void calc_alphas(guchar* in_img_array,  // height * width * num_channels of original img
                        gdouble* alphas,       // out, height * width * 2 (vert & hor)
                        gint height,
                        gint width);
static gdouble alpha_from_gradients(gdouble d0[], gdouble d1[]);

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
    // TODO Check if this makes any difference anymore
    gimp_tile_cache_ntiles(2 * (drawable->width / gimp_tile_width() + 1));

    GimpPixelRgn rgn_in, rgn_out;
    gimp_pixel_rgn_init(&rgn_in, drawable, x1, y1, in_width, in_height, FALSE, FALSE);
    gimp_pixel_rgn_init(&rgn_out, drawable, x1, y1, in_width, in_height, TRUE, TRUE);

    guchar* in_img_array = g_new(guchar, in_width * in_height * 3);  // RGB is 3 bpp
    gimp_pixel_rgn_get_rect(&rgn_in, in_img_array, x1, y1, in_width, in_height);

    gdouble* alphas = g_new(gdouble, in_width * in_height * 2);  // vert and hor ==> 2 channels
    calc_alphas(in_img_array, alphas, in_height, in_width);

    gint out_width = vals->x_size_out;
    gint out_height = vals->y_size_out;
    gdouble x_fact = (gdouble)out_width / in_width;
    gdouble y_fact = (gdouble)out_height / in_height;

    gimp_image_undo_group_start(gimp_item_get_image(drawable->drawable_id));

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

        // TODO Check if y first then x is faster
        for (int ix = 0; ix < out_width; ++ix)
            for (int iy = 0; iy < out_height; ++iy) {
                gdouble out_x = out_to_in_coord(ix, x_fact);
                gdouble out_y = out_to_in_coord(iy, y_fact);
                gdouble u = out_x - floor(out_x);
                gdouble v = out_y - floor(out_y);

                for (int ic = 0; ic < 3; ++ic) {
                    // TODO Edge cases?
                    gdouble w00 = in_img_array[to_1d_index(CLAMP(floor(out_x), 0, in_width - 1),
                                                           CLAMP(floor(out_y), 0, in_height - 1),
                                                           ic, in_width, 3)];
                    gdouble w10 = in_img_array[to_1d_index(CLAMP(ceil(out_x), 0, in_width - 1),
                                                           CLAMP(floor(out_y), 0, in_height - 1),
                                                           ic, in_width, 3)];
                    gdouble w01 = in_img_array[to_1d_index(CLAMP(floor(out_x), 0, in_width - 1),
                                                           CLAMP(ceil(out_y), 0, in_height - 1), ic,
                                                           in_width, 3)];
                    gdouble w11 = in_img_array[to_1d_index(CLAMP(ceil(out_x), 0, in_width - 1),
                                                           CLAMP(ceil(out_y), 0, in_height - 1), ic,
                                                           in_width, 3)];

                    out_img_array[to_1d_index(ix, iy, ic, out_width, 3)] = (1 - u) * (1 - v) * w00 +
                                                                           u * (1 - v) * w10 +  //
                                                                           (1 - u) * v * w01 +  //
                                                                           u * v * w11;
                }

                // DEBUG Write alphas to image
                // Keep image size unchanged for this to work
                guchar debug1 =
                    CLAMP(round(alphas[to_1d_index(ix, iy, 0, in_width, 2)] * 255.0 * 2.0), 0, 255);
                guchar debug2 =
                    CLAMP(round(alphas[to_1d_index(ix, iy, 1, in_width, 2)] * 255.0 * 2.0), 0, 255);
                out_img_array[to_1d_index(ix, iy, 0, in_width, 3)] = debug2;
                out_img_array[to_1d_index(ix, iy, 1, in_width, 3)] = debug1;
                out_img_array[to_1d_index(ix, iy, 2, in_width, 3)] = debug2;

                // TODO How should this be calculated when using two-pass?
                if (ix % 10 == 0)
                    gimp_progress_update((gdouble)(ix) / (gdouble)(out_width));
            }

        gimp_pixel_rgn_set_rect(&dest_rgn, (guchar*)out_img_array, 0, 0, out_width, out_height);

        gimp_drawable_flush(resized_drawable);
        gimp_drawable_merge_shadow(resized_drawable->drawable_id, TRUE);
        gimp_drawable_update(resized_drawable->drawable_id, x1, y1, out_width, out_height);

        gimp_drawable_detach(resized_drawable);
    }

    g_free(in_img_array);
    g_free(alphas);
    gimp_image_undo_group_end(gimp_item_get_image(drawable->drawable_id));
}

// Not alpha as in alpha channel
static void calc_alphas(guchar* in_img_array,  // height * width * num_channels of original img
                        gdouble* alphas,       // out, height * width * 2 (vert & hor)
                        gint height,
                        gint width) {
    for (gint ix = 0; ix < width; ++ix)
        for (gint iy = 0; iy < height; ++iy) {
            // Vertical
            if (iy > 0 && iy < height - 1) {
                gdouble d0[3], d1[3];
                for (gint ic = 0; ic < 3; ++ic) {
                    d0[ic] = (gdouble)in_img_array[to_1d_index(ix, iy, ic, width, 3)] -
                             (gdouble)in_img_array[to_1d_index(ix, iy - 1, ic, width, 3)];
                    d1[ic] = (gdouble)in_img_array[to_1d_index(ix, iy + 1, ic, width, 3)] -
                             (gdouble)in_img_array[to_1d_index(ix, iy, ic, width, 3)];
                }
                alphas[to_1d_index(ix, iy, 0, width, 2)] = alpha_from_gradients(d0, d1);
            } else {
                alphas[to_1d_index(ix, iy, 0, width, 2)] = 0.0;
            }

            // Horizontal
            if (ix > 0 && ix < width - 1) {
                gdouble d0[3], d1[3];
                for (gint ic = 0; ic < 3; ++ic) {
                    d0[ic] = (gdouble)in_img_array[to_1d_index(ix, iy, ic, width, 3)] -
                             (gdouble)in_img_array[to_1d_index(ix - 1, iy, ic, width, 3)];
                    d1[ic] = (gdouble)in_img_array[to_1d_index(ix + 1, iy, ic, width, 3)] -
                             (gdouble)in_img_array[to_1d_index(ix, iy, ic, width, 3)];
                }
                alphas[to_1d_index(ix, iy, 1, width, 2)] = alpha_from_gradients(d0, d1);
            } else {
                alphas[to_1d_index(ix, iy, 1, width, 2)] = 0.0;
            }
        }
}

static gdouble alpha_from_gradients(gdouble d0[], gdouble d1[]) {
    gdouble alpha = 0.0;
    gboolean changed = FALSE;

    for (gint ic = 0; ic < 3; ++ic) {
        // Ignore if any gradient is zero. If all are zero, then so is alpha.
        if (d0[ic] != 0.0 || d1[ic] != 0.0) {
            double maybe_alpha = 0.0;

            // If sgn(d0)==sgn(d1)
            if ((d0[ic] > 0.0 && d1[ic] > 0.0) || (d0[ic] < 0.0 && d1[ic] < 0.0)) {
                double td = 0.5 * (d0[ic] + d1[ic]);
                maybe_alpha = 0.5;

                if (fabs(d0[ic]) < fabs(td))
                    maybe_alpha = fabs(d0[ic]) / fabs(td) * 0.5;
                if (fabs(d1[ic]) < fabs(td))
                    maybe_alpha = fabs(d1[ic]) / fabs(td) * 0.5;
            }

            if (!changed || (alpha > maybe_alpha)) {
                alpha = maybe_alpha;
                changed = TRUE;
            }
        }
    }

    return alpha;
}

static inline gdouble out_to_in_coord(gint out, gdouble factor) {
    return (out + 0.5) * (1 / factor) - 0.5;
}

static inline gint to_1d_index(gint x, gint y, gint channel, gint width, gint num_channels) {
    assert(x >= 0 && y >= 0 && channel >= 0 && channel < num_channels && x < width);
    return (y * width * num_channels) + x * num_channels + channel;
}
