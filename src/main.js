import Vue from 'vue'
import App from './App.vue'
import 'materialize-css/dist/css/materialize.min.css'

Vue.config.productionTip = false

new Vue({
  render: h => h(App),
}).$mount('#app')