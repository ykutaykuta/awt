var ref = require('ref-napi');
var ffi = require('ffi-napi');

var AWT = ref.types.void;
var AWTPtr = ref.refType(AWT);
var AWTPtrPtr = ref.refType(AWTPtr);
const sleep = ms => new Promise( res => setTimeout(res, ms));

var counter = 0;
var callback = ffi.Callback('int', ['int', 'CString', 'float', 'float'], function(channel, wm, percent, pos_at) {
    console.log(`found at ${pos_at}s #${channel}: ${wm} ${percent}`);
    counter++;
    return counter > 20;
});

var log_callback = ffi.Callback('void', ['CString', 'CString', 'int'], function(level, msg, ff_ret) {
    console.log(`${level} ffmpeg ret_code ${ff_ret}: ${msg}`);
});

var libawt = ffi.Library("../prebuilt/libawt", {
    "awt_open": ['int', [AWTPtrPtr, 'int', 'int', 'int', 'float', 'int', 'pointer', 'pointer']],
    "awt_exec": ['int', [AWTPtrPtr, 'CString']],
    "awt_close": ['int', [AWTPtrPtr]]
});

var awt = ref.alloc(AWTPtrPtr);
libawt.awt_open(awt, 0, -1, 1, 20, 0, callback, log_callback);
var run = 1;
while (run-- > 0) {
    libawt.awt_exec(awt, "../test.mkv");
    sleep(200);
}
libawt.awt_close(awt);
console.log(counter);
