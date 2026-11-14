// Maintainer-only packager: original rift/hermes sources are never copied into a public source export.
import fs from 'node:fs';import path from 'node:path';import {createRequire,Module} from 'node:module';import {execFileSync} from 'node:child_process';
const root=path.resolve('.'),out=path.resolve(process.argv[2]||'.tmp/release-runtime'),deps=createRequire(path.join(root,'.tmp/build-deps/package.json'));
const esbuild=deps('esbuild'),bytenode=deps('bytenode'),acorn=deps('acorn');
if(fs.existsSync(out))throw Error('Choose a new runtime output directory; existing artifacts are preserved');
const mkdir=p=>fs.mkdirSync(p,{recursive:true}),put=(p,s)=>{mkdir(path.dirname(p));fs.writeFileSync(p,s)},copy=(a,b)=>{mkdir(path.dirname(b));fs.copyFileSync(a,b)};
function files(dir){return fs.readdirSync(dir,{withFileTypes:true}).flatMap(e=>e.isDirectory()?files(path.join(dir,e.name)):[path.join(dir,e.name)])}
const manifest={schema:1,node:process.versions.node,v8:process.versions.v8,compiler:{esbuild:'0.25.12',bytenode:'1.5.7',acorn:'8.15.0'},entries:[]};
for(const id of ['hermes','rift']){
 const src=path.join(root,'sidecar',id),dest=path.join(out,'sidecar',id);mkdir(dest);
 const own=files(src).filter(f=>!/[\\/](node_modules|vendor|native|client|test|tests|state|logs|dist|\.selftest)[\\/]/.test(f)&&/\.(mjs|js)$/.test(f));
 // Browser bundles are build artifacts, without original comments or source maps.
 if(id==='hermes')for(const f of files(path.join(src,'client'))){const rel=path.relative(src,f);if(f.endsWith('.js')){const z=await esbuild.transform(fs.readFileSync(f,'utf8'),{minify:true,target:'es2022'});put(path.join(dest,rel),z.code)}else copy(f,path.join(dest,rel))}
 if(id==='rift'&&fs.existsSync(path.join(src,'lib/client.js'))){const z=await esbuild.transform(fs.readFileSync(path.join(src,'lib/client.js'),'utf8'),{minify:true,target:'es2022'});put(path.join(dest,'lib/client.js'),z.code)}
 for(const entry of own){
  if(id==='rift'&&entry.endsWith(path.join('lib','client.js')))continue;
  const rel=path.relative(src,entry),target=path.join(dest,rel),entryDir=path.dirname(entry);
  const plugin={name:'runtime-meta',setup(b){b.onLoad({filter:/\.(mjs|js)$/},async a=>{
    if(!a.path.startsWith(src))return;
    let s=fs.readFileSync(a.path,'utf8');const relative=path.relative(entryDir,a.path).split(path.sep).join('/');
    s=s.replace(/import\.meta\.url/g,`require('node:url').pathToFileURL(require('node:path').join(__dirname,${JSON.stringify(relative)})).href`);
    if(id==='hermes'&&a.path.endsWith(path.join('src','native.mjs'))){
     s=s.replace('process.getBuiltinModule(\'module\').createRequire(${JSON.stringify(agentFile)})(${JSON.stringify(agentFile)}).install','(__KAIROS_EMBEDDED_AGENT__)(process.getBuiltinModule(\'module\').createRequire(${JSON.stringify(agentFile)})).install');
    }
    return{contents:s,loader:'js'};
  })}};
  const build=await esbuild.build({entryPoints:[entry],bundle:true,write:false,format:'esm',platform:'node',target:'node24',packages:'external',plugins:[plugin],metafile:true,logLevel:'silent',minify:true});
  let esm=build.outputFiles[0].text.replace(/^#![^\n]*\n/,'');const exports=Object.values(build.metafile.outputs)[0].exports;
  const ast=acorn.parse(esm,{ecmaVersion:'latest',sourceType:'module'});const imports=ast.body.filter(n=>n.type==='ImportDeclaration'),exportNodes=ast.body.filter(n=>n.type==='ExportNamedDeclaration');
  const exported=exportNodes.flatMap(n=>n.specifiers.map(x=>JSON.stringify(x.exported.name)+':'+x.local.name));
  let body=esm;for(const n of [...imports,...exportNodes].sort((a,b)=>b.start-a.start))body=body.slice(0,n.start)+body.slice(n.end);
  esm=imports.map(n=>esm.slice(n.start,n.end)).join('\n')+'\nmodule.exports=(async()=>{'+body+';return {'+exported.join(',')+'};})()';
  const code=(await esbuild.transform(esm,{format:'cjs',platform:'node',target:'node24',minify:true})).code;
  const bytefile=target.replace(/\.(mjs|js)$/,'.jsc');mkdir(path.dirname(bytefile));fs.writeFileSync(bytefile,bytenode.compileCode(Module.wrap(code)));
  const loader=path.relative(path.dirname(target),path.join(dest,'vendor/bytenode/index.cjs')).split(path.sep).join('/');const byte='./'+path.basename(bytefile);
  let stub=`// Generated runtime entry; implementation is compiled V8 bytecode.\nimport {createRequire} from 'node:module';const require=createRequire(import.meta.url);require(${JSON.stringify(loader.startsWith('.')?loader:'./'+loader)});const m=await require(${JSON.stringify(byte)});\n`;
  for(const name of exports)stub+=name==='default'?'export default m.default;\n':`export const ${name}=m.${name};\n`;
  put(target,stub);manifest.entries.push({component:id,path:rel,compiled:path.relative(out,bytefile),exports});
 }
 for(const f of files(src)){
  const rel=path.relative(src,f).split(path.sep).join('/');
  if(/(^|\/)(state|logs|test|tests|\.selftest|dist|\.git|__pycache__)(\/|$)/.test(rel)||/\.(log|pyc|tmp)$/.test(rel))continue;
  if(/^(vendor\/|node_modules\/|bin\/)/.test(rel)||/^native\/build\/.*\.(dll|exe)$/.test(rel)||/(LICENSE|NOTICE|COPYING)/i.test(path.basename(f))||['package.json','package-lock.json','start.cmd','cordis.patch.yml'].includes(rel))copy(f,path.join(dest,rel));
 }
 copy(path.join(root,'.tmp/build-deps/node_modules/bytenode/lib/index.js'),path.join(dest,'vendor/bytenode/index.cjs'));
 for(const name of ['LICENSE','package.json']){const f=path.join(root,'.tmp/build-deps/node_modules/bytenode',name);if(fs.existsSync(f))copy(f,path.join(dest,'vendor/bytenode',name))}
 if(id==='hermes'){
  // Compile the QQ-version-independent agent into the native DLL, not Node-specific bytecode.
  const privateNative=path.join(out,'.private-native');fs.cpSync(path.join(src,'native'),privateNative,{recursive:true});
  const agent=fs.readFileSync(path.join(src,'native/agent.cjs'),'utf8');const wrapped='(function(require){const module={exports:{}};const exports=module.exports;'+agent+';return module.exports;})';
  const encoded=[...Buffer.from(wrapped)].map(v=>v^0xa7);put(path.join(privateNative,'agent_blob.h'),'static const unsigned char kairosAgentBlob[]={'+encoded.join(',')+'};\n');
  let hook=fs.readFileSync(path.join(privateNative,'hook.cpp'),'utf8').replace('#include <atomic>','#include <atomic>\n#include "agent_blob.h"');
  hook=hook.replace('bootstrap.assign(std::istreambuf_iterator<char>(input), {});','bootstrap.assign(std::istreambuf_iterator<char>(input), {});\n    const std::string marker="__KAIROS_EMBEDDED_AGENT__";const auto markerAt=bootstrap.find(marker);if(markerAt!=std::string::npos){std::string agent;for(auto v:kairosAgentBlob)agent.push_back((char)(v^0xa7));bootstrap.replace(markerAt,marker.size(),agent);}');
  put(path.join(privateNative,'hook.cpp'),hook);
  execFileSync('powershell.exe',['-NoProfile','-ExecutionPolicy','Bypass','-File',path.join(privateNative,'build.ps1'),'-Compiler',process.env.MINGW_BIN?path.join(process.env.MINGW_BIN,'g++.exe'):'F:/biancheng/mingw64/bin/g++.exe'],{stdio:'inherit'});
  for(const name of ['hermes-qq-hook.dll','hermes-qq-launcher.exe'])copy(path.join(privateNative,'build',name),path.join(dest,'native/build',name));
  put(path.join(dest,'native/agent.cjs'),'// Native agent implementation is embedded in hermes-qq-hook.dll.\n');
 }
 console.log('Compiled',id,own.length,'entries');
}
for(const id of ['node','keeper'])if(fs.existsSync(path.join(root,'sidecar',id)))fs.cpSync(path.join(root,'sidecar',id),path.join(out,'sidecar',id),{recursive:true});
for(const name of ['rift.config.template.json'])if(fs.existsSync(path.join(root,'sidecar',name)))copy(path.join(root,'sidecar',name),path.join(out,'sidecar',name));
put(path.join(out,'runtime-build.json'),JSON.stringify(manifest,null,2));
console.log('Runtime build completed:',out);
