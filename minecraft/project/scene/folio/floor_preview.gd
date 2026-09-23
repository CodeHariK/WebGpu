extends Node3D

# Debug HUD / controls (enabled with --debug). See _unhandled_key_input for keys.
var _debug := false
var _hud: Label = null
var _fast := false
var _day_min := 4.0   # day/night loop length (minutes) — matches C++ default 240s
var _year_min := 30.0 # year loop length (minutes) in normal mode (fast mode = 5)
# Year-length presets (Y key cycles) + parameter logger (L key).
var _year_lengths := [2.0, 5.0, 10.0, 20.0]
var _year_idx := 3
var _log_on := false
var _log_interval := 1.0 # seconds of game-time between samples
var _log_samples := []
var _log_accum := 0.0
var _log_total := 0.0
var _t_snow := 0.0
var _t_rain := 0.0
var _t_leaves := 0.0
var _temp_min := 999.0
var _temp_max := -999.0

# Folio floor/island preview. `make run_folio_floor`. `--phase=<0..1>` locks a
# day phase; `--shot=<name>` saves a screenshot and quits.
func _ready() -> void:
	# Cap the framerate so the preview doesn't run flat-out (default 60; --fps=N).
	var fps := 60
	for a in OS.get_cmdline_args() + OS.get_cmdline_user_args():
		if a.begins_with("--fps="):
			fps = int(a.substr(6))
	Engine.max_fps = fps
	await get_tree().process_frame
	if has_node("Cam"):
		$Cam.current = true
		$Cam.look_at(Vector3(0, 0, 0), Vector3(0, 1, 0))
	var phase := -1.0
	var shot := ""
	for a in OS.get_cmdline_args() + OS.get_cmdline_user_args():
		if a.begins_with("--phase="):
			phase = a.substr(8).to_float()
		elif a.begins_with("--shot="):
			shot = a.substr(7)
	if OS.get_cmdline_args().has("--nofog") or OS.get_cmdline_user_args().has("--nofog"):
		var fm := get_node_or_null("Floor/FloorMesh")
		if fm and fm.material_override:
			fm.material_override.set_shader_parameter("has_fog", false)
	var g := get_node_or_null("Game")
	# Apply the normal-mode loop lengths up front so the preview matches the HUD
	# (F toggles fast: 5 min year / 0.4 min day; normal: 30 min year / 4 min day).
	if g and g.has_method("get_year_cycles"):
		g.get_year_cycles().set_duration(_year_min * 60.0)
	if g and g.has_method("get_day_cycles"):
		g.get_day_cycles().set_duration(_day_min * 60.0)
	if g and g.has_method("get_day_cycles") and phase >= 0.0:
		g.get_day_cycles().set_progress_override(phase)
	# --weather=cold|rain|clear forces Weather so ice / rain splashes are testable.
	var weather := ""
	for a in OS.get_cmdline_args() + OS.get_cmdline_user_args():
		if a.begins_with("--weather="):
			weather = a.substr(10)
	if g and g.has_method("get_weather") and weather != "":
		var w = g.get_weather()
		if weather == "cold":
			w.set_override({"temperature": -6.0, "rain": 0.0}, 1.0)
		elif weather == "rain":
			w.set_override({"temperature": 14.0, "rain": 1.0, "humidity": 1.0, "clouds": 1.0}, 1.0)
		elif weather == "snow":
			w.set_override({"temperature": -6.0, "rain": 0.9, "humidity": 1.0, "clouds": 1.0, "snow": 0.9, "wind": 0.4}, 1.0)
		elif weather == "clear":
			w.clear_override()
	# --season=winter|spring|summer|fall locks the YearCycles phase (seasonal test).
	var season := ""
	for a in OS.get_cmdline_args() + OS.get_cmdline_user_args():
		if a.begins_with("--season="):
			season = a.substr(9)
	if g and g.has_method("get_year_cycles") and season != "":
		var yc = g.get_year_cycles()
		var season_phase = {"winter": 0.125, "spring": 0.375, "summer": 0.625, "fall": 0.875}.get(season, -1.0)
		if season_phase >= 0.0:
			yc.set_progress_override(season_phase)
	# --debug: live environment HUD + debug keys.
	if OS.get_cmdline_args().has("--debug") or OS.get_cmdline_user_args().has("--debug"):
		_debug = true
		_build_hud()

	if shot != "":
		await get_tree().create_timer(0.8).timeout
		var img := get_viewport().get_texture().get_image()
		img.save_png("res://../docs/png/" + shot + ".png")
		get_tree().quit()


