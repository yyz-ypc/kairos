import fs from 'node:fs';
const text=fs.readFileSync(new URL('../win32/src/brand.h',import.meta.url),'utf8');
const key=process.argv[2]||'BRAND_EXE';
const m=text.match(new RegExp('^#define '+key+' L?"([^"\\n]+)"','m'));
if(!m)throw new Error('Unknown brand field');
console.log(JSON.parse('"'+m[1]+'"'));
