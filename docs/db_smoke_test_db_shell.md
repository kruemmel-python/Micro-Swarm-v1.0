# MycoDB Smoke Test (Shell)
# Start:
# .\micro_swarm.exe --mode db_shell --db chinook_optimized.myco --db-radius 1500

# Multi-statement + DISTINCT ON + NULLS LAST
sql SET LIMIT 5; SELECT DISTINCT ON (AlbumId) AlbumId, TrackId FROM Track ORDER BY AlbumId, TrackId NULLS LAST

# Functions
sql SELECT COALESCE(Composer,'(none)') AS C FROM Track ORDER BY C LIMIT 5
sql SELECT CAST(UnitPrice AS float) AS P FROM Track ORDER BY P DESC LIMIT 5
sql SELECT CASE WHEN UnitPrice >= 1 THEN 'high' ELSE 'low' END AS PriceBand FROM Track LIMIT 5

# Delta write path
sql INSERT INTO playlist (PlaylistId, Name) VALUES (9999,'MycoSmokeTest')
sql SELECT PlaylistId,Name FROM playlist WHERE PlaylistId=9999
sql UPDATE playlist SET Name='MycoSmokeTest2' WHERE PlaylistId=9999
sql SELECT PlaylistId,Name FROM playlist WHERE PlaylistId=9999
sql DELETE FROM playlist WHERE PlaylistId=9999
sql SELECT PlaylistId,Name FROM playlist WHERE PlaylistId=9999
merge
sql SELECT PlaylistId,Name FROM playlist WHERE PlaylistId=9999
