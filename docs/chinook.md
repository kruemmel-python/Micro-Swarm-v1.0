# Chinook: MycoDB Referenz (Micro-Swarm)

Diese Datei ist die komplette Referenz fuer die Datenbank `chinook_optimized.myco` im interaktiven `db_shell`.
Alle Beispiele sind kompatibel mit dem aktuellen MycoDB-Parser und der Shell-Syntax.

Ausgangspunkt:

```powershell
.\micro_swarm.exe --mode db_shell --db chinook_optimized.myco --db-radius 15
```

## Tabellen und Schema (Chinook)

Tabellen:

```
album
Artist
employee
genre
invoice
Customer
invoiceline
Track
mediatype
playlist
playlisttrack
```

Schema:

```
album: AlbumId, Title, ArtistId
Artist: ArtistId, Name
employee: EmployeeId, LastName, FirstName, Title, ReportsTo, BirthDate, HireDate, Address, City, State, Country, PostalCode, Phone, Fax, Email
genre: GenreId, Name
invoice: InvoiceId, CustomerId, InvoiceDate, BillingAddress, BillingCity, BillingState, BillingCountry, BillingPostalCode, Total
Customer: (keine Spalten bekannt)
invoiceline: InvoiceLineId, InvoiceId, TrackId, UnitPrice, Quantity
Track: TrackId, Name, AlbumId, MediaTypeId, GenreId, Composer, Milliseconds, Bytes, UnitPrice
mediatype: MediaTypeId, Name
playlist: PlaylistId, Name
playlisttrack: PlaylistId, TrackId
```

Hinweise:
- Tabellen-/Spaltennamen sind case-insensitive.
- Wenn eine Tabelle keine Spaltenliste hat (z. B. `Customer`), nutze `col0`, `col1`, ... oder re-ingest mit `CREATE TABLE`.

## Shell-Kommandos (vollstaendig)

```
help
tables
stats
delta
merge
schema <table>
goto <payload_id>
focus
radius <N>
unfocus
format <table|csv|json>
sort <col|index> [asc|desc] [num][, <col|index> [asc|desc] [num] ...]
sort reset
exit

<Table> <PKValue>
<Table> <col>=<val>
<col>=<val>
<Table> ... show <col1,col2,...>
sql <statement>
```

Kurzbeschreibung:
- `help`: zeigt alle Befehle
- `tables`: Tabellenliste
- `stats`: Payload-Counts pro Tabelle
- `delta`: zeigt Delta-Pending (Writes + Tombstones)
- `merge`: Delta in Cluster re-clustern (Batch)
- `schema <table>`: Spaltenliste
- `goto <payload_id>`: Fokus setzen (Payload-ID)
- `focus`: Fokus anzeigen
- `radius <N>`: Suchradius setzen
- `unfocus`: Fokus entfernen
- `format <table|csv|json>`: Ausgabeformat fuer SQL
- `sort ...`: sortiert letztes SQL- oder Shell-Result
- `sort reset`: stellt letztes Result wieder her
- `exit`: Shell beenden

## Shell-Beispiele (kompakt, komplett)

Primary-Key (impliziter PK):

```
Album 1
Artist 5
Track 12
Genre 3
MediaType 2
Employee 1
Invoice 10
InvoiceLine 20
Playlist 4
PlaylistTrack 15
```

Foreign-Key (Relationen):

```
Album ArtistId=1
Track AlbumId=1
Track GenreId=1
Track MediaTypeId=1
InvoiceLine InvoiceId=1
Invoice CustomerId=1
PlaylistTrack PlaylistId=1
```

Global (ohne Tabellenname):

```
TrackId=13
AlbumId=1
```

Fokus + Radius:

```
Album 1
goto 1
radius 8
Track AlbumId=1
focus
unfocus
```

Spalten filtern (show):

```
Track AlbumId=1 show TrackId,Name,Milliseconds
```

Sort (Shell-Result):

```
Track AlbumId=1
sort Milliseconds num
sort TrackId desc num, Name asc
sort reset
```

## SQL-Light Referenz (vollstaendig implementiert)

Parser-Garantie:
Alle in dieser Referenz aufgefuehrten SQL-Konstrukte werden vom aktuellen
MycoDB-Parser vollstaendig unterstuetzt und sind Teil der stabilen API.

Write-Hinweis: INSERT/UPDATE/DELETE schreiben in den Delta-Store (Merge on read). `merge` fuehrt die Batch-Re-Clusterung aus.

Nicht unterstuetzt (bewusst): DDL (CREATE/ALTER/DROP), Transaktionen, Stored Procedures.

