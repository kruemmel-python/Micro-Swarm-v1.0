# MycoDB Write-Test (db_shell)
# Start:
# .\micro_swarm.exe --mode db_shell --db chinook_optimized.myco --db-radius 1500

# Status

delta

# INSERT

sql INSERT INTO playlist (PlaylistId, Name) VALUES (9999,'MycoWriteTest')
sql SELECT PlaylistId,Name FROM playlist WHERE PlaylistId=9999

# MERGE

delta
merge
sql SELECT PlaylistId,Name FROM playlist WHERE PlaylistId=9999

# UPDATE

sql UPDATE playlist SET Name='MycoWriteTest2' WHERE PlaylistId=9999
sql SELECT PlaylistId,Name FROM playlist WHERE PlaylistId=9999

# DELETE

delta
sql DELETE FROM playlist WHERE PlaylistId=9999
sql SELECT PlaylistId,Name FROM playlist WHERE PlaylistId=9999

delta
merge
sql SELECT PlaylistId,Name FROM playlist WHERE PlaylistId=9999
