export class Keyboard {
    // Instance properties to track key states
    Up = false;
    Down = false;
    Left = false;
    Right = false;
    Nitrous = false;
    BANANA = false;
    JUMP = false;
    FLY = false;

    static keys: Keyboard;

    // Static key-to-action mapping
    static keyMap: { [key: string]: keyof Keyboard } = {
        ArrowUp: 'Up',
        KeyW: 'Up',

        ArrowDown: 'Down',
        KeyS: 'Down',

        ArrowLeft: 'Left',
        KeyA: 'Left',

        ArrowRight: 'Right',
        KeyD: 'Right',

        ShiftLeft: 'Nitrous',
        ShiftRight: 'Nitrous',

        KeyB: 'BANANA',

        KeyJ: 'JUMP',
        KeyF: 'FLY',
    };

    constructor() {
        // Handle keydown
        window.addEventListener('keydown', (event: KeyboardEvent) => {
            const action = Keyboard.keyMap[event.code];
            if (action) {
                this[action] = true; // Update instance property
            }
        });

        // Handle keyup
        window.addEventListener('keyup', (event: KeyboardEvent) => {
            const action = Keyboard.keyMap[event.code];
            if (action) {
                this[action] = false; // Update instance property
            }
        });

        Keyboard.keys = this
    }
}