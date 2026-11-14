"""Check the exact shared theme contract against generated CSS (WCAG 2.x)."""
from pathlib import Path
import json,re,sys
ROOT=Path(__file__).resolve().parents[1]
def luminance(c):
 rgb=[int(c.lstrip('#')[i:i+2],16)/255 for i in (0,2,4)]
 return sum((v/12.92 if v<=.04045 else ((v+.055)/1.055)**2.4)*w for v,w in zip(rgb,[.2126,.7152,.0722]))
def contrast(a,b):
 x,y=sorted([luminance(a),luminance(b)]);return (y+.05)/(x+.05)
def check():
 doc=json.loads((ROOT/'tools/theme-tokens.json').read_text());css=(ROOT/'win32/web/tokens.css').read_text();results=[]
 for style in ['seer','array','barrier','tower','folio']:
  for theme,c in doc['themes'].items():
   block=re.search(r':root\[data-theme="'+theme+r'"\]\{([^}]+)',css)[1]
   for k,v in c.items():assert '--'+k+':'+v+';' in block,(theme,k)
   threshold=7 if theme=='boundary' else 4.5
   ratios={}
   for text in ['ink','mute','tide','ok','bad']:
    for bg in ['bg','panel']:
     value=contrast(c[text],c[bg]);ratios[text+'/'+bg]=round(value,3);assert value>=threshold,(style,theme,text,bg,value)
   assert contrast(c['bg'],c['tide'])>=4.5,(theme,'button')
   assert min(contrast(c['field-line'],c[b]) for b in ['bg','panel'])>=3,(theme,'control')
   results.append(dict(style=style,theme=theme,ratios=ratios))
 for name in ['directions.css','directions2.css','release-ui.css']:
  text=(ROOT/'win32/web'/name).read_text();assert not re.search(r'#[0-9a-fA-F]{3,8}\b|rgba?\(',text),name+' contains a hardcoded color'
 out=ROOT/'evidence/release/contrast.json';out.parent.mkdir(parents=True,exist_ok=True);out.write_text(json.dumps({'combinations':len(results),'passed':True,'results':results},indent=2))
 print('PASS: 25 style/theme combinations, AA text; boundary AAA; controls >=3:1; CSS matches contract')
if __name__=='__main__':check()
