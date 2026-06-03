# gui/assets/

Binary assets for the GUI. **Tracked via Git LFS** (see root `.gitattributes`).

```
models/     glTF/glb meshes (drones, eggs, resources, terrain)
textures/   KTX2 compressed textures
audio/      ogg/wav sound effects & ambience
```

## Rules

- **License**: only assets we may redistribute (CC0 / CC-BY with attribution, or
  self-made). Record the source + license of every asset in `CREDITS.md` here.
- **Format**: meshes glTF 2.0 (`.gltf`/`.glb`), textures KTX2 (BasisU), audio ogg.
- Run `tools/build_assets.sh` to (re)compress source art into the runtime formats.
- Keep raw/source art OUT of the repo (Drive/cloud); commit only runtime-ready files.
