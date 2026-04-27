# Unified Movement System Design
*Inspired by: Mario Odyssey, A Hat in Time, Celeste, and Hollow Knight*

## Core Philosophy: "The Snappy Lie"
To achieve the high-fidelity feel of modern platformers, we prioritize **player intent** over realistic physics. The controller uses `CharacterBody3D` (Kinematic) to manually simulate forces, allowing for instant feedback and precise tuning.

### Influences
- **Mario Odyssey**: Momentum-based rolling, long jumps, and fluid animation-driven state transitions.
- **A Hat in Time**: The signature Dive & Dive-Flip, double jumps, and forgiving ledge interactions.
- **Celeste**: Mechanical precision, 8-way dashing, Coyote Time, and Jump Buffering.
- **Hollow Knight**: Impactful feedback, tight air control, and dash-canceling.

---

## 1. Grounded Mechanics
| Mechanic | Status | Description |
| :--- | :---: | :--- |
| **Run** | [x] | Camera-relative movement with adjustable acceleration/friction. |
| **Skid** | [ ] | High-friction turnaround animation when reversing direction at speed. |
| **Crouch/Slide** | [ ] | Lowering the profile; transitions into a Slide if moving fast (Mario roll style). |
| **Long Jump** | [ ] | Jump while sliding to gain massive horizontal momentum. |

## 2. Airborne Mechanics
| Mechanic | Status | Description |
| :--- | :---: | :--- |
| **Variable Jump** | [x] | Height depends on button hold duration (Kinematic jump math). |
| **Coyote Time** | [x] | ~5-10 frames of jump availability after leaving a ledge. |
| **Jump Buffering** | [x] | Storing a jump input if pressed slightly before hitting the ground. |
| **Double Jump** | [x] | Mid-air vertical boost (Hollow Knight / Hat in Time style). |
| **Dash (Ground/Air)** | [x] | Directional burst of speed; resets on ground (Celeste style). |
| **Super Jump** | [x] | Jumping during a dash carries momentum (Hyper/Super Jump). |
| **Dive** | [ ] | Lunging forward in mid-air (Hat in Time style). |
| **Dive-Flip** | [ ] | Jumping immediately upon landing from a dive for a high boost. |
| **Air Drift** | [x] | Fine-tuned horizontal control while falling for precise landing. |

## 3. Advanced Action States
| Mechanic | Status | Description |
| :--- | :---: | :--- |
| **Wall Cling/Slide** | [ ] | Sticking to walls with a slower slide (Celeste/Hollow Knight). |
| **Wall Kick** | [ ] | Snappy push-off from walls to gain height and flip direction. |
| **8-Way Dash** | [ ] | A high-speed burst in any direction (Celeste style). |
| **Ledge Grab** | [ ] | Automatically snapping to and climbing up edges. |

---

## Technical Architecture: HSM (Hierarchical State Machine)
The controller is managed by a modular state machine to prevent "if-else hell."

### Base States
- **GroundedState**: Logic for Idle, Walk, Run, and Skidding.
- **AirborneState**: Logic for Jump, Fall, and Gravity calculations.
- **InteractingState**: Logic for Ledge Climbing and Wall Interactions.
