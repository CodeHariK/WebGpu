// // worker.ts
// type Message = {
//     type: 'multiply';
//     payload: number;
// };

// self.onmessage = (event: MessageEvent<Message>) => {
//     const { type, payload } = event.data;
//     if (type === 'multiply') {
//         const result = payload * 2;
//         postMessage({ result });
//     }
// };
