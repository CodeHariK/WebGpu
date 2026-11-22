// Colour Blocks — every number worth tuning (the sliders in Map26.tsx) and the build steps.

export type Settings = {
    seed: number;
    step: number; // 1 .. STEPS.length
    cells: number; // grid cells along each side
    seeds: number; // how many colours start
    seedsPerColour: number; // each colour gets 1 .. this many seeds, near each other
    chainStep: number; // how far (cells) each extra seed is from the one before it
    roughness: number; // how much the noise slows the spread in places (0: even diamonds)
    noiseSize: number; // size of the noise's patches, in cells
    speedVariety: number; // how much faster some colours spread than others (0: all the same)
    districtSize: number; // about how wide a district is (block cells): bigger blocks get more of them
    streetBlock: number; // a street block's usual size (fine cells)
    blockLength: number; // step 12: how far apart the streets across the field are (fine cells): a block's length
    fieldReach: number; // step 12: how far (block cells) a road's direction reaches into the field
    plotWidth: number; // step 13: a plot's frontage along its road (fine cells)
    plotDepth: number; // step 13: how far back from its road a plot reaches (fine cells)
    downtownShare: number; // step 14: share of each city's area that is downtown
    industryShare: number; // step 14: ... industry (on the shore)
    parkShare: number; // step 14: ... parks
    dockLength: number; // step 17: how long each island's docks are along the ring road (fine cells)
    mallBlocks: number; // step 18: how many street blocks a mall takes in (with the streets between)
    parkBlocks: number; // step 18: ... a park
    streetGaps: number; // share of street pieces left out (bigger, irregular blocks)
    houseSize: number; // a house's usual width and depth (fine cells)
    houseFill: number; // share of lots that get a house
    islandRadius: number; // step 8: the island's falloff reaches this share of the map's width
    islandNoise: number; // step 8: how much the noise counts against the falloff (0: a circle)
    islandNoiseSize: number; // step 8: size of the noise's biggest features (pixels)
    twinRadius: number; // step 11: each island's falloff reaches this share of the map's width
    twinSpacing: number; // step 11: how far each island's middle is from the map's (share of its width)
    cityMargin: number; // step 9: how far (pixels) the city stays in from the coast (up to 50: the coast distance is measured that far)
    arterialBend: number; // step 10: the most (degrees) an arterial may bend at a junction
    arterialLength: number; // step 10: the shortest arterial (cells)
    straighten: number; // how far (cells) a highway may be pulled straight before it is rounded
    smoothing: number; // corner-cutting passes on the highways
};

export const DEFAULT_SETTINGS: Settings = {
    seed: 5,
    step: 4,
    cells: 100,
    seeds: 9,
    seedsPerColour: 3,
    chainStep: 26,
    roughness: 3,
    noiseSize: 14,
    speedVariety: 0.5,
    districtSize: 16,
    streetBlock: 9,
    blockLength: 20,
    fieldReach: 6,
    plotWidth: 3.4,
    plotDepth: 5,
    downtownShare: 0.15,
    industryShare: 0.1,
    parkShare: 0.1,
    dockLength: 36,
    mallBlocks: 4,
    parkBlocks: 2,
    streetGaps: 0.15,
    houseSize: 2.6,
    houseFill: 0.85,
    islandRadius: 0.45,
    islandNoise: 0.55,
    islandNoiseSize: 170,
    twinRadius: 0.24,
    twinSpacing: 0.26,
    cityMargin: 28,
    arterialBend: 55,
    arterialLength: 20,
    straighten: 1.5,
    smoothing: 3,
};

