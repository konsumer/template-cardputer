The purpose of this is a full build system, designed for M5 CardputerADV with CAP-1262 lora hat.

I want to emulate everything on native, so I can quickly test things, in a much easier way.

```sh
# reloading web version
npm start

# build and uplaod to real cardputer
npm run cardputer

# run native
npm run native
```

The web-demo lets you test the radio:

```js
import loadFW from './index.mjs'

const m = await loadFW({ canvas: document.getElementById('canvas') })

// Hook outgoing LoRa sends
m.loraOut = bytes => console.log('LoRa sent:', bytes)

// Simulate incoming LoRa bytes
m.loraInject([0x48, 0x65, 0x6c, 0x6c, 0x6f])

// fake the number of  satellites seen
m.satelliteCount = 5

// Set a GPS fix
m.gpsSet(51.5074, -0.1278)
```