// 언리얼 UDataTable 대체. 첫 열 = 행 이름(키), 나머지 열 = 행 클래스의 public 필드 (이름이 같아야 함)
// 언리얼 CSV 임포트와 같은 규칙: 모르는 열은 버리고, 빈 칸 / 못 읽는 값은 필드 기본값 그대로 + 경고
using System;
using System.Collections.Generic;
using System.Globalization;
using System.Reflection;
using Terminus.Data.Csv;

namespace Terminus.Data
{
    public interface ITableRow
    {
        string RowName { get; set; }
    }

    /// <summary>
    /// 언리얼 FName 이던 필드. CSV 의 "None" 을 빈 문자열(= 없음)로 바꿔 읽는다.
    /// FString 이던 필드는 "None" 을 글자 그대로 둔다 (언리얼과 같음)
    /// </summary>
    [AttributeUsage(AttributeTargets.Field)]
    public sealed class NameFieldAttribute : Attribute { }

    public static class Ids
    {
        // 언리얼 NAME_None 에 해당
        public static bool IsNone(string id) => string.IsNullOrEmpty(id) || id == "None";
    }

    public sealed class DataTable<TRow> where TRow : class, ITableRow, new()
    {
        readonly Dictionary<string, TRow> _rows = new Dictionary<string, TRow>(StringComparer.Ordinal);
        readonly List<TRow> _ordered = new List<TRow>();

        public string Name { get; }
        public IReadOnlyList<TRow> Rows => _ordered;
        public IEnumerable<string> RowNames => _rows.Keys;
        public int Count => _ordered.Count;

        // 읽다가 나온 경고 (열 이름 틀림 / 값 못 읽음). 테스트에서 비어 있는지 확인
        public List<string> Warnings { get; } = new List<string>();

        public DataTable(string name)
        {
            Name = name;
        }

        public TRow Find(string rowName)
        {
            if (Ids.IsNone(rowName)) return null;
            return _rows.TryGetValue(rowName, out var row) ? row : null;
        }

        public bool Contains(string rowName) => !Ids.IsNone(rowName) && _rows.ContainsKey(rowName);

        public void Add(TRow row)
        {
            if (_rows.ContainsKey(row.RowName))
            {
                Warnings.Add($"{Name}: 행 이름 중복 '{row.RowName}' (뒤의 것을 버림)");
                return;
            }
            _rows.Add(row.RowName, row);
            _ordered.Add(row);
        }

        public static DataTable<TRow> FromCsv(string name, string csvText)
        {
            var table = new DataTable<TRow>(name);
            var lines = CsvReader.Parse(csvText);
            if (lines.Count == 0) return table;

            var header = lines[0];
            var binders = new FieldBinder[header.Count];
            for (int c = 1; c < header.Count; ++c)
            {
                var path = header[c].Trim();
                if (path.Length == 0) continue;
                binders[c] = FieldBinder.Create(typeof(TRow), path);
                if (binders[c] == null)
                {
                    table.Warnings.Add($"{name}: 열 '{path}' 에 맞는 필드가 없음 (무시)");
                }
            }

            for (int r = 1; r < lines.Count; ++r)
            {
                var line = lines[r];
                if (line.Count == 0 || string.IsNullOrWhiteSpace(line[0])) continue;

                var row = new TRow { RowName = line[0].Trim() };
                for (int c = 1; c < header.Count && c < line.Count; ++c)
                {
                    var binder = binders[c];
                    if (binder == null) continue;
                    if (!binder.TrySet(row, line[c], out var error))
                    {
                        table.Warnings.Add($"{name}[{row.RowName}].{header[c]}: {error}");
                    }
                }
                table.Add(row);
            }
            return table;
        }

        // 열 하나를 필드(또는 "BaseStats.MaxHealth" 같은 중첩 필드)에 넣는 것
        sealed class FieldBinder
        {
            FieldInfo[] _path;

