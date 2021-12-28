'use strict';

const ping = require('bindings')('ping');

const a = new ping.Ping({
    addr: 'www.baidu.com',
    retry: 10,
    timeout: 10
})
const b= a.start();

console.log(a,b)
