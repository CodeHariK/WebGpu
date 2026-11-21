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
    streetGaps: number; // share of street pieces left out (bigger, irregular blocks)
    houseSize: number; // a house's usual width and depth (fine cells)
    houseFill: number; // share of lots that get a house
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
    streetGaps: 0.15,
    houseSize: 2.6,
    houseFill: 0.85,
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
];
