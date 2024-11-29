let n = 4; // Number of circles
let circles = []; // Array to store circle data

function setup() {
    createCanvas(800, 600); // Set canvas size
    noLoop(); // Ensures that draw() only runs once

    // Define fixed circle positions for testing
    circles.push({ x: 300, y: 300, radius: 50 });
    circles.push({ x: 500, y: 300, radius: 50 });

    // Add more circles as needed
    for (let i = 2; i < n; i++) {
        let radius = random(20, 50); // Random radius between 20 and 50
        let x = random(100, width - 100); // Random X position
        let y = random(100, height - 100); // Random Y position
        circles.push({ x, y, radius });
    }
}

function draw() {
    background(220); // Set background color

    noFill(); // No fill for circles
    strokeWeight(2); // Set stroke weight

    for (let i = 0; i < circles.length; i++) {
        let curr = circles[i];
        let next = circles[(i + 1) % circles.length]; // Wrap around to connect last to first

        // Calculate all tangents
        let tangents = calculateTangents(curr.x, curr.y, curr.radius, next.x, next.y, next.radius);

        // Choose a tangent that doesn't reuse the previous point
        let tangent = chooseValidTangent(tangents, curr.usedTangent);

        // Draw the tangent
        line(tangent.start.x, tangent.start.y, tangent.end.x, tangent.end.y);

        // Draw lines from center to tangent points
        line(curr.x, curr.y, tangent.start.x, tangent.start.y); // Center to start point
        line(next.x, next.y, tangent.end.x, tangent.end.y); // Center to end point

        // Draw both arcs
        drawArc(curr, tangent, true); // Larger arc
        drawArc(curr, tangent, false); // Smaller arc

        // Mark the tangent as used for the next circle
        curr.usedTangent = tangent.start; // Store the start point of the tangent
        next.usedTangent = tangent.end; // Ensure a different tangent for the next circle
    }
}

// Function to calculate tangents and return their points
function calculateTangents(x1, y1, r1, x2, y2, r2) {
    let dx = x2 - x1;
    let dy = y2 - y1;
    let d = sqrt(dx * dx + dy * dy); // Distance between circle centers
    let tangents = []; // Array to store tangent points

    if (d > abs(r1 - r2)) {
        // Calculate the angle between the centers
        let angle = atan2(dy, dx);

        // External tangents
        let deltaExt = acos((r1 - r2) / d); // Angle adjustment for external tangents

        // First external tangent
        tangents.push({
            start: {
                x: x1 + r1 * cos(angle + deltaExt),
                y: y1 + r1 * sin(angle + deltaExt),
                angle: angle + deltaExt,
            },
            end: {
                x: x2 + r2 * cos(angle + deltaExt),
                y: y2 + r2 * sin(angle + deltaExt),
                angle: angle + deltaExt,
            },
            angle: angle + deltaExt,
        });

        // Second external tangent
        tangents.push({
            start: {
                x: x1 + r1 * cos(angle - deltaExt),
                y: y1 + r1 * sin(angle - deltaExt),
                angle: angle - deltaExt,
            },
            end: {
                x: x2 + r2 * cos(angle - deltaExt),
                y: y2 + r2 * sin(angle - deltaExt),
                angle: angle - deltaExt,
            },
            angle: angle - deltaExt,
        });
    }

    return tangents;
}

// Function to choose a tangent that doesn't reuse the previous touching point
function chooseValidTangent(tangents, usedTangent) {
    // Select the tangent that does not share the previous touching point
    let validTangents = tangents.filter(tangent => tangent.start !== usedTangent);
    return validTangents.length > 0 ? validTangents[0] : tangents[0]; // Fallback if no valid tangent is found
}

// Function to draw arcs between tangent points
function drawArc(circle, tangent, isLargerArc) {
    let { x, y, radius } = circle;

    // Compute angles for tangent points
    let angleStart = atan2(tangent.start.y - y, tangent.start.x - x);
    let angleEnd = atan2(tangent.end.y - y, tangent.end.x - x);

    // Normalize angles to ensure correct order
    let arcStart = angleStart;
    let arcEnd = angleEnd;

    // Calculate the angle difference
    let angleDiff = arcEnd - arcStart;

    // Ensure the difference is between -π and π
    if (angleDiff > PI) {
        arcEnd -= TWO_PI;
    } else if (angleDiff < -PI) {
        arcStart -= TWO_PI;
    }

    // Draw the appropriate arc (larger or smaller)
    if (isLargerArc) {
        // Larger arc (greater than 180 degrees)
        stroke(255, 0, 0); // Red color for larger arc
        arc(x, y, radius * 2, radius * 2, arcStart, arcEnd);
    } else {
        // Smaller arc (less than 180 degrees)
        stroke(0, 0, 255); // Blue color for smaller arc
        arc(x, y, radius * 2, radius * 2, arcEnd, arcStart);
    }
}