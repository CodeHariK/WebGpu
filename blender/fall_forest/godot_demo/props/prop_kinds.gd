class_name PropKinds
extends RefCounted
## What grows where, how many, and how far away each kind is still drawn.
##   meshes    the meshes (names in assets/fall_assets.glb) picked from at random
##   count     how many to place on the whole terrain
##   range     metres from the camera past which the kind isn't drawn (visibility range): the
##             small things go first, trees last; fog hides the line
##   shadow    whether it casts a shadow (small things don't: nobody sees it, it costs a pass)
##   lie       lean with the ground's slope (things lying on it) instead of standing up
##   scale     smallest .. largest random size
##   where     "forest" (deep in a wood), "edge" (a wood's edge), "clearing", "anywhere"
##   leaf      double-sided material (the leaves are flat, one-sided cards)

const KINDS := [
	{"name": "trees", "meshes": ["Tree1", "Tree2", "Tree3", "Tree4"], "count": 1500,
		"range": 400.0, "shadow": true, "lie": false, "scale": [0.8, 1.25], "where": "forest"},
	{"name": "bushes", "meshes": ["Bush1", "Bush2", "Bush3"], "count": 1500,
		"range": 160.0, "shadow": true, "lie": false, "scale": [0.8, 1.3], "where": "edge"},
	{"name": "pumpkins", "meshes": ["Pumpkin_Large", "Pumpkin_Medium", "Pumpkin_Small", "Gourd1", "Gourd2"], "count": 800,
		"range": 90.0, "shadow": true, "lie": true, "scale": [0.8, 1.2], "where": "clearing"},
	{"name": "mushrooms", "meshes": [
			"Mushroom_Flat_Brown_1", "Mushroom_Flat_Brown_2", "Mushroom_Flat_Brown_3", "Mushroom_Flat_Brown_4",
			"Mushroom_Flat_Yellow_1", "Mushroom_Flat_Yellow_2", "Mushroom_Flat_Yellow_3", "Mushroom_Flat_Yellow_4",
			"Mushroom_Red_1", "Mushroom_Red_2", "Mushroom_Red_3", "Mushroom_Red_4",
			"Mushroom_Round_Brown_1", "Mushroom_Round_Brown_2", "Mushroom_Round_Brown_3", "Mushroom_Round_Brown_4",
			"Mushroom_Round_Yellow_1", "Mushroom_Round_Yellow_2", "Mushroom_Round_Yellow_3", "Mushroom_Round_Yellow_4"],
		"count": 5000, "range": 60.0, "shadow": false, "lie": true, "scale": [0.6, 1.1], "where": "forest"},
	{"name": "nuts", "meshes": ["Acorn1", "Acorn2", "Pinecone_Closed", "Pinecone_Open1", "Pinecone_Open2"], "count": 3000,
		"range": 30.0, "shadow": false, "lie": true, "scale": [0.8, 1.2], "where": "forest"},
	{"name": "leaves", "meshes": ["Leaf_Basic_1", "Leaf_Basic_2", "Leaf_Fanned_1", "Leaf_Fanned_2",
			"Leaf_Pointy_1", "Leaf_Pointy_2", "Leaf_Rounded_1", "Leaf_Rounded_2"], "count": 8000,
		"range": 35.0, "shadow": false, "lie": true, "scale": [0.7, 1.2], "where": "anywhere", "leaf": true},
]


## How likely a kind grows at (x, z), 0 .. 1.
static func chance(p_where: String, p_x: float, p_z: float) -> float:
	var forest := TerrainHeight.forest(p_x, p_z)
	match p_where:
		"forest":
			return forest
		"edge":
			return 1.0 - absf(forest - 0.5) * 2.0
		"clearing":
			return (1.0 - forest) * 0.6
		_:
			return 0.3 + 0.7 * forest
