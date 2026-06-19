# Graph Report - .  (2026-06-19)

## Corpus Check
- Corpus is ~2,223 words - fits in a single context window. You may not need a graph.

## Summary
- 123 nodes · 150 edges · 9 communities
- Extraction: 100% EXTRACTED · 0% INFERRED · 0% AMBIGUOUS
- Token cost: 0 input · 0 output

## Community Hubs (Navigation)
- [[_COMMUNITY_Raylib Wrapper|Raylib Wrapper]]
- [[_COMMUNITY_GameMap Structure|GameMap Structure]]
- [[_COMMUNITY_Interface Orchestration|Interface Orchestration]]
- [[_COMMUNITY_Shape & Geometry Types|Shape & Geometry Types]]
- [[_COMMUNITY_RaylibEngine Interface|RaylibEngine Interface]]
- [[_COMMUNITY_GameMap Methods|GameMap Methods]]
- [[_COMMUNITY_Interface Implementation|Interface Implementation]]
- [[_COMMUNITY_Count Label Rendering|Count Label Rendering]]

## God Nodes (most connected - your core abstractions)
1. `Interface` - 19 edges
2. `GameMap` - 14 edges
3. `RaylibEngine` - 13 edges
4. `Shape` - 12 edges
5. `MapTile` - 8 edges
6. `Shape` - 7 edges
7. `Vector3D` - 7 edges
8. `CountLabel` - 6 edges
9. `Vector3D()` - 6 edges
10. `string` - 6 edges

## Surprising Connections (you probably didn't know these)
- None detected - all connections are within the same source files.

## Import Cycles
- None detected.

## Communities (9 total, 0 thin omitted)

### Community 0 - "Raylib Wrapper"
Cohesion: 0.14
Nodes (19): changeTexture(), Camera3D, Color, RaylibEngine(), Vector3D(), Vector3, createShape(), createTextureShape() (+11 more)

### Community 1 - "GameMap Structure"
Cohesion: 0.12
Nodes (19): array, addPlayerToTile, addResource, getHeight, getIndex, getWidth, _height, isValidCoordinate (+11 more)

### Community 2 - "Interface Orchestration"
Cohesion: 0.11
Nodes (19): Camera3D, GameMap, Interface, Model, vector, _camera, _engine, handleInput (+11 more)

### Community 3 - "Shape & Geometry Types"
Cohesion: 0.12
Nodes (17): Color, Model, Vector3D, Shape, color, model, position, size (+9 more)

### Community 4 - "RaylibEngine Interface"
Cohesion: 0.15
Nodes (13): RaylibEngine, beginDrawing, changeTexture, createShape, createTextureShape, createVector, drawShape, endDrawing (+5 more)

### Community 5 - "GameMap Methods"
Cohesion: 0.22
Nodes (3): addPlayerToTile(), uint32_t, removePlayerFromTile()

### Community 6 - "Interface Implementation"
Cohesion: 0.36
Nodes (9): computeNormalizedScale(), Interface(), Model, handleInput(), initCamera(), loadResourceModels(), render(), run() (+1 more)

### Community 7 - "Count Label Rendering"
Cohesion: 0.33
Nodes (6): CountLabel, color, count, worldPos, Color, Vector3

## Knowledge Gaps
- **69 isolated node(s):** `MAP_RESOURCE_COUNT`, `resources`, `uint32_t`, `player_ids`, `getWidth` (+64 more)
  These have ≤1 connection - possible missing edges or undocumented components.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `string` connect `Raylib Wrapper` to `Interface Orchestration`?**
  _High betweenness centrality (0.236) - this node is a cross-community bridge._
- **Why does `RaylibEngine` connect `RaylibEngine Interface` to `Raylib Wrapper`?**
  _High betweenness centrality (0.108) - this node is a cross-community bridge._
- **What connects `MAP_RESOURCE_COUNT`, `resources`, `uint32_t` to the rest of the system?**
  _69 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `Raylib Wrapper` be split into smaller, more focused modules?**
  _Cohesion score 0.14492753623188406 - nodes in this community are weakly interconnected._
- **Should `GameMap Structure` be split into smaller, more focused modules?**
  _Cohesion score 0.12105263157894737 - nodes in this community are weakly interconnected._
- **Should `Interface Orchestration` be split into smaller, more focused modules?**
  _Cohesion score 0.10526315789473684 - nodes in this community are weakly interconnected._
- **Should `Shape & Geometry Types` be split into smaller, more focused modules?**
  _Cohesion score 0.11764705882352941 - nodes in this community are weakly interconnected._