using System;
using System.Runtime.InteropServices;
using System.Text;

internal static class MycoDbExample {
    [StructLayout(LayoutKind.Sequential)]
    private struct ms_db_payload_t {
        public int id;
        public int table_id;
        public int x;
        public int y;
        public int field_count;
        public int fk_count;
    }

    private const string DllName = "micro_swarm";

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr ms_db_create();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    private static extern void ms_db_destroy(IntPtr h);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern IntPtr ms_db_get_last_error(IntPtr h);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern int ms_db_load_myco(IntPtr h, string path);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern int ms_db_query_by_id(IntPtr h, string table, int id, int radius);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern int ms_db_query_simple(IntPtr h, string table, string column, string value, int radius);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern int ms_db_query_simple_focus(IntPtr h, string table, string column, string value, int centerX, int centerY, int radius);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern int ms_db_query_by_id_focus(IntPtr h, string table, int id, int centerX, int centerY, int radius);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    private static extern int ms_db_get_result_count(IntPtr h);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    private static extern int ms_db_get_result_indices(IntPtr h, [Out] int[] outIndices, int maxOut);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    private static extern int ms_db_find_payload_by_id(IntPtr h, int payloadId);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    private static extern int ms_db_get_table_count(IntPtr h);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    private static extern int ms_db_get_payload(IntPtr h, int payloadIndex, out ms_db_payload_t payload);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    private static extern int ms_db_get_payload_raw(IntPtr h, int payloadIndex, [Out] byte[] dst, int dstSize);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    private static extern int ms_db_get_table_name(IntPtr h, int tableId, [Out] byte[] dst, int dstSize);

    private static string PtrToStringAnsi(IntPtr ptr) {
        return ptr == IntPtr.Zero ? "" : Marshal.PtrToStringAnsi(ptr) ?? "";
    }

    private static string GetAscii(byte[] buffer) {
        int len = Array.IndexOf(buffer, (byte)0);
        if (len < 0) len = buffer.Length;
        return Encoding.ASCII.GetString(buffer, 0, len);
    }

