# -*- coding: utf-8 -*-
# 밸런싱 엑셀 -> Assets/Terminus/Resources/Data/DT_*.csv 반영
#   python -X utf8 Tools/Balance/xlsx_to_csv.py [엑셀폴더] [--write] [-v]
#   --write 없으면 바뀔 칸만 보여 줌. 엑셀폴더 생략 시 OneDrive/*/테르미누스 (code_artifact.html 있는 곳)
# 규칙: 엑셀 빈칸은 CSV 값 유지, 스킬은 SkillID 로 매칭, CSV 의 열 구조 / BOM / CRLF 유지, 엑셀에 없는 열(Icon 등)은 그대로
import openpyxl,csv,io,sys,os
sys.stdout.reconfigure(encoding='utf-8')
import glob
_args=[a for a in sys.argv[1:] if not a.startswith('-')]
if _args: X=os.path.join(_args[0],'')
else:
  _found=glob.glob(os.path.expanduser('~')+'/OneDrive/*/*/code_artifact.html')
  if not _found: sys.exit('엑셀 폴더를 못 찾음. 인자로 넘겨 주세요')
  X=os.path.dirname(_found[0])+os.sep
D=os.path.join(os.path.dirname(os.path.abspath(__file__)),'..','..','Assets','Terminus','Resources','Data','')
WRITE='--write' in sys.argv
ALIAS={'RelicName_KR':'RelicName','RelicDesc_KR':'RelicDesc','SpecialMechanic':'SpecialMechanic_Notes','ChoiceID':'RowName','QuestID':'RowName','RoomID':'RowName'}
JOBS=[('DT_Monster','monster_data.xlsx','Monsters',4),('DT_Skill','skill_data.xlsx','Skill_DataTable',1),('DT_Relic','skill_data.xlsx','Relic_DataTable',3),
 ('DT_Event','Event_DataTable.xlsx','Event_DataTable',1),('DT_EventChoice','Event_DataTable.xlsx','Event_Choices',1),('DT_Quest','Quest_DataTable.xlsx','Quest_DataTable',1),('DT_QuestRoom','Quest_DataTable.xlsx','Room_DataTable',1)]
def fmt(v):
  if v is None: return ''
  if isinstance(v,bool): return 'True' if v else 'False'
  if isinstance(v,float) and v.is_integer(): return str(int(v))
  if isinstance(v,float): return repr(round(v,6))
  return str(v)
def num(s):
  try: return float(s)
  except: return None
# skill id -> row name (from current CSV)
sk={r['SkillID']:r['RowName'] for r in csv.DictReader(open(D+'DT_Skill.csv',encoding='utf-8-sig'))}
def same(old,new):
  if old==new or new=='': return True  # 엑셀 빈칸 = CSV 값 유지 (None/0/IconID 등 유니티 쪽 채움)
  a,b=num(old),num(new)
  if a is not None and b is not None and abs(a-b)<1e-9: return True
  if sk.get(new)==old: return True
  if old.replace('\r\n','\n')==new.replace('\r\n','\n'): return True
  return False
for csvname,xf,sheet,hr in JOBS:
  raw=open(D+csvname+'.csv','rb').read().decode('utf-8-sig')
  rows=list(csv.reader(io.StringIO(raw,newline='')))
  head,body=rows[0],rows[1:]
  ws=openpyxl.load_workbook(X+xf,data_only=True)[sheet]
  xr=list(ws.iter_rows(min_row=hr,values_only=True))
  xh=[ALIAS.get(str(h),str(h)) if h is not None else None for h in xr[0]]
  idx={h:i for i,h in enumerate(xh) if h in head}
  missing=[h for h in head if h not in idx]
  key='SkillID' if csvname=='DT_Skill' else 'RowName'  # 유니티에서 행 이름 7개 바뀜 (king's_sword -> kings_sword 등)
  kc=head.index(key)
  xrows={fmt(r[idx[key]]):r for r in xr[1:] if r[idx[key]] not in (None,'')}
  print(f'\n== {csvname} <- {xf}/{sheet}  엑셀 {len(xrows)}행 / CSV {len(body)}행. 엑셀에 없는 열: {missing}')
  have={b[kc] for b in body}
  print('  CSV에만:',[k for k in have if k not in xrows][:20],' 엑셀에만:',[k for k in xrows if k not in have][:20])
  changes=[]
  for b in body:
    r=xrows.get(b[kc])
    if r is None: continue
    for c,h in enumerate(head):
      if h not in idx or h=='RowName': continue
      new=fmt(r[idx[h]])
      if not same(b[c],new):
        changes.append((b[0],h,b[c],new)); b[c]=new
  print('  바뀐 칸',len(changes))
  for ch in changes:
    if '-v' in sys.argv or ch[1] not in ('Description_KR','Notes','RelicDesc','Concept_KR','SpecialMechanic_Notes','Design_Notes','Choice_Design_Notes','ChoiceTooltip_KR','ResultText_KR','SituationDesc_KR','QuestDesc'):
      print('   ',ch[0],ch[1],repr(ch[2][:30]),'->',repr(ch[3][:30]))
    else: print('   ',ch[0],ch[1],'(텍스트 변경)')
  if WRITE:
    o=io.StringIO(newline=''); csv.writer(o,lineterminator='\r\n').writerows([head]+body)
    open(D+csvname+'.csv','wb').write(b'\xef\xbb\xbf'+o.getvalue().encode())
