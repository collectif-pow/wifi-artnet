<template>
  <div>
    <nav>
      <div class="nav-wrapper centered">
        <h1 class="title">
          WiFi node by
          <a href="https://pow.cool" target="_bank">pow.cool</a>
        </h1>
      </div>
    </nav>
    <div class="container" v-if="loaded">
      <form class="col s12 settings" @submit.prevent="updateSettings">
        <div class="input-field col s12">
          <i class="material-icons prefix">wifi</i>
          <input id="ssid" type="text" v-model="ssid" />
          <label for="ssid" :class="ssid && 'active'">SSID</label>
        </div>
        <div class="input-field col s12">
          <i class="material-icons prefix" @click.prevent="visible = !visible">
            {{ visible ? 'visibility_off' : 'visibility' }}
          </i>
          <input
            id="password"
            :type="visible ? 'text' : 'password'"
            v-model="password"
          />
          <label for="password" :class="password && 'active'">Password</label>
        </div>
        <div class="input-field col s12">
          <i class="material-icons prefix">edit</i>
          <input id="nodeName" type="text" v-model="nodeName" />
          <label for="nodeName" :class="nodeName && 'active'">Node name</label>
        </div>
        <div class="input-field col s12">
          <i class="material-icons prefix">straighten</i>
          <input
            id="startUniverse"
            type="number"
            :min="startUniverseMin"
            v-model="startUniverse"
            @change="handleStartUniverseChange"
          />
          <label
            for="startUniverse"
            :class="typeof startUniverse === 'number' && 'active'"
          >
            Start Universe
          </label>
        </div>
        <div class="input-field col s12">
          <i class="material-icons prefix">straighten</i>
          <input
            id="pixelCount"
            type="number"
            :min="pixelCountMin"
            :max="pixelCountMax"
            v-model="pixelCount"
            @change="handlePixelCountChange"
          />
          <label
            for="pixelCount"
            :class="typeof pixelCount === 'number' && 'active'"
          >
            Pixel Count
          </label>
        </div>
        <div class="input-field col s12">
          <i class="material-icons prefix">wb_incandescent</i>
          <select v-model="pixelSize" ref="select">
            <option disabled :value="undefined">Please choose</option>
            <option :value="4">RGBW</option>
            <option :value="3">RGB</option>
          </select>
          <label>Pixel type</label>
        </div>

        <div class="row col s12">
          <button
            class="btn waves-effect waves-light col s4 offset-s1"
            type="submit"
            name="action"
            :disabled="!nodeName"
          >
            Update
          </button>
          <button
            class="btn waves-effect waves-light red col s4 offset-s2"
            @click.prevent="reset"
          >
            Reset
          </button>
        </div>
      </form>
    </div>
    <div class="loader" v-else>
      <h2>Loading settings</h2>
      <h2>...</h2>
    </div>
    <div class="toast-layer" v-if="toast" />
  </div>
</template>

<script>
import M from 'materialize-css'

export default {
  name: 'App',
  data: () => ({
    visible: false,
    toast: false,
    pixelCountMin: 10,
    pixelCountMax: 300,
    startUniverseMin: 0,
    loaded: false,
    serverUrl:
      process.env.NODE_ENV === 'production'
        ? window.location.href
        : 'http://192.168.43.15/',
    // v-model default values
    // will be overwritten by the settings api call
    ssid: '',
    password: '',
    nodeName: 'wifi-node',
    pixelSize: 4,
    pixelCount: 60,
    startUniverse: 0,
  }),
  mounted() {
    this.init().then(() => {
      M.FormSelect.init(this.$refs.select)
    })
  },
  methods: {
    handlePixelCountChange(e) {
      const newValue = parseInt(e.target.value, 10)
      if (newValue) {
        if (newValue < this.pixelCountMin) this.pixelCount = this.pixelCountMin
        else if (newValue > this.pixelCountMax)
          this.pixelCount = this.pixelCountMax
        else this.pixelCount = newValue
      }
    },
    handleStartUniverseChange(e) {
      const newValue = parseInt(e.target.value, 10)
      if (newValue) {
        if (newValue < this.startUniverseMin)
          this.startUniverse = this.startUniverseMin
        else this.startUniverse = newValue
      }
    },
    async init() {
      const res = await fetch(`${this.serverUrl}settings`)
      const {
        ssid,
        password,
        nodeName,
        pixelSize,
        pixelCount,
        startUniverse,
      } = await res.json()
      this.ssid = ssid
      this.password = password
      this.nodeName = nodeName
      this.pixelSize = pixelSize
      this.pixelCount = pixelCount
      this.startUniverse = startUniverse
      this.loaded = true
    },
    async reset() {
      this.ssid = ''
      this.password = ''
      this.nodeName = 'wifi-node'
      this.pixelSize = 4
      this.pixelCount = 60
      this.startUniverse = 0
      await this.updateSettings()
    },
    async updateSettings() {
      await fetch(`${this.serverUrl}update`, {
        method: 'POST',
        headers: {
          Accept: 'application/json',
          'Content-Type': 'application/json',
        },
        body: JSON.stringify({
          ssid: this.ssid,
          password: this.password,
          nodeName: this.nodeName,
          pixelSize: this.pixelSize,
          pixelCount: this.pixelCount,
          startUniverse: this.startUniverse,
        }),
      })
      this.toast = true
      M.toast({
        html: `${this.nodeName} will restart! (this page will reload in 15s)`,
        displayLength: 15000,
        completeCallback: () => {
          window.location.reload()
        },
      })
    },
  },
}
</script>

<style lang="css" scoped>
h1 > a {
  font-weight: bold;
}
.centered {
  display: flex;
  justify-content: center;
  align-items: center;
}
.title {
  font-size: 32px;
  font-weight: 100;
  margin: 0;
}
.settings {
  margin-top: 30px;
}
.loader {
  position: absolute;
  width: 100vw;
  height: calc(100vh - 56px);
  top: 56px;
  display: flex;
  flex-direction: column;
  justify-content: center;
  align-items: center;
  text-align: center;
}
.toast-layer {
  position: absolute;
  width: 100vw;
  height: calc(100vh - 56px);
  top: 56px;
  background-color: rgba(255, 255, 255, 0.7);
  z-index: 1;
}

@media (min-width: 601px) {
  .loader,
  .toast-layer {
    height: calc(100vh - 64px);
    top: 64px;
  }
}
</style>
