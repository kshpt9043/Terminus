"""
기획 엑셀 -> 언리얼 DataTable 용 CSV

    python Data/xlsx_to_csv.py

엑셀을 새로 받으면 Data 폴더에 덮어쓰고 이거 한 번 돌린 다음
에디터에서 해당 DT 우클릭 -> Reimport. 뭐가 바뀌었는지는 git diff 로 보임

필요한 것: pip install openpyxl

언리얼 CSV 임포트는 모르는 열거형 값을 에러 없이 기본값으로 바꿔 버림
-> 여기서 C++ 헤더의 열거형과 대조해서 하나라도 없으면 멈춤 (값을 새로 받으면 헤더 맨 뒤에 추가)
"""
import csv
import io
import os
import re
import sys
import warnings

try:
    import openpyxl
except ImportError:
    sys.exit('openpyxl 이 없음 -> pip install openpyxl')

# 엑셀의 데이터 유효성 검사 확장을 openpyxl 이 못 읽는다는 경고. 값엔 상관없음
warnings.filterwarnings('ignore', category=UserWarning, module='openpyxl')

HERE = os.path.dirname(os.path.abspath(__file__))
HEADERS = os.path.join(HERE, '..', 'Source', 'Terminus', 'Public', 'Data')

# 표 하나당 한 줄
# int_cols  : 빈 칸이면 int_default(기본 0) 를 넣을 컬럼. 숫자 칸에 빈 문자열이 들어가면 엔진이 어떻게 받는지 애매함
# none_cols : 빈 칸이면 None 을 넣을 열거형 컬럼 (빈 칸 그대로면 열거형 첫 값이 들어감)
# enums     : 컬럼 -> C++ 열거형 이름. 값이 헤더에 없으면 멈춤
# rename    : 엑셀 열 이름 -> 구조체 프로퍼티 이름
# drop      : 기획용 열이라 안 가져감
# add       : 엑셀에 없는 열 (이름, 기본값). 맨 뒤에 붙음
# keep_names: 행 이름을 그대로 둠 (RLC_Common_001 처럼 코드가 대문자로 찾는 것)
# skill_refs: DT_Skill 행 이름을 가리키는 컬럼 -> 스킬 행 이름과 같은 규칙으로 정리
TABLES = [
    dict(src='skill_data.xlsx', sheet='Skill_DataTable', out='DT_Skill.csv',
         int_cols=['EnergyCost', 'SkillEnergyCost', 'BaseValue', 'HitCount',
                   'SecondaryValue', 'StatusValue', 'StatusDuration', 'SelfDamage',
                   'BuyPrice_Dungeon', 'SellPrice_Dungeon'],
         float_cols=['ScalingRatio'],
         none_cols=['ScalingStat', 'StatusEffect'],
         enums={'OwnerClass': 'ESkillOwner', 'SkillCategory': 'ESkillCategory', 'SkillType': 'ESkillType',
                'SkillTier': 'ESkillTier', 'ActionKind': 'EActionKind', 'TargetType': 'ETargetType',
                'ScalingStat': 'EScalingStat', 'StatusEffect': 'EStatusEffect'}),
    dict(src='skill_data.xlsx', sheet='Relic_DataTable', out='DT_Relic.csv',
         int_cols=['BaseValue', 'StatusValue', 'StatusDuration', 'RemainBattle',
                   'BuyPrice_Dungeon', 'SellPrice_Dungeon', 'SellPrice_Gold'],
         int_default={'RemainBattle': '-1', 'BuyPrice_Dungeon': '-1', 'SellPrice_Dungeon': '-1', 'SellPrice_Gold': '-1'},
         float_cols=['ScalingRatio'],
         none_cols=['ScalingStat', 'StatusEffect', 'OnSkill'],
         enums={'RelicTier': 'ERelicTier', 'OwnerClass': 'ESkillOwner', 'TriggerTiming': 'ERelicTrigger',
                'ActionKind': 'EActionKind', 'TargetType': 'ETargetType',
                'ScalingStat': 'EScalingStat', 'StatusEffect': 'EStatusEffect'},
         rename={'RelicName_KR': 'RelicName', 'RelicDesc_KR': 'RelicDesc'},
         drop=['Calc_StandardPrice', 'Effect_Category'],
         add=[('Icon', 'None')],
         keep_names=True,
         skill_refs=['OnSkill']),
    dict(src='monster_data.xlsx', sheet='Monsters', out='DT_Monster.csv',
         int_cols=['BaseHP', 'BaseATK', 'BaseDEF',
                   'Skill1_Weight', 'Skill2_Weight', 'Skill3_Weight'],
         enums={'MonsterCategory': 'EMonsterCategory'},
         skill_refs=['Skill1_ID', 'Skill2_ID', 'Skill3_ID']),

    # 이벤트 / 퀘스트. 첫 열이 RowName 이 아니라 ChoiceID / QuestID / RoomID 인 시트는 key 로 알려 줌
    dict(src='Event_DataTable.xlsx', sheet='Event_DataTable', out='DT_Event.csv',
         int_cols=['AppearanceWeight', 'ChoiceCount'],
         enums={'AppearTier': 'EEventAppearTier', 'EventConceptType': 'EEventConceptType'},
         keep_names=True),
    dict(src='Event_DataTable.xlsx', sheet='Event_Choices', out='DT_EventChoice.csv', key='ChoiceID',
         int_cols=['ChoiceIndex', 'CostValue', 'OutcomeValueMin', 'OutcomeValueMax', 'BattleDuration_Limit'],
         int_default={'BattleDuration_Limit': '-1'},
         none_cols=['CostType'],
         enums={'CostType': 'EEventCostType', 'OutcomeType': 'EEventOutcomeType'},
         keep_names=True),
    dict(src='Quest_DataTable.xlsx', sheet='Quest_DataTable', out='DT_Quest.csv', key='QuestID',
         int_cols=['AppearanceWeight', 'ConditionValue', 'AcceptRewardValue', 'ClearRewardValue'],
         none_cols=['ConditionType', 'AcceptRewardType', 'ClearRewardType'],
         enums={'ConditionType': 'EQuestConditionType', 'AcceptRewardType': 'EQuestRewardType',
                'ClearRewardType': 'EQuestRewardType'},
         keep_names=True),
    dict(src='Quest_DataTable.xlsx', sheet='Room_DataTable', out='DT_QuestRoom.csv', key='RoomID',
         int_cols=['Floor', 'SpawnValue', 'Depth'],
         keep_names=True),
]

