/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "win_state.h"
#include "vo.h"

#include "video/mp_image.h"

double vo_calc_monitor_par(struct mp_vo_opts *opts, const struct mp_rect *screen)
{
    double pixelaspect = 1.0 / opts->monitor_pixel_aspect;

    int scr_w = mp_rect_w(*screen);
    int scr_h = mp_rect_h(*screen);
    if (scr_w > 0 && scr_h > 0 && opts->force_monitor_aspect)
        pixelaspect = opts->force_monitor_aspect * scr_h / scr_w;

    return pixelaspect;
}

// Fit *w/*h into the size specified by geo.
static void apply_autofit(int *w, int *h, int scr_w, int scr_h,
                          struct m_geometry *geo, bool allow_upscale)
{
    if (!geo->wh_valid)
        return;

    int dummy;
    int n_w = *w, n_h = *h;
    m_geometry_apply(&dummy, &dummy, &n_w, &n_h, scr_w, scr_h, geo);

    if (!allow_upscale && *w <= n_w && *h <= n_h)
        return;

    // If aspect mismatches, always make the window smaller than the fit box
    double asp = (double)*w / *h;
    double n_asp = (double)n_w / n_h;
    if (n_asp <= asp) {
        *w = n_w;
        *h = n_w / asp;
    } else {
        *w = n_h * asp;
        *h = n_h;
    }
}

// Compute the "suggested" window size and position and return it in *out_geo.
// screen is the bounding box of the current screen within the virtual desktop.
// Does not change *vo.
// Use vo_apply_window_geometry() to copy the result into the vo.
// NOTE: currently, all windowing backends do their own handling of window
//       geometry additional to this code. This is to deal with initial window
//       placement, fullscreen handling, avoiding resize on reconfig() with no
//       size change, multi-monitor stuff, and possibly more.
void vo_calc_window_geometry(struct vo *vo, const struct mp_rect *screen,
                             struct vo_win_geometry *out_geo)
{


    // The case of calling this function even though no video was configured
    // yet (i.e. vo->params==NULL) happens when vo_opengl creates a hidden
    // window in order to create an OpenGL context.
    struct mp_image_params params = { .d_w = 320, .d_h = 200 };
    if (vo->params)
        params = *vo->params;

    int d_w = params.d_w;
    int d_h = params.d_h;
    if ((vo->driver->caps & VO_CAP_ROTATE90) && params.rotate % 180 == 90)
        MPSWAP(int, d_w, d_h);

    vo_calc_window_size(vo->opts, d_w, d_h, screen, out_geo);
}

void vo_calc_window_size(struct mp_vo_opts *opts, int d_w, int d_h,
                         const struct mp_rect *screen,
                         struct vo_win_geometry *out_geo)
{
    *out_geo = (struct vo_win_geometry){0};

    int scr_w = screen->x1 - screen->x0;
    int scr_h = screen->y1 - screen->y0;

    double monitor_par = vo_calc_monitor_par(opts, screen);
    if (monitor_par < 1) {
        scr_h /= monitor_par;
    } else {
        scr_w *= monitor_par;
    }

    apply_autofit(&d_w, &d_h, scr_w, scr_h, &opts->autofit, true);
    apply_autofit(&d_w, &d_h, scr_w, scr_h, &opts->autofit_larger, false);

    out_geo->win.x0 = (int)(scr_w - d_w) / 2;
    out_geo->win.y0 = (int)(scr_h - d_h) / 2;
    m_geometry_apply(&out_geo->win.x0, &out_geo->win.y0, &d_w, &d_h,
                     scr_w, scr_h, &opts->geometry);

    out_geo->win.x0 += screen->x0;
    out_geo->win.y0 += screen->y0;
    out_geo->win.x1 = out_geo->win.x0 + d_w;
    out_geo->win.y1 = out_geo->win.y0 + d_h;

    if (opts->geometry.xy_valid || opts->force_window_position)
        out_geo->flags |= VO_WIN_FORCE_POS;
}

// Copy the parameters in *geo to the vo fields.
// (Doesn't do anything else - windowing backends should trigger VO_EVENT_RESIZE
//  to ensure that the VO reinitializes rendering properly.)
void vo_apply_window_geometry(struct vo *vo, const struct vo_win_geometry *geo)
{
    vo->dwidth = geo->win.x1 - geo->win.x0;
    vo->dheight = geo->win.y1 - geo->win.y0;
    vo->monitor_par = geo->monitor_par;
}
