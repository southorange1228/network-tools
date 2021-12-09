'use strict';

const ping = require('bindings')('ping');

console.log(ping.hello())
