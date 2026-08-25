# df_game_data
Minimal but sufficient game data middleware by Alan Wolfe. describe schemas, edit data, pack data as binary, load data, hot reloading support. [MIT licensed](LICENSE).

## Building

TODO: fill out

## Using

### Making a .dbroot file

a .dbroot file is a text file which contains relative paths to the database tables that make up the database, seperated by new lines.

An example: (TODO: put actual example files here when done)

```
NPCs/NPCs.db
weapons/weapons.db
items/items.db
```

### Make a .db file

TODO: fill out and continue

## Open Sourced Software Used

| Software | Comment | URL |
| -- | -- | -- |
| wxWidgets | For editor UI | https://wxwidgets.org/ |
| FlatBuffers | To make binary data and static loaders | https://flatbuffers.dev/ |