```
sql SELECT TrackId,Name FROM Track WHERE AlbumId=1
sql SELECT TrackId,Name FROM Track WHERE TrackId=1
sql SELECT TrackId,Name FROM Track WHERE AlbumId=1 AND GenreId=1
sql SELECT TrackId,Name FROM Track WHERE AlbumId=1 OR AlbumId=2
sql SELECT TrackId,Name FROM Track WHERE NOT GenreId=1
sql SELECT Name FROM Artist WHERE ArtistId IN (1,2,3,4)
sql SELECT TrackId,Name FROM Track WHERE Milliseconds BETWEEN 200000 AND 300000
sql SELECT Name FROM Artist WHERE Name LIKE 'A%'
sql SELECT Name FROM Artist WHERE Name REGEXP '^A'
sql SELECT Name FROM Artist WHERE Name IS NOT NULL LIMIT 5
sql SELECT TrackId,Name FROM Track WHERE AlbumId=1 ORDER BY TrackId
sql SELECT TrackId,Name FROM Track WHERE AlbumId=1 ORDER BY TrackId DESC LIMIT 5
sql SELECT TrackId,Name FROM Track ORDER BY TrackId LIMIT 5 OFFSET 5
sql SELECT TrackId,Name FROM Track ORDER BY 1 LIMIT 5
sql SELECT DISTINCT GenreId FROM Track ORDER BY GenreId LIMIT 10
sql SELECT AlbumId, COUNT(*) AS C FROM Track GROUP BY AlbumId HAVING C > 5 ORDER BY C DESC LIMIT 10
sql SELECT GenreId, AVG(Milliseconds) AS AvgMs FROM Track GROUP BY GenreId ORDER BY AvgMs DESC LIMIT 5
sql SELECT AlbumId, SUM(Milliseconds) AS SumMs FROM Track GROUP BY AlbumId ORDER BY SumMs DESC LIMIT 5
sql SELECT AlbumId, MIN(Milliseconds) AS MinMs, MAX(Milliseconds) AS MaxMs FROM Track GROUP BY AlbumId LIMIT 5
sql SELECT t.Name, a.Title FROM Track t JOIN Album a ON t.AlbumId=a.AlbumId WHERE t.TrackId=13
sql SELECT t.Name, a.Title FROM Track t LEFT JOIN Album a ON t.AlbumId=a.AlbumId WHERE a.AlbumId=1
sql SELECT t.Name, a.Title FROM Track t RIGHT JOIN Album a ON t.AlbumId=a.AlbumId WHERE a.AlbumId=1
sql SELECT AlbumId, COUNT(*) AS C FROM Track GROUP BY AlbumId HAVING COUNT(*) > 5 ORDER BY C DESC LIMIT 5
sql SELECT LOWER(Name) AS n FROM Artist ORDER BY n LIMIT 5
sql SELECT UPPER(Name) AS n FROM Artist ORDER BY n LIMIT 5
sql SELECT LENGTH(Name) AS L FROM Artist ORDER BY L DESC LIMIT 5
sql SELECT SUBSTRING(Name,1,5) AS S FROM Artist ORDER BY S LIMIT 5
sql SELECT CONCAT(FirstName,' ',LastName) AS FullName FROM employee ORDER BY FullName LIMIT 5
sql SELECT Name FROM Artist a WHERE EXISTS (SELECT AlbumId FROM Album WHERE Album.ArtistId=a.ArtistId)
sql SELECT * FROM Track CROSS JOIN MediaType LIMIT 3
sql WITH top_albums AS (SELECT AlbumId FROM Track GROUP BY AlbumId HAVING COUNT(*) > 5) SELECT AlbumId FROM top_albums ORDER BY AlbumId
sql SELECT AlbumId, COUNT(*) AS C FROM Track GROUP BY AlbumId HAVING COUNT(*) > 5 ORDER BY C DESC LIMIT 5
sql SELECT * FROM (SELECT ArtistId, Name FROM Artist) a WHERE a.ArtistId=1
sql SELECT TrackId,Name,Milliseconds FROM Track WHERE AlbumId=1
```

Shell-Sort (letztes SQL-Result):

```
sort Milliseconds num
sort TrackId desc num, Name asc
sort reset
```

## SQL-Write Beispiele (Delta-Store)

```
sql INSERT INTO playlist (PlaylistId, Name) VALUES (9999,'MycoWriteTest')
sql SELECT PlaylistId,Name FROM playlist WHERE PlaylistId=9999
sql UPDATE playlist SET Name='MycoWriteTest2' WHERE PlaylistId=9999
sql SELECT PlaylistId,Name FROM playlist WHERE PlaylistId=9999
sql DELETE FROM playlist WHERE PlaylistId=9999
sql SELECT PlaylistId,Name FROM playlist WHERE PlaylistId=9999
delta
merge
```

--- 

Ende.
