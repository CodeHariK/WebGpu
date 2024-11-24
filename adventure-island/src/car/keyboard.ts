type KeyStates = { [key: string]: boolean };
export class Keyboard {
    static keys: KeyStates = {
        ArrowUp: false,
        ArrowDown: false,
        ArrowLeft: false,
        ArrowRight: false,
    };

    constructor() {
        window.addEventListener('keydown', (event: KeyboardEvent) => {
            Keyboard.keys[event.code] = true;
        });

        window.addEventListener('keyup', (event: KeyboardEvent) => {
            Keyboard.keys[event.code] = false;
        });
    }
}