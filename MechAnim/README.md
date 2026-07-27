Here is a clear, standard naming convention guide designed specifically for **Mech & Robot Rigging**. 

Mechs are great to rig because unlike humans, mechs usually use **rigid parenting** (each piece of metal/armor is parented 100% to one single bone) instead of organic vertex weight painting.

---

### 1. Naming Suffixes (`_L` and `_R`)
Always end side-specific bones and meshes with `_L` or `_R` (or `.L` / `.R`). 
* **Why?** Blender's auto-mirroring, symmetry tools, and our add-on script rely on this to automatically match left & right sides.
* *Example:* `UpperArm_L`, `UpperArm_R`

---

### 2. Bone Naming Convention for FK, IK, and Deform Chains

To make FK/IK switching & snapping easy, use explicit prefixes or suffixes for bone roles:

| Bone Role | Prefix / Suffix Pattern | Example Bone Name | Purpose |
| :--- | :--- | :--- | :--- |
| **Deform / Mechanical** | `DEF_` | `DEF_UpperArm_L` | The physical bone that holds the mesh (or controls the limb segment). |
| **FK Controls** | `FK_` | `FK_UpperArm_L` | FK control bone (Rotation only). |
| **IK Controls** | `IK_` | `IK_Hand_L` | Target bone that IK chain reaches for (Location & Rotation). |
| **IK Pole Vector** | `PTR_` or `POLE_` | `POLE_Elbow_L` | Controls knee / elbow direction. |
| **Switch Control** | `CTRL_` | `CTRL_ArmSwitch_L` | Has custom property `FK_IK_Switch` (0.0 = FK, 1.0 = IK). |

#### Limb Structure Example (Left Arm):
1. **FK Chain**: `FK_UpperArm_L` $\rightarrow$ `FK_LowerArm_L` $\rightarrow$ `FK_Hand_L`
2. **IK Chain**: `IK_Hand_L` + `POLE_Elbow_L`
3. **Deform Chain**: `DEF_UpperArm_L` $\rightarrow$ `DEF_LowerArm_L` $\rightarrow$ `DEF_Hand_L`
*(The `DEF_` bones use Copy Transform constraints pointing to FK or IK depending on your switch slider!)*

---

### 3. Naming Meshes for Easy Auto-Parenting

For mechs, every mesh piece (shoulder armor, forearm plate, hydraulic piston) should match its corresponding deform bone name.

#### Mesh Naming Pattern:
`GEO_<BoneName>` or `<PartName>_<Side>`

* **Example:**
  - Mesh Object: `GEO_UpperArm_L` $\rightarrow$ Parents directly to Bone: `DEF_UpperArm_L`
  - Mesh Object: `GEO_LowerArm_L` $\rightarrow$ Parents directly to Bone: `DEF_LowerArm_L`
  - Mesh Object: `GEO_Chest` $\rightarrow$ Parents directly to Bone: `DEF_Chest`

---

### 4. Piston Naming (Mechanical Pistons / Hydraulics)

Hydraulic pistons have two parts sliding into each other. Name them in pairs:
- **Top Piston Cylinder**: `Piston_Cylinder_L` (or `DEF_Piston_Top_L`)
- **Bottom Piston Rod**: `Piston_Rod_L` (or `DEF_Piston_Bottom_L`)

---

### Summary Checklist Before Scripting:
1. **Meshes**: Name every mesh piece with a clear base name + `_L` or `_R`.
2. **Bones**: Use prefixes (`DEF_`, `FK_`, `IK_`, `POLE_`).
3. **Hierarchy**: Group meshes clean in the Outliner so your list is clear.
