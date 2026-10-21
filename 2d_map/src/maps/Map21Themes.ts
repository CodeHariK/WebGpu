// Combat Sandbox — the themes: each a different place in the human operation on the clown planet,
// with its own look (Map21RenderWorld) and its own set of levels that get harder as you go.
import { FACTORY_LEVELS, type LevelDef } from './Map21Levels';
import { PLANT_LEVELS } from './Map21LevelsPlant';

export type ThemeId = 'factory' | 'plant';
export type Theme = { id: ThemeId; name: string; levels: LevelDef[] };

export const THEMES: Theme[] = [
    { id: 'factory', name: 'Toxic Factory', levels: FACTORY_LEVELS },
    { id: 'plant', name: 'Toy Assembly Plant', levels: PLANT_LEVELS },
];
