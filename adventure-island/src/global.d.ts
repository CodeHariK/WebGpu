// global.d.ts
export { };

declare global {
    interface Window {
        Jolt: typeof Jolt; // Replace `typeof Jolt` with the appropriate type if known
    }
}