/* Exercise the real shared page script against a small DOM/HTTP adapter.
 * This checks endpoint semantics and text handling, not browser rendering. */
const fs = require('node:fs');
const vm = require('node:vm');
const assert = require('node:assert/strict');
const path = require('node:path');
const buttons = [];
function element(tag = 'div') {
  const node = {children: [], textContent: '', value: '', disabled: false,
    append(...children) { this.children.push(...children); },
    replaceChildren(...children) { this.children = children; }, focus() {}};
  if (tag === 'button') buttons.push(node);
  Object.defineProperty(node, 'innerHTML', {set() { throw Error('Do not insert SSIDs as HTML'); }});
  return node;
}
const nodes = Object.fromEntries(['status','networks','saved','ssid','password','connect','scan','exit'].map(id=>[id,element()]));
const unusual = '<img src=x onerror=alert(1)>"\\';
let saved = ['Home','Office'], calls = [], error = false, loseSubmit=false, failRefresh=false, connectionResult={success:true};
async function fetch(url, options) {
  calls.push({url, options}); let data = {};
  if (error) throw Error('Network unavailable');
  if (url === '/saved/list') { if(failRefresh) throw Error('Refresh lost'); data = saved.slice(); }
  else if (url === '/scan') data = {aps:[{ssid:unusual,rssi:-50},{ssid:unusual,rssi:-60},{ssid:'',rssi:-20}]};
  else if (url === '/submit') {
    const body = JSON.parse(options.body); saved.unshift(body.ssid); data = {success:true};
    if(loseSubmit) throw Error('Response lost during channel switch');
  } else if (url.startsWith('/connection/status?id=')) {
    const body=JSON.parse(calls.filter(c=>c.url==='/submit').at(-1).options.body);
    assert.equal(decodeURIComponent(url.split('=')[1]),body.request_id);
    data=connectionResult;
  } else if (url.startsWith('/saved/delete?index=')) saved.splice(Number(url.split('=')[1]),1);
  else if (url.startsWith('/saved/set_default?index=')) saved.unshift(...saved.splice(Number(url.split('=')[1]),1));
  else if (url === '/exit') data = {success:true};
  else throw Error('Unexpected endpoint '+url);
  return {ok:true,json:async()=>data};
}
const document = {getElementById:id=>nodes[id],createElement:element,querySelectorAll:()=>buttons};
const context = {document,fetch,AbortController,TextEncoder,setTimeout,clearTimeout};
vm.createContext(context);
const html = fs.readFileSync(path.join(__dirname,'../src/application/ui/web/wifi_configuration.html'),'utf8');
vm.runInContext(html.match(/<script>([\s\S]*?)<\/script>/)[1]+'\nglobalThis.test={action,scan,saved,busy:()=>busy};',context);
async function idle() {
  for (let i=0;i<500 && context.test.busy();++i) await new Promise(resolve=>setTimeout(resolve,10));
  assert.equal(context.test.busy(),false);
}
(async()=>{
  await idle(); assert.equal(nodes.networks.children.length,1);
  assert.equal(nodes.networks.children[0].textContent,unusual+' · -50 dBm');
  await nodes.networks.children[0].onclick(); assert.equal(nodes.ssid.value,unusual);
  await nodes.saved.children[1].children[1].onclick(); assert.deepEqual(saved,['Office','Home']);
  await nodes.saved.children[1].children[2].onclick(); assert.deepEqual(saved,['Office']);
  nodes.ssid.value='中文网络'; nodes.password.value='a"\\password';
  nodes.connect.onsubmit({preventDefault(){}}); await idle();
  const submit = calls.find(c=>c.url==='/submit');
  const submitted=JSON.parse(submit.options.body);
  assert.equal(submitted.ssid,'中文网络'); assert.equal(submitted.password,'a"\\password');
  assert.ok(submitted.request_id.length>0 && submitted.request_id.length<=32);
  assert.equal(nodes.password.value,''); assert.equal(saved[0],'中文网络');
  const before = calls.length; nodes.ssid.value='中'.repeat(11);
  nodes.connect.onsubmit({preventDefault(){}}); await idle(); assert.equal(calls.length,before);
  assert.match(nodes.status.textContent,/32/);
  nodes.ssid.value='Recovered';loseSubmit=true;failRefresh=true;
  const submitsBefore=calls.filter(c=>c.url==='/submit').length;
  nodes.connect.onsubmit({preventDefault(){}});await idle();
  assert.match(nodes.status.textContent,/连接成功/);
  assert.equal(calls.filter(c=>c.url==='/submit').length,submitsBefore+1);
  connectionResult={success:false,error:'Storage write failed'};
  nodes.connect.onsubmit({preventDefault(){}});await idle();
  assert.equal(nodes.status.textContent,'Storage write failed');
  loseSubmit=false;failRefresh=false;
  error=true; await nodes.scan.onclick(); assert.match(nodes.status.textContent,/AI-Setup/);
  error=false; await nodes.exit.onclick(); assert.equal(calls.at(-1).url,'/exit');
  assert.equal(calls.at(-1).options.method,'POST');
  console.log('Shared Wi-Fi page: scan/dedup, safe SSID text, connect/save, default/delete, exit and errors passed.');
})().catch(e=>{console.error(e);process.exitCode=1;});
