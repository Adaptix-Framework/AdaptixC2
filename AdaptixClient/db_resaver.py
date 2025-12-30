import sqlite3
import json
import os
import sys

OLD_DB_PATH = os.path.expanduser('~/.adaptix/storage.db')
NEW_DB_PATH = os.path.expanduser('~/.adaptix/storage-v1.db')

def create_new_schema(cursor):
    cursor.execute("""
        CREATE TABLE IF NOT EXISTS Projects (
            project TEXT UNIQUE PRIMARY KEY,
            data TEXT
        );
    """)
    cursor.execute("""
        CREATE TABLE IF NOT EXISTS Extensions (
            filepath TEXT UNIQUE PRIMARY KEY,
            enabled BOOLEAN
        );
    """)
    cursor.execute("""
        CREATE TABLE IF NOT EXISTS Settings (
            key TEXT UNIQUE PRIMARY KEY,
            data TEXT
        );
    """)

def migrate_projects(old_cur, new_cur):
    print("Migrating Projects...")
    try:
        old_cur.execute("SELECT * FROM Projects")
        rows = old_cur.fetchall()
        for row in rows:
            data = {
                "host": row['host'],
                "port": str(row['port']),
                "endpoint": row['endpoint'],
                "username": row['username'],
                "password": row['password']
            }
            new_cur.execute(
                "INSERT INTO Projects (project, data) VALUES (?, ?)",
                (row['project'], json.dumps(data, separators=(',', ':')))
            )
        print(f"  - Migrated {len(rows)} projects.")
    except sqlite3.OperationalError:
        print("  - Table 'Projects' not found in old DB.")

def migrate_extensions(old_cur, new_cur):
    print("Migrating Extensions...")
    try:
        old_cur.execute("SELECT * FROM Extensions")
        rows = old_cur.fetchall()
        for row in rows:
            new_cur.execute(
                "INSERT INTO Extensions (filepath, enabled) VALUES (?, ?)",
                (row['filepath'], row['enabled'])
            )
        print(f"  - Migrated {len(rows)} extensions.")
    except sqlite3.OperationalError:
        print("  - Table 'Extensions' not found in old DB.")

def migrate_settings_main(old_cur, new_cur):
    print("Migrating SettingsMain...")
    try:
        old_cur.execute("SELECT * FROM SettingsMain WHERE id=1")
        row = old_cur.fetchone()
        if row:
            data = {
                "theme": row['theme'],
                "fontFamily": row['fontFamily'],
                "fontSize": row['fontSize'],
                "consoleTime": bool(row['consoleTime'])
            }
            new_cur.execute(
                "INSERT INTO Settings (key, data) VALUES (?, ?)",
                ("SettingsMain", json.dumps(data, separators=(',', ':')))
            )
            print("  - SettingsMain migrated.")
    except sqlite3.OperationalError:
        print("  - Table 'SettingsMain' not found.")

def migrate_settings_console(old_cur, new_cur):
    print("Migrating SettingsConsole...")
    try:
        old_cur.execute("SELECT * FROM SettingsConsole WHERE id=1")
        row = old_cur.fetchone()
        if row:
            data = {
                "terminalBuffer": row['terminalBuffer'],
                "consoleBuffer": row['consoleBuffer'],
                "noWrap": bool(row['noWrap']),
                "autoScroll": bool(row['autoScroll'])
            }
            new_cur.execute(
                "INSERT INTO Settings (key, data) VALUES (?, ?)",
                ("SettingsConsole", json.dumps(data, separators=(',', ':')))
            )
            print("  - SettingsConsole migrated.")
    except sqlite3.OperationalError:
        print("  - Table 'SettingsConsole' not found.")

def migrate_settings_sessions(old_cur, new_cur):
    print("Migrating SettingsSessions...")
    try:
        old_cur.execute("SELECT * FROM SettingsSessions WHERE id=1")
        row = old_cur.fetchone()
        if row:
            columns = []
            for i in range(15):
                col_name = f"column{i}"
                if col_name in row.keys():
                    columns.append(bool(row[col_name]))
                else:
                    columns.append(False)

            data = {
                "healthCheck": bool(row['healthCheck']),
                "healthCoaf": row['healthCoaf'],
                "healthOffset": row['healthOffset'],
                "columns": columns
            }
            new_cur.execute(
                "INSERT INTO Settings (key, data) VALUES (?, ?)",
                ("SettingsSessions", json.dumps(data, separators=(',', ':')))
            )
            print("  - SettingsSessions migrated.")
    except sqlite3.OperationalError:
        print("  - Table 'SettingsSessions' not found.")

def migrate_settings_graph(old_cur, new_cur):
    print("Migrating SettingsGraph...")
    try:
        old_cur.execute("SELECT * FROM SettingsGraph WHERE id=1")
        row = old_cur.fetchone()
        if row:
            data = {
                "version": row['version']
            }
            new_cur.execute(
                "INSERT INTO Settings (key, data) VALUES (?, ?)",
                ("SettingsGraph", json.dumps(data, separators=(',', ':')))
            )
            print("  - SettingsGraph migrated.")
    except sqlite3.OperationalError:
        print("  - Table 'SettingsGraph' not found.")

def migrate_settings_tasks(old_cur, new_cur):
    print("Migrating SettingsTasks...")
    try:
        old_cur.execute("SELECT * FROM SettingsTasks WHERE id=1")
        row = old_cur.fetchone()
        if row:
            columns = []
            for i in range(11):
                col_name = f"column{i}"
                if col_name in row.keys():
                    columns.append(bool(row[col_name]))
                else:
                    columns.append(False)
            
            data = {
                "columns": columns
            }
            new_cur.execute(
                "INSERT INTO Settings (key, data) VALUES (?, ?)",
                ("SettingsTasks", json.dumps(data, separators=(',', ':')))
            )
            print("  - SettingsTasks migrated.")
    except sqlite3.OperationalError:
        print("  - Table 'SettingsTasks' not found.")

def main():
    if not os.path.exists(OLD_DB_PATH):
        print(f"Error: {OLD_DB_PATH} not found.")
        return

    if os.path.exists(NEW_DB_PATH):
        print(f"Warning: {NEW_DB_PATH} already exists. Deleting it to start fresh...")
        os.remove(NEW_DB_PATH)

    print(f"Opening {OLD_DB_PATH}...")
    conn_old = sqlite3.connect(OLD_DB_PATH)
    conn_old.row_factory = sqlite3.Row  # To access columns by name
    cur_old = conn_old.cursor()

    print(f"Creating {NEW_DB_PATH}...")
    conn_new = sqlite3.connect(NEW_DB_PATH)
    cur_new = conn_new.cursor()

    create_new_schema(cur_new)

    # Begin transaction
    conn_new.execute("BEGIN TRANSACTION")

    try:
        migrate_projects(cur_old, cur_new)
        migrate_extensions(cur_old, cur_new)
        migrate_settings_main(cur_old, cur_new)
        migrate_settings_console(cur_old, cur_new)
        migrate_settings_sessions(cur_old, cur_new)
        migrate_settings_graph(cur_old, cur_new)
        migrate_settings_tasks(cur_old, cur_new)

        conn_new.commit()
        print("\nMigration successfully finished!")
        print(f"New database saved to: {NEW_DB_PATH}")

    except Exception as e:
        conn_new.rollback()
        print(f"\nCRITICAL ERROR during migration: {e}")
        print("Rolled back changes.")
    finally:
        conn_old.close()
        conn_new.close()

if __name__ == "__main__":
    main()
