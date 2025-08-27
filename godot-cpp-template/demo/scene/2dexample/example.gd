extends Node

func _ready() -> void:
    var s = SummatorClass.new()
    s.add(4)
    s.sub(5)
    s.mul(4)
    print(s.get_total())

    $HelloNode.hello_world("Doom")


func _on_hello_node_hello_world_signal(data: String) -> void:
    print("Data from signal " + data)
