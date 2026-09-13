import { useState } from 'react';
import { Tabs } from '@base-ui/react/tabs';
import Map1 from '../maps/Map1';
import Map1Explanation from '../maps/Map1Explanation';
import Map2 from '../maps/Map2';
import Map2Explanation from '../maps/Map2Explanation';
import Map3 from '../maps/Map3';
import Map3Explanation from '../maps/Map3Explanation';
import Map4 from '../maps/Map4';
import Map4Explanation from '../maps/Map4Explanation';
import Map5 from '../maps/Map5';
import Map5Explanation from '../maps/Map5Explanation';
import Map6 from '../maps/Map6';
import Map6Explanation from '../maps/Map6Explanation';
import Map7 from '../maps/Map7';
import Map7Explanation from '../maps/Map7Explanation';
import Map8 from '../maps/Map8';
import Map8Explanation from '../maps/Map8Explanation';
import Map9 from '../maps/Map9';
import Map9Explanation from '../maps/Map9Explanation';
import Map10 from '../maps/Map10';
import Map10Explanation from '../maps/Map10Explanation';
import Map11 from '../maps/Map11';
import Map11Explanation from '../maps/Map11Explanation';
import Map12 from '../maps/Map12';
import Map12Explanation from '../maps/Map12Explanation';
import Map13 from '../maps/Map13';
import Map13Explanation from '../maps/Map13Explanation';
import Map14 from '../maps/Map14';
import Map14Explanation from '../maps/Map14Explanation';
import Map15 from '../maps/Map15';
import Map15Explanation from '../maps/Map15Explanation';
import Map16 from '../maps/Map16';
import Map16Explanation from '../maps/Map16Explanation';
import Map17 from '../maps/Map17';
import Map17Explanation from '../maps/Map17Explanation';
import Map18 from '../maps/Map18';
import Map18Explanation from '../maps/Map18Explanation';

import './MapSelector.css';

type MapEntry = {
    value: string;
    label: string;
    Comp: React.ComponentType<{ width: number; height: number }>;
    Expl: React.ComponentType;
};

const MAPS: MapEntry[] = [
    { value: 'map1', label: 'Algorithmic Curves', Comp: Map1, Expl: Map1Explanation },
    { value: 'map2', label: 'City Roads', Comp: Map2, Expl: Map2Explanation },
    { value: 'map3', label: 'Hedge Maze', Comp: Map3, Expl: Map3Explanation },
    { value: 'map4', label: 'Seamless Tile', Comp: Map4, Expl: Map4Explanation },
    { value: 'map5', label: 'Grid Tiling', Comp: Map5, Expl: Map5Explanation },
    { value: 'map6', label: 'Plant Growth', Comp: Map6, Expl: Map6Explanation },
    { value: 'map7', label: 'Procedural City', Comp: Map7, Expl: Map7Explanation },
    { value: 'map8', label: 'Multi-Tree', Comp: Map8, Expl: Map8Explanation },
    { value: 'map9', label: 'Mansion Layout', Comp: Map9, Expl: Map9Explanation },
    { value: 'map10', label: 'Voronoi Zone', Comp: Map10, Expl: Map10Explanation },
    { value: 'map11', label: 'Topographic Map', Comp: Map11, Expl: Map11Explanation },
    { value: 'map12', label: 'Marching Squares', Comp: Map12, Expl: Map12Explanation },
    { value: 'map13', label: 'Hex Growth', Comp: Map13, Expl: Map13Explanation },
    { value: 'map14', label: 'Hex Truchet', Comp: Map14, Expl: Map14Explanation },
    { value: 'map15', label: 'Street Growth', Comp: Map15, Expl: Map15Explanation },
    { value: 'map16', label: 'Shape Packer', Comp: Map16, Expl: Map16Explanation },
    { value: 'map17', label: 'Watabou City', Comp: Map17, Expl: Map17Explanation },
    { value: 'map18', label: 'Hex Biomes', Comp: Map18, Expl: Map18Explanation },
];

export default function MapSelector() {
    const [value, setValue] = useState<string>('map17');

    return (
        <div className="selector-container">
            <nav className="sidebar">
                <h1 className="sidebar-title">Generative Maps</h1>
                <ul className="sidebar-nav">
                    {MAPS.map((m) => (
                        <li key={m.value}>
                            <button
                                type="button"
                                className={`sidebar-item${value === m.value ? ' active' : ''}`}
                                aria-current={value === m.value ? 'page' : undefined}
                                onClick={() => setValue(m.value)}
                            >
                                {m.label}
                            </button>
                        </li>
                    ))}
                </ul>
            </nav>

            <main className="content">
                <Tabs.Root value={value} onValueChange={(v) => setValue(v as string)} className="tabs-root">
                    {MAPS.map(({ value: v, Comp, Expl }) => (
                        <Tabs.Panel key={v} value={v} className="tabs-panel">
                            <div className="panel-layout">
                                <div className="canvas-container">
                                    <Comp width={800} height={800} />
                                </div>
                                <div className="explanation-container">
                                    <Expl />
                                </div>
                            </div>
                        </Tabs.Panel>
                    ))}
                </Tabs.Root>
            </main>
        </div>
    );
}
