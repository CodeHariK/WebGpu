export class PriorityQueue<T> {
    private elements: { item: T; priority: number }[];
    private positions: Map<T, number>

    constructor() {
        this.elements = [];
        this.positions = new Map<T, number>();
    }

    getElements() {
        return this.elements
    }

    // Insert an item with a given priority into the priority queue
    insert(item: T, priority: number): void {
        this.elements.push({ item, priority });
        this.positions.set(item, this.elements.length - 1);
        this.heapifyUp(this.elements.length - 1);
    }

    // Remove and return the item with the highest priority
    extractMin(): T | null {
        if (this.elements.length === 0) return null;

        const min = this.elements[0];
        this.positions.delete(min.item);

        const last = this.elements.pop();
        if (this.elements.length > 0 && last) {
            this.elements[0] = last;
            this.positions.set(last.item, 0)
            this.heapifyDown(0);
        }

        return min.item;
    }

    // Return the item with the highest priority without removing it
    peek(): T | null {
        return this.elements.length > 0 ? this.elements[0].item : null;
    }

    // Get the size of the priority queue
    size(): number {
        return this.elements.length;
    }

    updatePriority(item: T, newPriority: number) {
        if (!this.positions.has(item)) {
            throw new Error("Item not found in priority queue");
        }

        const index = this.positions.get(item)!;
        const currentPriority = this.elements[index].priority;
        this.elements[index].priority = newPriority;

        if (newPriority < currentPriority) {
            this.heapifyUp(index);
        } else if (newPriority > currentPriority) {
            this.heapifyDown(index);
        }
    }

    swap(i: number, j: number) {
        [this.elements[i], this.elements[j]] = [this.elements[j], this.elements[i]];
        this.positions.set(this.elements[i].item, i);
        this.positions.set(this.elements[j].item, j);
    }

    // Helper method to maintain heap property on insertion
    private heapifyUp(index: number): void {
        const element = this.elements[index];
        while (index > 0) {
            const parentIndex = Math.floor((index - 1) / 2);
            const parent = this.elements[parentIndex];
            if (element.priority >= parent.priority) break;

            this.swap(index, parentIndex);
            index = parentIndex;
        }
    }

    // Helper method to maintain heap property on extraction
    private heapifyDown(index: number): void {
        const length = this.elements.length;

        while (index < length) {
            let left = 2 * index + 1;
            let right = 2 * index + 2;
            let smallest = index;

            if (left < length && this.elements[left].priority < this.elements[smallest].priority) {
                smallest = left;
            }
            if (right < length && this.elements[right].priority < this.elements[smallest].priority) {
                smallest = right;
            }
            if (smallest === index) break;

            this.swap(index, smallest);
            index = smallest;
        }
    }
}