    public static int Main(string[] args) {
        string mycoPath = args.Length > 0 ? args[0] : "shop.myco";
        int radius = 5;
        bool focusSet = false;
        int focusX = 0;
        int focusY = 0;

        IntPtr db = ms_db_create();
        if (db == IntPtr.Zero) {
            Console.WriteLine("ms_db_create returned NULL");
            return 1;
        }

        if (ms_db_load_myco(db, mycoPath) == 0) {
            Console.WriteLine("load failed: " + PtrToStringAnsi(ms_db_get_last_error(db)));
            ms_db_destroy(db);
            return 2;
        }

        Console.WriteLine("myco shell bereit. 'help' fuer Befehle, 'exit' zum Beenden.");
        while (true) {
            Console.Write("myco> ");
            string? line = Console.ReadLine();
            if (line == null) break;
            line = line.Trim();
            if (line.Length == 0) continue;
            if (line.Equals("exit", StringComparison.OrdinalIgnoreCase) ||
                line.Equals("quit", StringComparison.OrdinalIgnoreCase)) {
                break;
            }
            if (line.Equals("help", StringComparison.OrdinalIgnoreCase)) {
                Console.WriteLine("Formate:");
                Console.WriteLine("  Album 1                -> Primary-Key Query");
                Console.WriteLine("  Track AlbumId=1         -> Foreign-Key Query");
                Console.WriteLine("  goto <payload_id>       -> Fokus setzen");
                Console.WriteLine("  radius <n>              -> Suchradius setzen");
                Console.WriteLine("  focus                   -> Aktuellen Fokus anzeigen");
                Console.WriteLine("  unfocus                 -> Fokus entfernen");
                Console.WriteLine("  tables                  -> Tabellenliste");
                Console.WriteLine("  stats                   -> Payload-Counts pro Tabelle");
                Console.WriteLine("  exit                    -> Beenden");
                continue;
            }
            if (line.Equals("focus", StringComparison.OrdinalIgnoreCase)) {
                if (focusSet) {
                    Console.WriteLine($"focus={focusX},{focusY} radius={radius}");
                } else {
                    Console.WriteLine($"focus=none radius={radius}");
                }
                continue;
            }
            if (line.Equals("unfocus", StringComparison.OrdinalIgnoreCase)) {
                focusSet = false;
                Console.WriteLine("focus=none");
                continue;
            }
            if (line.Equals("tables", StringComparison.OrdinalIgnoreCase)) {
                int tableCount = ms_db_get_table_count(db);
                for (int t = 0; t < tableCount; t++) {
                    string name = GetTableName(db, t);
                    Console.WriteLine(t + ": " + name);
                }
                continue;
            }
            if (line.Equals("stats", StringComparison.OrdinalIgnoreCase)) {
                int tableCount = ms_db_get_table_count(db);
                int payloadCount = ms_db_get_payload_count(db);
                int[] counts = new int[Math.Max(1, tableCount)];
                for (int i = 0; i < payloadCount; i++) {
                    if (ms_db_get_payload(db, i, out ms_db_payload_t payload) == 0) {
                        continue;
                    }
                    if (payload.table_id >= 0 && payload.table_id < counts.Length) {
                        counts[payload.table_id]++;
                    }
                }
                for (int t = 0; t < tableCount; t++) {
                    string name = GetTableName(db, t);
                    Console.WriteLine(t + ": " + name + " -> " + counts[t]);
                }
                continue;
            }
            if (line.StartsWith("radius ", StringComparison.OrdinalIgnoreCase)) {
                string rStr = line.Substring(7).Trim();
                if (int.TryParse(rStr, out int r) && r > 0) {
                    radius = r;
                    Console.WriteLine($"radius={radius}");
                } else {
                    Console.WriteLine("Ungueltiger Radius.");
                }
                continue;
            }
            if (line.StartsWith("goto ", StringComparison.OrdinalIgnoreCase)) {
                string idStr = line.Substring(5).Trim();
                if (!int.TryParse(idStr, out int payloadId)) {
                    Console.WriteLine("Ungueltige ID.");
                    continue;
                }
                int idx = ms_db_find_payload_by_id(db, payloadId);
                if (idx < 0) {
                    Console.WriteLine("ID nicht gefunden.");
                    continue;
                }
                if (ms_db_get_payload(db, idx, out ms_db_payload_t payload) == 0) {
                    Console.WriteLine("Payload nicht lesbar.");
                    continue;
                }
                focusX = payload.x;
                focusY = payload.y;
                focusSet = true;
                string table = GetTableName(db, payload.table_id);
                Console.WriteLine($"goto id={payload.id} table={table} x={payload.x} y={payload.y}");
                continue;
            }

            string tableName = "";
            string cond = "";
            int spaceIdx = line.IndexOf(' ');
            if (spaceIdx <= 0) {
                Console.WriteLine("Ungueltige Query.");
                continue;
            }
            tableName = line.Substring(0, spaceIdx).Trim();
            cond = line.Substring(spaceIdx + 1).Trim();
            if (tableName.Length == 0 || cond.Length == 0) {
                Console.WriteLine("Ungueltige Query.");
                continue;
            }
            int eqIdx = cond.IndexOf('=');
            int hits;
            if (eqIdx < 0) {
                if (!int.TryParse(cond, out int idValue)) {
                    Console.WriteLine("Ungueltige ID.");
                    continue;
                }
                hits = focusSet
                    ? ms_db_query_by_id_focus(db, tableName, idValue, focusX, focusY, radius)
                    : ms_db_query_by_id(db, tableName, idValue, radius);
            } else {
                string column = cond.Substring(0, eqIdx).Trim();
                string value = cond.Substring(eqIdx + 1).Trim();
                hits = focusSet
                    ? ms_db_query_simple_focus(db, tableName, column, value, focusX, focusY, radius)
                    : ms_db_query_simple(db, tableName, column, value, radius);
            }
            Console.WriteLine("hits=" + hits);
            PrintResults(db);
        }

        ms_db_destroy(db);
        return 0;
    }

    private static void PrintResults(IntPtr db) {
        int count = ms_db_get_result_count(db);
        if (count <= 0) {
            Console.WriteLine("no results");
            return;
        }
        int[] indices = new int[count];
        int got = ms_db_get_result_indices(db, indices, indices.Length);
        for (int i = 0; i < got; i++) {
            int idx = indices[i];
            if (ms_db_get_payload(db, idx, out ms_db_payload_t payload) == 0) {
                continue;
            }
            string table = GetTableName(db, payload.table_id);

            byte[] rawBuf = new byte[1024];
            ms_db_get_payload_raw(db, idx, rawBuf, rawBuf.Length);
            string raw = GetAscii(rawBuf);

            Console.WriteLine("id=" + payload.id + " table=" + table + " x=" + payload.x + " y=" + payload.y + " data=\"" + raw + "\"");
        }
    }

    private static string GetTableName(IntPtr db, int tableId) {
        byte[] tableBuf = new byte[64];
        ms_db_get_table_name(db, tableId, tableBuf, tableBuf.Length);
        return GetAscii(tableBuf);
    }
}
