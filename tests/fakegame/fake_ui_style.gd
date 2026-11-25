class_name UiStyle

const PRIMARY := Color("ffd23f")
const INK := Color("232b3b")
const GAME_ICONS := ["movie", "wrench", "gear", "gamepad", "question", "target", "singleplayer", "massiveMultiplayer"]
const BOARD_ICONS := ["dice", "suit_hearts"]
static func _tex(name: String, set_name: String) -> Texture2D:
	var t := PlaceholderTexture2D.new(); t.size = Vector2(24, 24); t.set_meta("icon", name); t.set_meta("set", set_name)
	return t
static func icon(name: String, _force_white := false, _force_black := false) -> Texture2D:
	return _tex(name, "game") if GAME_ICONS.has(name) else null
static func icon_board(name: String) -> Texture2D:
	return _tex(name, "board") if BOARD_ICONS.has(name) else null

static func rail_tile(label: String, glyph: Texture2D) -> Button:
	var b := Button.new(); b.text = label; b.icon = glyph
	return b
static func paint_rail_tile(b: Button, lit: bool) -> void: b.set_meta("lit", lit)
static func style_button(btn: Button, kind := "primary", _size := 16) -> void: btn.set_meta("kind", kind)
static func style_label(_l: Label, _role := "body", _color := Color.WHITE) -> void: pass
static func slim_rail_tile(b: Button, size: String) -> void: b.set_meta("slim", size)
