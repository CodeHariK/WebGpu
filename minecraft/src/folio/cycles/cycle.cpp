#include "cycle.h"

#include <cmath>

using namespace godot;

// folio Cycles.createKeyframes(): append a copy of the first stop past 1.0 and
// prepend a copy of the last stop below 0.0 so progress always sits between two
// stops (seamless loop). Capture the original first/last before mutating.
void FolioCycle::finalize() {
	if (finalized || stops.size() == 0) {
		return;
	}

	const int n = stops.size();
	const double first_stop = stops[0];
	const double last_stop = stops[n - 1];

	// Append fake-last = copy of first, at stop 1 + first_stop.
	if (last_stop < 1.0) {
		stops.push_back(1.0 + first_stop);
		for (KeyValue<String, Vector<double>> &kv : float_tracks) {
			kv.value.push_back(kv.value[0]);
		}
		for (KeyValue<String, Vector<Color>> &kv : color_tracks) {
			kv.value.push_back(kv.value[0]);
		}
	}

	// Prepend fake-first = copy of last, at stop -(1 - last_stop).
	if (first_stop > 0.0) {
		stops.insert(0, -(1.0 - last_stop));
		for (KeyValue<String, Vector<double>> &kv : float_tracks) {
			kv.value.insert(0, kv.value[n - 1]);
		}
		for (KeyValue<String, Vector<Color>> &kv : color_tracks) {
			kv.value.insert(0, kv.value[n - 1]);
		}
	}

	finalized = true;
}

void FolioCycle::update(double p_elapsed_seconds) {
	if (stops.size() == 0) {
		return;
	}

	// Progress in [0, 1).
	if (forced_progress >= 0.0) {
		progress = forced_progress;
	} else {
		double d = (duration > 0.0) ? duration : 1.0;
		progress = std::fmod(p_elapsed_seconds / d + phase_offset, 1.0);
		if (progress < 0.0) {
			progress += 1.0;
		}
	}

	// Surrounding stops: index_prev = last stop <= progress.
	int index_prev = 0;
	for (int i = 0; i < stops.size(); i++) {
		if (stops[i] <= progress) {
			index_prev = i;
		}
	}
	int index_next = index_prev + 1;
	if (index_next >= stops.size()) {
		index_next = stops.size() - 1;
	}

	const double prev_stop = stops[index_prev];
	const double next_stop = stops[index_next];

	// smoothstep(progress) between the two stops.
	double t = 0.0;
	const double span = next_stop - prev_stop;
	if (span > 1e-9) {
		t = (progress - prev_stop) / span;
		t = t < 0.0 ? 0.0 : (t > 1.0 ? 1.0 : t);
	}
	const double s = t * t * (3.0 - 2.0 * t);

	// Interpolate every track.
	for (const KeyValue<String, Vector<double>> &kv : float_tracks) {
		const Vector<double> &v = kv.value;
		const double a = v[index_prev];
		const double b = v[index_next];
		float_values[kv.key] = a + (b - a) * s;
	}
	for (const KeyValue<String, Vector<Color>> &kv : color_tracks) {
		const Vector<Color> &v = kv.value;
		color_values[kv.key] = v[index_prev].lerp(v[index_next], s);
	}
}

void FolioCycle::seek(double p_target_phase, double p_elapsed_seconds) {
	forced_progress = -1.0; // resume time-driven progression
	double d = (duration > 0.0) ? duration : 1.0;
	double base = std::fmod(p_elapsed_seconds / d, 1.0);
	if (base < 0.0) {
		base += 1.0;
	}
	phase_offset = p_target_phase - base; // so progress == target right now
}

double FolioCycle::get_float(const String &p_name) const {
	const double *p = float_values.getptr(p_name);
	return p ? *p : 0.0;
}

Color FolioCycle::get_color(const String &p_name) const {
	const Color *p = color_values.getptr(p_name);
	return p ? *p : Color(0, 0, 0);
}
