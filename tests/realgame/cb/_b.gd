extends RefCounted

const K := 1.0
func f() -> float: return K
func g() -> float: return f()
static func sf() -> float: return K
func sg() -> float: return sf()
