# Music 2.x

Music 2.x is a compact C music library and HTML5 player. Google Drive is the authoritative audio source, SQLite stores the local catalog, requested tracks are kept in a persistent server-side STORE, and selected tracks can also be saved OFFLINE on each browser or mobile device.

## Architecture

- SOURCE: Google Drive folder `Music`.
- CATALOG: local SQLite file `music.db`.
- STORE: `tmpdata/store`, containing server-side copies already requested from SOURCE.
- OFFLINE: per-user browser/device storage managed with Cache Storage and a Service Worker.
- PLAYER CACHE: normal browser buffering, separate from Music OFFLINE storage.

The expected Drive layout is:

```
Music/
  Artist/
    Album/
      Song.mp3
```

## Build

Required components are a C compiler, libcurl, SQLite 3 and OpenSSL libcrypto.

```
make
```

The same executable works as a command-line tool and as a CGI program.

## Configuration

Music reads the optional local `config` file from the executable directory. Relative paths are resolved from the real executable directory, so CGI symlinks work independently of the Apache working directory.

Typical settings are:

```
db=music.db
store=tmpdata/store
drive_token=/home/www/data/google_access_token
drive_folder=Music
acr_host=identify-...acrcloud.com
acr_access_key=...
acr_access_secret=...
```

`config` may contain private credentials and must remain local. `tmpdata` contains regenerable data and can be excluded from backups. `music.db` is persistent catalog, authentication and playlist data and should be backed up.

## Initial catalog

Create the SQLite catalog:

```
./music init
```

Schema creation and upgrades are transactional: the schema version is written only after all migration steps succeed, and a failed migration is rolled back as a unit.

Verify Google Drive access:

```
./music drive
```

Scan SOURCE into the catalog:

```
./music scan
```

The scan is read-only on Google Drive. It first collects a complete SOURCE snapshot without holding a SQLite write transaction, then applies that snapshot in one short transaction. It stores Drive file IDs and the Artist/Album hierarchy without downloading the audio files. Existing Drive entries are refreshed, new files are added, files no longer present in SOURCE are retained historically but marked unavailable, and a file that later reappears becomes available again. An unavailable SOURCE track is not served from STORE even if an old local copy still exists.

Google Drive MD5 checksums are saved in the catalog during scans and can be used to identify exact duplicate source files without downloading the audio.

## STORE

A track is copied from SOURCE only when requested. Once present, subsequent requests use STORE directly. `./music store classified` preloads the canonical playable representative of every valid ACRCloud ACRID that is not already present in STORE. It reads the required Drive references into memory and closes SQLite before starting network transfers, so long preload runs do not block catalog writers such as concurrent ACRCloud classification.

```
./music store ID
./music store classified
./music check
```

`store classified` preloads every currently playable classified catalog track that is missing from STORE. It uses the same canonical ACRID representative as the web catalog, so duplicate SOURCE copies of the same recognized recording are not downloaded unnecessarily. Existing STORE files are skipped. The command is idempotent and may be interrupted with Ctrl+C and rerun; completed files are kept, the current partial temporary download is removed, and the next run naturally resumes from the missing tracks. This makes it suitable after a server migration or after STORE has been cleared.

`./music check` is the operational health check. It verifies the expected schema, SQLite `quick_check` and foreign keys, reports physical/available tracks, logical ACRCloud recognition, NO MATCH and pending counts, current and historical SOURCE entries, STORE size and orphan files, user/session and per-user statistics-row counts, and the state of C0-C9. Structural integrity failures return a non-zero exit status; non-destructive conditions such as orphan STORE files are reported as warnings.

## ACRCloud identification

Music identifies tracks with ACRCloud Music from a 10-second audio clip. Configure an ACRCloud Audio & Video Recognition project attached to the ACRCloud Music bucket, then set `acr_host`, `acr_access_key` and `acr_access_secret` in the local `config`. Run `./music acr ID`; an existing valid `acr_result` is reused unless `--force` is specified. `./music acr random N` selects random available songs without a valid result, excluding entries already classified as final NO MATCH. Only ACRCloud responses with `status.code == 0` are stored in `acr_result`; request-limit and other error responses are never inserted there, and a random batch stops immediately when the account request limit is reached.

