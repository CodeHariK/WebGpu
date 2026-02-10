extends Node

var spin_multiplier = 1.0

func _ready() -> void:
    print("Hello GdZig!")
    if $UI/ToggleSpin:
        $UI/ToggleSpin.connect("pressed", _on_toggle_spin_pressed)
    if $UI/SetSpeed:
        $UI/SetSpeed.connect("pressed", _on_set_speed_pressed)

    # Simulate button presses for automated verification
    call_deferred("_on_set_speed_pressed")
    call_deferred("_on_toggle_spin_pressed")

func _find_node_by_class_or_method(root: Node, klass_names: Array, method_name: String = "") -> Node:
    for child in root.get_children():
        var cname = child.get_class()
        if cname in klass_names or (method_name != "" and child.has_method(method_name)):
            return child
        var found = _find_node_by_class_or_method(child, klass_names, method_name)
        if found:
            return found
    return null

func _on_toggle_spin_pressed() -> void:
    var example = $ExampleNode
    var node = _find_node_by_class_or_method(example, ["My3dNode", "My3DNode"], "toggle_spin")
    if node:
        var cname = node.get_class()
        print("Invoking toggle_spin on", cname)
        if node.has_method("toggle_spin"):
            var res = node.call("toggle_spin")
            print("call(toggle_spin) result=", res)
        else:
            print("toggle_spin not found on instance")
    else:
        print("No My3DNode found under", example.name)

func _on_set_speed_pressed() -> void:
    var example = $ExampleNode

    var node = _find_node_by_class_or_method(example, ["My3dNode", "My3DNode"], "set_spin_multiplier")
    if node:
        var cname = node.get_class()
        print("Invoking set_spin_multiplier on", cname)
        if node.has_method("set_spin_multiplier"):
            var res = node.call("set_spin_multiplier", spin_multiplier)
            if (spin_multiplier == 1.0):
                spin_multiplier = -1.0
            else:
                spin_multiplier = 1.0
            print("call(set_spin_multiplier) result=", res)
        else:
            print("set_spin_multiplier not found on instance")
    else:
        print("No My3DNode found under", example.name)
