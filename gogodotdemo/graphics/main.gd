extends Node

func _ready() -> void:
	var s = Summator.new()
	s.add(10)
	s.sub(10)
	s.add(20)
	s.add(30)
	s.mul(10)
	print(s.get_total())
	s.reset()
