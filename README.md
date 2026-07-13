# CD Burner

Desktop app for Compact Disc programs: CD74 / CD80 / custom length, per-track Red Book prepare, and Burn CD on MacOS, Windows, and Linux.

**Site:** https://denispopkov.github.io/CD-Burner/  
**Latest release:** https://github.com/DenisPopkov/CD-Burner/releases/latest

## Downloads

| Platform | Link |
|----------|------|
| MacOS (Apple Silicon) | [Site](https://denispopkov.github.io/CD-Burner/#download) / [GitHub Releases](https://github.com/DenisPopkov/CD-Burner/releases/latest) |
| MacOS (Intel / Mojave) | same |
| Windows | same |
| Linux | same |

## Build

```bash
cmake -C cmake/ProdOptions.cmake -B build -DCMAKE_BUILD_TYPE=Release -DCASSETTE_BUILD_TESTS=OFF
cmake --build build --config Release --target CDBurner -j
```

Tagged `v*` releases build all four platforms via `.github/workflows/release.yml`.
Push to `main` updates GitHub Pages.
