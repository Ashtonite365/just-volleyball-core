class_name AccessoryItem
extends Resource
const CATEGORIES := ["head", "face", "extra"]
const HAIR_NORMAL := "normal"
const HAIR_TOP := "top_covered"
const HAIR_FULL := "full_covered"
@export var id := ""
@export var display_name := ""
@export var mesh: PackedScene
@export var category := "face"
@export var mount := "head"
@export var price := 0
@export var supporter := false
@export var anchor_offset := Vector3.ZERO
@export var anchor_rotation := Vector3.ZERO
@export var anchor_scale := 1.0
@export var hair_mode := "normal"
@export var portrait := "head"
@export var worn_on_back := false
@export var turns_back := false
@export var jiggle_stiffness := 1.0
@export var jiggle_damping := 1.0
@export var jiggle_gravity := 1.0
func name_or_id() -> String:
	return display_name if display_name != "" else id
func portrait_preset() -> String:
	return portrait if portrait != "" else "head"
