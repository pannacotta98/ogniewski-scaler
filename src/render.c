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

/*  Public functions  */

void render(gint32 image_ID,
            GimpDrawable* drawable,
            PlugInVals* vals,
            PlugInImageVals* image_vals,
            PlugInDrawableVals* drawable_vals) {
    gint x1, y1, x2, y2;
    gimp_drawable_mask_bounds(drawable->drawable_id, &x1, &y1, &x2, &y2);
    gint channels = gimp_drawable_bpp(drawable->drawable_id);
    gint width = x2 - x1;
    gint height = y2 - y1;

    gimp_tile_cache_ntiles(2 * (drawable->width / gimp_tile_width() + 1));

    GimpPixelRgn rgn_in, rgn_out;
    gimp_pixel_rgn_init(&rgn_in, drawable, x1, y1, width, height, FALSE, FALSE);
    gimp_pixel_rgn_init(&rgn_out, drawable, x1, y1, width, height, TRUE, TRUE);

    /* Initialise enough memory for row1, row2, row3, outrow */
    guchar* row1 = g_new(guchar, channels * width);
    guchar* row2 = g_new(guchar, channels * width);
    guchar* row3 = g_new(guchar, channels * width);
    guchar* outrow = g_new(guchar, channels * width);

    for (gint i = y1; i < y2; i++) {
        /* Get row i-1, i, i+1 */
        gimp_pixel_rgn_get_row(&rgn_in, row1, x1, MAX(y1, i - 1), width);
        gimp_pixel_rgn_get_row(&rgn_in, row2, x1, i, width);
        gimp_pixel_rgn_get_row(&rgn_in, row3, x1, MIN(y2 - 1, i + 1), width);

        for (gint j = x1; j < x2; j++) {
            /* For each layer, compute the average of the nine
             * pixels */
            for (gint k = 0; k < channels; k++) {
                int sum = 0;
                sum = row1[channels * MAX((j - 1 - x1), 0) + k] + row1[channels * (j - x1) + k] +
                      row1[channels * MIN((j + 1 - x1), width - 1) + k] +
                      row2[channels * MAX((j - 1 - x1), 0) + k] + row2[channels * (j - x1) + k] +
                      row2[channels * MIN((j + 1 - x1), width - 1) + k] +
                      row3[channels * MAX((j - 1 - x1), 0) + k] + row3[channels * (j - x1) + k] +
                      row3[channels * MIN((j + 1 - x1), width - 1) + k];
                outrow[channels * (j - x1) + k] = sum / 9;
            }
        }

        gimp_pixel_rgn_set_row(&rgn_out, outrow, x1, i, width);

        if (i % 10 == 0)
            gimp_progress_update((gdouble)(i - y1) / (gdouble)(height));
    }

    g_free(row1);
    g_free(row2);
    g_free(row3);
    g_free(outrow);

    gimp_drawable_flush(drawable);
    gimp_drawable_merge_shadow(drawable->drawable_id, TRUE);
    gimp_drawable_update(drawable->drawable_id, x1, y1, width, height);

    g_message(_("Lakrits2"));
}
