extends Node

enum Priv { PUBLIC, FRIENDS, PASSWORD }
signal lobby_changed
var active := false
var is_host := false
var in_match := false
var practice_live := false
var custom_lobby := false
var party_lan := false
var _lobby_kind := "custom"
var privacy: int = Priv.PUBLIC
var password := ""
var steam_lobby_id := 0
var players := {}
