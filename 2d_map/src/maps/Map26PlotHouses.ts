// Colour Blocks — step 13: one house on each plot (Map26Plots), at its front, facing its road.
//
// The house is turned to the road's direction at the plot's frontage, a little back from the road
// (SETBACK), a random size near houseSize but no wider than the plot allows (its frontage less a
// gap) and no deeper (its area over its frontage, less the setback and a back garden). It has to
// be off the roads and nearly all on its own plot (onPlot); if not, it is tried smaller, three
// times, and else the plot stays empty. Some plots are left empty anyway (houseFill).
import type { House, Houses } from './Map26Houses';
import { PLOT, ROAD, type Plot, type Plots } from './Map26Plots';
import type { Settings } from './Map26Settings';

const SETBACK = 0.3; // fine cells from the plot's front edge to the house's front
const GARDEN = 0.4; // fine cells: the least left behind a house
const GAP = 0.35; // fine cells: a house is at least this much narrower than its plot's frontage
const ROOFS = 6;
const INSET = 0.25;
const OWN = 0.85; // the share of a house that must be on its own plot // fine cells: a house's edges are tested this far inside (the plot's edge is a staircase)

// House p_h on plot p_k? None of it on a road, and nearly all of it (OWN) on its own plot: a plot's
// edge is a staircase of cells, so a little over it is let go. (Checked every half plot cell over
// it, a little inside its edges.)
function onPlot(p_h: House, p_plots: Plots, p_k: number): boolean {
    const ux = Math.cos(p_h.angle);
    const uy = Math.sin(p_h.angle);
    const n = p_plots.n;
    const ea = INSET / p_h.width;
    const eb = INSET / p_h.depth;
    let all = 0;
    let own = 0;
    for (let a = -0.5 + ea; a <= 0.5 - ea; a += 0.25 / (p_h.width * PLOT)) {
        for (let b = -0.5 + eb; b <= 0.5 - eb; b += 0.25 / (p_h.depth * PLOT)) {
            const i = Math.floor((p_h.x + ux * a * p_h.width - uy * b * p_h.depth) * PLOT);
            const j = Math.floor((p_h.y + uy * a * p_h.width + ux * b * p_h.depth) * PLOT);
            if (i < 0 || j < 0 || i >= n || j >= n || p_plots.id[j * n + i] === ROAD) return false;
            all++;
            if (p_plots.id[j * n + i] === p_k) own++;
        }
    }
    return own >= all * OWN;
}

function houseOn(p_plot: Plot, p_width: number, p_depth: number, p_roof: number): House {
    const back = SETBACK + p_depth / 2;
    return {
        x: p_plot.front.x + p_plot.inward.x * back,
        y: p_plot.front.y + p_plot.inward.y * back,
        angle: Math.atan2(p_plot.along.y, p_plot.along.x),
        width: p_width,
        depth: p_depth,
        roof: p_roof,
    };
}

export function housesOnPlots(p_plots: Plots, p_settings: Settings, p_random: () => number): Houses {
    const houses: House[] = [];
    const size = p_settings.houseSize;
    p_plots.plots.forEach((plot, k) => {
        const width = Math.min(plot.frontage - GAP, size * (0.85 + 0.35 * p_random()));
        const depth = Math.min(plot.area / Math.max(1, plot.frontage) - SETBACK - GARDEN, size * (0.85 + 0.4 * p_random()));
        const roof = Math.floor(p_random() * ROOFS);
        if (p_random() >= p_settings.houseFill || width < size * 0.5 || depth < size * 0.5) return;
        for (const shrink of [1, 0.85, 0.7, 0.55]) {
            const h = houseOn(plot, width * shrink, depth * shrink, roof);
            if (onPlot(h, p_plots, k)) {
                houses.push(h);
                return;
            }
        }
    });
    return { houses };
}
