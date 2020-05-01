const express = require('express')
const cors = require('cors')

const app = express()

app.use(cors())
app.use(express.json())

let settings = {
  ssid: 'test',
  password: 'password',
  nodeName: 'node',
  pixelSize: 4,
  pixelCount: 60,
  startUniverse: 0,
  sync: true,
}

app.get('/settings', (req, res) => {
  res.json(settings)
})

app.post('/update', (req, res) => {
  settings = req.body
  res.json({ ok: true })
})

app.listen(3000, () => {
  console.log('dev server started')
})
