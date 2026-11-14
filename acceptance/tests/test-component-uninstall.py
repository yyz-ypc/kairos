import pathlib,subprocess,json,uuid
ROOT=pathlib.Path(__file__).resolve().parents[2];base=ROOT/'.tmp/uninstall-contract'/uuid.uuid4().hex
rift=base/'bots/Rift';(rift/'node_modules/test').mkdir(parents=True);(rift/'data').mkdir();(rift/'package.json').write_text('{}');(rift/'node_modules/test/package.json').write_text('{}');(rift/'data/config.json').write_text('keep');(rift/'config.json').write_text('keep')
node=base/'sidecar/node/node.exe';node.parent.mkdir(parents=True);node.write_bytes(b'test-required-node')
def run(id):return subprocess.run(['powershell.exe','-NoProfile','-ExecutionPolicy','Bypass','-File',str(ROOT/'tools/bootstrap.ps1'),'-Base',str(base),'-Workspace',str(base),'-Bots',str(base/'bots'),'-Component',id,'-Uninstall'],capture_output=True,timeout=20)
r=run('rift');assert r.returncode==0,r.stdout+r.stderr;assert not (rift/'package.json').exists();assert not (rift/'node_modules').exists();assert node.exists();assert (rift/'data/config.json').read_text()=='keep';assert (rift/'config.json').read_text()=='keep'
r=run('node');assert r.returncode!=0;assert node.read_bytes()==b'test-required-node'
out=ROOT/'evidence/component-supply/uninstall.json';out.write_text(json.dumps({'default_uninstall':True,'dependencies_kept':True,'user_data_kept':True,'required_uninstall_rejected':True},indent=2),encoding='utf-8');print(out.read_text())
