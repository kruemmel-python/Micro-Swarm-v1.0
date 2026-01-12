# MycoDB SQL Parser Stress-Test

Ziel: Harte, aber gueltige Queries fuer Regressionstests des SQL-Parsers.
Jede Zeile ist ein vollstaendiges SQL-Statement (im `db_shell` mit `sql ` praefixen).

```
SELECT TrackId,Name FROM Track WHERE AlbumId=1
SELECT TrackId,Name FROM Track WHERE TrackId=1
SELECT TrackId,Name FROM Track WHERE AlbumId=1 AND GenreId=1
SELECT TrackId,Name FROM Track WHERE AlbumId=1 OR AlbumId=2
SELECT TrackId,Name FROM Track WHERE NOT GenreId=1
SELECT Name FROM Artist WHERE ArtistId IN (1,2,3,4)
SELECT TrackId,Name FROM Track WHERE Milliseconds BETWEEN 200000 AND 300000
SELECT Name FROM Artist WHERE Name LIKE 'A%'
SELECT Name FROM Artist WHERE Name REGEXP '^A'
SELECT Name FROM Artist WHERE Name IS NOT NULL LIMIT 5
SELECT TrackId,Name FROM Track WHERE AlbumId=1 ORDER BY TrackId
SELECT TrackId,Name FROM Track WHERE AlbumId=1 ORDER BY TrackId DESC LIMIT 5
SELECT TrackId,Name FROM Track ORDER BY TrackId LIMIT 5 OFFSET 5
SELECT TrackId,Name FROM Track ORDER BY 1 LIMIT 5
SELECT DISTINCT GenreId FROM Track ORDER BY GenreId LIMIT 10
SELECT AlbumId, COUNT(*) AS C FROM Track GROUP BY AlbumId HAVING C > 5 ORDER BY C DESC LIMIT 10
SELECT GenreId, AVG(Milliseconds) AS AvgMs FROM Track GROUP BY GenreId ORDER BY AvgMs DESC LIMIT 5
SELECT AlbumId, SUM(Milliseconds) AS SumMs FROM Track GROUP BY AlbumId ORDER BY SumMs DESC LIMIT 5
SELECT AlbumId, MIN(Milliseconds) AS MinMs, MAX(Milliseconds) AS MaxMs FROM Track GROUP BY AlbumId LIMIT 5
SELECT t.Name, a.Title FROM Track t JOIN Album a ON t.AlbumId=a.AlbumId WHERE t.TrackId=13
SELECT t.Name, a.Title FROM Track t LEFT JOIN Album a ON t.AlbumId=a.AlbumId WHERE a.AlbumId=1
SELECT t.Name, a.Title FROM Track t RIGHT JOIN Album a ON t.AlbumId=a.AlbumId WHERE a.AlbumId=1
SELECT AlbumId, COUNT(*) AS C FROM Track GROUP BY AlbumId HAVING COUNT(*) > 5 ORDER BY C DESC LIMIT 5
SELECT LOWER(Name) AS n FROM Artist ORDER BY n LIMIT 5
SELECT UPPER(Name) AS n FROM Artist ORDER BY n LIMIT 5
SELECT LENGTH(Name) AS L FROM Artist ORDER BY L DESC LIMIT 5
SELECT SUBSTRING(Name,1,5) AS S FROM Artist ORDER BY S LIMIT 5
SELECT CONCAT(FirstName,' ',LastName) AS FullName FROM employee ORDER BY FullName LIMIT 5
SELECT Name FROM Artist a WHERE EXISTS (SELECT AlbumId FROM Album WHERE Album.ArtistId=a.ArtistId)
SELECT * FROM Track CROSS JOIN MediaType LIMIT 3
WITH top_albums AS (SELECT AlbumId FROM Track GROUP BY AlbumId HAVING COUNT(*) > 5) SELECT AlbumId FROM top_albums ORDER BY AlbumId
SELECT AlbumId, COUNT(*) AS C FROM Track GROUP BY AlbumId HAVING COUNT(*) > 5 ORDER BY C DESC LIMIT 5
SELECT * FROM (SELECT ArtistId, Name FROM Artist) a WHERE a.ArtistId=1
SELECT TrackId,Name,Milliseconds FROM Track WHERE AlbumId=1
```

## Write-Pfad (Delta-Store)

```
INSERT INTO playlist (PlaylistId, Name) VALUES (9999,'MycoWriteTest')
SELECT PlaylistId,Name FROM playlist WHERE PlaylistId=9999
UPDATE playlist SET Name='MycoWriteTest2' WHERE PlaylistId=9999
SELECT PlaylistId,Name FROM playlist WHERE PlaylistId=9999
DELETE FROM playlist WHERE PlaylistId=9999
SELECT PlaylistId,Name FROM playlist WHERE PlaylistId=9999
```
