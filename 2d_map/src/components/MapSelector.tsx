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

import './MapSelector.css';

export default function MapSelector() {
    return (
        <div className="selector-container">
            <h1 className="selector-title">Generative Maps Showcase</h1>

            <Tabs.Root defaultValue="map4" className="tabs-root">
                <Tabs.List className="tabs-list">
                    <Tabs.Tab value="map1" className="tabs-tab">
                        Algorithmic Curves
                    </Tabs.Tab>
                    <Tabs.Tab value="map2" className="tabs-tab">
                        City Roads
                    </Tabs.Tab>
                    <Tabs.Tab value="map3" className="tabs-tab">
                        Hedge Maze
                    </Tabs.Tab>
                    <Tabs.Tab value="map4" className="tabs-tab">
                        Seamless Tile
                    </Tabs.Tab>
                    <Tabs.Tab value="map5" className="tabs-tab">
                        Grid Tiling
                    </Tabs.Tab>
                    <Tabs.Indicator className="tabs-indicator" />
                </Tabs.List>

                <Tabs.Panel value="map1" className="tabs-panel">
                    <div className="panel-layout">
                        <div className="canvas-container">
                            <Map1 width={800} height={800} />
                        </div>
                        <div className="explanation-container">
                            <Map1Explanation />
                        </div>
                    </div>
                </Tabs.Panel>

                <Tabs.Panel value="map2" className="tabs-panel">
                    <div className="panel-layout">
                        <div className="canvas-container">
                            <Map2 width={800} height={800} />
                        </div>
                        <div className="explanation-container">
                            <Map2Explanation />
                        </div>
                    </div>
                </Tabs.Panel>

                <Tabs.Panel value="map3" className="tabs-panel">
                    <div className="panel-layout">
                        <div className="canvas-container">
                            <Map3 width={800} height={800} />
                        </div>
                        <div className="explanation-container">
                            <Map3Explanation />
                        </div>
                    </div>
                </Tabs.Panel>

                <Tabs.Panel value="map4" className="tabs-panel">
                    <div className="panel-layout">
                        <div className="canvas-container">
                            <Map4 width={800} height={800} />
                        </div>
                        <div className="explanation-container">
                            <Map4Explanation />
                        </div>
                    </div>
                </Tabs.Panel>

                <Tabs.Panel value="map5" className="tabs-panel">
                    <div className="panel-layout">
                        <div className="canvas-container">
                            <Map5 width={800} height={800} />
                        </div>
                        <div className="explanation-container">
                            <Map5Explanation />
                        </div>
                    </div>
                </Tabs.Panel>
            </Tabs.Root>

        </div>
    );
}
