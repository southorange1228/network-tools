'use strict';

const ping = require('bindings')('ping');

const a = new ping.Ping({
    addr: '127.0.0.1',
    retry: 10,
    timeout: 10
})

console.log(a)
