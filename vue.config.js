module.exports = {
  filenameHashing: false,
  productionSourceMap: false,
  chainWebpack: config => {
    config.optimization.delete('splitChunks')
  },
}
