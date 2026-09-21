"""
기획 엑셀 -> 언리얼 DataTable 용 CSV

    python Data/xlsx_to_csv.py

엑셀을 새로 받으면 Data 폴더에 덮어쓰고 이거 한 번 돌린 다음
에디터에서 해당 DT 우클릭 -> Reimport. 뭐가 바뀌었는지는 git diff 로 보임

필요한 것: pip install openpyxl
"""
import csv
import io
import os
import re
import sys

try:
    import openpyxl
except ImportError:
    sys.exit('openpyxl 이 없음 -> pip install openpyxl')

HERE = os.path.dirname(os.path.abspath(__file__))

# 표 하나당 한 줄
# int_cols 는 빈 칸이면 0 을 넣을 컬럼. 숫자 칸에 빈 문자열이 들어가면
# 엔진이 어떻게 받는지 애매해서 그냥 0 으로 채움
TABLES = [
    dict(src='skill_data.xlsx', sheet='Skill_DataTable', out='DT_Skill.csv',
         int_cols=['EnergyCost', 'SkillEnergyCost', 'BaseValue', 'HitCount',
                   'SecondaryValue', 'StatusValue', 'StatusDuration', 'SelfDamage']),
    dict(src='monster_data.xlsx', sheet='Monsters', out='DT_Monster.csv',
         int_cols=['BaseHP', 'BaseATK', 'BaseDEF',
                   'Skill1_Weight', 'Skill2_Weight', 'Skill3_Weight']),
]


def norm_row_name(name):
    # 엔진 MakeValidName 은 공백이랑 따옴표를 그냥 지워버려서 Aura of Valor 가 AuraofValor 가 됨
    # 기획 Field_Guide 권장대로 소문자 + 언더바로 미리 맞춰둠
    s = name.strip().lower().replace("'", '').replace('"', '')
    s = re.sub(r'[^a-z0-9]+', '_', s)
    return s.strip('_')


def cell(v):
    if v is None:
        return ''
    if isinstance(v, float) and v.is_integer():
        return str(int(v))
    if isinstance(v, str):
        return v.strip()
    return str(v)


def convert(t):
    ws = openpyxl.load_workbook(os.path.join(HERE, t['src']), data_only=True)[t['sheet']]
    rows = list(ws.iter_rows(values_only=True))

    # 몬스터 시트처럼 위에 제목 줄이 붙어 있을 수 있어서 RowName 으로 시작하는 줄을 헤더로 찾음
    hi = next(i for i, r in enumerate(rows) if r and r[0] == 'RowName')
    hdr = [cell(c) for c in rows[hi]]
    while hdr and hdr[-1] == '':
        hdr.pop()
    n = len(hdr)
    int_idx = [hdr.index(c) for c in t['int_cols']]

    out, dropped, renamed = [], 0, 0
    for r in rows[hi + 1:]:
        vals = [cell(r[i]) if i < len(r) else '' for i in range(n)]
        if not vals[0]:
            dropped += 1        # RowName 빈 줄. 엑셀에 서식만 남은 빈 행들
            continue
        fixed = norm_row_name(vals[0])
        if fixed != vals[0]:
            renamed += 1
        vals[0] = fixed
        for i in int_idx:
            if vals[i] == '':
                vals[i] = '0'
        out.append(vals)

    names = [v[0] for v in out]
    dup = sorted({x for x in names if names.count(x) > 1})
    if dup:
        sys.exit('%s: 행 이름 충돌 %s' % (t['out'], dup))

    buf = io.StringIO()
    w = csv.writer(buf, lineterminator='\n')
    w.writerow(hdr)
    w.writerows(out)

    # BOM 은 언리얼은 건너뛰고, 엑셀은 이게 있어야 CSV 열었을 때 한글이 안 깨짐
    with open(os.path.join(HERE, t['out']), 'wb') as f:
        f.write(b'\xef\xbb\xbf' + buf.getvalue().encode('utf-8'))

    print('%-16s %2d행  (빈 행 %d개 버림, 행 이름 정리 %d개)' % (t['out'], len(out), dropped, renamed))


if __name__ == '__main__':
    for t in TABLES:
        convert(t)