func _build_hud() -> void:
	var layer := CanvasLayer.new()
	layer.layer = 128
	add_child(layer)
	_hud = Label.new()
	_hud.position = Vector2(14, 12)
	_hud.add_theme_color_override("font_color", Color(1, 1, 1))
	_hud.add_theme_font_size_override("font_size", 15)
	_hud.add_theme_color_override("font_outline_color", Color(0, 0, 0, 0.85))
	_hud.add_theme_constant_override("outline_size", 4)
	layer.add_child(_hud)

func _process(delta: float) -> void:
	if not _debug or _hud == null:
		return
	var g := get_node_or_null("Game")
	if g == null:
		return
	_hud.text = _hud_text(g)
	if _log_on:
		_log_step(g, delta)

func _day_phase(p: float) -> String:
	if p < 0.2: return "day"
	elif p < 0.32: return "dusk"
	elif p < 0.7: return "night"
	elif p < 0.85: return "dawn"
	return "day"

func _season(p: float) -> String:
	if p < 0.25: return "winter"
	elif p < 0.5: return "spring"
	elif p < 0.75: return "summer"
	return "fall"

func _hud_text(g) -> String:
	var lines: Array = []
	lines.append("FPS %d" % Engine.get_frames_per_second())
	# Cycles
	if g.has_method("get_day_cycles"):
		var dp: float = g.get_day_cycles().get_progress()
		lines.append("Day   %3d%%  %-6s (%.1f min loop)" % [int(dp * 100.0), _day_phase(dp), _day_min])
	if g.has_method("get_year_cycles"):
		var yc = g.get_year_cycles()
		var yp: float = yc.get_progress()
		lines.append("Year  %3d%%  %-6s (%.1f min loop)" % [int(yp * 100.0), _season(yp), _year_min])
		lines.append("  season: leaves %.2f  temp %+.0f  hum %.2f" % [yc.get_leaves(), yc.get_temperature(), yc.get_humidity()])
		if yc.has_method("get_winter_temperature"):
			lines.append("  winter min %+.0f C  ([ colder / ] warmer)" % yc.get_winter_temperature())
	lines.append("Speed %s   year %.0fm   log %s" % [("FAST" if _fast else "normal"), _year_min, ("REC" if _log_on else "off")])
	# Weather
	if g.has_method("get_weather"):
		var w = g.get_weather()
		lines.append("--- weather ---")
		lines.append("Temp   %+6.1f C" % w.get_temperature())
		lines.append("Humid   %5.2f" % w.get_humidity())
		lines.append("Clouds %+5.2f" % w.get_clouds())
		lines.append("Wind    %5.2f" % w.get_wind())
		lines.append("Rain    %5.2f" % w.get_rain())
		var snow: float = w.get_snow()
		lines.append("Snow    %5.2f %s" % [snow, ("<-- SNOWING" if snow > 0.01 else "")])
	lines.append("--- keys: 1-4 season | 0 real | F fast | S snow | X clear | Y year-len | [ ] winterT | L log | H hide ---")
	return "\n".join(lines)

