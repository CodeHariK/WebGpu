export class PriorityQueue<T> {
    private heap: { item: T; priority: number }[];

    constructor() {
        this.heap = [];
    }

    // Insert an item with a given priority into the priority queue
    insert(item: T, priority: number): void {
        this.heap.push({ item, priority });
        this.bubbleUp(this.heap.length - 1);
    }

    // Remove and return the item with the highest priority
    extractMin(): T | null {
        if (this.heap.length === 0) return null;

        const min = this.heap[0].item;
        const end = this.heap.pop();

        if (this.heap.length > 0 && end) {
            this.heap[0] = end;
            this.bubbleDown(0);
        }

        return min;
    }

    // Return the item with the highest priority without removing it
    peek(): T | null {
        return this.heap.length > 0 ? this.heap[0].item : null;
    }

    // Get the size of the priority queue
    size(): number {
        return this.heap.length;
    }

    // Helper method to maintain heap property on insertion
    private bubbleUp(index: number): void {
        const element = this.heap[index];
        while (index > 0) {
            const parentIndex = Math.floor((index - 1) / 2);
            const parent = this.heap[parentIndex];
            if (element.priority >= parent.priority) break;

            this.heap[index] = parent;
            index = parentIndex;
        }
        this.heap[index] = element;
    }

    // Helper method to maintain heap property on extraction
    private bubbleDown(index: number): void {
        const length = this.heap.length;
        const element = this.heap[index];

        while (true) {
            let leftChildIndex = 2 * index + 1;
            let rightChildIndex = 2 * index + 2;
            let swapIndex = null;

            if (leftChildIndex < length) {
                const leftChild = this.heap[leftChildIndex];
                if (leftChild.priority < element.priority) {
                    swapIndex = leftChildIndex;
                }
            }

            if (rightChildIndex < length) {
                const rightChild = this.heap[rightChildIndex];
                if (
                    rightChild.priority < (swapIndex === null ? element.priority : this.heap[leftChildIndex].priority)
                ) {
                    swapIndex = rightChildIndex;
                }
            }

            if (swapIndex === null) break;
            this.heap[index] = this.heap[swapIndex];
            index = swapIndex;
        }
        this.heap[index] = element;
    }
}