Recognition failures are tracked separately in `acr_failure`. A first ACRCloud `1001` (No result) remains eligible for a later normal random batch; the next attempt automatically uses a different 10-second point in the same audio when the track length allows it. A second `1001` marks the song as final NO MATCH, so ordinary `acr random` no longer spends requests on it. ACRCloud `2004` (Can't generate fingerprint) is treated as a technical failure: it is recorded and later attempts also rotate the clip position, but it is not made final NO MATCH. A successful recognition removes any previous failure state.

Final NO MATCH entries are deliberately recoverable because the ACRCloud catalog may improve over time. `./music acr retry N` explicitly retries up to N of them, using another clip position. A specific unmatched song can also always be retried directly with `./music acr ID`; `--force` remains reserved for re-identifying a song that already has a saved successful result.

Music stores ACRID, artist, title, album, release date, label, ISRC, UPC, confidence score and the complete original JSON response. Each successful identity also keeps a stable `first_seen` timestamp. The canonical representative of duplicate SOURCE files sharing one ACRID is selected by `first_seen` and song ID, so re-running `--force` with the same ACRID does not arbitrarily change the canonical song; if a forced recognition changes the ACRID, `first_seen` is restarted for that new identity. A valid ACRCloud result has precedence over the source classification for artist, title, album and ISRC; tracks without ACRCloud data keep their original SOURCE values. `./music acr sync` is fully offline: it reparses every saved raw JSON, refreshes normalized `acr_result` fields and rebuilds catalog artist/title/album/ISRC from ACRCloud where available and SOURCE otherwise. This makes future parser improvements reusable without any additional recognition requests.

## Authentication and privacy

The web interface is private and requires a local Music account. Create accounts from the command line; passwords are entered interactively and are never passed on the command line.

```
./music user add NAME
./music user list
./music user info NAME
./music user password NAME
./music user delete NAME
./music user disable NAME
./music user enable NAME
```

`./music user info NAME` shows internal aggregate account statistics: account creation and status, statistics start time, login count, last login, last activity, playback count, last playback, playlist and playlist-entry counts, active sessions and currently attributed shared-queue entries. Login and playback counters are collected only from the schema-8 upgrade onward; Music does not invent historical counts. For existing users, the upgrade preserves the most recent login timestamp already recorded by Music and the most recent still-known session activity. `./music user delete NAME` permanently removes that Music account, including its aggregate usage statistics. Its sessions and owned playlists are deleted by the database relationships; shared-queue tracks remain, while attribution references to the deleted user become empty. Deleting a nonexistent user returns an error rather than reporting a false success. `user disable` prevents login and immediately removes active sessions while preserving the account, playlists, aggregate statistics and shared-queue attribution; `user enable` re-enables login.

Passwords are stored only as PBKDF2-HMAC-SHA256 hashes with a random per-user salt and 200,000 iterations. Password-hash verification uses a constant-time comparison. Login creates a random session token; only its SHA-256 hash is stored in SQLite. Sessions expire after 30 days of inactivity. The browser cookie expiry is renewed on authenticated requests, while the server-side session `last_seen`, expiry and aggregate per-user last-activity timestamp are refreshed at most once per minute to reduce unnecessary SQLite writes. Per-user usage storage is deliberately aggregate: Music keeps login count, last login, last activity, playback-start count and last playback time, but no per-user history of which individual tracks were listened to. Private and shared-queue playback starts both contribute to the aggregate playback count. The `music_session` cookie is first-party, `Secure`, `HttpOnly` and `SameSite=Lax`. Authenticated POST operations are protected by a session-derived CSRF token. Authentication is required for the HTML interface, media streaming, activity endpoints and Service Worker delivery.

Before login, Music displays a mandatory privacy and cookie information page. `music_notice` records the accepted notice version for one year and is also `Secure`, `HttpOnly` and `SameSite=Lax`; the accepted notice version and timestamp are recorded on successful login. The current notice includes the aggregate per-account usage statistics described above; its version is incremented when that information changes, forcing the notice to be presented again. The application uses no advertising, analytics, profiling or third-party cookies and does not store client IP addresses or User-Agent values. Apache or other infrastructure logs remain outside the Music application and must be configured consistently with the deployment privacy policy.

The authentication cookie is technically necessary for the requested service; the acceptance screen records acknowledgement of the privacy information rather than treating a necessary authentication cookie as optional tracking consent. The page exposes the Apache `SERVER_ADMIN` contact when available. The deployed privacy text and controller/contact details must always reflect the actual operator and hosting environment.

The `playlist` table is user-owned (`user_id`), and browser-side OFFLINE storage, player mode and private queue are also namespaced by authenticated user. Shared queues store the user ID and insertion time for each newly queued track (`added_by`, `added_at`) and the user ID responsible for the latest queue-state change (`changed_by`). Deleting a user cascades to that user's sessions and playlists and clears those shared-queue user references without deleting the shared music entries.

## Playlists

Playlists belong to the authenticated user. They support creation, rename/description editing, ordered playback, add/remove, TOP/UP/DOWN/BOTTOM ordering and optional sharing with other Music users. A shared playlist can be copied into the current user's private playlists while preserving description, order and pending entries. Playlist entries refer to catalog songs, while display and duplicate handling are based on ACRCloud ACRID: multiple SOURCE copies recognized as the same recording appear only once.

A playlist may contain SOURCE tracks that have not yet been recognized by ACRCloud. Unlike the normal Music catalog, playlists show every selected entry: unrecognized tracks use their original SOURCE artist/album/title, are marked `PENDING ACR` and are not playable. When recognition succeeds, the same playlist position automatically becomes the verified ACRCloud track. If multiple selected SOURCE files resolve to the same ACRID, they collapse to one logical playlist entry at the earliest selected position.

## Player queue

The player supports two queue modes. `PRIVATE` is the per-user temporary queue stored in browser session storage. Selecting `C0` through `C9` attaches the player to one of ten global shared queues stored in SQLite on the Music server. Every authenticated user attached to the same shared queue sees the same ordered tracks and can add, remove, reorder, clear, play, pause, resume, skip or replace the queue. Each shared-queue entry records the Music user that first added it and the insertion timestamp; the interface shows the username next to the track. Re-adding an existing track does not change its original attribution. The queue state also records the user responsible for its latest change.

A shared queue is a synchronized playback timeline, not only a shared list. The server stores the current song, play/pause state, a base position in milliseconds, an authoritative server timestamp and a monotonically increasing revision. Clients measure their clock offset from the server and derive the target position from the same timeline. Commands are normally scheduled about 1.5 seconds in the future so all attached devices can receive the new revision before the common effective timestamp. During playback each browser checks the shared state every 500 ms and corrects drift by seeking when necessary or by a small temporary playback-rate adjustment. When a browser has to wait for new media metadata, it recalculates the authoritative target position at the instant the media becomes ready rather than using a stale pre-load position.

Before a shared PLAY/NEXT operation is scheduled, Music verifies that the target MP3 is in the global server-side STORE. This is normally an immediate STORE hit; if STORE was deleted or that file is missing, Music first restores the track from SOURCE and only then creates the common playback timestamp. STORE is global to the server and is never per-user.

Browser autoplay policies can require one user gesture when a device first joins a queue that is already playing. In that case pressing the player PLAY button joins the existing server timeline rather than changing it. Hardware and Bluetooth output latency remain device-dependent, but the HTML5 players are kept on the same server-defined song and playback instant.

`+ QUEUE` appends without interrupting playback. `QUEUE ALL` appends a page or album, while `PLAY ALL` replaces the selected queue and schedules its first track. The queue panel supports immediate play, UP/DOWN, individual removal and CLEAR. Internal Music navigation does not replace the player document, so playback continues while moving between Library, Search, Albums and Playlists.

## Web player

The CGI interface exposes only tracks with a valid ACRCloud recognition (`acr_result.acrid` present). SOURCE remains the complete authoritative archive, but a track becomes visible in Library, Artists, Albums and Search only after its audio has been recognized. Tracks sharing the same ACRID are one logical catalog track: all SOURCE copies are retained, while the web interface, counts and search show only the first ACRCloud-recognized representative. The visible catalog therefore grows automatically as ACRCloud processing progresses without exposing duplicate copies of the same recording. The track detail view shows the recognized artist, title and album together with available ACRCloud release date, label, ISRC, UPC, ACRID and confidence score. Selecting a track requests `?media=ID`; if it is absent from STORE, Music downloads it from SOURCE and then streams it from STORE.

The media endpoint supports HTTP byte ranges for seeking. Multiple browser instances can use the same server concurrently; downloads use per-process temporary files and SQLite writes use a busy timeout.

## More

The `More` page shows the authenticated account, Music version, logical ACRCloud track count, personal playlist count, SOURCE/STORE/OFFLINE statistics, session policy and a link to the privacy/cookie information.

## ONLINE and OFFLINE modes

The web interface has one persistent ONLINE/OFFLINE selector rather than a separate offline page.

In ONLINE mode the ACRCloud-recognized server catalog is available. A track can be saved explicitly with `SAVE OFFLINE`. An entire album can also be saved with `SAVE ALBUM OFFLINE`; Music does not provide an artist-wide offline action. Album rows always show the current device state as `OFFLINE n/total`, and saved album content can be removed directly. A partially saved album can either be completed or removed.

The complete MP3 and its basic metadata are stored on that device only. Music imposes no application-level OFFLINE size limit; the effective limit is the browser/device storage quota. The interface shows both track count and storage size for SOURCE, STORE and OFFLINE so usage remains visible.

In OFFLINE mode the same interface shows only tracks stored for the authenticated user on the current device. Metadata, mode state, media Cache Storage and Service Worker shell caches are namespaced by user. Existing pre-2.51 global OFFLINE data are migrated once to the currently authenticated user. Playback reads the local browser copy and does not require SOURCE or STORE access. The selected mode is remembered per user. If the application is reopened without network access and offline tracks exist, it switches to OFFLINE automatically.

A Service Worker caches the application shell and serves locally stored audio, including HTTP Range requests. Removing a track from OFFLINE deletes only the device copy; SOURCE, STORE and the server catalog are unchanged.