# 진짜 데이터 행: 영문자로 시작하고 공백 없음. 시트 중간의 메모 줄("패시브: ...", "SecondaryValue = ...")은 버림
ROW_NAME = re.compile(r"^[A-Za-z][A-Za-z0-9_']*$")


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


def load_enums():
    enums = {}
    for f in os.listdir(HEADERS):
        if not f.endswith('.h'):
            continue
        src = open(os.path.join(HEADERS, f), encoding='utf-8-sig').read()
        for m in re.finditer(r'enum class (\w+)[^{]*\{(.*?)\};', src, re.S):
            body = re.sub(r'//.*', '', m.group(2))
            enums[m.group(1)] = set(re.findall(r'^\s*(\w+)', body, re.M))
    return enums


def read_sheet(src, sheet, key='RowName'):
    ws = openpyxl.load_workbook(os.path.join(HERE, src), data_only=True)[sheet]
    rows = list(ws.iter_rows(values_only=True))
    # 몬스터 시트처럼 위에 제목 줄이 붙어 있을 수 있어서 RowName(또는 key) 으로 시작하는 줄을 헤더로 찾음
    hi = next(i for i, r in enumerate(rows) if r and r[0] == key)
    hdr = [cell(c) for c in rows[hi]]
    hdr[0] = 'RowName'   # 언리얼 CSV 는 첫 열을 행 이름으로 씀
    while hdr and hdr[-1] == '':
        hdr.pop()
    data = [[cell(r[i]) if i < len(r) else '' for i in range(len(hdr))] for r in rows[hi + 1:]]
    return hdr, data


def convert(t, enums, skill_names):
    hdr, data = read_sheet(t['src'], t['sheet'], t.get('key', 'RowName'))
    col = {c: i for i, c in enumerate(hdr)}
    int_default = t.get('int_default', {})

    out, dropped, renamed, errors = [], 0, 0, []
    for vals in data:
        if not ROW_NAME.match(vals[0]):
            if vals[0]:
                dropped += 1    # 메모 줄
            continue
        if not t.get('keep_names'):
            fixed = norm_row_name(vals[0])
            if fixed != vals[0]:
                renamed += 1
            vals[0] = fixed
        for c in t.get('skill_refs', []):
            v = vals[col[c]]
            if v in skill_names:
                vals[col[c]] = skill_names[v]
        for c in t['int_cols']:
            if vals[col[c]] == '':
                vals[col[c]] = int_default.get(c, '0')
        for c in t.get('float_cols', []):
            if vals[col[c]] == '':
                vals[col[c]] = '0'
        for c in t.get('none_cols', []):
            if vals[col[c]] == '':
                vals[col[c]] = 'None'
        for c, e in t.get('enums', {}).items():
            v = vals[col[c]]
            if v not in enums.get(e, set()):
                errors.append('%s %s=%s (%s 에 없음)' % (vals[0], c, v, e))
        out.append(vals)

    if errors:
        sys.exit('%s: 열거형 값 %d개가 C++ 에 없음\n  ' % (t['out'], len(errors)) + '\n  '.join(errors[:30]))

    names = [v[0] for v in out]
    dup = sorted({x for x in names if names.count(x) > 1})
    if dup:
        sys.exit('%s: 행 이름 충돌 %s' % (t['out'], dup))

    # 열 이름 바꾸기 / 빼기 / 더하기
    keep = [i for i, c in enumerate(hdr) if c not in t.get('drop', [])]
    out_hdr = [t.get('rename', {}).get(hdr[i], hdr[i]) for i in keep] + [a for a, _ in t.get('add', [])]
    out_rows = [[v[i] for i in keep] + [d for _, d in t.get('add', [])] for v in out]

    buf = io.StringIO()
    w = csv.writer(buf, lineterminator='\n')
    w.writerow(out_hdr)
    w.writerows(out_rows)

    # BOM 은 언리얼은 건너뛰고, 엑셀은 이게 있어야 CSV 열었을 때 한글이 안 깨짐
    with open(os.path.join(HERE, t['out']), 'wb') as f:
        f.write(b'\xef\xbb\xbf' + buf.getvalue().encode('utf-8'))

    print('%-16s %3d행  (메모 줄 %d개 버림, 행 이름 정리 %d개)' % (t['out'], len(out), dropped, renamed))


if __name__ == '__main__':
    enums = load_enums()

    # 스킬 원래 행 이름 -> 정리된 행 이름. 몬스터 / 유물이 원래 이름(general's_sword)으로 가리키니까 같이 바꿔 줌
    hdr, data = read_sheet('skill_data.xlsx', 'Skill_DataTable')
    skill_names = {v[0]: norm_row_name(v[0]) for v in data if ROW_NAME.match(v[0])}

    for t in TABLES:
        convert(t, enums, skill_names)
