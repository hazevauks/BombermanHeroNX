# SD_CARD

The test build, laid out the way the SD card expects it. `python tools/build.py sd` fills this folder:

```text
SD_CARD/
└── switch/
    └── BombermanHeroNX/
        ├── BombermanHeroNX.nro
        ├── recompcontrollerdb.txt
        ├── assets/              menus, fonts and icons of the launcher
        └── bmhero.z64           your ROM (added with --rom, or copied by hand)
```

Copy what is inside `SD_CARD/` to the root of the card. The game creates `config/`, `saves/`, `mods/`,
`cache/` and `SD_LOG/` next to the NRO the first time it runs.

Only this README is kept in the repository.
