## The comment at the top of a step's shader file, for a step viewer's panel (steps.gd,
## waterfall_simple/waterfall_simple.gd).

## [its first line without "STEP n — ", the rest]. The rest is joined into one paragraph, except
## lines indented 3 spaces ("//   r  how much ..."): a list, each item on its own line, and the
## text after it on a new line. A line indented further carries on the item above it.
static func parse(p_code: String) -> Array:
	var lines: PackedStringArray = []
	for line in p_code.split("\n"):
		if not line.begins_with("//"):
			break
		lines.append(line.trim_prefix("//"))
	if lines.is_empty():
		return ["", ""]
	var first := lines[0].strip_edges()
	var dash := first.find("— ")
	if first.begins_with("STEP") and dash >= 0:
		first = first.substr(dash + 2)
	var rest := ""
	var in_list := false
	for line in lines.slice(1):
		var indented := line.begins_with("   ")
		var carries_on := line.begins_with("    ") and in_list # the item above, wrapped
		if rest.is_empty():
			rest = line.strip_edges()
		elif carries_on:
			rest += " " + line.strip_edges()
		else:
			# a list item, or the first line after a list: a new line; else the same paragraph
			rest += ("\n" if indented or in_list else " ") + line.strip_edges()
		in_list = indented
	return [first, rest]
