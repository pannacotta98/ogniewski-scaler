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
static void calc_alphas(guchar* in_img_array, gdouble* alphas, gint height, gint width);
static gdouble alpha_from_gradients(gdouble d0[], gdouble d1[]);

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
                gdouble in_x = out_to_in_coord(ix, x_fact);
                gdouble in_y = out_to_in_coord(iy, y_fact);
                gint h = CLAMP((gint)in_y, 0, in_height - 1);
                gint g = CLAMP((gint)in_x, 0, in_width - 1);
                gdouble bx = in_x - floor(in_x) - 0.5;
                gdouble by = in_y - floor(in_y) - 0.5;
                gint signx = (bx >= 0) ? 1 : -1;
                gint signy = (by >= 0) ? 1 : -1;
                bx = ABS(bx);
                by = ABS(by);

                double ax0 = 0.0;
                double ax1 = 0.0;
                double ay0 = 0.0;
                double ay1 = 0.0;

                gdouble bc[3 * 3 * 3];  // 3x3 neighborhood, with RGB
                for (gint dy = 0; dy < 3; ++dy)
                    for (gint dx = 0; dx < 3; ++dx) {
                        gint tx = CLAMP(dx * signx - signx + g, 0, in_width - 1);
                        gint ty = CLAMP(dy * signy - signy + h, 0, in_height - 1);

                        bc[(dy * 3 + dx) * 3] = in_img_array[to_1d_index(tx, ty, 0, in_width, 3)];
                        bc[(dy * 3 + dx) * 3 + 1] =
                            in_img_array[to_1d_index(tx, ty, 1, in_width, 3)];
                        bc[(dy * 3 + dx) * 3 + 2] =
                            in_img_array[to_1d_index(tx, ty, 2, in_width, 3)];

                        ax0 = alphas[to_1d_index(g, h, 0, in_width, 2)] * (1 - 2 * by * by) +
                              alphas[to_1d_index(g, ty, 0, in_width, 2)] * 2 * by * by;
                        ax1 = alphas[to_1d_index(tx, h, 0, in_width, 2)] * (1 - 2 * by * by) +
                              alphas[to_1d_index(tx, ty, 0, in_width, 2)] * 2 * by * by;
                        ay0 = alphas[to_1d_index(g, h, 1, in_width, 2)] * (1 - 2 * bx * bx) +
                              alphas[to_1d_index(tx, h, 1, in_width, 2)] * 2 * bx * bx;
                        ay1 = alphas[to_1d_index(g, ty, 1, in_width, 2)] * (1 - 2 * bx * bx) +
                              alphas[to_1d_index(tx, ty, 1, in_width, 2)] * 2 * bx * bx;
                    }

                gdouble ad4x = 3 * bx * bx - 4 * bx * bx * bx;
                gdouble ad4y = 3 * by * by - 4 * by * by * by;

                gdouble wx[3], wy[3];
                wx[0] = ax0 * (bx * bx - bx + ad4x);
                wx[1] = 1.0 - bx * bx - (1.0 + ax0 * 2.0 - ax1 * 2.0) * ad4x;
                wx[2] =
                    (1.0 - ax0) * bx * bx + ax0 * bx + ((1.0 + ax0 * 2.0 - ax1 * 2.0) - ax0) * ad4x;
                wy[0] = ay0 * (by * by - by + ad4y);
                wy[1] = 1.0 - by * by - (1.0 + ay0 * 2.0 - ay1 * 2.0) * ad4y;
                wy[2] =
                    (1.0 - ay0) * by * by + ay0 * by + ((1.0 + ay0 * 2.0 - ay1 * 2.0) - ay0) * ad4y;

                for (gint c = 0; c < 3; c++) {
                    gdouble tc = bc[c] * wx[0] * wy[0] + bc[3 + c] * wx[1] * wy[0] +
                                 bc[6 + c] * wx[2] * wy[0] + bc[9 + c] * wx[0] * wy[1] +
                                 bc[12 + c] * wx[1] * wy[1] + bc[15 + c] * wx[2] * wy[1] +
                                 bc[18 + c] * wx[0] * wy[2] + bc[21 + c] * wx[1] * wy[2] +
                                 bc[24 + c] * wx[2] * wy[2];

                    out_img_array[to_1d_index(ix, iy, c, out_width, 3)] =
                        roundf(CLAMP(tc, 0.0, 255.0));
                }

                // TODO How should this be calculated when using two-pass?
                if (ix % 10 == 0)
                    gimp_progress_update((gdouble)(ix) / (gdouble)(out_width));
            }

        gimp_pixel_rgn_set_rect(&dest_rgn, (guchar*)out_img_array, 0, 0, out_width, out_height);

        gimp_drawable_flush(resized_drawable);
        gimp_drawable_merge_shadow(resized_drawable->drawable_id, TRUE);
        gimp_drawable_update(resized_drawable->drawable_id, x1, y1, out_width, out_height);

        gimp_drawable_detach(resized_drawable);

        g_free(out_img_array);
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
            // Horizontal
            if (ix > 0 && ix < width - 1) {
                gdouble d0[3], d1[3];
                for (gint ic = 0; ic < 3; ++ic) {
                    d0[ic] = (gdouble)in_img_array[to_1d_index(ix, iy, ic, width, 3)] -
                             (gdouble)in_img_array[to_1d_index(ix - 1, iy, ic, width, 3)];
                    d1[ic] = (gdouble)in_img_array[to_1d_index(ix + 1, iy, ic, width, 3)] -
                             (gdouble)in_img_array[to_1d_index(ix, iy, ic, width, 3)];
                }
                alphas[to_1d_index(ix, iy, 0, width, 2)] = alpha_from_gradients(d0, d1);
            } else {
                alphas[to_1d_index(ix, iy, 0, width, 2)] = 0.0;
            }

            // Vertical
            if (iy > 0 && iy < height - 1) {
                gdouble d0[3], d1[3];
                for (gint ic = 0; ic < 3; ++ic) {
                    d0[ic] = (gdouble)in_img_array[to_1d_index(ix, iy, ic, width, 3)] -
                             (gdouble)in_img_array[to_1d_index(ix, iy - 1, ic, width, 3)];
                    d1[ic] = (gdouble)in_img_array[to_1d_index(ix, iy + 1, ic, width, 3)] -
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
    return (out + 0.5) * (1 / factor);
}

static inline gint to_1d_index(gint x, gint y, gint channel, gint width, gint num_channels) {
    assert(x >= 0 && y >= 0 && channel >= 0 && channel < num_channels && x < width);
    return (y * width * num_channels) + x * num_channels + channel;
}
