import * as THREE from 'three';

export function RectangularGrid(xDiv: number, yDiv: number): THREE.LineSegments {
    const width = xDiv
    const height = yDiv

    const color = new THREE.Color(0x888888); // Color of the grid lines

    // Create an empty geometry for line segments
    const gridGeometry = new THREE.BufferGeometry();
    const gridMaterial = new THREE.LineBasicMaterial({ color });

    // Vertices array to store the grid lines
    const vertices = [];

    // X-axis lines (vertical lines)
    for (let i = 0; i <= xDiv; i++) {
        const x = (i / xDiv) * width - width / 2;
        vertices.push(x, 0, -height / 2);
        vertices.push(x, 0, height / 2);
    }

    // Y-axis lines (horizontal lines)
    for (let j = 0; j <= yDiv; j++) {
        const y = (j / yDiv) * height - height / 2;
        vertices.push(-width / 2, 0, y);
        vertices.push(width / 2, 0, y);
    }

    // Set the vertices to the geometry
    gridGeometry.setAttribute('position', new THREE.Float32BufferAttribute(vertices, 3));

    // Create the line segments object and add to the scene
    const grid = new THREE.LineSegments(gridGeometry, gridMaterial);

    return grid
}
