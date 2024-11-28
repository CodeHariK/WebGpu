import { Vector3 } from "three"
import { Car } from "./car"
import { Game } from "./game"

export const CAR_MONSTER_TRUCK = (game: Game): Car => {
    return new Car(
        game,

        new Vector3(0, 4, 0),

        50000,
        70000,
        10000,
        'RWD',

        1500,

        1,
        2.5,
        .3,

        5,
        7.5,
        -2,
        1,

        2,
        5,
        4,

        100,
        80,
        400,
    )
}

export const CAR_FERRAI = (game: Game): Car => {
    return new Car(
        game,

        new Vector3(0, 4, 0),

        50000,
        70000,
        10000,
        'RWD',

        1500,

        .5,
        1.5,
        .3,

        3.2,
        7.5,
        .2,
        1,

        2,
        5,
        1,

        100,
        100,
        100,
    )
} 