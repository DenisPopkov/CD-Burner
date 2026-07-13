# CD Burner

Desktop app for building Compact Disc programs: Side A / Side B layout, CD74 / CD80 / custom length, Prepare, and WAV export.

**Site:** https://denispopkov.github.io/CD-Burner/

## Downloads

| Platform | Link |
|----------|------|
| MacOS (Apple Silicon) | [Site](https://denispopkov.github.io/CD-Burner/#download) / GitHub Releases |
| MacOS (Intel / Mojave) | same |
| Windows | same |
| Linux | same |

## Build

```bash
cmake -C cmake/ProdOptions.cmake -B build -DCMAKE_BUILD_TYPE=Release -DCASSETTE_BUILD_TESTS=OFF
cmake --build build --config Release --target CDBurner -j
```

Tagged `v*` releases build all four platforms via `.github/workflows/release.yml`.
Push to `main` updates GitHub Pages and refreshes Windows / Linux / Intel Mac “latest” downloads.