            public static FieldBinder Create(Type type, string columnPath)
            {
                var parts = columnPath.Split('.');
                var path = new FieldInfo[parts.Length];
                var current = type;
                for (int i = 0; i < parts.Length; ++i)
                {
                    var f = current.GetField(parts[i], BindingFlags.Public | BindingFlags.Instance);
                    if (f == null) return null;
                    path[i] = f;
                    current = f.FieldType;
                }
                return new FieldBinder { _path = path };
            }

            public bool TrySet(object row, string raw, out string error)
            {
                object target = row;
                for (int i = 0; i < _path.Length - 1; ++i)
                {
                    var next = _path[i].GetValue(target);
                    if (next == null)
                    {
                        next = Activator.CreateInstance(_path[i].FieldType);
                        _path[i].SetValue(target, next);
                    }
                    target = next;
                }
                return ValueParser.TrySetField(_path[_path.Length - 1], target, raw, out error);
            }
        }
    }

    public static class ValueParser
    {
        public static bool TrySetField(FieldInfo field, object target, string raw, out string error)
        {
            error = null;
            raw = raw ?? "";
            var t = field.FieldType;

            if (t == typeof(string))
            {
                var s = raw;
                if (field.IsDefined(typeof(NameFieldAttribute), false) && Ids.IsNone(s.Trim())) s = "";
                field.SetValue(target, field.IsDefined(typeof(NameFieldAttribute), false) ? s.Trim() : s);
                return true;
            }

            var trimmed = raw.Trim();
            if (trimmed.Length == 0) return true; // 빈 칸 -> 기본값

            if (t == typeof(int))
            {
                if (int.TryParse(trimmed, NumberStyles.Integer, CultureInfo.InvariantCulture, out var v)) { field.SetValue(target, v); return true; }
                // 엑셀이 "3.0" 으로 내보낸 정수
                if (double.TryParse(trimmed, NumberStyles.Float, CultureInfo.InvariantCulture, out var d) && Math.Abs(d - Math.Round(d)) < 1e-9) { field.SetValue(target, (int)Math.Round(d)); return true; }
                error = $"정수가 아님 '{raw}'";
                return false;
            }
            if (t == typeof(float))
            {
                if (float.TryParse(trimmed, NumberStyles.Float, CultureInfo.InvariantCulture, out var v)) { field.SetValue(target, v); return true; }
                error = $"실수가 아님 '{raw}'";
                return false;
            }
            if (t == typeof(bool))
            {
                if (bool.TryParse(trimmed, out var v)) { field.SetValue(target, v); return true; }
                if (trimmed == "1") { field.SetValue(target, true); return true; }
                if (trimmed == "0") { field.SetValue(target, false); return true; }
                error = $"참/거짓이 아님 '{raw}'";
                return false;
            }
            if (t.IsEnum)
            {
                // 언리얼 내보내기는 "ESkillOwner::Fighter" 처럼 올 때도 있음
                var name = trimmed;
                int sep = name.LastIndexOf("::", StringComparison.Ordinal);
                if (sep >= 0) name = name.Substring(sep + 2);
                if (Enum.IsDefined(t, name)) { field.SetValue(target, Enum.Parse(t, name)); return true; }
                error = $"{t.Name} 에 '{raw}' 값이 없음";
                return false;
            }
            if (t.IsClass)
            {
                // 언리얼 구조체 리터럴 "(MaxHealth=70,Attack=0)"
                if (trimmed.StartsWith("(") && trimmed.EndsWith(")"))
                {
                    var obj = field.GetValue(target) ?? Activator.CreateInstance(t);
                    foreach (var pair in trimmed.Substring(1, trimmed.Length - 2).Split(','))
                    {
                        int eq = pair.IndexOf('=');
                        if (eq <= 0) continue;
                        var sub = t.GetField(pair.Substring(0, eq).Trim(), BindingFlags.Public | BindingFlags.Instance);
                        if (sub == null) continue;
                        if (!TrySetField(sub, obj, pair.Substring(eq + 1).Trim().Trim('"'), out error)) return false;
                    }
                    field.SetValue(target, obj);
                    return true;
                }
            }

            error = $"읽을 수 없는 타입 {t.Name}";
            return false;
        }
    }
}
