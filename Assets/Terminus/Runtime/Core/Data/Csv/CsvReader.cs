// RFC 4180 CSV 파서. 따옴표 안 쉼표 / 줄바꿈 / "" 이스케이프, UTF-8 BOM 처리
// 기획 엑셀 -> xlsx_to_csv.py -> DT_*.csv 결과물을 그대로 읽는다
using System.Collections.Generic;
using System.Text;

namespace Terminus.Data.Csv
{
    public static class CsvReader
    {
        public static List<List<string>> Parse(string text)
        {
            var rows = new List<List<string>>();
            if (string.IsNullOrEmpty(text)) return rows;

            int i = 0;
            if (text[0] == '﻿') i = 1;

            var row = new List<string>();
            var cell = new StringBuilder();
            bool inQuotes = false;
            bool cellStarted = false;

            for (; i < text.Length; ++i)
            {
                char c = text[i];
                if (inQuotes)
                {
                    if (c == '"')
                    {
                        if (i + 1 < text.Length && text[i + 1] == '"')
                        {
                            cell.Append('"');
                            ++i;
                        }
                        else
                        {
                            inQuotes = false;
                        }
                    }
                    else
                    {
                        cell.Append(c);
                    }
                    continue;
                }

                switch (c)
                {
                    case '"':
                        inQuotes = true;
                        cellStarted = true;
                        break;
                    case ',':
                        row.Add(cell.ToString());
                        cell.Clear();
                        cellStarted = true;
                        break;
                    case '\r':
                        break;
                    case '\n':
                        if (cellStarted || cell.Length > 0 || row.Count > 0)
                        {
                            row.Add(cell.ToString());
                            rows.Add(row);
                        }
                        row = new List<string>();
                        cell.Clear();
                        cellStarted = false;
                        break;
                    default:
                        cell.Append(c);
                        cellStarted = true;
                        break;
                }
            }

            if (cellStarted || cell.Length > 0 || row.Count > 0)
            {
                row.Add(cell.ToString());
                rows.Add(row);
            }
            return rows;
        }
    }
}
