export enum InputKeys {
    ArrowUP = "ArrowUP",
    ArrowDOWN = "ArrowDOWN",
    Enter = "Enter",
}

export class INPUT {

    static current: InputKeys | undefined = undefined;

    static RegisterKeys() {
        document.addEventListener("keyup", (event) => {
            this.current = undefined
        })
        document.addEventListener("keydown", (event) => {
            console.log(`Key pressed: ${event.key}`);

            switch (event.key) {
                case "ArrowUp":
                    this.current = InputKeys.ArrowUP
                    break;
                case "ArrowDown":
                    this.current = InputKeys.ArrowDOWN
                    break;
                case "Enter":
                    this.current = InputKeys.Enter
                    break;
            }
        });
    }
} 
