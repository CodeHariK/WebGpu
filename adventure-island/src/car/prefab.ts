import { Vector3 } from "three"
import { Car } from "./car"
import { Game } from "./game"

export const CAR_TOY_CAR = (game: Game): Car => {
    return new Car(
        game,

        new Vector3(0, 15, 0),

        3000,
        3000,
        500,
        'FWD',

        100,

        1,
        10,
        .1,

        1,
        2,
        1,
        .5,

        1.2,
        2.4,
        .3,

        20,
        16,
        20,
    )
}

export const CAR_MONSTER_TRUCK = (game: Game): Car => {
    return new Car(
        game,

        new Vector3(0, 8, 0),

        40000,
        20000,
        5000,
        'FWD',

        1500,

        4,
        4,
        .3,

        5,
        7.5,
        5,
        2,

        3,
        6,
        1,

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
        1,
        1,

        2,
        5,
        1,

        100,
        100,
        100,
    )
}

export const CAR_TURN = (game: Game): Car => {
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
        1,
        1,

        2,
        5,
        1,

        100,
        70,
        400,
    )
} 