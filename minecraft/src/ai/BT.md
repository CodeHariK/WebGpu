# Walkthrough - Behavior Tree

This walkthrough documents the transition of the AI system to a robust, task-based Behavior Tree architecture inspired by LimboAI.

## Key Architecture Changes
- **Task Lifecycle**: Every node (now called `BTTask`) has explicit `_enter`, `_tick`, and `_exit` methods. This ensures clean state management and allows for complex behaviors (like starting/stopping animations).
- **Execution Flow**: The `execute()` method on `BTTask` handles the transition between lifecycle states, ensuring `_enter` is called when a task starts and `_exit` when it finishes.
- **BTStore Integration**: A centralized `BTStore` allows nodes to share data (e.g., the current target), making the AI more reactive to its environment.

## Implemented Components
- **`BTTask`**: Base class for all nodes. Defines the core lifecycle (`_enter`, `_tick`, `_exit`) and execution logic.
- **`BTComposite`**: Base class for branches. It holds and manages an array of multiple child nodes, providing methods to add, remove, and clear children. 
- **`BTStore`**: State management container.
- **`BTPlayer`**: A dedicated `Node` class that manages the execution of a Behavior Tree on an agent, abstracting the execution logic out of individual actors.

### Control Flow
- **`BTSequence`**: Ticks children in order; fails if any child fails, succeeds if all succeed.
- **`BTSelector`**: Ticks children in order; succeeds if any child succeeds, fails if all fail.
- **`BTRandomSequence`**: Shuffles its children when entered, then acts like a `BTSequence`.
- **`BTRandomSelector`**: Shuffles its children when entered, then acts like a `BTSelector`.

### Decorators
- **`BTDecorator`**: Base class for single-child nodes.
- **`BTInverter`**: Inverts the `SUCCESS`/`FAILURE` status of its child.
- **`BTForceSuccess`**: Always returns `SUCCESS` regardless of its child's status (unless `RUNNING`).
- **`BTForceFailure`**: Always returns `FAILURE` regardless of its child's status (unless `RUNNING`).
- **`BTProbability`**: Executes its child only if a random roll succeeds based on `run_chance`. Otherwise, returns `FAILURE`.
- **`BTRepeat`**: Repeats its child a specified number of times (`repeat_times`). Can run infinitely if set to `-1`. Returns `RUNNING` until the count is met.
- **`BTRepeatUntilSuccess`**: Repeats its child until the child returns `SUCCESS`. If the child returns `FAILURE`, this node will return `RUNNING` to try again on the next tick.
- **`BTRepeatUntilFailure`**: Repeats its child until the child returns `FAILURE`. If the child returns `SUCCESS`, this node will return `RUNNING` to try again on the next tick.
- **`BTDelay`**: Pauses execution and returns `RUNNING` for a set duration (`delay`) before allowing its child to execute.
- **`BTCooldown`**: Prevents its child from executing again until a specified time (`cooldown`) has passed since it last finished running. Returns `FAILURE` while on cooldown.
- **`BTRunLimit`**: Restricts its child from running more than a specified number of times (`run_limit`). Returns `FAILURE` once the limit is reached.
- **`BTTimeLimit`**: Aborts its child and returns `FAILURE` if the child runs longer than the specified `time_limit`.

### Actions & Conditions
- **`BTWait`**: Pauses execution for a specified duration.
- **`BTIsInRange`**: Conditional check based on distance to a target.
- **`BTActionShoot`**: Triggers the `shoot()` method on the `EnemyBase` actor.