func _unhandled_key_input(event: InputEvent) -> void:
	if not _debug:
		return
	if not (event is InputEventKey) or not event.pressed or event.echo:
		return
	var g := get_node_or_null("Game")
	if g == null:
		return
	match event.keycode:
		KEY_1: _season_preview(g, 0.125) # winter (snows on its own)
		KEY_2: _season_preview(g, 0.375) # spring
		KEY_3: _season_preview(g, 0.625) # summer
		KEY_4: _season_preview(g, 0.875) # fall
		KEY_0: _resume(g)
		KEY_F: _toggle_fast(g)
		KEY_S: _force_snow(g)
		KEY_X: _clear_weather(g)
		KEY_Y: _cycle_year_length(g)
		KEY_BRACKETLEFT: _adjust_winter_temp(g, -1.0)
		KEY_BRACKETRIGHT: _adjust_winter_temp(g, 1.0)
		KEY_L: _toggle_log(g)
		KEY_H:
			if _hud:
				_hud.visible = not _hud.visible

func _season_preview(g, phase: float) -> void:
	# Jump to this season and KEEP the year progressing from there (seek, not lock),
	# and clear any weather override so the season plays out naturally.
	if g.has_method("get_year_cycles"):
		var yc = g.get_year_cycles()
		if yc.has_method("seek_season"):
			yc.seek_season(phase)
		else:
			yc.set_progress_override(phase)
	if g.has_method("get_weather"):
		g.get_weather().clear_override()

func _resume(g) -> void:
	if g.has_method("get_year_cycles"):
		g.get_year_cycles().set_progress_override(-1.0)
	if g.has_method("get_day_cycles"):
		g.get_day_cycles().set_progress_override(-1.0)

func _force_snow(g) -> void:
	# Force winter + a cold, wet override so snow appears immediately.
	if g.has_method("get_year_cycles"):
		g.get_year_cycles().set_progress_override(0.125)
	if g.has_method("get_weather"):
		g.get_weather().set_override({"temperature": -6.0, "rain": 0.9, "humidity": 1.0, "clouds": 1.0, "snow": 0.9, "wind": 0.4}, 1.0)

func _clear_weather(g) -> void:
	if g.has_method("get_weather"):
		g.get_weather().clear_override()

func _adjust_winter_temp(g, delta: float) -> void:
	# Nudge the winter temperature keyframe live (colder = more reliable snow).
	if g.has_method("get_year_cycles"):
		var yc = g.get_year_cycles()
		if yc.has_method("set_winter_temperature"):
			var t: float = yc.get_winter_temperature() + delta
			yc.set_winter_temperature(t)
			print("[winter] min temp = %+.0f C" % t)

func _toggle_fast(g) -> void:
	_fast = not _fast
	_day_min = 0.4 if _fast else 4.0
	_year_min = 5.0 if _fast else 30.0
	if g.has_method("get_day_cycles"):
		g.get_day_cycles().set_duration(_day_min * 60.0)
	if g.has_method("get_year_cycles"):
		g.get_year_cycles().set_duration(_year_min * 60.0)
	# Speed the weather noise + shader animation via the world time scale.
	if g.has_method("get_time"):
		g.get_time().set_default_scale(8.0 if _fast else 2.0)


func _cycle_year_length(g) -> void:
	_year_idx = (_year_idx + 1) % _year_lengths.size()
	_year_min = _year_lengths[_year_idx]
	if g.has_method("get_year_cycles"):
		g.get_year_cycles().set_duration(_year_min * 60.0)
	print("[year] length = %.0f min" % _year_min)

func _toggle_log(g) -> void:
	_log_on = not _log_on
	if _log_on:
		_log_accum = 0.0
		_log_total = 0.0
		_t_snow = 0.0
		_t_rain = 0.0
		_t_leaves = 0.0
		_temp_min = 999.0
		_temp_max = -999.0
		_log_samples = []
		print("[log] START (year = %.0f min) -- collecting silently; press L again to write docs/env_report.html" % _year_min)
	else:
		_log_summary()

