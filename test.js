const path = require('path');
const cp = require('child_process');

const handle = cp.fork(path.join(__dirname, 'aclas.js'), { stdio: 'pipe' });

handle.stdout.on('data', function (chunk) {
  const str = chunk.toString();
  console.log(str);
});

console.log('++++ start ++++');
