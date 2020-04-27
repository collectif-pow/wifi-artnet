# wifi-artnet

## Prerequisites

- platformio cli: https://docs.platformio.org/en/latest/core/installation.html
- node.js 12.14.1+: https://nodejs.org/en/

## Project setup

```
npm install
```

### Develop web interace

```
npm run serve
```

### Upload firmware

```
npm run upload
```

or

```
pio run -t upload -e lolin32
```

### Upload firmware AND web page to esp32

```
npm run build
```