func _log_step(g, delta: float) -> void:
	if not g.has_method("get_weather") or not g.has_method("get_year_cycles"):
		return
	var w = g.get_weather()
	var yc = g.get_year_cycles()
	var temp: float = w.get_temperature()
	var rain: float = w.get_rain()
	var snow: float = w.get_snow()
	var leaves: float = yc.get_leaves()

	_log_total += delta
	_log_accum += delta
	if snow > 0.01: _t_snow += delta
	if rain > 0.05: _t_rain += delta
	if leaves > 0.5: _t_leaves += delta
	_temp_min = min(_temp_min, temp)
	_temp_max = max(_temp_max, temp)

	# Collect a sample silently (no per-line spam); dumped as JSON on stop.
	if _log_accum >= _log_interval:
		_log_accum = 0.0
		_log_samples.append({
			"t": snappedf(_log_total, 0.1),
			"year": snappedf(yc.get_progress(), 0.001),
			"season": _season(yc.get_progress()),
			"temp": snappedf(temp, 0.1),
			"humidity": snappedf(w.get_humidity(), 0.01),
			"clouds": snappedf(w.get_clouds(), 0.01),
			"wind": snappedf(w.get_wind(), 0.01),
			"rain": snappedf(rain, 0.01),
			"snow": snappedf(snow, 0.01),
			"leaves": snappedf(leaves, 0.01),
		})

func _log_summary() -> void:
	var year_s: float = maxf(_year_min * 60.0, 0.001)
	var result := {
		"year_minutes": _year_min,
		"duration_s": snappedf(_log_total, 0.1),
		"years_covered": snappedf(_log_total / year_s, 0.01),
		"interval_s": _log_interval,
		"stats": {
			"snow_active_s": snappedf(_t_snow, 0.1),
			"snow_pct_of_year": snappedf(100.0 * _t_snow / year_s, 0.1),
			"rain_active_s": snappedf(_t_rain, 0.1),
			"rain_pct_of_year": snappedf(100.0 * _t_rain / year_s, 0.1),
			"leaves_active_s": snappedf(_t_leaves, 0.1),
			"leaves_pct_of_year": snappedf(100.0 * _t_leaves / year_s, 0.1),
			"temp_min": snappedf(_temp_min, 0.1),
			"temp_max": snappedf(_temp_max, 0.1),
		},
		"samples": _log_samples,
	}
	var json := JSON.stringify(result)
	# Emit a SELF-CONTAINED report: the env_plot.html template with the run's JSON
	# injected as window.__ENV_LOG__, so the output opens straight from disk (no
	# sibling .json, no local server). Falls back to a bare page if the template is gone.
	var out_path := "res://../docs/env_report.html"
	var inject := "<script>window.__ENV_LOG__ = %s;</script>" % json
	var html := ""
	var tpl := FileAccess.open("res://../docs/env_plot.html", FileAccess.READ)
	if tpl:
		html = tpl.get_as_text()
		tpl.close()
		if html.find("<!--__ENV_LOG_EMBED__-->") != -1:
			html = html.replace("<!--__ENV_LOG_EMBED__-->", inject)
		else:
			html = html.replace("</head>", inject + "\n</head>") # marker missing; inject anyway
	else:
		html = "<!doctype html><meta charset=utf-8><title>Folio env log</title>%s<body><pre id=d></pre><script>document.getElementById('d').textContent=JSON.stringify(window.__ENV_LOG__,null,2)</script>" % inject
	var f := FileAccess.open(out_path, FileAccess.WRITE)
	if f:
		f.store_string(html)
		f.close()
		print("[log] STOP -- %d samples -> docs/env_report.html (%.2f years, %.0fs). Open it directly." % [_log_samples.size(), _log_total / year_s, _log_total])
	else:
		print("[log] STOP -- could not write docs/env_report.html")
	print("[log]   snow %.0f%% | rain %.0f%% | leaves %.0f%% of the year | temp %+.1f..%+.1f C" % [
		100.0 * _t_snow / year_s, 100.0 * _t_rain / year_s, 100.0 * _t_leaves / year_s, _temp_min, _temp_max])
