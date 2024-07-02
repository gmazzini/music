1. search songs in the Google Drive Directory with entry point /Music for Directory structure with /Music/Artist/Album/Song with php music/tool_scandir.php All new songs will be inserted in the table song
2. import song in local cached directory from the Google Drive repository with php music/tool_import.php
3. normalize name where necessary with respact to the source with php music/tool_normalizename.php
4. opbtain a spotify token for library access just for 1 hour with php music/tool_tokenspotify.php
5. add ISRC by searching on spotify database with php music/tool_addisrcfromspotify.php