export const STEPS: { title: string; text: string }[] = [
    {
        title: 'Grid and seeds',
        text: 'A square grid of cells. A few seed cells, spread apart, each its own colour; some colours get one or two more seeds in a chain (up to seeds per colour): each extra seed about chain step cells on from the one before, in roughly the same direction, kept clear of other colours\' seeds, so the colour grows into a long, bent block instead of a round one.',
    },
    {
        title: 'Spread',
        text: 'Every colour spreads out from its seed, all at once, like ink. It is a race: a cell takes the colour that reaches it first. Stepping into a cell costs time, and the cost comes from noise (roughness), so the fronts wander instead of growing as diamonds; seeds are put on easy ground so none is stuck in a slow patch; some colours also spread faster than others (speed variety) and win bigger areas. Found with a shortest-path search from all the seeds at once (Dijkstra): only the frontier is queued (cells next to burnt ones, each with the time it would catch), and the earliest always burns next. Shown here: the cost of the ground (brown is slow), the speed of each colour, the queue (outlined cells, in the colour they will take) and the next to burn (red ring).',
    },
    {
        title: 'Blocks',
        text: 'When every cell has a colour, each colour is an area block. Tidied first: twice, every cell takes the colour most of its 8 neighbours have (if 5 or more agree), which shaves off single-cell spikes. Then every block is made one piece: only the part joined side to side to its own seed stays, any other part joins the colour it touches most. Then strands, tips and notches thinner than 5 cells go to the colour beside them (an opening: shrink every block by 2 cells, grow it back). Where two colours meet is a border.',
    },
    {
        title: 'Highways',
        text: 'The borders, followed from junction to junction (where three blocks meet) or to the map\'s edge, become lines. Each is pulled straight first (any point less than `straighten` cells off the line between its neighbours is dropped: Ramer–Douglas–Peucker), then rounded by corner cutting. The junctions stay put, so the highways meet there.',
    },
    {
        title: 'Districts',
        text: 'Each block is split the same way it was made, one level down: on a grid 3 times finer, the highways are drawn in as walls, so each block is a region of its own. A few new seeds go into each (one for every district size² of area, 1 to 6) and spread exactly as the blocks did, but never through a wall: a district can\'t cross a highway. Tidied the same way. Where two districts meet runs a main road (white), ending where it meets a highway.',
    },
    {
        title: 'Streets',
        text: 'Each district fills with a straight grid of streets of its own, turned to the direction of the road nearest its middle (a city grid follows the main road it grew from), with its own block size and offset, so neighbouring districts\' streets don\'t line up: T-junctions at the roads between them. Every grid line is walked across the district; the stretches inside it are streets, cut where the cross streets meet them, and some pieces are left out (street gaps) for bigger, irregular blocks.',
    },
    {
        title: 'Houses',
        text: 'Every street and main road is walked along both sides: a house (a small rectangle, front to the road, its own width and depth near house size) goes a little back from the road, then the walk moves on by its width and a small gap. A house is only placed if all of it is inside one district and clear of every road and every house before it (checked on a grid twice as fine, with the roads drawn in first); some lots stay empty (house fill). Roofs in a few colours, a ridge along each.',
    },
    {
        title: 'Island',
        text: 'A new start, the city switched off for now: one island, a noise map with a circular falloff. Every point has a height: noise (4 sizes added together, each half the size and half as strong) mixed with a hill that is 1 in the middle of the map and 0 at island radius away (1 − (distance ÷ radius)²). Land is where the height is above sea level: near the middle the hill keeps it land, near the coast only the noise\'s high spots poke out (headlands, islets) and its low spots sink (bays). Island noise says how much the noise counts against the hill (0: a circle).',
    },
    {
        title: 'City on the island',
        text: 'The city switched back on, on the island, kept off the beach: a cell is city only if its middle is on land and at least city margin pixels from the coast (the coast distance the island\'s colours are painted from). Only the biggest piece of city cells is kept (narrow headlands and islets stay countryside). Every other cell is a wall: no seed goes there and the spread never enters it, so the blocks, districts, streets and houses all stop at the city\'s edge, everything else exactly as in steps 1 to 7. The edge gets a highway too, the ring road: while finding the borders the outside counts as one more colour, so the side between a block and the outside is a border like any other, straightened and rounded the same, and the highways between blocks meet it at junctions. The slivers the rounding cuts off past it go outside.',
    },
    {
        title: 'Arterials',
        text: 'Every highway runs from one junction to the next, so the roads read as cells (stained glass); a real city has a few long roads that cross it. At every junction the highway ends meeting there are paired: the straightest pair first (their directions leaving the junction most nearly opposite), then the straightest of those left, as long as a pair bends less than arterial bend degrees. Paired highways are one road: followed from piece to piece they make chains, and a chain of two or more pieces at least arterial length cells long is an arterial, drawn wider and deeper orange. The ring road is left out (it stays a highway): the arterials cross the city and end at it. Where three blocks meet, the roads are about 120 degrees apart, so going straight on bends about 60: arterial bend decides how many of those count.',
    },
    {
        title: 'Two islands',
        text: 'Two islands, joined by the motorway. Two hills instead of one, smaller (twin radius), each twin spacing of the map\'s width from the middle, opposite ways along a random direction; the falloff at a point is the higher of the two, and the same noise roughens both coasts, so a strait opens between them. The city grows on each (the two biggest pieces of city cells, each with its own ring road). The motorway is now only the bridge between them: the cheapest path from just outside one city to just outside the other, never through a city (so it crosses no other road) and never round an island. A step on land costs 1, over water 8, so it walks out to where the strait is narrowest and crosses there. Each end is carried on to the nearest point of the ring road, where it joins. Over water it is a bridge (a pale deck, piers, its shadow on the sea).',
    },
    {
        title: 'Street field',
        text: 'Streets follow the roads instead of one straight grid per district. First a street field: a direction at every point, taken from the roads (highways, the ring road, main roads), each road sampled every fine cell, each sample counting less fast with distance (1 / (1 + (distance / field reach)⁴)): next to a road its own direction wins, between roads it turns smoothly from one to the other (the dark ticks; a direction has no front or back, so they are added with the angle doubled, a tensor). Then streets are traced along it, two families: along the field (parallel to the nearest road) and across it (at right angles). A street is followed from a seed both ways, half a fine cell a step, until it reaches a road, comes closer than half its spacing to another street of its family, or turns too sharply. Seeds are tried a spacing either side of every street already traced, only where no street of the family is that close (evenly spaced streamlines). The first are by the roads: streets along start one street block (a block\'s depth: two rows of houses back to back) in from a road, streets across start at the road itself, block length apart, so they meet it in T-junctions. The houses line up along them as before, each facing its street, and now along the highways and the ring road too (a little further back), so the band between a road and its first street is built up on both sides.',
    },
    {
        title: 'Plots',
        text: 'Every block between the roads is cut into plots, as a real street is: a strip of ground running back from one road, the house at its front facing the road, gardens meeting back to back. On a grid twice as fine as the districts\' the roads are drawn in, each road cell remembering which road it is, how far along it, and the road\'s direction there. A block is a piece of ground between road cells. Its cells beside a road are its frontage, each keyed by that road, which side of it, and which stretch of plot width along it. Then every block cell takes the key of the frontage nearest it (searched out from all the frontage at once), up to plot depth back: cells sharing a key are one plot. Corners split themselves (a corner cell goes to the nearer road); deeper than plot depth is the block\'s middle (green: a courtyard). Each plot gets one house at its front, turned to its road, as wide as the plot allows, all of it on the plot (else tried smaller, else the plot stays empty; some stay empty anyway: house fill).',
    },
    {
        title: 'Zoning',
        text: 'Each block (the ground between the streets) is given a use. For every block: its area, its middle, its island, how central it is (1 at the middle of its island\'s city, 0 at the farthest block), which kinds of road it touches (street, main road, highway, arterial), how near it comes to the coast, and how near it is to an end of the bridge. Then the uses are handed out in turn, each taking the best blocks still free. Mall (red): one per island, the biggest block that touches an arterial or is by the bridge. Downtown (purple): the most central blocks, touching an arterial counts extra, up to downtown share of the city\'s area. Industry (grey): the blocks nearest the coast, touching a highway counts extra, up to industry share. Park (green): mid-sized blocks at random, up to park share. Houses (cream): the rest, and only they keep their houses for now; the other uses get their buildings in the next steps.',
    },
    {
        title: 'Lots',
        text: 'The plots are joined into lots as big as each use\'s buildings need. Houses: one plot is one lot, as before. Downtown: the plots along each road (same side, stretches one after another) are joined in runs of 2 to 4, wider lots for bigger buildings. Industry: half a block, split across its long way through its middle (the long way from how its cells spread out), or the whole block if it is small. Mall and park: the whole block. A lot never crosses a road (its plots don\'t), and lots from whole or half blocks take the block\'s courtyard too. Each lot faces the road it has the most frontage on (the arrow: from the middle of that frontage into the lot); its building will stand there in the next step. Only the lots that are not single houses are outlined.',
    },
    {
        title: 'Buildings',
        text: 'A placeholder building on every lot, each with a height in floors for the 3D map later (its shadow to the bottom right is as long as it is tall). A rectangle is the biggest of its shape that fits on the lot, turned to the lot\'s road and standing a setback back from it (tried full size, then smaller until all of it is on the lot). Houses: as before, 1 to 2 floors. Downtown: a block filling most of its lot, 3 floors at the city\'s edge up to about 20 in its middle; the most central downtown lot of each island gets a round tower, the biggest circle on the lot (the lot cell farthest from its edge). Industry: a long low shed with ridges, 1 to 2 floors. Mall: a big low block at the back of its lot, a car park between it and the road. Park: no building, a pond (the biggest circle, smaller) in a big park, and trees: small circles tried at random, kept if on the lot, off the pond and apart from each other.',
    },
    {
        title: 'Docks',
        text: 'Docks on one stretch of each island\'s coast, in the strip between the ring road and the sea (the city stops city margin short of the coast). The site: the ring road is walked a fine cell at a time, every stretch dock length long is scored by how near the sea it runs (the average coast distance of its points), stretches near the bridge left out; the nearest wins. Along it the yard (grey concrete) reaches from the road\'s edge, square to the road and away from the city, to the coast. On it: warehouses along the road in 3 lengths, as deep as the yard allows. From the coast: piers straight out into the water every 7 fine cells, each kept only if all of it is over water, and beside some of them a ship (on water too).',
    },
    {
        title: 'Superblocks',
        text: 'A mall or a park on one street block is long and thin, so it takes in the blocks round it and the streets between them are closed. Two blocks are neighbours across a street if a straight run from a cell of one (4 ways, a few cells) over street cells only reaches the other; those street cells lie between them. Each mall grows to mall blocks blocks, each park to park blocks: one at a time it takes the neighbour whose middle is nearest its own (so it grows round, not long), only from blocks that are houses (or, for a park, park). A block taken in joins it with the street cells between them; those streets are cut out of the street lines (no longer drawn, and no lot faces them). Its houses go, and the mall or park is built on the whole superblock.',
    },
];
