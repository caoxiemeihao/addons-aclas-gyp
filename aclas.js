const path = require('path');
const aclas = require('./build/Release/aclas_cc.node');

const host = '192.168.1.2'

console.log(aclas);

aclas({
  host,
  filename: path.join(__dirname, 'txt/PLU.txt'),
  dll_path: path.join(__dirname, 'dll/AclasSDK.dll'),
  type: 0x0000,
  extra: `C++|${host}`,
}, function () { });
