class_name Rally
extends RefCounted


const BAR_NONE := 0
const BAR_CONSECUTIVE := 1
const BAR_THREE_TOUCH := 2
var practice := false
var touch_count := 0
var touch_side := 0
var last_toucher: Player = null
var players: Array = []
var fault := ""

func side_count(side: int) -> int:
	var n := 0
	for p in players: if p.team_side == side: n += 1
	return n

func pre_tick() -> void:
	for p in players:
		var r := BAR_NONE
		if not practice:
			if p == last_toucher and side_count(p.team_side) > 1: r = BAR_CONSECUTIVE
			elif touch_count >= 3 and float(p.team_side) == float(touch_side): r = BAR_THREE_TOUCH
		p.touch_bar_reason = r
		p.touch_barred = r != BAR_NONE

const KIND_BLOCK := 4
func judge_touch(_kind: int, who: Player) -> void:

	if _kind != KIND_BLOCK and touch_count >= 3 and who.team_side == touch_side and not practice:
		fault = "four touches"; return
	if _kind == KIND_BLOCK:
		touch_count = 0; last_toucher = null; touch_side = 0
		return
	if who.team_side != touch_side:
		touch_count = 0; last_toucher = null; touch_side = who.team_side
	touch_count += 1
	last_toucher = who
