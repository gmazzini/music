// Gianluca Mazzini @2026- Version 2.67

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <signal.h>
#include <curl/curl.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/crypto.h>

#define MUSIC_VERSION "2.67"
#define VALUE_SIZE 1024
#define SCHEMA_VERSION 8
#define POST_BODY_MAX 65536

#define SQLITE_OK 0
#define SQLITE_ROW 100
#define SQLITE_DONE 101
#define SQLITE_OPEN_READONLY 0x00000001
#define SQLITE_OPEN_READWRITE 0x00000002
#define SQLITE_OPEN_CREATE 0x00000004

typedef struct sqlite3 sqlite3;
typedef struct sqlite3_stmt sqlite3_stmt;

extern int sqlite3_open_v2(const char *,sqlite3 **,int,const char *);
extern int sqlite3_close(sqlite3 *);
extern int sqlite3_busy_timeout(sqlite3 *,int);
extern int sqlite3_exec(sqlite3 *,const char *,int (*)(void *,int,char **,char **),void *,char **);
extern void sqlite3_free(void *);
extern const char *sqlite3_errmsg(sqlite3 *);
extern int sqlite3_prepare_v2(sqlite3 *,const char *,int,sqlite3_stmt **,const char **);
extern int sqlite3_step(sqlite3_stmt *);
extern int sqlite3_finalize(sqlite3_stmt *);
extern int sqlite3_reset(sqlite3_stmt *);
extern const unsigned char *sqlite3_column_text(sqlite3_stmt *,int);
extern int sqlite3_column_int(sqlite3_stmt *,int);
extern long long sqlite3_column_int64(sqlite3_stmt *,int);
extern int sqlite3_bind_text(sqlite3_stmt *,int,const char *,int,void (*)(void *));
extern int sqlite3_bind_int64(sqlite3_stmt *,int,long long);
extern int sqlite3_bind_int(sqlite3_stmt *,int,int);
extern long long sqlite3_last_insert_rowid(sqlite3 *);
extern int sqlite3_changes(sqlite3 *);

static long long db_scalar(sqlite3 *db,const char *sql);
static long long canonical_song_id(sqlite3 *db,long long song_id);
static void html_header(const char *title);

struct music_config {
  char db[VALUE_SIZE];
  char store[VALUE_SIZE];
  char drive_token[VALUE_SIZE];
  char drive_root[VALUE_SIZE];
  char drive_folder[VALUE_SIZE];
  char acr_host[VALUE_SIZE];
  char acr_access_key[VALUE_SIZE];
  char acr_access_secret[VALUE_SIZE];
};

static int is_cgi(void) {
  const char *gateway;

  gateway=getenv("GATEWAY_INTERFACE");
  return gateway!=NULL && *gateway!='\0';
}

static void trim(char *s) {
  char *p;
  size_t len;

  p=s;
  for(;*p!='\0' && isspace((unsigned char)*p);p++);
  if(p!=s)memmove(s,p,strlen(p)+1);
  len=strlen(s);
  for(;len>0 && isspace((unsigned char)s[len-1]);len--)s[len-1]='\0';
}

static void executable_dir(char *dir,size_t size) {
  char path[VALUE_SIZE*2];
  char *slash;
  ssize_t n;

  n=readlink("/proc/self/exe",path,sizeof(path)-1);
  if(n<=0 || (size_t)n>=sizeof(path)) {
    snprintf(dir,size,".");
    return;
  }
  path[n]='\0';
  slash=strrchr(path,'/');
  if(slash==NULL) {
    snprintf(dir,size,".");
    return;
  }
  *slash='\0';
  if(strlen(path)+1>size) {
    snprintf(dir,size,".");
    return;
  }
  memcpy(dir,path,strlen(path)+1);
}

static void local_path(char *dst,size_t size,const char *base,const char *path) {
  size_t base_len;
  size_t path_len;

  if(size==0)return;
  base_len=strlen(base);
  path_len=strlen(path);
  if(path[0]=='/') {
    if(path_len+1>size) { dst[0]='\0'; return; }
    memcpy(dst,path,path_len+1);
    return;
  }
  if(base_len+1+path_len+1>size) { dst[0]='\0'; return; }
  memcpy(dst,base,base_len);
  dst[base_len]='/';
  memcpy(dst+base_len+1,path,path_len+1);
}

static void load_config(struct music_config *cfg) {
  const char *env_path;
  FILE *fp;
  char base[VALUE_SIZE];
  char path[VALUE_SIZE*2];
  char db[VALUE_SIZE];
  char store[VALUE_SIZE];
  char line[2048];
  char *eq;
  char *key;
  char *value;

  executable_dir(base,sizeof(base));
  snprintf(db,sizeof(db),"music.db");
  snprintf(store,sizeof(store),"tmpdata/store");
  snprintf(cfg->drive_token,sizeof(cfg->drive_token),"/home/www/data/google_access_token");
  cfg->drive_root[0]='\0';
  snprintf(cfg->drive_folder,sizeof(cfg->drive_folder),"Music");
  cfg->acr_host[0]='\0';
  cfg->acr_access_key[0]='\0';
  cfg->acr_access_secret[0]='\0';
  env_path=getenv("MUSIC_CONFIG");
  if(env_path==NULL || *env_path=='\0')snprintf(path,sizeof(path),"%s/config",base);
  else local_path(path,sizeof(path),base,env_path);
  fp=fopen(path,"r");
  if(fp!=NULL) {
    for(;fgets(line,sizeof(line),fp)!=NULL;) {
      trim(line);
      if(line[0]=='\0' || line[0]=='#')continue;
      eq=strchr(line,'=');
      if(eq==NULL)continue;
      *eq='\0';
      key=line;
      value=eq+1;
      trim(key);
      trim(value);
      if(strcmp(key,"db")==0)snprintf(db,sizeof(db),"%s",value);
      else if(strcmp(key,"store")==0)snprintf(store,sizeof(store),"%s",value);
      else if(strcmp(key,"drive_token")==0)snprintf(cfg->drive_token,sizeof(cfg->drive_token),"%s",value);
      else if(strcmp(key,"drive_root")==0)snprintf(cfg->drive_root,sizeof(cfg->drive_root),"%s",value);
      else if(strcmp(key,"drive_folder")==0)snprintf(cfg->drive_folder,sizeof(cfg->drive_folder),"%s",value);
      else if(strcmp(key,"acr_host")==0)snprintf(cfg->acr_host,sizeof(cfg->acr_host),"%s",value);
      else if(strcmp(key,"acr_access_key")==0)snprintf(cfg->acr_access_key,sizeof(cfg->acr_access_key),"%s",value);
      else if(strcmp(key,"acr_access_secret")==0)snprintf(cfg->acr_access_secret,sizeof(cfg->acr_access_secret),"%s",value);
    }
    fclose(fp);
  }
  local_path(cfg->db,sizeof(cfg->db),base,db);
  local_path(cfg->store,sizeof(cfg->store),base,store);
}

static sqlite3 *db_open(const struct music_config *cfg,int write) {
  sqlite3 *db;
  int flags;

  db=NULL;
  flags=write ? SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE : SQLITE_OPEN_READONLY;
  if(sqlite3_open_v2(cfg->db,&db,flags,NULL)!=SQLITE_OK) {
    if(db!=NULL)sqlite3_close(db);
    return NULL;
  }
  sqlite3_busy_timeout(db,5000);
  sqlite3_exec(db,"PRAGMA foreign_keys=ON",NULL,NULL,NULL);
  return db;
}

static sqlite3 *db_open_web_write(const struct music_config *cfg) {
  sqlite3 *db;

  db=db_open(cfg,1);
  if(db==NULL)return NULL;
  if(sqlite3_exec(db,"PRAGMA journal_mode=MEMORY",NULL,NULL,NULL)!=SQLITE_OK) {
    sqlite3_close(db);
    return NULL;
  }
  return db;
}

static int db_exec(sqlite3 *db,const char *sql) {
  char *error;
  int rc;

  error=NULL;
  rc=sqlite3_exec(db,sql,NULL,NULL,&error);
  if(rc!=SQLITE_OK) {
    fprintf(stderr,"sqlite: %s\n",error!=NULL ? error : sqlite3_errmsg(db));
    if(error!=NULL)sqlite3_free(error);
    return 0;
  }
  return 1;
}

static int db_table_has_column(sqlite3 *db,const char *table,const char *name) {
  sqlite3_stmt *stmt;
  char sql[256];
  int rc;
  int found;

  stmt=NULL;
  found=0;
  snprintf(sql,sizeof(sql),"PRAGMA table_info(%s)",table);
  if(sqlite3_prepare_v2(db,sql,-1,&stmt,NULL)!=SQLITE_OK)return 0;
  for(;(rc=sqlite3_step(stmt))==SQLITE_ROW;) {
    if(sqlite3_column_text(stmt,1)!=NULL && strcmp((const char *)sqlite3_column_text(stmt,1),name)==0) {
      found=1;
      break;
    }
  }
  sqlite3_finalize(stmt);
  return found;
}

static int db_init(const struct music_config *cfg) {
  sqlite3 *db;
  const char *schema;
  int ok;

  db=db_open(cfg,1);
  if(db==NULL)return 0;
  if(!db_exec(db,"PRAGMA journal_mode=DELETE;PRAGMA foreign_keys=ON;")) {
    sqlite3_close(db);
    return 0;
  }
  if(!db_exec(db,"BEGIN IMMEDIATE")) {
    sqlite3_close(db);
    return 0;
  }
  schema=
    "CREATE TABLE IF NOT EXISTS meta(key TEXT PRIMARY KEY,value TEXT NOT NULL);"
    "CREATE TABLE IF NOT EXISTS song("
      "id INTEGER PRIMARY KEY,"
      "title TEXT NOT NULL DEFAULT '',"
      "artist TEXT NOT NULL DEFAULT '',"
      "album TEXT NOT NULL DEFAULT '',"
      "duration INTEGER NOT NULL DEFAULT 0,"
      "isrc TEXT,"
      "played INTEGER NOT NULL DEFAULT 0,"
      "added INTEGER NOT NULL DEFAULT (unixepoch()),"
      "last_played INTEGER,"
      "available INTEGER NOT NULL DEFAULT 1"
    ");"
    "CREATE INDEX IF NOT EXISTS song_artist_album ON song(artist,album);"
    "CREATE INDEX IF NOT EXISTS song_title ON song(title);"
    "CREATE INDEX IF NOT EXISTS song_isrc ON song(isrc);"
    "CREATE TABLE IF NOT EXISTS source("
      "id INTEGER PRIMARY KEY,"
      "song_id INTEGER NOT NULL REFERENCES song(id) ON DELETE CASCADE,"
      "type TEXT NOT NULL,"
      "ref TEXT NOT NULL,"
      "original_title TEXT,"
      "original_artist TEXT,"
      "original_album TEXT,"
      "bytes INTEGER NOT NULL DEFAULT 0,"
      "md5 TEXT,"
      "UNIQUE(type,ref)"
    ");"
    "CREATE TABLE IF NOT EXISTS user("
      "id INTEGER PRIMARY KEY,"
      "username TEXT NOT NULL UNIQUE COLLATE NOCASE,"
      "password_hash TEXT NOT NULL,"
      "password_salt TEXT NOT NULL,"
      "iterations INTEGER NOT NULL,"
      "enabled INTEGER NOT NULL DEFAULT 1,"
      "created INTEGER NOT NULL DEFAULT (unixepoch()),"
      "privacy_version TEXT,"
      "privacy_accepted INTEGER"
    ");"
    "CREATE TABLE IF NOT EXISTS user_stats("
      "user_id INTEGER PRIMARY KEY REFERENCES user(id) ON DELETE CASCADE,"
      "stats_since INTEGER NOT NULL DEFAULT (unixepoch()),"
      "login_count INTEGER NOT NULL DEFAULT 0,"
      "last_login INTEGER,"
      "last_seen INTEGER,"
      "played INTEGER NOT NULL DEFAULT 0,"
      "last_played INTEGER"
    ");"
    "CREATE TABLE IF NOT EXISTS session("
      "token_hash TEXT PRIMARY KEY,"
      "user_id INTEGER NOT NULL REFERENCES user(id) ON DELETE CASCADE,"
      "created INTEGER NOT NULL DEFAULT (unixepoch()),"
      "last_seen INTEGER NOT NULL DEFAULT (unixepoch()),"
      "expires INTEGER NOT NULL"
    ");"
    "CREATE INDEX IF NOT EXISTS session_user ON session(user_id);"
    "CREATE TABLE IF NOT EXISTS playlist("
      "id INTEGER PRIMARY KEY,"
      "user_id INTEGER NOT NULL REFERENCES user(id) ON DELETE CASCADE,"
      "name TEXT NOT NULL COLLATE NOCASE,"
      "description TEXT NOT NULL DEFAULT '',"
      "shared INTEGER NOT NULL DEFAULT 0,"
      "UNIQUE(user_id,name)"
    ");"
    "CREATE TABLE IF NOT EXISTS playlist_song("
      "playlist_id INTEGER NOT NULL REFERENCES playlist(id) ON DELETE CASCADE,"
      "song_id INTEGER NOT NULL REFERENCES song(id) ON DELETE CASCADE,"
      "position INTEGER NOT NULL,"
      "PRIMARY KEY(playlist_id,song_id),"
      "UNIQUE(playlist_id,position)"
    ");"
    "CREATE TABLE IF NOT EXISTS acr_result("
      "song_id INTEGER PRIMARY KEY REFERENCES song(id) ON DELETE CASCADE,"
      "clip_start INTEGER NOT NULL,"
      "clip_seconds INTEGER NOT NULL,"
      "acrid TEXT,"
      "artist TEXT,"
      "title TEXT,"
      "album TEXT,"
      "release_date TEXT,"
      "label TEXT,"
      "isrc TEXT,"
      "upc TEXT,"
      "score INTEGER NOT NULL DEFAULT 0,"
      "raw_json TEXT NOT NULL,"
      "first_seen INTEGER NOT NULL DEFAULT (unixepoch()),"
      "updated INTEGER NOT NULL DEFAULT (unixepoch())"
    ");"
    "CREATE TABLE IF NOT EXISTS acr_failure("
      "song_id INTEGER PRIMARY KEY REFERENCES song(id) ON DELETE CASCADE,"
      "status_code INTEGER NOT NULL,"
      "attempts INTEGER NOT NULL DEFAULT 0,"
      "no_match_count INTEGER NOT NULL DEFAULT 0,"
      "last_clip_start INTEGER NOT NULL DEFAULT 0,"
      "terminal INTEGER NOT NULL DEFAULT 0,"
      "updated INTEGER NOT NULL DEFAULT (unixepoch())"
    ");"
    "CREATE TABLE IF NOT EXISTS shared_queue("
      "id INTEGER PRIMARY KEY,"
      "revision INTEGER NOT NULL DEFAULT 0,"
      "state INTEGER NOT NULL DEFAULT 0,"
      "current_song_id INTEGER REFERENCES song(id) ON DELETE SET NULL,"
      "base_position_ms INTEGER NOT NULL DEFAULT 0,"
      "base_time_ms INTEGER NOT NULL DEFAULT 0,"
      "changed_by INTEGER REFERENCES user(id) ON DELETE SET NULL"
    ");"
    "CREATE TABLE IF NOT EXISTS shared_queue_song("
      "queue_id INTEGER NOT NULL REFERENCES shared_queue(id) ON DELETE CASCADE,"
      "position INTEGER NOT NULL,"
      "song_id INTEGER NOT NULL REFERENCES song(id) ON DELETE CASCADE,"
      "added_by INTEGER REFERENCES user(id) ON DELETE SET NULL,"
      "added_at INTEGER NOT NULL DEFAULT 0,"
      "PRIMARY KEY(queue_id,position),"
      "UNIQUE(queue_id,song_id)"
    ");";
  ok=db_exec(db,schema);
  if(ok)ok=db_exec(db,"DROP TABLE IF EXISTS song_tag;DROP TABLE IF EXISTS tag;DROP TABLE IF EXISTS audd_result;DROP TABLE IF EXISTS fingerprint;DROP TABLE IF EXISTS identification;DROP INDEX IF EXISTS song_sha256;");
  if(ok && !db_table_has_column(db,"source","bytes"))ok=db_exec(db,"ALTER TABLE source ADD COLUMN bytes INTEGER NOT NULL DEFAULT 0");
  if(ok && !db_table_has_column(db,"source","md5"))ok=db_exec(db,"ALTER TABLE source ADD COLUMN md5 TEXT");
  if(ok && !db_table_has_column(db,"user","privacy_version"))ok=db_exec(db,"ALTER TABLE user ADD COLUMN privacy_version TEXT");
  if(ok && !db_table_has_column(db,"user","privacy_accepted"))ok=db_exec(db,"ALTER TABLE user ADD COLUMN privacy_accepted INTEGER");
  if(ok && !db_table_has_column(db,"shared_queue","changed_by"))ok=db_exec(db,"ALTER TABLE shared_queue ADD COLUMN changed_by INTEGER REFERENCES user(id) ON DELETE SET NULL");
  if(ok && !db_table_has_column(db,"shared_queue_song","added_by"))ok=db_exec(db,"ALTER TABLE shared_queue_song ADD COLUMN added_by INTEGER REFERENCES user(id) ON DELETE SET NULL");
  if(ok && !db_table_has_column(db,"shared_queue_song","added_at"))ok=db_exec(db,"ALTER TABLE shared_queue_song ADD COLUMN added_at INTEGER NOT NULL DEFAULT 0");
  if(ok && !db_table_has_column(db,"acr_result","first_seen"))ok=db_exec(db,"ALTER TABLE acr_result ADD COLUMN first_seen INTEGER NOT NULL DEFAULT 0");
  if(ok)ok=db_exec(db,"UPDATE acr_result SET first_seen=updated WHERE first_seen=0");
  if(ok && db_table_has_column(db,"song","sha256"))ok=db_exec(db,"ALTER TABLE song DROP COLUMN sha256");
  if(ok)ok=db_exec(db,"CREATE INDEX IF NOT EXISTS source_md5 ON source(md5) WHERE type='drive' AND md5 IS NOT NULL AND md5<>''");
  if(ok && !db_table_has_column(db,"playlist","user_id")) {
    if(db_scalar(db,"SELECT count(*) FROM playlist")!=0) {
      fprintf(stderr,"sqlite: cannot migrate non-empty legacy playlist table automatically\n");
      ok=0;
    } else {
      ok=db_exec(db,"DROP TABLE playlist_song;DROP TABLE playlist;CREATE TABLE playlist(id INTEGER PRIMARY KEY,user_id INTEGER NOT NULL REFERENCES user(id) ON DELETE CASCADE,name TEXT NOT NULL COLLATE NOCASE,description TEXT NOT NULL DEFAULT '',shared INTEGER NOT NULL DEFAULT 0,UNIQUE(user_id,name));CREATE TABLE playlist_song(playlist_id INTEGER NOT NULL REFERENCES playlist(id) ON DELETE CASCADE,song_id INTEGER NOT NULL REFERENCES song(id) ON DELETE CASCADE,position INTEGER NOT NULL,PRIMARY KEY(playlist_id,song_id),UNIQUE(playlist_id,position));");
    }
  }
  if(ok)ok=db_exec(db,"INSERT OR IGNORE INTO user_stats(user_id,last_login,last_seen) SELECT u.id,u.privacy_accepted,coalesce((SELECT max(s.last_seen) FROM session s WHERE s.user_id=u.id),u.privacy_accepted) FROM user u");
  if(ok)ok=db_exec(db,"INSERT OR IGNORE INTO shared_queue(id) VALUES(0),(1),(2),(3),(4),(5),(6),(7),(8),(9)");
  if(ok)ok=db_exec(db,"INSERT OR REPLACE INTO meta(key,value) VALUES('schema','8')");
  if(ok)ok=db_exec(db,"COMMIT");
  else db_exec(db,"ROLLBACK");
  sqlite3_close(db);
  return ok;
}


struct mem {
  char *ptr;
  size_t len;
};

struct drive_scan_item {
  char id[VALUE_SIZE];
  char name[VALUE_SIZE];
  char artist[VALUE_SIZE];
  char album[VALUE_SIZE];
  char md5[64];
  long long bytes;
};

struct drive_scan_context {
  CURL *curl;
  struct curl_slist *headers;
  struct drive_scan_item *items;
  long count;
  long capacity;
  long folders;
  long files;
  long songs_new;
  long songs_known;
  long skipped;
};

static void mem_init(struct mem *m) {
  m->ptr=(char *)malloc(1);
  m->len=0;
  if(m->ptr!=NULL)m->ptr[0]='\0';
}

static size_t drive_write_cb(void *contents,size_t size,size_t nmemb,void *userp) {
  struct mem *m;
  char *p;
  size_t bytes;

  m=(struct mem *)userp;
  bytes=size*nmemb;
  p=(char *)realloc(m->ptr,m->len+bytes+1);
  if(p==NULL)return 0;
  m->ptr=p;
  memcpy(m->ptr+m->len,contents,bytes);
  m->len+=bytes;
  m->ptr[m->len]='\0';
  return bytes;
}

static int read_line_file(const char *path,char *value,size_t size) {
  FILE *fp;
  size_t n;

  if(size==0)return 0;
  value[0]='\0';
  fp=fopen(path,"r");
  if(fp==NULL)return 0;
  if(fgets(value,(int)size,fp)==NULL) {
    fclose(fp);
    value[0]='\0';
    return 0;
  }
  fclose(fp);
  n=strlen(value);
  for(;n>0 && (value[n-1]=='\n' || value[n-1]=='\r');n--)value[n-1]='\0';
  return value[0]!='\0';
}

static int drive_http_get(CURL *curl,const char *url,struct curl_slist *headers,struct mem *body,long *http) {
  CURLcode rc;

  mem_init(body);
  if(body->ptr==NULL)return 0;
  curl_easy_reset(curl);
  curl_easy_setopt(curl,CURLOPT_URL,url);
  curl_easy_setopt(curl,CURLOPT_HTTPHEADER,headers);
  curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,drive_write_cb);
  curl_easy_setopt(curl,CURLOPT_WRITEDATA,body);
  curl_easy_setopt(curl,CURLOPT_SSL_VERIFYPEER,1L);
  curl_easy_setopt(curl,CURLOPT_SSL_VERIFYHOST,2L);
  curl_easy_setopt(curl,CURLOPT_CONNECTTIMEOUT,15L);
  curl_easy_setopt(curl,CURLOPT_TIMEOUT,120L);
  curl_easy_setopt(curl,CURLOPT_USERAGENT,"music/" MUSIC_VERSION);
  rc=curl_easy_perform(curl);
  if(rc!=CURLE_OK) {
    fprintf(stderr,"drive: %s\n",curl_easy_strerror(rc));
    return 0;
  }
  *http=0;
  curl_easy_getinfo(curl,CURLINFO_RESPONSE_CODE,http);
  return 1;
}

static const char *json_object_end(const char *p) {
  int depth;
  int quoted;
  int escaped;

  depth=0;
  quoted=0;
  escaped=0;
  for(;*p!='\0';p++) {
    if(quoted) {
      if(escaped)escaped=0;
      else if(*p=='\\')escaped=1;
      else if(*p=='\"')quoted=0;
      continue;
    }
    if(*p=='\"')quoted=1;
    else if(*p=='{')depth++;
    else if(*p=='}') {
      depth--;
      if(depth==0)return p+1;
    }
  }
  return NULL;
}

static int json_hex(char c) {
  if(c>='0' && c<='9')return c-'0';
  if(c>='a' && c<='f')return c-'a'+10;
  if(c>='A' && c<='F')return c-'A'+10;
  return -1;
}

static int json_utf8(char *out,size_t size,size_t *i,unsigned long code) {
  if(code<=0x7f) {
    if(*i+1>=size)return 0;
    out[(*i)++]=(char)code;
  }
  else if(code<=0x7ff) {
    if(*i+2>=size)return 0;
    out[(*i)++]=(char)(0xc0|(code>>6));
    out[(*i)++]=(char)(0x80|(code&0x3f));
  }
  else {
    if(*i+3>=size)return 0;
    out[(*i)++]=(char)(0xe0|(code>>12));
    out[(*i)++]=(char)(0x80|((code>>6)&0x3f));
    out[(*i)++]=(char)(0x80|(code&0x3f));
  }
  return 1;
}

static int json_string_range(const char *begin,const char *end,const char *key,char *out,size_t size) {
  char pattern[128];
  const char *p;
  size_t i;
  unsigned long code;
  int h;
  int n;

  if(size==0)return 0;
  out[0]='\0';
  snprintf(pattern,sizeof(pattern),"\"%s\"",key);
  p=begin;
  for(;;) {
    p=strstr(p,pattern);
    if(p==NULL || p>=end)return 0;
    p+=strlen(pattern);
    if(p<end)break;
  }
  for(;p<end && isspace((unsigned char)*p);p++);
  if(p>=end || *p!=':')return 0;
  p++;
  for(;p<end && isspace((unsigned char)*p);p++);
  if(p>=end || *p!='\"')return 0;
  p++;
  i=0;
  for(;p<end && *p!='\0';p++) {
    if(*p=='\"') {
      out[i]='\0';
      return 1;
    }
    if(*p!='\\') {
      if(i+1<size)out[i++]=*p;
      continue;
    }
    p++;
    if(p>=end || *p=='\0')break;
    if(*p=='u' && p+4<end) {
      code=0;
      for(n=1;n<=4;n++) {
        h=json_hex(p[n]);
        if(h<0)break;
        code=(code<<4)|(unsigned long)h;
      }
      if(n==5) {
        json_utf8(out,size,&i,code);
        p+=4;
        continue;
      }
    }
    if(i+1>=size)continue;
    if(*p=='n')out[i++]='\n';
    else if(*p=='r')out[i++]='\r';
    else if(*p=='t')out[i++]='\t';
    else if(*p=='b')out[i++]='\b';
    else if(*p=='f')out[i++]='\f';
    else out[i++]=*p;
  }
  out[i]='\0';
  return 0;
}

static int json_string(const char *json,const char *key,char *out,size_t size) {
  return json_string_range(json,json+strlen(json),key,out,size);
}

static const char *json_top_value(const char *begin,const char *end,const char *key) {
  const char *p;
  const char *q;
  const char *r;
  size_t key_len;
  int object_depth;
  int array_depth;
  int escaped;

  key_len=strlen(key);
  object_depth=0;
  array_depth=0;
  p=begin;
  for(;p<end && *p!='\0';) {
    if(*p=='\"') {
      q=p+1;
      escaped=0;
      for(;q<end && *q!='\0';q++) {
        if(escaped)escaped=0;
        else if(*q=='\\')escaped=1;
        else if(*q=='\"')break;
      }
      if(q>=end)return NULL;
      if(object_depth==1 && array_depth==0 && (size_t)(q-p-1)==key_len && strncmp(p+1,key,key_len)==0) {
        r=q+1;
        for(;r<end && isspace((unsigned char)*r);r++);
        if(r<end && *r==':') {
          r++;
          for(;r<end && isspace((unsigned char)*r);r++);
          return r<end ? r : NULL;
        }
      }
      p=q+1;
      continue;
    }
    if(*p=='{')object_depth++;
    else if(*p=='}')object_depth--;
    else if(*p=='[')array_depth++;
    else if(*p==']')array_depth--;
    p++;
  }
  return NULL;
}

static int json_value_string(const char *value,const char *end,char *out,size_t size) {
  const char *p;
  size_t i;
  unsigned long code;
  int h;
  int n;

  if(size==0)return 0;
  out[0]='\0';
  if(value==NULL || value>=end || *value!='\"')return 0;
  i=0;
  for(p=value+1;p<end && *p!='\0';p++) {
    if(*p=='\"') {
      out[i]='\0';
      return 1;
    }
    if(*p!='\\') {
      if(i+1<size)out[i++]=*p;
      continue;
    }
    p++;
    if(p>=end || *p=='\0')break;
    if(*p=='u' && p+4<end) {
      code=0;
      for(n=1;n<=4;n++) {
        h=json_hex(p[n]);
        if(h<0)break;
        code=(code<<4)|(unsigned long)h;
      }
      if(n==5) {
        json_utf8(out,size,&i,code);
        p+=4;
        continue;
      }
    }
    if(i+1>=size)continue;
    if(*p=='n')out[i++]='\n';
    else if(*p=='r')out[i++]='\r';
    else if(*p=='t')out[i++]='\t';
    else if(*p=='b')out[i++]='\b';
    else if(*p=='f')out[i++]='\f';
    else out[i++]=*p;
  }
  out[i]='\0';
  return 0;
}

static int json_top_string(const char *begin,const char *end,const char *key,char *out,size_t size) {
  return json_value_string(json_top_value(begin,end,key),end,out,size);
}

static int json_top_number(const char *begin,const char *end,const char *key,double *number) {
  const char *p;
  char *tail;
  double value;

  p=json_top_value(begin,end,key);
  if(p==NULL || p>=end)return 0;
  value=strtod(p,&tail);
  if(tail==p || tail>end)return 0;
  *number=value;
  return 1;
}

static const char *json_array_end(const char *p,const char *end) {
  int depth;
  int quoted;
  int escaped;

  if(p==NULL || p>=end || *p!='[')return NULL;
  depth=0;
  quoted=0;
  escaped=0;
  for(;p<end && *p!='\0';p++) {
    if(quoted) {
      if(escaped)escaped=0;
      else if(*p=='\\')escaped=1;
      else if(*p=='\"')quoted=0;
      continue;
    }
    if(*p=='\"')quoted=1;
    else if(*p=='[')depth++;
    else if(*p==']') {
      depth--;
      if(depth==0)return p;
    }
  }
  return NULL;
}




static int drive_find_folder(CURL *curl,struct curl_slist *headers,const char *parent,const char *name,char *id,size_t id_size) {
  char query[VALUE_SIZE*2];
  char url[VALUE_SIZE*6];
  char *encoded;
  struct mem body;
  long http;
  int ok;

  snprintf(query,sizeof(query),"name='%s' and '%s' in parents and mimeType='application/vnd.google-apps.folder' and trashed=false",name,parent);
  encoded=curl_easy_escape(curl,query,0);
  if(encoded==NULL)return 0;
  snprintf(url,sizeof(url),"https://www.googleapis.com/drive/v3/files?q=%s&fields=files(id,name)&pageSize=2&supportsAllDrives=true&includeItemsFromAllDrives=true",encoded);
  curl_free(encoded);
  if(!drive_http_get(curl,url,headers,&body,&http)) {
    free(body.ptr);
    return 0;
  }
  ok=http>=200 && http<300 && json_string(body.ptr,"id",id,id_size);
  if(!ok && http>=200 && http<300)fprintf(stderr,"drive: folder '%s' not found\n",name);
  if(http<200 || http>=300)fprintf(stderr,"drive: HTTP %ld while finding '%s'\n",http,name);
  free(body.ptr);
  return ok;
}

static int has_mp3_extension(const char *name) {
  size_t n;

  n=strlen(name);
  if(n<4)return 0;
  return name[n-4]=='.' && tolower((unsigned char)name[n-3])=='m' && tolower((unsigned char)name[n-2])=='p' && name[n-1]=='3';
}

static void title_from_name(char *title,size_t size,const char *name) {
  size_t n;

  snprintf(title,size,"%s",name);
  n=strlen(title);
  if(n>=4 && has_mp3_extension(title))title[n-4]='\0';
}

static int drive_scan_add(struct drive_scan_context *ctx,const char *id,const char *name,const char *artist,const char *album,long long bytes,const char *md5) {
  struct drive_scan_item *next;
  struct drive_scan_item *item;
  long capacity;

  if(ctx->count==ctx->capacity) {
    capacity=ctx->capacity==0 ? 256 : ctx->capacity*2;
    next=(struct drive_scan_item *)realloc(ctx->items,(size_t)capacity*sizeof(*next));
    if(next==NULL)return 0;
    ctx->items=next;
    ctx->capacity=capacity;
  }
  item=&ctx->items[ctx->count++];
  snprintf(item->id,sizeof(item->id),"%s",id);
  snprintf(item->name,sizeof(item->name),"%s",name);
  snprintf(item->artist,sizeof(item->artist),"%s",artist);
  snprintf(item->album,sizeof(item->album),"%s",album);
  snprintf(item->md5,sizeof(item->md5),"%s",md5);
  item->bytes=bytes;
  return 1;
}

static int db_drive_song(sqlite3 *db,const struct drive_scan_item *item,int *is_new) {
  sqlite3_stmt *stmt;
  long long song_id;
  char title[VALUE_SIZE];
  int rc;

  *is_new=0;
  title_from_name(title,sizeof(title),item->name);
  stmt=NULL;
  if(sqlite3_prepare_v2(db,"SELECT song_id FROM source WHERE type='drive' AND ref=?1",-1,&stmt,NULL)!=SQLITE_OK)return 0;
  sqlite3_bind_text(stmt,1,item->id,-1,(void (*)(void *))-1);
  rc=sqlite3_step(stmt);
  if(rc==SQLITE_ROW) {
    song_id=sqlite3_column_int64(stmt,0);
    sqlite3_finalize(stmt);
    stmt=NULL;
    if(sqlite3_prepare_v2(db,"UPDATE source SET original_title=?1,original_artist=?2,original_album=?3,bytes=?4,md5=?5 WHERE type='drive' AND ref=?6",-1,&stmt,NULL)!=SQLITE_OK)return 0;
    sqlite3_bind_text(stmt,1,title,-1,(void (*)(void *))-1);
    sqlite3_bind_text(stmt,2,item->artist,-1,(void (*)(void *))-1);
    sqlite3_bind_text(stmt,3,item->album,-1,(void (*)(void *))-1);
    sqlite3_bind_int64(stmt,4,item->bytes);
    sqlite3_bind_text(stmt,5,item->md5,-1,(void (*)(void *))-1);
    sqlite3_bind_text(stmt,6,item->id,-1,(void (*)(void *))-1);
    rc=sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if(rc!=SQLITE_DONE)return 0;
    stmt=NULL;
    if(sqlite3_prepare_v2(db,"UPDATE song SET available=1 WHERE id=?1",-1,&stmt,NULL)!=SQLITE_OK)return 0;
    sqlite3_bind_int64(stmt,1,song_id);
    rc=sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc==SQLITE_DONE;
  }
  sqlite3_finalize(stmt);
  stmt=NULL;
  if(sqlite3_prepare_v2(db,"INSERT INTO song(title,artist,album) VALUES(?1,?2,?3)",-1,&stmt,NULL)!=SQLITE_OK)return 0;
  sqlite3_bind_text(stmt,1,title,-1,(void (*)(void *))-1);
  sqlite3_bind_text(stmt,2,item->artist,-1,(void (*)(void *))-1);
  sqlite3_bind_text(stmt,3,item->album,-1,(void (*)(void *))-1);
  rc=sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  if(rc!=SQLITE_DONE)return 0;
  song_id=sqlite3_last_insert_rowid(db);
  stmt=NULL;
  if(sqlite3_prepare_v2(db,"INSERT INTO source(song_id,type,ref,original_title,original_artist,original_album,bytes,md5) VALUES(?1,'drive',?2,?3,?4,?5,?6,?7)",-1,&stmt,NULL)!=SQLITE_OK)return 0;
  sqlite3_bind_int64(stmt,1,song_id);
  sqlite3_bind_text(stmt,2,item->id,-1,(void (*)(void *))-1);
  sqlite3_bind_text(stmt,3,title,-1,(void (*)(void *))-1);
  sqlite3_bind_text(stmt,4,item->artist,-1,(void (*)(void *))-1);
  sqlite3_bind_text(stmt,5,item->album,-1,(void (*)(void *))-1);
  sqlite3_bind_int64(stmt,6,item->bytes);
  sqlite3_bind_text(stmt,7,item->md5,-1,(void (*)(void *))-1);
  rc=sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  if(rc!=SQLITE_DONE)return 0;
  *is_new=1;
  return 1;
}

static int drive_scan_folder(struct drive_scan_context *ctx,const char *parent,int depth,const char *artist,const char *album) {
  char query[VALUE_SIZE*2];
  char url[VALUE_SIZE*8];
  char page[VALUE_SIZE*2];
  char next[VALUE_SIZE*2];
  char id[VALUE_SIZE];
  char name[VALUE_SIZE];
  char mime[VALUE_SIZE];
  char sizebuf[VALUE_SIZE];
  char md5[VALUE_SIZE];
  char next_artist[VALUE_SIZE];
  char next_album[VALUE_SIZE];
  char *encoded;
  struct mem body;
  const char *files;
  const char *p;
  const char *end;
  long http;
  long long bytes;
  int ok;

  page[0]='\0';
  for(;;) {
    snprintf(query,sizeof(query),"'%s' in parents and trashed=false",parent);
    encoded=curl_easy_escape(ctx->curl,query,0);
    if(encoded==NULL)return 0;
    if(page[0]=='\0')snprintf(url,sizeof(url),"https://www.googleapis.com/drive/v3/files?q=%s&fields=nextPageToken,files(id,name,mimeType,size,md5Checksum)&pageSize=1000&supportsAllDrives=true&includeItemsFromAllDrives=true",encoded);
    else snprintf(url,sizeof(url),"https://www.googleapis.com/drive/v3/files?q=%s&fields=nextPageToken,files(id,name,mimeType,size,md5Checksum)&pageSize=1000&pageToken=%s&supportsAllDrives=true&includeItemsFromAllDrives=true",encoded,page);
    curl_free(encoded);
    if(!drive_http_get(ctx->curl,url,ctx->headers,&body,&http)) {
      free(body.ptr);
      return 0;
    }
    if(http<200 || http>=300) {
      fprintf(stderr,"drive: HTTP %ld while listing folder\n",http);
      free(body.ptr);
      return 0;
    }
    next[0]='\0';
    json_string(body.ptr,"nextPageToken",next,sizeof(next));
    files=strstr(body.ptr,"\"files\"");
    p=files!=NULL ? strchr(files,'[') : NULL;
    if(p!=NULL)p++;
    for(;p!=NULL && *p!='\0';) {
      for(;*p!='\0' && *p!='{' && *p!=']';p++);
      if(*p==']' || *p=='\0')break;
      end=json_object_end(p);
      if(end==NULL)break;
      id[0]='\0';
      name[0]='\0';
      mime[0]='\0';
      sizebuf[0]='\0';
      md5[0]='\0';
      bytes=0;
      ok=json_string_range(p,end,"id",id,sizeof(id));
      ok=ok && json_string_range(p,end,"name",name,sizeof(name));
      ok=ok && json_string_range(p,end,"mimeType",mime,sizeof(mime));
      if(ok) {
        if(strcmp(mime,"application/vnd.google-apps.folder")==0) {
          ctx->folders++;
          snprintf(next_artist,sizeof(next_artist),"%s",artist);
          snprintf(next_album,sizeof(next_album),"%s",album);
          if(depth==0)snprintf(next_artist,sizeof(next_artist),"%s",name);
          else if(depth==1)snprintf(next_album,sizeof(next_album),"%s",name);
          if(!drive_scan_folder(ctx,id,depth+1,next_artist,next_album)) {
            free(body.ptr);
            return 0;
          }
        }
        else {
          ctx->files++;
          if(strcmp(mime,"audio/mpeg")==0 || has_mp3_extension(name)) {
            if(json_string_range(p,end,"size",sizebuf,sizeof(sizebuf)))bytes=atoll(sizebuf);
            json_string_range(p,end,"md5Checksum",md5,sizeof(md5));
            if(!drive_scan_add(ctx,id,name,artist,album,bytes,md5)) {
              free(body.ptr);
              return 0;
            }
          }
          else ctx->skipped++;
        }
      }
      p=end;
    }
    free(body.ptr);
    if(next[0]=='\0')break;
    snprintf(page,sizeof(page),"%s",next);
  }
  return 1;
}

static int drive_prepare(const struct music_config *cfg,CURL **curl_out,struct curl_slist **headers_out,char *root,size_t root_size) {
  CURL *curl;
  struct curl_slist *headers;
  char token[4096];
  char auth[4608];

  *curl_out=NULL;
  *headers_out=NULL;
  if(!read_line_file(cfg->drive_token,token,sizeof(token))) {
    fprintf(stderr,"drive: cannot read access token %s\n",cfg->drive_token);
    return 0;
  }
  if(curl_global_init(CURL_GLOBAL_DEFAULT)!=0)return 0;
  curl=curl_easy_init();
  if(curl==NULL) {
    curl_global_cleanup();
    return 0;
  }
  snprintf(auth,sizeof(auth),"Authorization: Bearer %s",token);
  headers=NULL;
  headers=curl_slist_append(headers,auth);
  if(headers==NULL) {
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    return 0;
  }
  if(cfg->drive_root[0]!='\0')snprintf(root,root_size,"%s",cfg->drive_root);
  else if(!drive_find_folder(curl,headers,"root",cfg->drive_folder,root,root_size)) {
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    return 0;
  }
  *curl_out=curl;
  *headers_out=headers;
  return 1;
}

static void drive_finish(CURL *curl,struct curl_slist *headers) {
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  curl_global_cleanup();
}

static int drive_test(const struct music_config *cfg) {
  CURL *curl;
  struct curl_slist *headers;
  char root[VALUE_SIZE];

  if(!drive_prepare(cfg,&curl,&headers,root,sizeof(root)))return 0;
  printf("Google Drive: ok, folder %s found\n",cfg->drive_folder);
  drive_finish(curl,headers);
  return 1;
}

static int drive_scan(const struct music_config *cfg) {
  struct drive_scan_context ctx;
  sqlite3 *db;
  char root[VALUE_SIZE];
  long unavailable;
  long i;
  int is_new;
  int ok;

  memset(&ctx,0,sizeof(ctx));
  db=db_open(cfg,0);
  if(db==NULL || db_scalar(db,"SELECT count(*) FROM sqlite_master WHERE type='table' AND name='song'")==0) {
    if(db!=NULL)sqlite3_close(db);
    fprintf(stderr,"catalog not initialized; run music init first\n");
    return 0;
  }
  sqlite3_close(db);
  if(!drive_prepare(cfg,&ctx.curl,&ctx.headers,root,sizeof(root)))return 0;
  ok=drive_scan_folder(&ctx,root,0,"","");
  drive_finish(ctx.curl,ctx.headers);
  if(!ok) {
    free(ctx.items);
    return 0;
  }
  db=db_open(cfg,1);
  if(db==NULL) {
    free(ctx.items);
    return 0;
  }
  ok=db_exec(db,"BEGIN IMMEDIATE");
  if(ok)ok=db_exec(db,"UPDATE song SET available=0 WHERE id IN (SELECT song_id FROM source WHERE type='drive')");
  for(i=0;ok && i<ctx.count;i++) {
    is_new=0;
    ok=db_drive_song(db,&ctx.items[i],&is_new);
    if(ok) {
      if(is_new)ctx.songs_new++;
      else ctx.songs_known++;
    }
  }
  if(ok)ok=db_exec(db,"COMMIT");
  else db_exec(db,"ROLLBACK");
  unavailable=ok ? db_scalar(db,"SELECT count(DISTINCT s.id) FROM song s JOIN source src ON src.song_id=s.id AND src.type='drive' WHERE s.available=0") : 0;
  sqlite3_close(db);
  free(ctx.items);
  if(!ok)return 0;
  printf("scan complete: %ld folders, %ld files, %ld new songs, %ld known songs, %ld unavailable, %ld skipped\n",ctx.folders,ctx.files,ctx.songs_new,ctx.songs_known,unavailable,ctx.skipped);
  return 1;
}


static const char *column_text(sqlite3_stmt *stmt,int column);

static int ensure_store(const struct music_config *cfg) {
  struct stat st;

  if(stat(cfg->store,&st)==0)return S_ISDIR(st.st_mode);
  if(mkdir(cfg->store,0755)==0)return 1;
  return 0;
}

static int song_drive_ref(sqlite3 *db,long long song_id,char *ref,size_t ref_size) {
  sqlite3_stmt *stmt;
  int rc;

  stmt=NULL;
  if(sqlite3_prepare_v2(db,"SELECT src.ref FROM source src JOIN song s ON s.id=src.song_id WHERE src.song_id=?1 AND src.type='drive' AND s.available=1 LIMIT 1",-1,&stmt,NULL)!=SQLITE_OK)return 0;
  sqlite3_bind_int64(stmt,1,song_id);
  rc=sqlite3_step(stmt);
  if(rc!=SQLITE_ROW) {
    sqlite3_finalize(stmt);
    return 0;
  }
  snprintf(ref,ref_size,"%s",column_text(stmt,0));
  sqlite3_finalize(stmt);
  return ref[0]!='\0';
}

static volatile sig_atomic_t store_interrupted=0;

static void store_sigint(int signum) {
  (void)signum;
  store_interrupted=1;
}

static int drive_progress(void *data,curl_off_t dltotal,curl_off_t dlnow,curl_off_t ultotal,curl_off_t ulnow) {
  (void)data;
  (void)dltotal;
  (void)dlnow;
  (void)ultotal;
  (void)ulnow;
  return store_interrupted ? 1 : 0;
}

static int drive_download_id(const struct music_config *cfg,const char *drive_id,const char *path) {
  CURL *curl;
  struct curl_slist *headers;
  FILE *fp;
  char token[4096];
  char auth[4608];
  char url[VALUE_SIZE*4];
  char tmp[VALUE_SIZE*2];
  CURLcode res;
  long http;

  if(!read_line_file(cfg->drive_token,token,sizeof(token)))return 0;
  if(curl_global_init(CURL_GLOBAL_DEFAULT)!=0)return 0;
  curl=curl_easy_init();
  if(curl==NULL) {
    curl_global_cleanup();
    return 0;
  }
  snprintf(auth,sizeof(auth),"Authorization: Bearer %s",token);
  headers=NULL;
  headers=curl_slist_append(headers,auth);
  if(headers==NULL) {
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    return 0;
  }
  snprintf(tmp,sizeof(tmp),"%s.tmp.%ld",path,(long)getpid());
  fp=fopen(tmp,"wb");
  if(fp==NULL) {
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    return 0;
  }
  snprintf(url,sizeof(url),"https://www.googleapis.com/drive/v3/files/%s?alt=media&supportsAllDrives=true",drive_id);
  curl_easy_setopt(curl,CURLOPT_URL,url);
  curl_easy_setopt(curl,CURLOPT_HTTPHEADER,headers);
  curl_easy_setopt(curl,CURLOPT_WRITEDATA,fp);
  curl_easy_setopt(curl,CURLOPT_SSL_VERIFYPEER,1L);
  curl_easy_setopt(curl,CURLOPT_SSL_VERIFYHOST,2L);
  curl_easy_setopt(curl,CURLOPT_CONNECTTIMEOUT,15L);
  curl_easy_setopt(curl,CURLOPT_TIMEOUT,300L);
  curl_easy_setopt(curl,CURLOPT_USERAGENT,"music/" MUSIC_VERSION);
  curl_easy_setopt(curl,CURLOPT_NOPROGRESS,0L);
  curl_easy_setopt(curl,CURLOPT_XFERINFOFUNCTION,drive_progress);
  res=curl_easy_perform(curl);
  http=0;
  if(res==CURLE_OK)curl_easy_getinfo(curl,CURLINFO_RESPONSE_CODE,&http);
  fclose(fp);
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  curl_global_cleanup();
  if(res!=CURLE_OK || http<200 || http>=300) {
    remove(tmp);
    return 0;
  }
  if(rename(tmp,path)!=0) {
    remove(tmp);
    return 0;
  }
  return 1;
}

static int store_song(const struct music_config *cfg,long long song_id,char *path,size_t path_size,int verbose) {
  sqlite3 *db;
  struct stat st;
  char drive_id[VALUE_SIZE];

  if(song_id<=0)return 0;
  db=db_open(cfg,0);
  if(db==NULL)return 0;
  if(!song_drive_ref(db,song_id,drive_id,sizeof(drive_id))) {
    sqlite3_close(db);
    return 0;
  }
  sqlite3_close(db);
  if(!ensure_store(cfg))return 0;
  snprintf(path,path_size,"%s/%lld.mp3",cfg->store,song_id);
  if(stat(path,&st)==0 && S_ISREG(st.st_mode) && st.st_size>0) {
    if(verbose)printf("STORE hit: %s (%lld bytes)\n",path,(long long)st.st_size);
    return 1;
  }
  if(verbose)printf("SOURCE -> STORE: song %lld\n",song_id);
  if(!drive_download_id(cfg,drive_id,path))return 0;
  if(verbose && stat(path,&st)==0)printf("stored: %s (%lld bytes)\n",path,(long long)st.st_size);
  return 1;
}

struct store_classified_item {
  long long song_id;
  char ref[VALUE_SIZE];
};

static int store_classified(const struct music_config *cfg) {
  sqlite3 *db;
  sqlite3_stmt *stmt;
  struct store_classified_item *items;
  struct store_classified_item *next;
  struct stat st;
  char path[VALUE_SIZE*2];
  long classified;
  long present;
  long downloaded;
  long failed;
  long count;
  long capacity;
  long i;
  int rc;

  if(!ensure_store(cfg)) {
    fprintf(stderr,"cannot access STORE %s\n",cfg->store);
    return 0;
  }
  db=db_open(cfg,0);
  if(db==NULL)return 0;
  stmt=NULL;
  if(sqlite3_prepare_v2(db,
      "SELECT s.id,src.ref FROM song s JOIN acr_result a ON a.song_id=s.id "
      "JOIN source src ON src.song_id=s.id AND src.type='drive' "
      "WHERE s.available=1 AND coalesce(a.acrid,'')<>'' "
      "AND s.id=(SELECT a2.song_id FROM acr_result a2 JOIN song s2 ON s2.id=a2.song_id "
      "WHERE a2.acrid=a.acrid AND s2.available=1 ORDER BY a2.first_seen,a2.song_id LIMIT 1) "
      "ORDER BY s.id",-1,&stmt,NULL)!=SQLITE_OK) {
    sqlite3_close(db);
    return 0;
  }
  items=NULL;
  count=0;
  capacity=0;
  for(;(rc=sqlite3_step(stmt))==SQLITE_ROW;) {
    if(count==capacity) {
      capacity=capacity==0 ? 256 : capacity*2;
      next=(struct store_classified_item *)realloc(items,(size_t)capacity*sizeof(*items));
      if(next==NULL) {
        free(items);
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return 0;
      }
      items=next;
    }
    items[count].song_id=sqlite3_column_int64(stmt,0);
    snprintf(items[count].ref,sizeof(items[count].ref),"%s",column_text(stmt,1));
    count++;
  }
  sqlite3_finalize(stmt);
  sqlite3_close(db);
  if(rc!=SQLITE_DONE) {
    free(items);
    return 0;
  }
  classified=count;
  present=0;
  downloaded=0;
  failed=0;
  store_interrupted=0;
  if(signal(SIGINT,store_sigint)==SIG_ERR) {
    free(items);
    return 0;
  }
  for(i=0;i<count && !store_interrupted;i++) {
    snprintf(path,sizeof(path),"%s/%lld.mp3",cfg->store,items[i].song_id);
    if(stat(path,&st)==0 && S_ISREG(st.st_mode) && st.st_size>0) {
      present++;
      continue;
    }
    printf("[%ld/%ld] SOURCE -> STORE: song %lld\n",present+downloaded+failed+1,classified,items[i].song_id);
    fflush(stdout);
    if(items[i].ref[0]!='\0' && drive_download_id(cfg,items[i].ref,path))downloaded++;
    else if(!store_interrupted) {
      fprintf(stderr,"store classified: cannot download song %lld\n",items[i].song_id);
      failed++;
    }
  }
  signal(SIGINT,SIG_DFL);
  free(items);
  if(store_interrupted) {
    fprintf(stderr,"store classified: interrupted; completed files kept, partial download removed\n");
    return 0;
  }
  printf("store classified: %ld classified, %ld already present, %ld downloaded, %ld failed\n",
         classified,present,downloaded,failed);
  return failed==0;
}

static int command_capture(const char *command,char **output) {
  FILE *fp;
  char buffer[4096];
  char *data;
  char *next;
  size_t len;
  size_t cap;
  size_t n;
  int rc;

  *output=NULL;
  fp=popen(command,"r");
  if(fp==NULL)return 0;
  cap=8192;
  len=0;
  data=(char *)malloc(cap);
  if(data==NULL) {
    pclose(fp);
    return 0;
  }
  for(;(n=fread(buffer,1,sizeof(buffer),fp))>0;) {
    if(len+n+1>cap) {
      for(;len+n+1>cap;cap*=2);
      next=(char *)realloc(data,cap);
      if(next==NULL) {
        free(data);
        pclose(fp);
        return 0;
      }
      data=next;
    }
    memcpy(data+len,buffer,n);
    len+=n;
  }
  data[len]='\0';
  rc=pclose(fp);
  if(rc!=0 || len==0) {
    free(data);
    return 0;
  }
  for(;len>0 && isspace((unsigned char)data[len-1]);len--)data[len-1]='\0';
  *output=data;
  return 1;
}

static int json_string_or_first_array(const char *value,const char *end,char *out,size_t size) {
  const char *p;

  if(size==0)return 0;
  out[0]='\0';
  if(value==NULL || value>=end)return 0;
  if(*value=='"')return json_value_string(value,end,out,size);
  if(*value!='[')return 0;
  for(p=value+1;p<end && isspace((unsigned char)*p);p++);
  if(p>=end || *p!='"')return 0;
  return json_value_string(p,end,out,size);
}

struct acr_metadata {
  char acrid[VALUE_SIZE];
  char artist[VALUE_SIZE];
  char title[VALUE_SIZE];
  char album[VALUE_SIZE];
  char release_date[128];
  char label[VALUE_SIZE];
  char isrc[256];
  char upc[256];
  char status[VALUE_SIZE];
  int status_code;
  int score;
};

static void acr_metadata_init(struct acr_metadata *m) {
  m->acrid[0]='\0';
  m->artist[0]='\0';
  m->title[0]='\0';
  m->album[0]='\0';
  m->release_date[0]='\0';
  m->label[0]='\0';
  m->isrc[0]='\0';
  m->upc[0]='\0';
  m->status[0]='\0';
  m->status_code=-1;
  m->score=0;
}

static void acr_artist_names(const char *array,const char *end,char *out,size_t size) {
  const char *array_end;
  const char *object_end;
  const char *p;
  char name[VALUE_SIZE];
  size_t len;

  if(size==0)return;
  out[0]='\0';
  if(array==NULL || array>=end || *array!='[')return;
  array_end=json_array_end(array,end);
  if(array_end==NULL)return;
  p=array+1;
  for(;p<array_end;) {
    for(;p<array_end && (isspace((unsigned char)*p) || *p==',');p++);
    if(p>=array_end)break;
    if(*p!='{') {
      p++;
      continue;
    }
    object_end=json_object_end(p);
    if(object_end==NULL || object_end>array_end)break;
    name[0]='\0';
    json_top_string(p,object_end,"name",name,sizeof(name));
    if(name[0]!='\0') {
      len=strlen(out);
      if(len>0 && len+3<size) {
        memcpy(out+len," & ",3);
        out[len+3]='\0';
        len+=3;
      }
      if(len+1<size)snprintf(out+len,size-len,"%s",name);
    }
    p=object_end+1;
  }
}

static int acr_parse_json(const char *json,struct acr_metadata *m) {
  const char *end;
  const char *status;
  const char *status_end;
  const char *metadata;
  const char *metadata_end;
  const char *music;
  const char *music_end;
  const char *track;
  const char *track_end;
  const char *value;
  const char *value_end;
  const char *p;
  double score_value;

  acr_metadata_init(m);
  if(json==NULL || *json=='\0')return 0;
  end=json+strlen(json);
  status=json_top_value(json,end,"status");
  if(status!=NULL && status<end && *status=='{') {
    double status_code;

    status_end=json_object_end(status);
    if(status_end!=NULL) {
      json_top_string(status,status_end,"msg",m->status,sizeof(m->status));
      status_code=0.0;
      if(json_top_number(status,status_end,"code",&status_code))m->status_code=(int)status_code;
    }
  }
  metadata=json_top_value(json,end,"metadata");
  if(metadata==NULL || metadata>=end || *metadata!='{')return 1;
  metadata_end=json_object_end(metadata);
  if(metadata_end==NULL)return 0;
  music=json_top_value(metadata,metadata_end,"music");
  if(music==NULL || music>=metadata_end || *music!='[')return 1;
  music_end=json_array_end(music,metadata_end);
  if(music_end==NULL)return 0;
  track=NULL;
  for(p=music+1;p<music_end && isspace((unsigned char)*p);p++);
  if(p<music_end && *p=='{')track=p;
  if(track==NULL)return 1;
  track_end=json_object_end(track);
  if(track_end==NULL)return 0;
  json_top_string(track,track_end,"acrid",m->acrid,sizeof(m->acrid));
  json_top_string(track,track_end,"title",m->title,sizeof(m->title));
  json_top_string(track,track_end,"release_date",m->release_date,sizeof(m->release_date));
  json_top_string(track,track_end,"label",m->label,sizeof(m->label));
  score_value=0.0;
  if(json_top_number(track,track_end,"score",&score_value))m->score=(int)(score_value*10000.0+0.5);
  value=json_top_value(track,track_end,"album");
  if(value!=NULL && value<track_end && *value=='{') {
    value_end=json_object_end(value);
    if(value_end!=NULL)json_top_string(value,value_end,"name",m->album,sizeof(m->album));
  }
  value=json_top_value(track,track_end,"artists");
  acr_artist_names(value,track_end,m->artist,sizeof(m->artist));
  value=json_top_value(track,track_end,"external_ids");
  if(value!=NULL && value<track_end && *value=='{') {
    value_end=json_object_end(value);
    if(value_end!=NULL) {
      json_string_or_first_array(json_top_value(value,value_end,"isrc"),value_end,m->isrc,sizeof(m->isrc));
      json_string_or_first_array(json_top_value(value,value_end,"upc"),value_end,m->upc,sizeof(m->upc));
    }
  }
  return 1;
}

static int acr_update_song(sqlite3 *db,long long song_id,const char *artist,const char *title,const char *album,const char *isrc) {
  sqlite3_stmt *stmt;
  int rc;

  stmt=NULL;
  if(sqlite3_prepare_v2(db,"UPDATE song SET artist=CASE WHEN ?1<>'' THEN ?1 ELSE artist END,title=CASE WHEN ?2<>'' THEN ?2 ELSE title END,album=CASE WHEN ?3<>'' THEN ?3 ELSE album END,isrc=CASE WHEN ?4<>'' THEN ?4 ELSE isrc END WHERE id=?5",-1,&stmt,NULL)!=SQLITE_OK)return 0;
  sqlite3_bind_text(stmt,1,artist,-1,(void (*)(void *))-1);
  sqlite3_bind_text(stmt,2,title,-1,(void (*)(void *))-1);
  sqlite3_bind_text(stmt,3,album,-1,(void (*)(void *))-1);
  sqlite3_bind_text(stmt,4,isrc,-1,(void (*)(void *))-1);
  sqlite3_bind_int64(stmt,5,song_id);
  rc=sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  return rc==SQLITE_DONE;
}

static int save_acr_result(const struct music_config *cfg,long long song_id,int clip_start,int clip_seconds,const char *acrid,const char *artist,const char *title,const char *album,const char *release_date,const char *label,const char *isrc,const char *upc,int score,const char *raw_json) {
  sqlite3 *db;
  sqlite3_stmt *stmt;
  int rc;

  db=db_open(cfg,1);
  if(db==NULL)return 0;
  stmt=NULL;
  if(sqlite3_prepare_v2(db,"INSERT INTO acr_result(song_id,clip_start,clip_seconds,acrid,artist,title,album,release_date,label,isrc,upc,score,raw_json,first_seen,updated) VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12,?13,unixepoch(),unixepoch()) ON CONFLICT(song_id) DO UPDATE SET clip_start=excluded.clip_start,clip_seconds=excluded.clip_seconds,acrid=excluded.acrid,artist=excluded.artist,title=excluded.title,album=excluded.album,release_date=excluded.release_date,label=excluded.label,isrc=excluded.isrc,upc=excluded.upc,score=excluded.score,raw_json=excluded.raw_json,first_seen=CASE WHEN acr_result.acrid=excluded.acrid THEN acr_result.first_seen ELSE excluded.first_seen END,updated=excluded.updated",-1,&stmt,NULL)!=SQLITE_OK) {
    sqlite3_close(db);
    return 0;
  }
  sqlite3_bind_int64(stmt,1,song_id);
  sqlite3_bind_int(stmt,2,clip_start);
  sqlite3_bind_int(stmt,3,clip_seconds);
  sqlite3_bind_text(stmt,4,acrid,-1,(void (*)(void *))-1);
  sqlite3_bind_text(stmt,5,artist,-1,(void (*)(void *))-1);
  sqlite3_bind_text(stmt,6,title,-1,(void (*)(void *))-1);
  sqlite3_bind_text(stmt,7,album,-1,(void (*)(void *))-1);
  sqlite3_bind_text(stmt,8,release_date,-1,(void (*)(void *))-1);
  sqlite3_bind_text(stmt,9,label,-1,(void (*)(void *))-1);
  sqlite3_bind_text(stmt,10,isrc,-1,(void (*)(void *))-1);
  sqlite3_bind_text(stmt,11,upc,-1,(void (*)(void *))-1);
  sqlite3_bind_int(stmt,12,score);
  sqlite3_bind_text(stmt,13,raw_json,-1,(void (*)(void *))-1);
  rc=sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  if(rc==SQLITE_DONE && !acr_update_song(db,song_id,artist,title,album,isrc))rc=SQLITE_OK;
  sqlite3_close(db);
  return rc==SQLITE_DONE;
}

static int acr_cached(const struct music_config *cfg,long long song_id,int show) {
  sqlite3 *db;
  sqlite3_stmt *stmt;
  const unsigned char *acrid;
  const unsigned char *artist;
  const unsigned char *title;
  const unsigned char *album;
  const unsigned char *release_date;
  const unsigned char *label;
  const unsigned char *isrc;
  const unsigned char *upc;
  int clip_start;
  int clip_seconds;
  int score;
  int found;

  db=db_open(cfg,0);
  if(db==NULL)return 0;
  stmt=NULL;
  found=0;
  if(sqlite3_prepare_v2(db,"SELECT clip_start,clip_seconds,acrid,artist,title,album,release_date,label,isrc,upc,score FROM acr_result WHERE song_id=?1",-1,&stmt,NULL)==SQLITE_OK) {
    sqlite3_bind_int64(stmt,1,song_id);
    if(sqlite3_step(stmt)==SQLITE_ROW) {
      found=1;
      if(show) {
        clip_start=sqlite3_column_int(stmt,0);
        clip_seconds=sqlite3_column_int(stmt,1);
        acrid=sqlite3_column_text(stmt,2);
        artist=sqlite3_column_text(stmt,3);
        title=sqlite3_column_text(stmt,4);
        album=sqlite3_column_text(stmt,5);
        release_date=sqlite3_column_text(stmt,6);
        label=sqlite3_column_text(stmt,7);
        isrc=sqlite3_column_text(stmt,8);
        upc=sqlite3_column_text(stmt,9);
        score=sqlite3_column_int(stmt,10);
        printf("song %lld (cached)\n",song_id);
        printf("  clip:       %d-%d s\n",clip_start,clip_start+clip_seconds);
        if((artist==NULL || *artist=='\0') && (title==NULL || *title=='\0'))printf("  result:     none\n");
        else {
          printf("  identified: %s | %s\n",artist==NULL ? (const unsigned char *)"" : artist,title==NULL ? (const unsigned char *)"" : title);
          if(album!=NULL && *album!='\0')printf("  album:      %s\n",album);
          if(release_date!=NULL && *release_date!='\0')printf("  release:    %s\n",release_date);
          if(label!=NULL && *label!='\0')printf("  label:      %s\n",label);
          if(isrc!=NULL && *isrc!='\0')printf("  ISRC:       %s\n",isrc);
          if(upc!=NULL && *upc!='\0')printf("  UPC:        %s\n",upc);
          if(acrid!=NULL && *acrid!='\0')printf("  ACRID:      %s\n",acrid);
          if(score>0)printf("  score:      %.1f\n",(double)score/10000.0);
        }
        printf("  raw JSON:   saved\n");
      }
    }
  }
  if(stmt!=NULL)sqlite3_finalize(stmt);
  sqlite3_close(db);
  return found;
}

static int acr_signature(const struct music_config *cfg,const char *timestamp,char *signature,size_t size) {
  unsigned char digest[EVP_MAX_MD_SIZE];
  unsigned int digest_len;
  char text[VALUE_SIZE*2];
  int encoded;

  snprintf(text,sizeof(text),"POST\n/v1/identify\n%s\naudio\n1\n%s",cfg->acr_access_key,timestamp);
  digest_len=0;
  if(HMAC(EVP_sha1(),cfg->acr_access_secret,(int)strlen(cfg->acr_access_secret),(const unsigned char *)text,strlen(text),digest,&digest_len)==NULL)return 0;
  if(size<(size_t)(4*((digest_len+2)/3)+1))return 0;
  encoded=EVP_EncodeBlock((unsigned char *)signature,digest,(int)digest_len);
  if(encoded<=0 || (size_t)encoded>=size)return 0;
  signature[encoded]='\0';
  return 1;
}

static int acr_failure_info(const struct music_config *cfg,long long song_id,int *attempts,int *no_match_count,int *terminal) {
  sqlite3 *db;
  sqlite3_stmt *stmt;
  int found;

  *attempts=0;
  *no_match_count=0;
  *terminal=0;
  db=db_open(cfg,0);
  if(db==NULL)return 0;
  stmt=NULL;
  found=0;
  if(sqlite3_prepare_v2(db,"SELECT attempts,no_match_count,terminal FROM acr_failure WHERE song_id=?1",-1,&stmt,NULL)==SQLITE_OK) {
    sqlite3_bind_int64(stmt,1,song_id);
    if(sqlite3_step(stmt)==SQLITE_ROW) {
      *attempts=sqlite3_column_int(stmt,0);
      *no_match_count=sqlite3_column_int(stmt,1);
      *terminal=sqlite3_column_int(stmt,2);
      found=1;
    }
  }
  if(stmt!=NULL)sqlite3_finalize(stmt);
  sqlite3_close(db);
  return found;
}

static int acr_failure_save(const struct music_config *cfg,long long song_id,int status_code,int clip_start) {
  sqlite3 *db;
  sqlite3_stmt *stmt;
  int attempts;
  int no_match_count;
  int terminal;
  int found;
  int rc;

  attempts=0;
  no_match_count=0;
  terminal=0;
  found=acr_failure_info(cfg,song_id,&attempts,&no_match_count,&terminal);
  attempts++;
  if(status_code==1001)no_match_count++;
  terminal=no_match_count>=2 ? 1 : 0;
  db=db_open(cfg,1);
  if(db==NULL)return 0;
  stmt=NULL;
  if(found) {
    if(sqlite3_prepare_v2(db,"UPDATE acr_failure SET status_code=?1,attempts=?2,no_match_count=?3,last_clip_start=?4,terminal=?5,updated=unixepoch() WHERE song_id=?6",-1,&stmt,NULL)!=SQLITE_OK) {
      sqlite3_close(db);
      return 0;
    }
    sqlite3_bind_int(stmt,1,status_code);
    sqlite3_bind_int(stmt,2,attempts);
    sqlite3_bind_int(stmt,3,no_match_count);
    sqlite3_bind_int(stmt,4,clip_start);
    sqlite3_bind_int(stmt,5,terminal);
    sqlite3_bind_int64(stmt,6,song_id);
  } else {
    if(sqlite3_prepare_v2(db,"INSERT INTO acr_failure(song_id,status_code,attempts,no_match_count,last_clip_start,terminal) VALUES(?1,?2,?3,?4,?5,?6)",-1,&stmt,NULL)!=SQLITE_OK) {
      sqlite3_close(db);
      return 0;
    }
    sqlite3_bind_int64(stmt,1,song_id);
    sqlite3_bind_int(stmt,2,status_code);
    sqlite3_bind_int(stmt,3,attempts);
    sqlite3_bind_int(stmt,4,no_match_count);
    sqlite3_bind_int(stmt,5,clip_start);
    sqlite3_bind_int(stmt,6,terminal);
  }
  rc=sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  sqlite3_close(db);
  return rc==SQLITE_DONE;
}

static void acr_failure_clear(const struct music_config *cfg,long long song_id) {
  sqlite3 *db;
  sqlite3_stmt *stmt;

  db=db_open(cfg,1);
  if(db==NULL)return;
  stmt=NULL;
  if(sqlite3_prepare_v2(db,"DELETE FROM acr_failure WHERE song_id=?1",-1,&stmt,NULL)==SQLITE_OK) {
    sqlite3_bind_int64(stmt,1,song_id);
    sqlite3_step(stmt);
  }
  if(stmt!=NULL)sqlite3_finalize(stmt);
  sqlite3_close(db);
}

static int acr_clip_start(double seconds,int attempt) {
  int start;

  if(seconds<=10.0)return 0;
  if(attempt<=0) {
    if(seconds>90.0)return 60;
    if(seconds>20.0)return (int)(seconds/2.0)-5;
    return 0;
  }
  if(attempt%3==1)start=seconds>20.0 ? 5 : (int)seconds-10;
  else if(attempt%3==2)start=(int)seconds-10;
  else start=(int)(seconds/3.0)-5;
  if(start<0)start=0;
  if((double)start+10.0>seconds)start=(int)seconds-10;
  if(start<0)start=0;
  return start;
}

static int acr_song(const struct music_config *cfg,long long song_id,int force) {
  CURL *curl;
  curl_mime *mime;
  curl_mimepart *part;
  CURLcode rc;
  struct mem body;
  struct stat st;
  char path[VALUE_SIZE*2];
  char clip[VALUE_SIZE*2];
  char command[VALUE_SIZE*5];
  char url[VALUE_SIZE*2];
  char signature[256];
  char timestamp[64];
  char sample_bytes[64];
  char *duration_text;
  struct acr_metadata meta;
  double seconds;
  long http;
  int clip_start;
  int clip_seconds;
  int ok;
  int attempt;
  int transient;
  int failure_attempts;
  int failure_no_match;
  int failure_terminal;

  if(!force && acr_cached(cfg,song_id,1))return 1;
  failure_attempts=0;
  failure_no_match=0;
  failure_terminal=0;
  acr_failure_info(cfg,song_id,&failure_attempts,&failure_no_match,&failure_terminal);
  (void)failure_terminal;
  if(cfg->acr_host[0]=='\0' || cfg->acr_access_key[0]=='\0' || cfg->acr_access_secret[0]=='\0') {
    fprintf(stderr,"acr: credentials required; set acr_host, acr_access_key and acr_access_secret in config\n");
    return 0;
  }
  if(!store_song(cfg,song_id,path,sizeof(path),0)) {
    fprintf(stderr,"acr: cannot access song %lld\n",song_id);
    return 0;
  }
  snprintf(command,sizeof(command),"ffprobe -v error -show_entries format=duration -of default=nw=1:nk=1 \"%s\" 2>/dev/null",path);
  if(!command_capture(command,&duration_text))return 0;
  seconds=atof(duration_text);
  free(duration_text);
  if(seconds<=0.0)return 0;
  clip_seconds=10;
  clip_start=acr_clip_start(seconds,failure_attempts);
  snprintf(clip,sizeof(clip),"%s/.acr.%ld.mp3",cfg->store,(long)getpid());
  snprintf(command,sizeof(command),"ffmpeg -v error -ss %d -t %d -i \"%s\" -vn -ac 1 -ar 44100 -b:a 128k -y \"%s\" 2>/dev/null",clip_start,clip_seconds,path,clip);
  if(system(command)!=0 || stat(clip,&st)!=0) {
    remove(clip);
    fprintf(stderr,"acr: cannot create clip\n");
    return 0;
  }
  snprintf(timestamp,sizeof(timestamp),"%ld",(long)time(NULL));
  snprintf(sample_bytes,sizeof(sample_bytes),"%lld",(long long)st.st_size);
  if(!acr_signature(cfg,timestamp,signature,sizeof(signature))) {
    remove(clip);
    fprintf(stderr,"acr: cannot sign request\n");
    return 0;
  }
  if(strncmp(cfg->acr_host,"https://",8)==0 || strncmp(cfg->acr_host,"http://",7)==0)snprintf(url,sizeof(url),"%s/v1/identify",cfg->acr_host);
  else snprintf(url,sizeof(url),"https://%s/v1/identify",cfg->acr_host);
  mem_init(&body);
  if(body.ptr==NULL) {
    remove(clip);
    return 0;
  }
  if(curl_global_init(CURL_GLOBAL_DEFAULT)!=0) {
    free(body.ptr);
    remove(clip);
    return 0;
  }
  curl=curl_easy_init();
  if(curl==NULL) {
    curl_global_cleanup();
    free(body.ptr);
    remove(clip);
    return 0;
  }
  mime=curl_mime_init(curl);
  part=curl_mime_addpart(mime);
  curl_mime_name(part,"access_key");
  curl_mime_data(part,cfg->acr_access_key,CURL_ZERO_TERMINATED);
  part=curl_mime_addpart(mime);
  curl_mime_name(part,"sample_bytes");
  curl_mime_data(part,sample_bytes,CURL_ZERO_TERMINATED);
  part=curl_mime_addpart(mime);
  curl_mime_name(part,"timestamp");
  curl_mime_data(part,timestamp,CURL_ZERO_TERMINATED);
  part=curl_mime_addpart(mime);
  curl_mime_name(part,"signature");
  curl_mime_data(part,signature,CURL_ZERO_TERMINATED);
  part=curl_mime_addpart(mime);
  curl_mime_name(part,"data_type");
  curl_mime_data(part,"audio",CURL_ZERO_TERMINATED);
  part=curl_mime_addpart(mime);
  curl_mime_name(part,"signature_version");
  curl_mime_data(part,"1",CURL_ZERO_TERMINATED);
  part=curl_mime_addpart(mime);
  curl_mime_name(part,"sample");
  curl_mime_filedata(part,clip);
  curl_easy_setopt(curl,CURLOPT_URL,url);
  curl_easy_setopt(curl,CURLOPT_MIMEPOST,mime);
  curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,drive_write_cb);
  curl_easy_setopt(curl,CURLOPT_WRITEDATA,&body);
  curl_easy_setopt(curl,CURLOPT_SSL_VERIFYPEER,1L);
  curl_easy_setopt(curl,CURLOPT_SSL_VERIFYHOST,2L);
  curl_easy_setopt(curl,CURLOPT_CONNECTTIMEOUT,15L);
  curl_easy_setopt(curl,CURLOPT_TIMEOUT,120L);
  curl_easy_setopt(curl,CURLOPT_USERAGENT,"music/" MUSIC_VERSION);
  rc=CURLE_OK;
  http=0;
  for(attempt=0;attempt<3;attempt++) {
    body.len=0;
    body.ptr[0]='\0';
    rc=curl_easy_perform(curl);
    http=0;
    if(rc==CURLE_OK)curl_easy_getinfo(curl,CURLINFO_RESPONSE_CODE,&http);
    if(rc==CURLE_OK && http>=200 && http<300)break;
    transient=rc!=CURLE_OK || http==408 || http==425 || http==429 || (http>=500 && http<600);
    if(!transient || attempt==2)break;
    fprintf(stderr,"acr: transient HTTP %ld, retry %d/2\n",http,attempt+1);
    sleep((unsigned int)(attempt+1));
  }
  curl_mime_free(mime);
  curl_easy_cleanup(curl);
  curl_global_cleanup();
  remove(clip);
  if(rc!=CURLE_OK || http<200 || http>=300) {
    fprintf(stderr,"acr: HTTP %ld, request failed\n",http);
    free(body.ptr);
    return 0;
  }
  if(!acr_parse_json(body.ptr,&meta)) {
    fprintf(stderr,"acr: invalid JSON response\n");
    free(body.ptr);
    return 0;
  }
  if(meta.status_code!=0) {
    fprintf(stderr,"acr: status %d%s%s\n",meta.status_code,meta.status[0]!='\0' ? ": " : "",meta.status);
    if(meta.status_code==1001 || meta.status_code==2004) {
      if(!acr_failure_save(cfg,song_id,meta.status_code,clip_start))fprintf(stderr,"acr: cannot save failure state\n");
      else if(meta.status_code==1001 && failure_no_match>=1)fprintf(stderr,"acr: NO MATCH after two attempts; excluded from normal random batches\n");
      else if(meta.status_code==1001)fprintf(stderr,"acr: no result recorded; next attempt will use another clip\n");
      else fprintf(stderr,"acr: fingerprint failure recorded; next attempt will use another clip\n");
    }
    free(body.ptr);
    return meta.status_code==3003 ? -1 : 0;
  }
  ok=save_acr_result(cfg,song_id,clip_start,clip_seconds,meta.acrid,meta.artist,meta.title,meta.album,meta.release_date,meta.label,meta.isrc,meta.upc,meta.score,body.ptr);
  if(!ok) {
    fprintf(stderr,"acr: cannot save result\n");
    free(body.ptr);
    return 0;
  }
  printf("song %lld\n",song_id);
  printf("  clip:       %d-%d s\n",clip_start,clip_start+clip_seconds);
  if(meta.title[0]=='\0' && meta.artist[0]=='\0') {
    printf("  result:     none\n");
    if(meta.status[0]!='\0')printf("  status:     %s\n",meta.status);
  } else {
    printf("  identified: %s | %s\n",meta.artist,meta.title);
    if(meta.album[0]!='\0')printf("  album:      %s\n",meta.album);
    if(meta.release_date[0]!='\0')printf("  release:    %s\n",meta.release_date);
    if(meta.label[0]!='\0')printf("  label:      %s\n",meta.label);
    if(meta.isrc[0]!='\0')printf("  ISRC:       %s\n",meta.isrc);
    if(meta.upc[0]!='\0')printf("  UPC:        %s\n",meta.upc);
    if(meta.acrid[0]!='\0')printf("  ACRID:      %s\n",meta.acrid);
    if(meta.score>0)printf("  score:      %.1f\n",(double)meta.score/10000.0);
  }
  printf("  raw JSON:   saved\n");
  acr_failure_clear(cfg,song_id);
  free(body.ptr);
  return 1;
}

static int acr_sync(const struct music_config *cfg) {
  sqlite3 *db;
  sqlite3_stmt *ids;
  sqlite3_stmt *read_stmt;
  sqlite3_stmt *update_stmt;
  sqlite3_stmt *song_stmt;
  sqlite3_stmt *delete_stmt;
  struct acr_metadata meta;
  long long *song_ids;
  long long song_id;
  const unsigned char *raw;
  char *json;
  int clip_start;
  int clip_seconds;
  int count;
  int i;
  int rc;
  int parsed;
  int matched;
  int invalid;
  int ok;

  db=db_open(cfg,1);
  if(db==NULL)return 0;
  count=(int)db_scalar(db,"SELECT count(*) FROM acr_result");
  song_ids=NULL;
  if(count>0) {
    song_ids=(long long *)malloc((size_t)count*sizeof(long long));
    if(song_ids==NULL) {
      sqlite3_close(db);
      return 0;
    }
  }
  ids=NULL;
  i=0;
  if(sqlite3_prepare_v2(db,"SELECT song_id FROM acr_result ORDER BY song_id",-1,&ids,NULL)!=SQLITE_OK) {
    free(song_ids);
    sqlite3_close(db);
    return 0;
  }
  for(;(rc=sqlite3_step(ids))==SQLITE_ROW && i<count;i++)song_ids[i]=sqlite3_column_int64(ids,0);
  sqlite3_finalize(ids);
  count=i;
  if(!db_exec(db,"BEGIN IMMEDIATE")) {
    free(song_ids);
    sqlite3_close(db);
    return 0;
  }
  ok=db_exec(db,"UPDATE source SET original_title=substr(original_title,1,length(original_title)-4) WHERE type='drive' AND lower(substr(original_title,-4))='.mp3'");
  if(ok)ok=db_exec(db,"UPDATE song SET artist=COALESCE((SELECT original_artist FROM source WHERE source.song_id=song.id AND type='drive' LIMIT 1),artist),title=COALESCE((SELECT original_title FROM source WHERE source.song_id=song.id AND type='drive' LIMIT 1),title),album=COALESCE((SELECT original_album FROM source WHERE source.song_id=song.id AND type='drive' LIMIT 1),album),isrc=NULL");
  read_stmt=NULL;
  update_stmt=NULL;
  song_stmt=NULL;
  delete_stmt=NULL;
  if(ok && sqlite3_prepare_v2(db,"SELECT clip_start,clip_seconds,raw_json FROM acr_result WHERE song_id=?1",-1,&read_stmt,NULL)!=SQLITE_OK)ok=0;
  if(ok && sqlite3_prepare_v2(db,"UPDATE acr_result SET acrid=?1,artist=?2,title=?3,album=?4,release_date=?5,label=?6,isrc=?7,upc=?8,score=?9 WHERE song_id=?10",-1,&update_stmt,NULL)!=SQLITE_OK)ok=0;
  if(ok && sqlite3_prepare_v2(db,"UPDATE song SET artist=CASE WHEN ?1<>'' THEN ?1 ELSE artist END,title=CASE WHEN ?2<>'' THEN ?2 ELSE title END,album=CASE WHEN ?3<>'' THEN ?3 ELSE album END,isrc=CASE WHEN ?4<>'' THEN ?4 ELSE isrc END WHERE id=?5",-1,&song_stmt,NULL)!=SQLITE_OK)ok=0;
  if(ok && sqlite3_prepare_v2(db,"DELETE FROM acr_result WHERE song_id=?1",-1,&delete_stmt,NULL)!=SQLITE_OK)ok=0;
  parsed=0;
  matched=0;
  invalid=0;
  for(i=0;ok && i<count;i++) {
    song_id=song_ids[i];
    sqlite3_reset(read_stmt);
    sqlite3_bind_int64(read_stmt,1,song_id);
    if(sqlite3_step(read_stmt)!=SQLITE_ROW) {
      invalid++;
      continue;
    }
    clip_start=sqlite3_column_int(read_stmt,0);
    clip_seconds=sqlite3_column_int(read_stmt,1);
    raw=sqlite3_column_text(read_stmt,2);
    json=raw==NULL ? NULL : strdup((const char *)raw);
    sqlite3_reset(read_stmt);
    if(json==NULL || !acr_parse_json(json,&meta)) {
      free(json);
      invalid++;
      continue;
    }
    free(json);
    if(meta.status_code!=0) {
      sqlite3_reset(delete_stmt);
      sqlite3_bind_int64(delete_stmt,1,song_id);
      if(sqlite3_step(delete_stmt)!=SQLITE_DONE) {
        ok=0;
        break;
      }
      invalid++;
      continue;
    }
    parsed++;
    if(meta.artist[0]!='\0' || meta.title[0]!='\0')matched++;
    sqlite3_reset(update_stmt);
    sqlite3_bind_text(update_stmt,1,meta.acrid,-1,(void (*)(void *))-1);
    sqlite3_bind_text(update_stmt,2,meta.artist,-1,(void (*)(void *))-1);
    sqlite3_bind_text(update_stmt,3,meta.title,-1,(void (*)(void *))-1);
    sqlite3_bind_text(update_stmt,4,meta.album,-1,(void (*)(void *))-1);
    sqlite3_bind_text(update_stmt,5,meta.release_date,-1,(void (*)(void *))-1);
    sqlite3_bind_text(update_stmt,6,meta.label,-1,(void (*)(void *))-1);
    sqlite3_bind_text(update_stmt,7,meta.isrc,-1,(void (*)(void *))-1);
    sqlite3_bind_text(update_stmt,8,meta.upc,-1,(void (*)(void *))-1);
    sqlite3_bind_int(update_stmt,9,meta.score);
    sqlite3_bind_int64(update_stmt,10,song_id);
    if(sqlite3_step(update_stmt)!=SQLITE_DONE) {
      ok=0;
      break;
    }
    if(meta.artist[0]=='\0' && meta.title[0]=='\0' && meta.album[0]=='\0' && meta.isrc[0]=='\0')continue;
    sqlite3_reset(song_stmt);
    sqlite3_bind_text(song_stmt,1,meta.artist,-1,(void (*)(void *))-1);
    sqlite3_bind_text(song_stmt,2,meta.title,-1,(void (*)(void *))-1);
    sqlite3_bind_text(song_stmt,3,meta.album,-1,(void (*)(void *))-1);
    sqlite3_bind_text(song_stmt,4,meta.isrc,-1,(void (*)(void *))-1);
    sqlite3_bind_int64(song_stmt,5,song_id);
    if(sqlite3_step(song_stmt)!=SQLITE_DONE) {
      ok=0;
      break;
    }
    (void)clip_start;
    (void)clip_seconds;
  }
  if(read_stmt!=NULL)sqlite3_finalize(read_stmt);
  if(update_stmt!=NULL)sqlite3_finalize(update_stmt);
  if(song_stmt!=NULL)sqlite3_finalize(song_stmt);
  if(delete_stmt!=NULL)sqlite3_finalize(delete_stmt);
  if(ok)ok=db_exec(db,"COMMIT");
  else db_exec(db,"ROLLBACK");
  sqlite3_close(db);
  free(song_ids);
  if(!ok)return 0;
  printf("acr sync: %d JSON parsed, %d matched, %d invalid; catalog updated\n",parsed,matched,invalid);
  return 1;
}

static int acr_random(const struct music_config *cfg,int count) {
  sqlite3 *db;
  sqlite3_stmt *stmt;
  long long *songs;
  int n;
  int rc;
  int ok;
  int failed;

  if(count<=0) {
    fprintf(stderr,"acr: random count must be positive\n");
    return 0;
  }
  songs=(long long *)malloc((size_t)count*sizeof(long long));
  if(songs==NULL)return 0;
  db=db_open(cfg,0);
  if(db==NULL) {
    free(songs);
    return 0;
  }
  stmt=NULL;
  if(sqlite3_prepare_v2(db,"SELECT s.id FROM song s WHERE s.available=1 AND NOT EXISTS(SELECT 1 FROM acr_result a WHERE a.song_id=s.id) AND NOT EXISTS(SELECT 1 FROM acr_failure f WHERE f.song_id=s.id AND f.terminal=1) ORDER BY random() LIMIT ?1",-1,&stmt,NULL)!=SQLITE_OK) {
    sqlite3_close(db);
    free(songs);
    return 0;
  }
  sqlite3_bind_int(stmt,1,count);
  n=0;
  for(;n<count;) {
    rc=sqlite3_step(stmt);
    if(rc!=SQLITE_ROW)break;
    songs[n++]=sqlite3_column_int64(stmt,0);
  }
  sqlite3_finalize(stmt);
  sqlite3_close(db);
  if(n==0) {
    printf("acr: no unrecognized songs remain\n");
    free(songs);
    return 1;
  }
  printf("acr: selected %d random song%s not already in database\n",n,n==1 ? "" : "s");
  ok=0;
  failed=0;
  for(rc=0;rc<n;rc++) {
    int result;

    printf("\n[%d/%d] ",rc+1,n);
    fflush(stdout);
    result=acr_song(cfg,songs[rc],0);
    if(result>0)ok++;
    else {
      failed++;
      if(result<0) {
        printf("\nacr: request limit reached; batch stopped\n");
        break;
      }
    }
  }
  printf("\nacr: completed %d, failed %d\n",ok,failed);
  free(songs);
  return failed==0;
}

static int acr_retry_nomatch(const struct music_config *cfg,int count) {
  sqlite3 *db;
  sqlite3_stmt *stmt;
  long long *songs;
  int n;
  int rc;
  int ok;
  int failed;

  if(count<=0) {
    fprintf(stderr,"acr: retry count must be positive\n");
    return 0;
  }
  songs=(long long *)malloc((size_t)count*sizeof(long long));
  if(songs==NULL)return 0;
  db=db_open(cfg,0);
  if(db==NULL) {
    free(songs);
    return 0;
  }
  stmt=NULL;
  if(sqlite3_prepare_v2(db,"SELECT s.id FROM song s JOIN acr_failure f ON f.song_id=s.id WHERE s.available=1 AND f.terminal=1 AND NOT EXISTS(SELECT 1 FROM acr_result a WHERE a.song_id=s.id) ORDER BY f.updated,s.id LIMIT ?1",-1,&stmt,NULL)!=SQLITE_OK) {
    sqlite3_close(db);
    free(songs);
    return 0;
  }
  sqlite3_bind_int(stmt,1,count);
  n=0;
  for(;n<count;) {
    rc=sqlite3_step(stmt);
    if(rc!=SQLITE_ROW)break;
    songs[n++]=sqlite3_column_int64(stmt,0);
  }
  sqlite3_finalize(stmt);
  sqlite3_close(db);
  if(n==0) {
    printf("acr: no NO MATCH songs to retry\n");
    free(songs);
    return 1;
  }
  printf("acr: retrying %d NO MATCH song%s\n",n,n==1 ? "" : "s");
  ok=0;
  failed=0;
  for(rc=0;rc<n;rc++) {
    int result;

    printf("\n[%d/%d] ",rc+1,n);
    fflush(stdout);
    result=acr_song(cfg,songs[rc],0);
    if(result>0)ok++;
    else {
      failed++;
      if(result<0) {
        printf("\nacr: request limit reached; retry stopped\n");
        break;
      }
    }
  }
  printf("\nacr retry: completed %d, failed %d\n",ok,failed);
  free(songs);
  return failed==0;
}

static void media_cgi(const struct music_config *cfg,long long song_id) {
  FILE *fp;
  struct stat st;
  const char *range;
  char path[VALUE_SIZE*2];
  char buf[65536];
  long long first;
  long long last;
  long long remain;
  long long size;
  size_t want;
  size_t n;
  int partial;

  if(!store_song(cfg,song_id,path,sizeof(path),0) || stat(path,&st)!=0) {
    printf("Status: 404 Not Found\r\nContent-Type: text/plain\r\n\r\nTrack unavailable\n");
    return;
  }
  size=(long long)st.st_size;
  first=0;
  last=size-1;
  partial=0;
  range=getenv("HTTP_RANGE");
  if(range!=NULL && strncmp(range,"bytes=",6)==0) {
    if(sscanf(range+6,"%lld-%lld",&first,&last)==2)partial=1;
    else if(sscanf(range+6,"%lld-",&first)==1) {
      last=size-1;
      partial=1;
    }
    if(first<0 || first>=size || last<first) {
      printf("Status: 416 Range Not Satisfiable\r\n");
      printf("Content-Range: bytes */%lld\r\n\r\n",size);
      return;
    }
    if(last>=size)last=size-1;
  }
  fp=fopen(path,"rb");
  if(fp==NULL) {
    printf("Status: 500 Internal Server Error\r\nContent-Type: text/plain\r\n\r\nCannot open STORE file\n");
    return;
  }
  if(first>0 && fseek(fp,(long)first,SEEK_SET)!=0) {
    fclose(fp);
    printf("Status: 500 Internal Server Error\r\nContent-Type: text/plain\r\n\r\nCannot seek STORE file\n");
    return;
  }
  if(partial) {
    printf("Status: 206 Partial Content\r\n");
    printf("Content-Range: bytes %lld-%lld/%lld\r\n",first,last,size);
  }
  printf("Content-Type: audio/mpeg\r\n");
  printf("Accept-Ranges: bytes\r\n");
  printf("Content-Length: %lld\r\n",last-first+1);
  printf("Cache-Control: private, max-age=86400\r\n");
  printf("X-Content-Type-Options: nosniff\r\n\r\n");
  remain=last-first+1;
  for(;remain>0;) {
    want=remain<(long long)sizeof(buf) ? (size_t)remain : sizeof(buf);
    n=fread(buf,1,want,fp);
    if(n==0)break;
    fwrite(buf,1,n,stdout);
    remain-=(long long)n;
  }
  fclose(fp);
}

static void activity_cgi(const struct music_config *cfg,long long user_id,long long song_id,const char *event,int duration) {
  sqlite3 *db;
  sqlite3_stmt *stmt;
  int changed;
  int rc;
  int ok;

  if(song_id<=0 || event==NULL) {
    printf("Status: 400 Bad Request\r\nContent-Type: text/plain\r\n\r\nBad request\n");
    return;
  }
  if(strcmp(event,"played")!=0 && !(strcmp(event,"duration")==0 && duration>0 && duration<=86400)) {
    printf("Status: 400 Bad Request\r\nContent-Type: text/plain\r\n\r\nBad request\n");
    return;
  }
  db=db_open_web_write(cfg);
  if(db==NULL) {
    printf("Status: 500 Internal Server Error\r\nContent-Type: text/plain\r\n\r\nCatalog unavailable\n");
    return;
  }
  ok=1;
  rc=SQLITE_OK;
  changed=0;
  if(strcmp(event,"played")==0) {
    ok=db_exec(db,"BEGIN IMMEDIATE");
    stmt=NULL;
    if(ok)rc=sqlite3_prepare_v2(db,"UPDATE song SET played=played+1,last_played=unixepoch() WHERE id=?1 AND available=1",-1,&stmt,NULL);
    if(ok && rc==SQLITE_OK) {
      sqlite3_bind_int64(stmt,1,song_id);
      rc=sqlite3_step(stmt);
      if(rc==SQLITE_DONE)changed=sqlite3_changes(db);
    }
    if(stmt!=NULL)sqlite3_finalize(stmt);
    stmt=NULL;
    if(ok && rc==SQLITE_DONE && changed>0 && user_id>0) {
      rc=sqlite3_prepare_v2(db,"INSERT INTO user_stats(user_id,played,last_played,last_seen) VALUES(?1,1,unixepoch(),unixepoch()) ON CONFLICT(user_id) DO UPDATE SET played=user_stats.played+1,last_played=unixepoch(),last_seen=unixepoch()",-1,&stmt,NULL);
      if(rc==SQLITE_OK) {
        sqlite3_bind_int64(stmt,1,user_id);
        rc=sqlite3_step(stmt);
      }
    }
    if(stmt!=NULL)sqlite3_finalize(stmt);
    if(ok && rc==SQLITE_DONE)ok=db_exec(db,"COMMIT");
    else {
      if(ok)db_exec(db,"ROLLBACK");
      ok=0;
    }
  } else {
    stmt=NULL;
    rc=sqlite3_prepare_v2(db,"UPDATE song SET duration=?1 WHERE id=?2 AND available=1 AND duration=0",-1,&stmt,NULL);
    if(rc==SQLITE_OK) {
      sqlite3_bind_int(stmt,1,duration);
      sqlite3_bind_int64(stmt,2,song_id);
      rc=sqlite3_step(stmt);
    }
    if(stmt!=NULL)sqlite3_finalize(stmt);
    ok=rc==SQLITE_DONE;
  }
  sqlite3_close(db);
  if(!ok) {
    printf("Status: 500 Internal Server Error\r\nContent-Type: text/plain\r\n\r\nCatalog update failed\n");
    return;
  }
  printf("Status: 204 No Content\r\nCache-Control: no-store\r\n\r\n");
}

static void html_text(const char *s) {
  const unsigned char *p;

  if(s==NULL)return;
  p=(const unsigned char *)s;
  for(;*p!='\0';p++) {
    if(*p=='&')fputs("&amp;",stdout);
    else if(*p=='<')fputs("&lt;",stdout);
    else if(*p=='>')fputs("&gt;",stdout);
    else if(*p=='\"')fputs("&quot;",stdout);
    else if(*p=='\'')fputs("&#39;",stdout);
    else fputc(*p,stdout);
  }
}

static int hex_value(int c) {
  if(c>='0' && c<='9')return c-'0';
  if(c>='a' && c<='f')return c-'a'+10;
  if(c>='A' && c<='F')return c-'A'+10;
  return -1;
}

static void url_decode(char *dst,size_t size,const char *src) {
  size_t i;
  int hi;
  int lo;

  i=0;
  for(;*src!='\0' && i+1<size;src++) {
    if(*src=='+')dst[i++]=' ';
    else if(*src=='%' && src[1]!='\0' && src[2]!='\0') {
      hi=hex_value((unsigned char)src[1]);
      lo=hex_value((unsigned char)src[2]);
      if(hi>=0 && lo>=0) {
        dst[i++]=(char)((hi<<4)|lo);
        src+=2;
      }
      else dst[i++]=*src;
    }
    else dst[i++]=*src;
  }
  dst[i]='\0';
}

static void query_value(char *dst,size_t size,const char *name) {
  char encoded[VALUE_SIZE];
  const char *query;
  const char *p;
  size_t nlen;
  size_t i;

  dst[0]='\0';
  query=getenv("QUERY_STRING");
  if(query==NULL)return;
  nlen=strlen(name);
  p=query;
  for(;;) {
    if(strncmp(p,name,nlen)==0 && p[nlen]=='=') {
      p+=nlen+1;
      for(i=0;p[i]!='\0' && p[i]!='&' && i<sizeof(encoded)-1;i++)encoded[i]=p[i];
      encoded[i]='\0';
      url_decode(dst,size,encoded);
      return;
    }
    p=strchr(p,'&');
    if(p==NULL)return;
    p++;
  }
}

static void url_value(const char *s) {
  const unsigned char *p;
  static const char hex[]="0123456789ABCDEF";

  p=(const unsigned char *)s;
  for(;*p!='\0';p++) {
    if(isalnum(*p) || *p=='-' || *p=='_' || *p=='.' || *p=='~')fputc(*p,stdout);
    else {
      fputc('%',stdout);
      fputc(hex[*p>>4],stdout);
      fputc(hex[*p&15],stdout);
    }
  }
}

static const char *column_text(sqlite3_stmt *stmt,int column) {
  const unsigned char *text;

  text=sqlite3_column_text(stmt,column);
  return text==NULL ? "" : (const char *)text;
}

static long long db_scalar(sqlite3 *db,const char *sql) {
  sqlite3_stmt *stmt;
  long long value;

  stmt=NULL;
  value=0;
  if(sqlite3_prepare_v2(db,sql,-1,&stmt,NULL)!=SQLITE_OK)return 0;
  if(sqlite3_step(stmt)==SQLITE_ROW)value=sqlite3_column_int64(stmt,0);
  sqlite3_finalize(stmt);
  return value;
}

static int db_quick_check(sqlite3 *db) {
  sqlite3_stmt *stmt;
  int ok;

  stmt=NULL;
  ok=0;
  if(sqlite3_prepare_v2(db,"PRAGMA quick_check",-1,&stmt,NULL)==SQLITE_OK) {
    if(sqlite3_step(stmt)==SQLITE_ROW && strcmp(column_text(stmt,0),"ok")==0)ok=1;
  }
  if(stmt!=NULL)sqlite3_finalize(stmt);
  return ok;
}

static long long db_foreign_key_errors(sqlite3 *db) {
  sqlite3_stmt *stmt;
  long long count;
  int rc;

  stmt=NULL;
  count=0;
  if(sqlite3_prepare_v2(db,"PRAGMA foreign_key_check",-1,&stmt,NULL)!=SQLITE_OK)return -1;
  for(;(rc=sqlite3_step(stmt))==SQLITE_ROW;)count++;
  sqlite3_finalize(stmt);
  return rc==SQLITE_DONE ? count : -1;
}

static void size_text(char *dst,size_t size,long long bytes) {
  double value;
  const char *unit;

  value=(double)bytes;
  unit="B";
  if(bytes>=1099511627776LL) { value/=1099511627776.0; unit="TB"; }
  else if(bytes>=1073741824LL) { value/=1073741824.0; unit="GB"; }
  else if(bytes>=1048576LL) { value/=1048576.0; unit="MB"; }
  else if(bytes>=1024LL) { value/=1024.0; unit="KB"; }
  if(bytes>=1024LL)snprintf(dst,size,"%.1f %s",value,unit);
  else snprintf(dst,size,"%lld %s",bytes,unit);
}


#define AUTH_ITERATIONS 200000
#define SESSION_SECONDS 2592000
#define NOTICE_VERSION "3"

static void hex_encode(const unsigned char *src,size_t len,char *dst,size_t size) {
  static const char hex[]="0123456789abcdef";
  size_t i;

  if(size<len*2+1) {
    if(size>0)dst[0]='\0';
    return;
  }
  for(i=0;i<len;i++) {
    dst[i*2]=hex[src[i]>>4];
    dst[i*2+1]=hex[src[i]&15];
  }
  dst[len*2]='\0';
}

static int password_hash(const char *password,const char *salt_hex,int iterations,char *out,size_t size) {
  unsigned char salt[16];
  unsigned char hash[32];
  size_t i;
  int hi;
  int lo;

  if(strlen(salt_hex)!=32 || size<65)return 0;
  for(i=0;i<16;i++) {
    hi=hex_value((unsigned char)salt_hex[i*2]);
    lo=hex_value((unsigned char)salt_hex[i*2+1]);
    if(hi<0 || lo<0)return 0;
    salt[i]=(unsigned char)((hi<<4)|lo);
  }
  if(!PKCS5_PBKDF2_HMAC(password,(int)strlen(password),salt,sizeof(salt),iterations,EVP_sha256(),sizeof(hash),hash))return 0;
  hex_encode(hash,sizeof(hash),out,size);
  return out[0]!='\0';
}

static int random_hex(char *out,size_t bytes,size_t size) {
  unsigned char buf[64];

  if(bytes>sizeof(buf) || size<bytes*2+1)return 0;
  if(RAND_bytes(buf,(int)bytes)!=1)return 0;
  hex_encode(buf,bytes,out,size);
  return 1;
}

static int sha256_hex(const char *text,char *out,size_t size) {
  EVP_MD_CTX *ctx;
  unsigned char hash[EVP_MAX_MD_SIZE];
  unsigned int len;
  int ok;

  if(size<65)return 0;
  ctx=EVP_MD_CTX_new();
  if(ctx==NULL)return 0;
  ok=EVP_DigestInit_ex(ctx,EVP_sha256(),NULL)==1 && EVP_DigestUpdate(ctx,text,strlen(text))==1 && EVP_DigestFinal_ex(ctx,hash,&len)==1;
  EVP_MD_CTX_free(ctx);
  if(!ok || len!=32)return 0;
  hex_encode(hash,len,out,size);
  return 1;
}

static int secure_equal(const char *a,const char *b) {
  size_t n;

  if(a==NULL || b==NULL)return 0;
  n=strlen(a);
  if(n!=strlen(b))return 0;
  return CRYPTO_memcmp(a,b,n)==0;
}

static int csrf_from_session(const char *token,char *out,size_t size) {
  char text[160];

  if(token==NULL || strlen(token)!=64)return 0;
  snprintf(text,sizeof(text),"music-csrf:%s",token);
  return sha256_hex(text,out,size);
}

static void cookie_value(char *dst,size_t size,const char *name) {
  const char *cookie;
  const char *p;
  size_t n;
  size_t i;

  dst[0]='\0';
  cookie=getenv("HTTP_COOKIE");
  if(cookie==NULL)return;
  n=strlen(name);
  p=cookie;
  for(;;) {
    for(;*p==' ' || *p==';';p++);
    if(strncmp(p,name,n)==0 && p[n]=='=') {
      p+=n+1;
      for(i=0;p[i]!='\0' && p[i]!=';' && i+1<size;i++)dst[i]=p[i];
      dst[i]='\0';
      return;
    }
    p=strchr(p,';');
    if(p==NULL)return;
    p++;
  }
}

static char *post_body(void) {
  const char *method;
  const char *length_text;
  char *body;
  long length;
  size_t got;

  method=getenv("REQUEST_METHOD");
  if(method==NULL || strcmp(method,"POST")!=0)return NULL;
  length_text=getenv("CONTENT_LENGTH");
  if(length_text==NULL)return NULL;
  length=strtol(length_text,NULL,10);
  if(length<=0 || length>POST_BODY_MAX)return NULL;
  body=(char *)malloc((size_t)length+1);
  if(body==NULL)return NULL;
  got=fread(body,1,(size_t)length,stdin);
  body[got]='\0';
  return body;
}

static void form_value(char *dst,size_t size,const char *body,const char *name) {
  char *encoded;
  const char *p;
  const char *end;
  size_t n;
  size_t len;

  if(size==0)return;
  dst[0]='\0';
  if(body==NULL)return;
  n=strlen(name);
  p=body;
  for(;;) {
    if(strncmp(p,name,n)==0 && p[n]=='=') {
      p+=n+1;
      end=strchr(p,'&');
      len=end==NULL ? strlen(p) : (size_t)(end-p);
      encoded=(char *)malloc(len+1);
      if(encoded==NULL)return;
      memcpy(encoded,p,len);
      encoded[len]='\0';
      url_decode(dst,size,encoded);
      free(encoded);
      return;
    }
    p=strchr(p,'&');
    if(p==NULL)return;
    p++;
  }
}

static const char *cookie_path(void) {
  const char *path;

  path=getenv("SCRIPT_NAME");
  return path==NULL || *path=='\0' ? "/" : path;
}

static void set_notice_cookie(void) {
  printf("Set-Cookie: music_notice=%s; Path=%s; Max-Age=31536000; Secure; HttpOnly; SameSite=Lax\r\n",NOTICE_VERSION,cookie_path());
}

static void set_session_cookie(const char *token) {
  printf("Set-Cookie: music_session=%s; Path=%s; Max-Age=%d; Secure; HttpOnly; SameSite=Lax\r\n",token,cookie_path(),SESSION_SECONDS);
}

static void clear_session_cookie(void) {
  printf("Set-Cookie: music_session=; Path=%s; Max-Age=0; Secure; HttpOnly; SameSite=Lax\r\n",cookie_path());
}

static void redirect_home(void) {
  printf("Status: 303 See Other\r\nLocation: %s\r\nCache-Control: no-store\r\n\r\n",cookie_path());
}

static int user_count(const struct music_config *cfg) {
  sqlite3 *db;
  int count;

  db=db_open(cfg,0);
  if(db==NULL)return 0;
  count=(int)db_scalar(db,"SELECT count(*) FROM user WHERE enabled=1");
  sqlite3_close(db);
  return count;
}

static int user_add(const struct music_config *cfg,const char *username) {
  sqlite3 *db;
  sqlite3_stmt *stmt;
  char salt[33];
  char hash[65];
  char *p1;
  char *p2;
  char first[VALUE_SIZE];
  long long user_id;
  int rc;

  if(username==NULL || *username=='\0' || strlen(username)>64)return 0;
  p1=getpass("Password: ");
  if(p1==NULL || strlen(p1)<8) {
    fprintf(stderr,"user: password must be at least 8 characters\n");
    return 0;
  }
  snprintf(first,sizeof(first),"%s",p1);
  p2=getpass("Confirm password: ");
  if(p2==NULL || strcmp(first,p2)!=0) {
    memset(first,0,sizeof(first));
    fprintf(stderr,"user: passwords do not match\n");
    return 0;
  }
  if(!random_hex(salt,16,sizeof(salt)) || !password_hash(first,salt,AUTH_ITERATIONS,hash,sizeof(hash))) {
    memset(first,0,sizeof(first));
    return 0;
  }
  memset(first,0,sizeof(first));
  db=db_open(cfg,1);
  if(db==NULL)return 0;
  if(!db_exec(db,"BEGIN IMMEDIATE")) {
    sqlite3_close(db);
    return 0;
  }
  stmt=NULL;
  if(sqlite3_prepare_v2(db,"INSERT INTO user(username,password_hash,password_salt,iterations) VALUES(?1,?2,?3,?4)",-1,&stmt,NULL)!=SQLITE_OK) {
    db_exec(db,"ROLLBACK");
    sqlite3_close(db);
    return 0;
  }
  sqlite3_bind_text(stmt,1,username,-1,(void (*)(void *))-1);
  sqlite3_bind_text(stmt,2,hash,-1,(void (*)(void *))-1);
  sqlite3_bind_text(stmt,3,salt,-1,(void (*)(void *))-1);
  sqlite3_bind_int(stmt,4,AUTH_ITERATIONS);
  rc=sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  user_id=rc==SQLITE_DONE ? sqlite3_last_insert_rowid(db) : 0;
  stmt=NULL;
  if(user_id>0 && sqlite3_prepare_v2(db,"INSERT INTO user_stats(user_id) VALUES(?1)",-1,&stmt,NULL)==SQLITE_OK) {
    sqlite3_bind_int64(stmt,1,user_id);
    rc=sqlite3_step(stmt);
  } else if(user_id>0)rc=SQLITE_OK;
  if(stmt!=NULL)sqlite3_finalize(stmt);
  if(rc==SQLITE_DONE)db_exec(db,"COMMIT");
  else {
    if(user_id==0)fprintf(stderr,"user: %s\n",sqlite3_errmsg(db));
    db_exec(db,"ROLLBACK");
  }
  sqlite3_close(db);
  return rc==SQLITE_DONE;
}

static int user_password(const struct music_config *cfg,const char *username) {
  sqlite3 *db;
  sqlite3_stmt *stmt;
  char salt[33];
  char hash[65];
  char *p1;
  char *p2;
  char first[VALUE_SIZE];
  long long user_id;
  int rc;

  if(username==NULL || *username=='\0' || strlen(username)>64)return 0;
  db=db_open(cfg,0);
  if(db==NULL)return 0;
  stmt=NULL;
  user_id=0;
  if(sqlite3_prepare_v2(db,"SELECT id FROM user WHERE username=?1 COLLATE NOCASE",-1,&stmt,NULL)==SQLITE_OK) {
    sqlite3_bind_text(stmt,1,username,-1,(void (*)(void *))-1);
    if(sqlite3_step(stmt)==SQLITE_ROW)user_id=sqlite3_column_int64(stmt,0);
  }
  if(stmt!=NULL)sqlite3_finalize(stmt);
  sqlite3_close(db);
  if(user_id==0) {
    fprintf(stderr,"user: not found\n");
    return 0;
  }
  p1=getpass("New password: ");
  if(p1==NULL || strlen(p1)<8) {
    fprintf(stderr,"user: password must be at least 8 characters\n");
    return 0;
  }
  snprintf(first,sizeof(first),"%s",p1);
  p2=getpass("Confirm password: ");
  if(p2==NULL || strcmp(first,p2)!=0) {
    memset(first,0,sizeof(first));
    fprintf(stderr,"user: passwords do not match\n");
    return 0;
  }
  if(!random_hex(salt,16,sizeof(salt)) || !password_hash(first,salt,AUTH_ITERATIONS,hash,sizeof(hash))) {
    memset(first,0,sizeof(first));
    return 0;
  }
  memset(first,0,sizeof(first));
  db=db_open(cfg,1);
  if(db==NULL)return 0;
  if(!db_exec(db,"BEGIN IMMEDIATE")) {
    sqlite3_close(db);
    return 0;
  }
  stmt=NULL;
  if(sqlite3_prepare_v2(db,"UPDATE user SET password_hash=?1,password_salt=?2,iterations=?3 WHERE id=?4",-1,&stmt,NULL)!=SQLITE_OK) {
    db_exec(db,"ROLLBACK");
    sqlite3_close(db);
    return 0;
  }
  sqlite3_bind_text(stmt,1,hash,-1,(void (*)(void *))-1);
  sqlite3_bind_text(stmt,2,salt,-1,(void (*)(void *))-1);
  sqlite3_bind_int(stmt,3,AUTH_ITERATIONS);
  sqlite3_bind_int64(stmt,4,user_id);
  rc=sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  if(rc==SQLITE_DONE) {
    stmt=NULL;
    if(sqlite3_prepare_v2(db,"DELETE FROM session WHERE user_id=?1",-1,&stmt,NULL)==SQLITE_OK) {
      sqlite3_bind_int64(stmt,1,user_id);
      rc=sqlite3_step(stmt);
    } else rc=SQLITE_OK;
    if(stmt!=NULL)sqlite3_finalize(stmt);
  }
  if(rc==SQLITE_DONE)db_exec(db,"COMMIT");
  else db_exec(db,"ROLLBACK");
  sqlite3_close(db);
  return rc==SQLITE_DONE;
}

static int user_delete(const struct music_config *cfg,const char *username) {
  sqlite3 *db;
  sqlite3_stmt *stmt;
  int rc;
  int changed;

  db=db_open(cfg,1);
  if(db==NULL)return 0;
  stmt=NULL;
  if(sqlite3_prepare_v2(db,"DELETE FROM user WHERE username=?1 COLLATE NOCASE",-1,&stmt,NULL)!=SQLITE_OK) {
    sqlite3_close(db);
    return 0;
  }
  sqlite3_bind_text(stmt,1,username,-1,(void (*)(void *))-1);
  rc=sqlite3_step(stmt);
  changed=rc==SQLITE_DONE ? sqlite3_changes(db) : 0;
  sqlite3_finalize(stmt);
  sqlite3_close(db);
  if(rc==SQLITE_DONE && changed==0)fprintf(stderr,"user: not found\n");
  return rc==SQLITE_DONE && changed>0;
}

static int user_set_enabled(const struct music_config *cfg,const char *username,int enabled) {
  sqlite3 *db;
  sqlite3_stmt *stmt;
  long long user_id;
  int rc;

  db=db_open(cfg,1);
  if(db==NULL)return 0;
  if(!db_exec(db,"BEGIN IMMEDIATE")) {
    sqlite3_close(db);
    return 0;
  }
  stmt=NULL;
  user_id=0;
  if(sqlite3_prepare_v2(db,"SELECT id FROM user WHERE username=?1 COLLATE NOCASE",-1,&stmt,NULL)==SQLITE_OK) {
    sqlite3_bind_text(stmt,1,username,-1,(void (*)(void *))-1);
    if(sqlite3_step(stmt)==SQLITE_ROW)user_id=sqlite3_column_int64(stmt,0);
  }
  if(stmt!=NULL)sqlite3_finalize(stmt);
  if(user_id==0) {
    db_exec(db,"ROLLBACK");
    sqlite3_close(db);
    fprintf(stderr,"user: not found\n");
    return 0;
  }
  stmt=NULL;
  rc=SQLITE_OK;
  if(sqlite3_prepare_v2(db,"UPDATE user SET enabled=?1 WHERE id=?2",-1,&stmt,NULL)==SQLITE_OK) {
    sqlite3_bind_int(stmt,1,enabled ? 1 : 0);
    sqlite3_bind_int64(stmt,2,user_id);
    rc=sqlite3_step(stmt);
  }
  if(stmt!=NULL)sqlite3_finalize(stmt);
  if(rc==SQLITE_DONE && !enabled) {
    stmt=NULL;
    if(sqlite3_prepare_v2(db,"DELETE FROM session WHERE user_id=?1",-1,&stmt,NULL)==SQLITE_OK) {
      sqlite3_bind_int64(stmt,1,user_id);
      rc=sqlite3_step(stmt);
    } else rc=SQLITE_OK;
    if(stmt!=NULL)sqlite3_finalize(stmt);
  }
  if(rc==SQLITE_DONE)db_exec(db,"COMMIT");
  else db_exec(db,"ROLLBACK");
  sqlite3_close(db);
  return rc==SQLITE_DONE;
}

static int user_list(const struct music_config *cfg) {
  sqlite3 *db;
  sqlite3_stmt *stmt;
  int rc;

  db=db_open(cfg,0);
  if(db==NULL)return 0;
  stmt=NULL;
  if(sqlite3_prepare_v2(db,"SELECT username,enabled,datetime(created,'unixepoch','localtime') FROM user ORDER BY username COLLATE NOCASE",-1,&stmt,NULL)!=SQLITE_OK) {
    sqlite3_close(db);
    return 0;
  }
  for(;(rc=sqlite3_step(stmt))==SQLITE_ROW;)printf("%s\t%s\t%s\n",column_text(stmt,0),sqlite3_column_int(stmt,1) ? "enabled" : "disabled",column_text(stmt,2));
  sqlite3_finalize(stmt);
  sqlite3_close(db);
  return rc!=SQLITE_ROW;
}

static int user_info(const struct music_config *cfg,const char *username) {
  sqlite3 *db;
  sqlite3_stmt *stmt;
  int rc;
  int found;

  db=db_open(cfg,0);
  if(db==NULL)return 0;
  stmt=NULL;
  found=0;
  rc=sqlite3_prepare_v2(db,
      "SELECT u.username,u.enabled,datetime(u.created,'unixepoch','localtime'),"
      "datetime(us.stats_since,'unixepoch','localtime'),coalesce(us.login_count,0),"
      "coalesce(datetime(us.last_login,'unixepoch','localtime'),'-'),"
      "coalesce(datetime(us.last_seen,'unixepoch','localtime'),'-'),coalesce(us.played,0),"
      "coalesce(datetime(us.last_played,'unixepoch','localtime'),'-'),"
      "(SELECT count(*) FROM playlist p WHERE p.user_id=u.id),"
      "(SELECT count(*) FROM playlist p WHERE p.user_id=u.id AND p.shared=1),"
      "(SELECT count(*) FROM playlist_song ps JOIN playlist p ON p.id=ps.playlist_id WHERE p.user_id=u.id),"
      "(SELECT count(*) FROM session se WHERE se.user_id=u.id AND se.expires>unixepoch()),"
      "(SELECT count(*) FROM shared_queue_song q WHERE q.added_by=u.id) "
      "FROM user u LEFT JOIN user_stats us ON us.user_id=u.id WHERE u.username=?1 COLLATE NOCASE",-1,&stmt,NULL);
  if(rc==SQLITE_OK) {
    sqlite3_bind_text(stmt,1,username,-1,(void (*)(void *))-1);
    if(sqlite3_step(stmt)==SQLITE_ROW) {
      found=1;
      printf("user: %s\n",column_text(stmt,0));
      printf("status: %s\n",sqlite3_column_int(stmt,1) ? "enabled" : "disabled");
      printf("created: %s\n",column_text(stmt,2));
      printf("statistics since: %s\n",column_text(stmt,3));
      printf("logins: %lld\n",sqlite3_column_int64(stmt,4));
      printf("last login: %s\n",column_text(stmt,5));
      printf("last activity: %s\n",column_text(stmt,6));
      printf("tracks played: %lld\n",sqlite3_column_int64(stmt,7));
      printf("last played: %s\n",column_text(stmt,8));
      printf("playlists: %lld (%lld shared), %lld entries\n",sqlite3_column_int64(stmt,9),sqlite3_column_int64(stmt,10),sqlite3_column_int64(stmt,11));
      printf("active sessions: %lld\n",sqlite3_column_int64(stmt,12));
      printf("shared queue entries: %lld\n",sqlite3_column_int64(stmt,13));
    }
  }
  if(stmt!=NULL)sqlite3_finalize(stmt);
  sqlite3_close(db);
  if(!found)fprintf(stderr,"user: not found\n");
  return found;
}

static int login_user(const struct music_config *cfg,const char *username,const char *password,char *token,size_t token_size) {
  sqlite3 *db;
  sqlite3_stmt *stmt;
  const unsigned char *saved_hash;
  const unsigned char *salt;
  char hash[65];
  char token_hash[65];
  long long user_id;
  int iterations;
  int rc;
  int ok;

  db=db_open_web_write(cfg);
  if(db==NULL)return 0;
  db_exec(db,"DELETE FROM session WHERE expires<=unixepoch()");
  stmt=NULL;
  user_id=0;
  ok=0;
  if(sqlite3_prepare_v2(db,"SELECT id,password_hash,password_salt,iterations FROM user WHERE username=?1 COLLATE NOCASE AND enabled=1",-1,&stmt,NULL)==SQLITE_OK) {
    sqlite3_bind_text(stmt,1,username,-1,(void (*)(void *))-1);
    if(sqlite3_step(stmt)==SQLITE_ROW) {
      user_id=sqlite3_column_int64(stmt,0);
      saved_hash=sqlite3_column_text(stmt,1);
      salt=sqlite3_column_text(stmt,2);
      iterations=sqlite3_column_int(stmt,3);
      if(saved_hash!=NULL && salt!=NULL && password_hash(password,(const char *)salt,iterations,hash,sizeof(hash)) && secure_equal(hash,(const char *)saved_hash))ok=1;
    }
  }
  if(stmt!=NULL)sqlite3_finalize(stmt);
  if(!ok) {
    sqlite3_close(db);
    sleep(1);
    return 0;
  }
  if(!random_hex(token,32,token_size) || !sha256_hex(token,token_hash,sizeof(token_hash)) || !db_exec(db,"BEGIN IMMEDIATE")) {
    sqlite3_close(db);
    return 0;
  }
  stmt=NULL;
  rc=sqlite3_prepare_v2(db,"UPDATE user SET privacy_version=?1,privacy_accepted=unixepoch() WHERE id=?2",-1,&stmt,NULL);
  if(rc==SQLITE_OK) {
    sqlite3_bind_text(stmt,1,NOTICE_VERSION,-1,(void (*)(void *))-1);
    sqlite3_bind_int64(stmt,2,user_id);
    rc=sqlite3_step(stmt);
  }
  if(stmt!=NULL)sqlite3_finalize(stmt);
  stmt=NULL;
  if(rc==SQLITE_DONE) {
    rc=sqlite3_prepare_v2(db,"INSERT INTO user_stats(user_id,login_count,last_login,last_seen) VALUES(?1,1,unixepoch(),unixepoch()) ON CONFLICT(user_id) DO UPDATE SET login_count=user_stats.login_count+1,last_login=unixepoch(),last_seen=unixepoch()",-1,&stmt,NULL);
    if(rc==SQLITE_OK) {
      sqlite3_bind_int64(stmt,1,user_id);
      rc=sqlite3_step(stmt);
    }
  }
  if(stmt!=NULL)sqlite3_finalize(stmt);
  stmt=NULL;
  if(rc==SQLITE_DONE) {
    rc=sqlite3_prepare_v2(db,"INSERT INTO session(token_hash,user_id,expires) VALUES(?1,?2,unixepoch()+?3)",-1,&stmt,NULL);
    if(rc==SQLITE_OK) {
      sqlite3_bind_text(stmt,1,token_hash,-1,(void (*)(void *))-1);
      sqlite3_bind_int64(stmt,2,user_id);
      sqlite3_bind_int(stmt,3,SESSION_SECONDS);
      rc=sqlite3_step(stmt);
    }
  }
  if(stmt!=NULL)sqlite3_finalize(stmt);
  if(rc==SQLITE_DONE)ok=db_exec(db,"COMMIT");
  else {
    db_exec(db,"ROLLBACK");
    ok=0;
  }
  sqlite3_close(db);
  return ok;
}

static int session_user(const struct music_config *cfg,long long *user_id,char *username,size_t username_size,char *session_token,size_t token_size) {
  sqlite3 *db;
  sqlite3_stmt *stmt;
  char token[128];
  char hash[65];
  long long last_seen;
  int ok;

  *user_id=0;
  last_seen=0;
  if(username_size>0)username[0]='\0';
  if(token_size>0)session_token[0]='\0';
  cookie_value(token,sizeof(token),"music_session");
  if(strlen(token)!=64 || !sha256_hex(token,hash,sizeof(hash)))return 0;
  db=db_open(cfg,0);
  if(db==NULL)return 0;
  stmt=NULL;
  ok=0;
  if(sqlite3_prepare_v2(db,"SELECT s.user_id,u.username,s.last_seen FROM session s JOIN user u ON u.id=s.user_id WHERE s.token_hash=?1 AND s.expires>unixepoch() AND u.enabled=1",-1,&stmt,NULL)==SQLITE_OK) {
    sqlite3_bind_text(stmt,1,hash,-1,(void (*)(void *))-1);
    if(sqlite3_step(stmt)==SQLITE_ROW) {
      *user_id=sqlite3_column_int64(stmt,0);
      snprintf(username,username_size,"%s",column_text(stmt,1));
      last_seen=sqlite3_column_int64(stmt,2);
      ok=1;
    }
  }
  if(stmt!=NULL)sqlite3_finalize(stmt);
  sqlite3_close(db);
  if(ok && (long long)time(NULL)-last_seen>=60) {
    db=db_open_web_write(cfg);
    if(db!=NULL) {
      int rc;

      rc=SQLITE_OK;
      if(db_exec(db,"BEGIN IMMEDIATE")) {
        stmt=NULL;
        rc=sqlite3_prepare_v2(db,"UPDATE session SET last_seen=unixepoch(),expires=unixepoch()+?1 WHERE token_hash=?2",-1,&stmt,NULL);
        if(rc==SQLITE_OK) {
          sqlite3_bind_int(stmt,1,SESSION_SECONDS);
          sqlite3_bind_text(stmt,2,hash,-1,(void (*)(void *))-1);
          rc=sqlite3_step(stmt);
        }
        if(stmt!=NULL)sqlite3_finalize(stmt);
        stmt=NULL;
        if(rc==SQLITE_DONE) {
          rc=sqlite3_prepare_v2(db,"INSERT INTO user_stats(user_id,last_seen) VALUES(?1,unixepoch()) ON CONFLICT(user_id) DO UPDATE SET last_seen=unixepoch()",-1,&stmt,NULL);
          if(rc==SQLITE_OK) {
            sqlite3_bind_int64(stmt,1,*user_id);
            rc=sqlite3_step(stmt);
          }
        }
        if(stmt!=NULL)sqlite3_finalize(stmt);
        if(rc==SQLITE_DONE)db_exec(db,"COMMIT"); else db_exec(db,"ROLLBACK");
      }
      sqlite3_close(db);
    }
  }
  if(ok && token_size>0)snprintf(session_token,token_size,"%s",token);
  return ok;
}

static void session_logout(const struct music_config *cfg) {
  sqlite3 *db;
  sqlite3_stmt *stmt;
  char token[128];
  char hash[65];

  cookie_value(token,sizeof(token),"music_session");
  if(strlen(token)!=64 || !sha256_hex(token,hash,sizeof(hash)))return;
  db=db_open_web_write(cfg);
  if(db==NULL)return;
  stmt=NULL;
  if(sqlite3_prepare_v2(db,"DELETE FROM session WHERE token_hash=?1",-1,&stmt,NULL)==SQLITE_OK) {
    sqlite3_bind_text(stmt,1,hash,-1,(void (*)(void *))-1);
    sqlite3_step(stmt);
  }
  if(stmt!=NULL)sqlite3_finalize(stmt);
  sqlite3_close(db);
}

static int notice_accepted(void) {
  char value[32];

  cookie_value(value,sizeof(value),"music_notice");
  return strcmp(value,NOTICE_VERSION)==0;
}


#define SHARED_QUEUE_COUNT 10
#define SHARED_LEAD_MS 1500

struct shared_snapshot {
  long long revision;
  long long current_song_id;
  long long base_position_ms;
  long long base_time_ms;
  long long changed_by;
  int state;
};

static long long wall_ms(void) {
  struct timeval tv;

  if(gettimeofday(&tv,NULL)!=0)return (long long)time(NULL)*1000LL;
  return (long long)tv.tv_sec*1000LL+(long long)tv.tv_usec/1000LL;
}

static void json_emit_string(const char *s) {
  const unsigned char *p;

  if(s==NULL)s="";
  p=(const unsigned char *)s;
  fputc('"',stdout);
  for(;*p!='\0';p++) {
    if(*p=='"' || *p=='\\') {
      fputc('\\',stdout);
      fputc(*p,stdout);
    } else if(*p=='\n')fputs("\\n",stdout);
    else if(*p=='\r')fputs("\\r",stdout);
    else if(*p=='\t')fputs("\\t",stdout);
    else if(*p<32)printf("\\u%04x",(unsigned int)*p);
    else fputc(*p,stdout);
  }
  fputc('"',stdout);
}

static int shared_queue_valid(int queue_id) {
  return queue_id>=0 && queue_id<SHARED_QUEUE_COUNT;
}

static int shared_snapshot_load(sqlite3 *db,int queue_id,struct shared_snapshot *snap) {
  sqlite3_stmt *stmt;
  int ok;

  stmt=NULL;
  ok=0;
  memset(snap,0,sizeof(*snap));
  snap->current_song_id=0;
  if(sqlite3_prepare_v2(db,"SELECT revision,state,coalesce(current_song_id,0),base_position_ms,base_time_ms,coalesce(changed_by,0) FROM shared_queue WHERE id=?1",-1,&stmt,NULL)==SQLITE_OK) {
    sqlite3_bind_int(stmt,1,queue_id);
    if(sqlite3_step(stmt)==SQLITE_ROW) {
      snap->revision=sqlite3_column_int64(stmt,0);
      snap->state=sqlite3_column_int(stmt,1);
      snap->current_song_id=sqlite3_column_int64(stmt,2);
      snap->base_position_ms=sqlite3_column_int64(stmt,3);
      snap->base_time_ms=sqlite3_column_int64(stmt,4);
      snap->changed_by=sqlite3_column_int64(stmt,5);
      ok=1;
    }
  }
  if(stmt!=NULL)sqlite3_finalize(stmt);
  return ok;
}

static long long shared_position_at(const struct shared_snapshot *snap,long long when_ms) {
  long long position;

  position=snap->base_position_ms;
  if(snap->state && when_ms>snap->base_time_ms)position+=when_ms-snap->base_time_ms;
  if(position<0)position=0;
  return position;
}

static int shared_queue_append(sqlite3 *db,int queue_id,long long song_id,long long user_id) {
  sqlite3_stmt *stmt;
  long long position;
  int exists;
  int rc;

  stmt=NULL;
  exists=0;
  if(sqlite3_prepare_v2(db,"SELECT 1 FROM shared_queue_song WHERE queue_id=?1 AND song_id=?2",-1,&stmt,NULL)==SQLITE_OK) {
    sqlite3_bind_int(stmt,1,queue_id);
    sqlite3_bind_int64(stmt,2,song_id);
    if(sqlite3_step(stmt)==SQLITE_ROW)exists=1;
  }
  if(stmt!=NULL)sqlite3_finalize(stmt);
  if(exists)return 1;
  position=0;
  stmt=NULL;
  if(sqlite3_prepare_v2(db,"SELECT coalesce(max(position),-1)+1 FROM shared_queue_song WHERE queue_id=?1",-1,&stmt,NULL)!=SQLITE_OK)return 0;
  sqlite3_bind_int(stmt,1,queue_id);
  if(sqlite3_step(stmt)==SQLITE_ROW)position=sqlite3_column_int64(stmt,0);
  sqlite3_finalize(stmt);
  stmt=NULL;
  if(sqlite3_prepare_v2(db,"INSERT INTO shared_queue_song(queue_id,position,song_id,added_by,added_at) VALUES(?1,?2,?3,?4,unixepoch())",-1,&stmt,NULL)!=SQLITE_OK)return 0;
  sqlite3_bind_int(stmt,1,queue_id);
  sqlite3_bind_int64(stmt,2,position);
  sqlite3_bind_int64(stmt,3,song_id);
  sqlite3_bind_int64(stmt,4,user_id);
  rc=sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  return rc==SQLITE_DONE;
}

static long long shared_first_csv_song(sqlite3 *db,const char *ids) {
  const char *p;
  char *end;
  long long id;
  long long canonical;

  if(ids==NULL)return 0;
  p=ids;
  for(;*p!='\0';) {
    id=strtoll(p,&end,10);
    if(end!=p) {
      canonical=canonical_song_id(db,id);
      if(canonical>0)return canonical;
      p=end;
    } else p++;
    for(;*p==',' || *p==' ' || *p=='\t';p++);
  }
  return 0;
}

static int shared_queue_add_csv(sqlite3 *db,int queue_id,const char *ids,long long *first_song,long long user_id) {
  const char *p;
  char *end;
  long long id;
  long long canonical;
  int any;

  if(first_song!=NULL)*first_song=0;
  if(ids==NULL)return 0;
  p=ids;
  any=0;
  for(;*p!='\0';) {
    id=strtoll(p,&end,10);
    if(end==p) {
      for(;*p!='\0' && *p!=',';p++);
    } else {
      canonical=canonical_song_id(db,id);
      if(canonical>0) {
        if(first_song!=NULL && *first_song==0)*first_song=canonical;
        if(!shared_queue_append(db,queue_id,canonical,user_id))return 0;
        any=1;
      }
      p=end;
    }
    for(;*p==',' || *p==' ' || *p=='\t';p++);
  }
  return any;
}

static int shared_queue_revision_bump(sqlite3 *db,int queue_id,long long user_id) {
  sqlite3_stmt *stmt;
  int rc;

  stmt=NULL;
  if(sqlite3_prepare_v2(db,"UPDATE shared_queue SET revision=revision+1,changed_by=?2 WHERE id=?1",-1,&stmt,NULL)!=SQLITE_OK)return 0;
  sqlite3_bind_int(stmt,1,queue_id);
  sqlite3_bind_int64(stmt,2,user_id);
  rc=sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  return rc==SQLITE_DONE;
}

static int shared_queue_schedule(sqlite3 *db,int queue_id,long long song_id,int state,long long position_ms,long long effective_ms,long long user_id) {
  sqlite3_stmt *stmt;
  int rc;

  stmt=NULL;
  if(song_id>0) {
    if(sqlite3_prepare_v2(db,"UPDATE shared_queue SET state=?1,current_song_id=?2,base_position_ms=?3,base_time_ms=?4,revision=revision+1,changed_by=?6 WHERE id=?5",-1,&stmt,NULL)!=SQLITE_OK)return 0;
    sqlite3_bind_int(stmt,1,state ? 1 : 0);
    sqlite3_bind_int64(stmt,2,song_id);
    sqlite3_bind_int64(stmt,3,position_ms);
    sqlite3_bind_int64(stmt,4,effective_ms);
    sqlite3_bind_int(stmt,5,queue_id);
    sqlite3_bind_int64(stmt,6,user_id);
  } else {
    if(sqlite3_prepare_v2(db,"UPDATE shared_queue SET state=0,current_song_id=NULL,base_position_ms=0,base_time_ms=?1,revision=revision+1,changed_by=?3 WHERE id=?2",-1,&stmt,NULL)!=SQLITE_OK)return 0;
    sqlite3_bind_int64(stmt,1,effective_ms);
    sqlite3_bind_int(stmt,2,queue_id);
    sqlite3_bind_int64(stmt,3,user_id);
  }
  rc=sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  return rc==SQLITE_DONE;
}

static long long shared_queue_neighbor(sqlite3 *db,int queue_id,long long current_song,int direction) {
  sqlite3_stmt *stmt;
  long long song_id;
  const char *sql;

  stmt=NULL;
  song_id=0;
  if(current_song<=0)sql="SELECT song_id FROM shared_queue_song WHERE queue_id=?1 ORDER BY position LIMIT 1";
  else if(direction<0)sql="SELECT x.song_id FROM shared_queue_song c JOIN shared_queue_song x ON x.queue_id=c.queue_id AND x.position<c.position WHERE c.queue_id=?1 AND c.song_id=?2 ORDER BY x.position DESC LIMIT 1";
  else sql="SELECT x.song_id FROM shared_queue_song c JOIN shared_queue_song x ON x.queue_id=c.queue_id AND x.position>c.position WHERE c.queue_id=?1 AND c.song_id=?2 ORDER BY x.position LIMIT 1";
  if(sqlite3_prepare_v2(db,sql,-1,&stmt,NULL)!=SQLITE_OK)return 0;
  sqlite3_bind_int(stmt,1,queue_id);
  if(current_song>0)sqlite3_bind_int64(stmt,2,current_song);
  if(sqlite3_step(stmt)==SQLITE_ROW)song_id=sqlite3_column_int64(stmt,0);
  sqlite3_finalize(stmt);
  return song_id;
}

static void shared_queue_state_cgi(const struct music_config *cfg,int queue_id) {
  sqlite3 *db;
  sqlite3_stmt *stmt;
  struct shared_snapshot snap;
  long long server_ms;
  int rc;
  int first;
  char changed_by[VALUE_SIZE];

  changed_by[0]='\0';
  if(!shared_queue_valid(queue_id)) {
    printf("Status: 400 Bad Request\r\nContent-Type: application/json\r\nCache-Control: no-store\r\n\r\n{\"error\":\"invalid queue\"}\n");
    return;
  }
  db=db_open(cfg,0);
  if(db==NULL || !shared_snapshot_load(db,queue_id,&snap)) {
    if(db!=NULL)sqlite3_close(db);
    printf("Status: 500 Internal Server Error\r\nContent-Type: application/json\r\nCache-Control: no-store\r\n\r\n{\"error\":\"queue unavailable\"}\n");
    return;
  }
  server_ms=wall_ms();
  if(snap.changed_by>0) {
    stmt=NULL;
    if(sqlite3_prepare_v2(db,"SELECT username FROM user WHERE id=?1",-1,&stmt,NULL)==SQLITE_OK) {
      sqlite3_bind_int64(stmt,1,snap.changed_by);
      if(sqlite3_step(stmt)==SQLITE_ROW)snprintf(changed_by,sizeof(changed_by),"%s",column_text(stmt,0));
    }
    if(stmt!=NULL)sqlite3_finalize(stmt);
  }
  printf("Content-Type: application/json; charset=utf-8\r\nCache-Control: no-store\r\n\r\n");
  printf("{\"server_ms\":%lld,\"queue\":%d,\"revision\":%lld,\"state\":%d,\"current_id\":%lld,\"base_position_ms\":%lld,\"base_time_ms\":%lld,\"changed_by_id\":%lld,\"changed_by\":",server_ms,queue_id,snap.revision,snap.state,snap.current_song_id,snap.base_position_ms,snap.base_time_ms,snap.changed_by);
  json_emit_string(changed_by);
  fputs(",\"items\":[",stdout);
  stmt=NULL;
  first=1;
  if(sqlite3_prepare_v2(db,"SELECT q.position,s.id,s.title,s.artist,s.album,s.duration,coalesce(a.acrid,''),coalesce(u.username,''),q.added_at,coalesce(q.added_by,0) FROM shared_queue_song q JOIN song s ON s.id=q.song_id LEFT JOIN acr_result a ON a.song_id=s.id LEFT JOIN user u ON u.id=q.added_by WHERE q.queue_id=?1 ORDER BY q.position",-1,&stmt,NULL)==SQLITE_OK) {
    sqlite3_bind_int(stmt,1,queue_id);
    for(;(rc=sqlite3_step(stmt))==SQLITE_ROW;) {
      if(!first)fputc(',',stdout);
      first=0;
      printf("{\"position\":%lld,\"id\":%lld,\"title\":",sqlite3_column_int64(stmt,0),sqlite3_column_int64(stmt,1));
      json_emit_string(column_text(stmt,2));
      fputs(",\"artist\":",stdout); json_emit_string(column_text(stmt,3));
      fputs(",\"album\":",stdout); json_emit_string(column_text(stmt,4));
      printf(",\"duration\":%d,\"acrid\":",sqlite3_column_int(stmt,5));
      json_emit_string(column_text(stmt,6));
      fputs(",\"added_by\":",stdout); json_emit_string(column_text(stmt,7));
      printf(",\"added_at\":%lld,\"added_by_id\":%lld",sqlite3_column_int64(stmt,8),sqlite3_column_int64(stmt,9));
      fputc('}',stdout);
    }
  }
  if(stmt!=NULL)sqlite3_finalize(stmt);
  printf("]}\n");
  sqlite3_close(db);
}

static int shared_prefetch(const struct music_config *cfg,long long song_id) {
  char path[VALUE_SIZE*2];

  if(song_id<=0)return 0;
  return store_song(cfg,song_id,path,sizeof(path),0);
}

static int shared_revision_matches(sqlite3 *db,int queue_id,long long expected) {
  struct shared_snapshot snap;

  if(expected<0)return 1;
  if(!shared_snapshot_load(db,queue_id,&snap))return 0;
  return snap.revision==expected;
}

static void shared_queue_action_cgi(const struct music_config *cfg,const char *action,const char *body,long long user_id) {
  sqlite3 *db;
  sqlite3_stmt *stmt;
  struct shared_snapshot snap;
  char queuebuf[32];
  char songbuf[64];
  char ids[POST_BODY_MAX+1];
  char dirbuf[32];
  char posbuf[64];
  char revbuf[64];
  int queue_id;
  int direction;
  int rc;
  int ok;
  long long requested_song;
  long long song_id;
  long long other_song;
  long long position;
  long long other_position;
  long long effective;
  long long expected;

  form_value(queuebuf,sizeof(queuebuf),body,"queue");
  form_value(songbuf,sizeof(songbuf),body,"song_id");
  form_value(ids,sizeof(ids),body,"ids");
  form_value(dirbuf,sizeof(dirbuf),body,"direction");
  form_value(posbuf,sizeof(posbuf),body,"position_ms");
  form_value(revbuf,sizeof(revbuf),body,"revision");
  queue_id=atoi(queuebuf);
  direction=atoi(dirbuf);
  requested_song=atoll(songbuf);
  position=atoll(posbuf);
  expected=revbuf[0]=='\0' ? -1 : atoll(revbuf);
  if(!shared_queue_valid(queue_id)) {
    printf("Status: 400 Bad Request\r\nContent-Type: application/json\r\nCache-Control: no-store\r\n\r\n{\"error\":\"invalid queue\"}\n");
    return;
  }
  if(position<0)position=0;
  if(position>86400000LL)position=86400000LL;

  if(strcmp(action,"shared_play")==0) {
    db=db_open(cfg,0);
    if(db==NULL)return;
    song_id=canonical_song_id(db,requested_song);
    sqlite3_close(db);
    if(song_id<=0 || !shared_prefetch(cfg,song_id)) {
      printf("Status: 409 Conflict\r\nContent-Type: application/json\r\nCache-Control: no-store\r\n\r\n{\"error\":\"track unavailable\"}\n");
      return;
    }
    db=db_open_web_write(cfg);
    if(db==NULL)return;
    ok=db_exec(db,"BEGIN IMMEDIATE");
    if(ok)ok=shared_queue_append(db,queue_id,song_id,user_id);
    effective=wall_ms()+SHARED_LEAD_MS;
    if(ok)ok=shared_queue_schedule(db,queue_id,song_id,1,position,effective,user_id);
    if(ok)db_exec(db,"COMMIT"); else db_exec(db,"ROLLBACK");
    sqlite3_close(db);
  } else if(strcmp(action,"shared_add")==0) {
    db=db_open_web_write(cfg);
    if(db==NULL)return;
    song_id=canonical_song_id(db,requested_song);
    ok=song_id>0 && db_exec(db,"BEGIN IMMEDIATE");
    if(ok)ok=shared_queue_append(db,queue_id,song_id,user_id);
    if(ok)ok=shared_queue_revision_bump(db,queue_id,user_id);
    if(ok)db_exec(db,"COMMIT"); else db_exec(db,"ROLLBACK");
    sqlite3_close(db);
  } else if(strcmp(action,"shared_addmany")==0) {
    db=db_open_web_write(cfg);
    if(db==NULL)return;
    ok=db_exec(db,"BEGIN IMMEDIATE");
    if(ok)ok=shared_queue_add_csv(db,queue_id,ids,NULL,user_id);
    if(ok)ok=shared_queue_revision_bump(db,queue_id,user_id);
    if(ok)db_exec(db,"COMMIT"); else db_exec(db,"ROLLBACK");
    sqlite3_close(db);
  } else if(strcmp(action,"shared_replace")==0) {
    db=db_open(cfg,0);
    if(db==NULL)return;
    song_id=shared_first_csv_song(db,ids);
    sqlite3_close(db);
    if(song_id<=0 || !shared_prefetch(cfg,song_id)) {
      printf("Status: 409 Conflict\r\nContent-Type: application/json\r\nCache-Control: no-store\r\n\r\n{\"error\":\"track unavailable\"}\n");
      return;
    }
    db=db_open_web_write(cfg);
    if(db==NULL)return;
    ok=db_exec(db,"BEGIN IMMEDIATE");
    stmt=NULL;
    if(ok && sqlite3_prepare_v2(db,"DELETE FROM shared_queue_song WHERE queue_id=?1",-1,&stmt,NULL)==SQLITE_OK) {
      sqlite3_bind_int(stmt,1,queue_id);
      ok=sqlite3_step(stmt)==SQLITE_DONE;
    } else if(ok)ok=0;
    if(stmt!=NULL)sqlite3_finalize(stmt);
    if(ok)ok=shared_queue_add_csv(db,queue_id,ids,&song_id,user_id);
    effective=wall_ms()+SHARED_LEAD_MS;
    if(ok)ok=shared_queue_schedule(db,queue_id,song_id,1,0,effective,user_id);
    if(ok)db_exec(db,"COMMIT"); else db_exec(db,"ROLLBACK");
    sqlite3_close(db);
  } else if(strcmp(action,"shared_clear")==0) {
    db=db_open_web_write(cfg);
    if(db==NULL)return;
    ok=db_exec(db,"BEGIN IMMEDIATE");
    stmt=NULL;
    if(ok && sqlite3_prepare_v2(db,"DELETE FROM shared_queue_song WHERE queue_id=?1",-1,&stmt,NULL)==SQLITE_OK) {
      sqlite3_bind_int(stmt,1,queue_id);
      ok=sqlite3_step(stmt)==SQLITE_DONE;
    } else if(ok)ok=0;
    if(stmt!=NULL)sqlite3_finalize(stmt);
    if(ok)ok=shared_queue_schedule(db,queue_id,0,0,0,wall_ms(),user_id);
    if(ok)db_exec(db,"COMMIT"); else db_exec(db,"ROLLBACK");
    sqlite3_close(db);
  } else if(strcmp(action,"shared_remove")==0) {
    db=db_open_web_write(cfg);
    if(db==NULL)return;
    song_id=canonical_song_id(db,requested_song);
    ok=song_id>0 && db_exec(db,"BEGIN IMMEDIATE");
    if(ok && !shared_snapshot_load(db,queue_id,&snap))ok=0;
    stmt=NULL;
    if(ok && sqlite3_prepare_v2(db,"DELETE FROM shared_queue_song WHERE queue_id=?1 AND song_id=?2",-1,&stmt,NULL)==SQLITE_OK) {
      sqlite3_bind_int(stmt,1,queue_id);
      sqlite3_bind_int64(stmt,2,song_id);
      ok=sqlite3_step(stmt)==SQLITE_DONE;
    } else if(ok)ok=0;
    if(stmt!=NULL)sqlite3_finalize(stmt);
    if(ok && snap.current_song_id==song_id)ok=shared_queue_schedule(db,queue_id,0,0,0,wall_ms(),user_id);
    else if(ok)ok=shared_queue_revision_bump(db,queue_id,user_id);
    if(ok)db_exec(db,"COMMIT"); else db_exec(db,"ROLLBACK");
    sqlite3_close(db);
  } else if(strcmp(action,"shared_move")==0) {
    db=db_open_web_write(cfg);
    if(db==NULL)return;
    song_id=canonical_song_id(db,requested_song);
    ok=song_id>0 && (direction==1 || direction==-1) && db_exec(db,"BEGIN IMMEDIATE");
    position=0;
    other_song=0;
    other_position=0;
    stmt=NULL;
    if(ok && sqlite3_prepare_v2(db,"SELECT position FROM shared_queue_song WHERE queue_id=?1 AND song_id=?2",-1,&stmt,NULL)==SQLITE_OK) {
      sqlite3_bind_int(stmt,1,queue_id);
      sqlite3_bind_int64(stmt,2,song_id);
      if(sqlite3_step(stmt)==SQLITE_ROW)position=sqlite3_column_int64(stmt,0); else ok=0;
    } else if(ok)ok=0;
    if(stmt!=NULL)sqlite3_finalize(stmt);
    stmt=NULL;
    if(ok) {
      if(direction<0)rc=sqlite3_prepare_v2(db,"SELECT song_id,position FROM shared_queue_song WHERE queue_id=?1 AND position<?2 ORDER BY position DESC LIMIT 1",-1,&stmt,NULL);
      else rc=sqlite3_prepare_v2(db,"SELECT song_id,position FROM shared_queue_song WHERE queue_id=?1 AND position>?2 ORDER BY position LIMIT 1",-1,&stmt,NULL);
      if(rc==SQLITE_OK) {
        sqlite3_bind_int(stmt,1,queue_id);
        sqlite3_bind_int64(stmt,2,position);
        if(sqlite3_step(stmt)==SQLITE_ROW) {
          other_song=sqlite3_column_int64(stmt,0);
          other_position=sqlite3_column_int64(stmt,1);
        }
      }
    }
    if(stmt!=NULL)sqlite3_finalize(stmt);
    if(ok && other_song>0) {
      stmt=NULL;
      if(sqlite3_prepare_v2(db,"UPDATE shared_queue_song SET position=-1 WHERE queue_id=?1 AND song_id=?2",-1,&stmt,NULL)==SQLITE_OK) {
        sqlite3_bind_int(stmt,1,queue_id); sqlite3_bind_int64(stmt,2,song_id); ok=sqlite3_step(stmt)==SQLITE_DONE;
      } else ok=0;
      if(stmt!=NULL)sqlite3_finalize(stmt);
      stmt=NULL;
      if(ok && sqlite3_prepare_v2(db,"UPDATE shared_queue_song SET position=?1 WHERE queue_id=?2 AND song_id=?3",-1,&stmt,NULL)==SQLITE_OK) {
        sqlite3_bind_int64(stmt,1,position); sqlite3_bind_int(stmt,2,queue_id); sqlite3_bind_int64(stmt,3,other_song); ok=sqlite3_step(stmt)==SQLITE_DONE;
      } else if(ok)ok=0;
      if(stmt!=NULL)sqlite3_finalize(stmt);
      stmt=NULL;
      if(ok && sqlite3_prepare_v2(db,"UPDATE shared_queue_song SET position=?1 WHERE queue_id=?2 AND song_id=?3",-1,&stmt,NULL)==SQLITE_OK) {
        sqlite3_bind_int64(stmt,1,other_position); sqlite3_bind_int(stmt,2,queue_id); sqlite3_bind_int64(stmt,3,song_id); ok=sqlite3_step(stmt)==SQLITE_DONE;
      } else if(ok)ok=0;
      if(stmt!=NULL)sqlite3_finalize(stmt);
    }
    if(ok)ok=shared_queue_revision_bump(db,queue_id,user_id);
    if(ok)db_exec(db,"COMMIT"); else db_exec(db,"ROLLBACK");
    sqlite3_close(db);
  } else if(strcmp(action,"shared_pause")==0) {
    db=db_open_web_write(cfg);
    if(db==NULL)return;
    ok=db_exec(db,"BEGIN IMMEDIATE") && shared_snapshot_load(db,queue_id,&snap) && shared_revision_matches(db,queue_id,expected);
    effective=wall_ms()+SHARED_LEAD_MS;
    if(ok && snap.current_song_id>0)ok=shared_queue_schedule(db,queue_id,snap.current_song_id,0,shared_position_at(&snap,effective),effective,user_id);
    if(ok)db_exec(db,"COMMIT"); else db_exec(db,"ROLLBACK");
    sqlite3_close(db);
  } else if(strcmp(action,"shared_resume")==0 || strcmp(action,"shared_next")==0 || strcmp(action,"shared_prev")==0 || strcmp(action,"shared_ended")==0) {
    db=db_open(cfg,0);
    if(db==NULL)return;
    ok=shared_snapshot_load(db,queue_id,&snap) && shared_revision_matches(db,queue_id,expected);
    song_id=0;
    position=0;
    if(ok && strcmp(action,"shared_resume")==0) {
      song_id=snap.current_song_id;
      position=snap.base_position_ms;
      if(song_id<=0)song_id=shared_queue_neighbor(db,queue_id,0,1);
    } else if(ok && strcmp(action,"shared_prev")==0) {
      song_id=shared_queue_neighbor(db,queue_id,snap.current_song_id,-1);
      if(song_id<=0)song_id=snap.current_song_id;
    } else if(ok) {
      song_id=shared_queue_neighbor(db,queue_id,snap.current_song_id,1);
    }
    sqlite3_close(db);
    if(!ok) {
      shared_queue_state_cgi(cfg,queue_id);
      return;
    }
    if(song_id<=0) {
      db=db_open_web_write(cfg);
      if(db!=NULL) {
        if(db_exec(db,"BEGIN IMMEDIATE")) {
          effective=wall_ms()+SHARED_LEAD_MS;
          if(shared_queue_schedule(db,queue_id,snap.current_song_id,0,shared_position_at(&snap,effective),effective,user_id))db_exec(db,"COMMIT"); else db_exec(db,"ROLLBACK");
        }
        sqlite3_close(db);
      }
    } else if(shared_prefetch(cfg,song_id)) {
      db=db_open_web_write(cfg);
      if(db!=NULL) {
        ok=db_exec(db,"BEGIN IMMEDIATE") && shared_revision_matches(db,queue_id,expected);
        effective=wall_ms()+SHARED_LEAD_MS;
        if(ok)ok=shared_queue_schedule(db,queue_id,song_id,1,position,effective,user_id);
        if(ok)db_exec(db,"COMMIT"); else db_exec(db,"ROLLBACK");
        sqlite3_close(db);
      }
    }
  } else {
    printf("Status: 400 Bad Request\r\nContent-Type: application/json\r\nCache-Control: no-store\r\n\r\n{\"error\":\"invalid action\"}\n");
    return;
  }
  shared_queue_state_cgi(cfg,queue_id);
}

static void auth_page_start(const char *title) {
  html_header(title);
  printf("<main style='max-width:640px'><div class='list'><div class='row'>");
}

static void auth_page_end(void) {
  printf("</div></div></main></body></html>\n");
}

static void privacy_notice_page(void) {
  const char *admin;

  admin=getenv("SERVER_ADMIN");
  auth_page_start("Privacy");
  printf("<strong style='font-size:24px'>Privacy and cookie information</strong>");
  printf("<p>This Music service uses only first-party technical cookies required for access and authentication. It does not use advertising, analytics, profiling or third-party cookies.</p>");
  printf("<p><b>music_session</b>: authenticates the user. It expires after 30 days of inactivity and is protected with Secure, HttpOnly and SameSite=Lax. <b>music_notice</b>: remembers that this information has been read for one year.</p>");
  printf("<p>The service stores the account name, a salted password hash, login sessions, personal playlists, shared-queue contributions and aggregate per-account usage statistics: login count, last login, last activity, playback count and last playback time. It does not keep a per-user history of which individual tracks were listened to. Shared-queue entries record the user who added each track and the addition time; the queue state also records the user responsible for its latest change. Passwords, IP addresses and browser User-Agent values are not stored by the Music application. Session identifiers are stored server-side only as cryptographic hashes.</p>");
  printf("<p>These data are used only to provide and secure the requested Music service and to associate personal playlists with the correct account. The application does not perform profiling or automated decision-making and does not disclose authentication data to advertising or analytics providers.</p>");
  printf("<p>Account data and aggregate usage statistics are kept until the account is deleted; session records expire after 30 days of inactivity. Shared-queue track attribution remains while the entry remains in that queue, and latest-change attribution remains until a later queue change; deleting the account removes those user references and the associated aggregate statistics. You may request access, rectification, erasure, restriction or other rights applicable to your personal data from the service administrator, and you may lodge a complaint with the competent data protection supervisory authority.</p>");
  if(admin!=NULL && *admin!='\0') { printf("<p>Service administrator contact: "); html_text(admin); printf(".</p>"); }
  printf("<p>The authentication cookie is strictly necessary for the requested service and therefore does not rely on optional cookie consent. The button below records that you have read and accepted this privacy information.</p>");
  printf("<form method='post'><input type='hidden' name='action' value='privacy_accept'><button class='search' type='submit'>I HAVE READ AND ACCEPT</button></form>");
  auth_page_end();
}

static void login_page(const struct music_config *cfg,int failed) {
  auth_page_start("Login");
  printf("<strong style='font-size:24px'>Music login</strong>");
  if(user_count(cfg)==0)printf("<div class='notice'>No users are configured. Create the first account with <b>./music user add NAME</b>.</div>");
  if(failed)printf("<div class='notice'>Invalid username or password.</div>");
  printf("<form method='post' autocomplete='on'><input type='hidden' name='action' value='login'><p><input class='search' name='username' maxlength='64' autocomplete='username' placeholder='Username' required></p><p><input class='search' type='password' name='password' autocomplete='current-password' placeholder='Password' required></p><button class='search' type='submit'>LOGIN</button></form>");
  printf("<p class='muted' style='margin-top:18px'><a href='?view=privacy'>Privacy and cookie information</a></p>");
  auth_page_end();
}

static void service_worker_cgi(long long user_id) {
  printf("Content-Type: application/javascript; charset=utf-8\r\n");
  printf("Cache-Control: no-cache\r\n");
  printf("Service-Worker-Allowed: /\r\n\r\n");
  printf("const MEDIA='music-offline-media-v2-%lld',SHELL_PREFIX='music-shell-%lld-',SHELL=SHELL_PREFIX+'%s',APP=self.location.pathname;\n",user_id,user_id,MUSIC_VERSION);
  printf("self.addEventListener('install',e=>{e.waitUntil(caches.open(SHELL).then(c=>fetch(APP,{cache:'no-store'}).then(r=>{if(r.ok)return c.put(APP,r)}).catch(()=>{})));self.skipWaiting()});\n");
  printf("self.addEventListener('activate',e=>e.waitUntil(caches.keys().then(k=>Promise.all(k.filter(n=>n.startsWith(SHELL_PREFIX)&&n!==SHELL).map(n=>caches.delete(n)))).then(()=>self.clients.claim())));\n");
  printf("function ranged(r,q){const h=q.headers.get('range');if(!h)return Promise.resolve(r);return r.arrayBuffer().then(b=>{const n=b.byteLength,m=/bytes=(\\d*)-(\\d*)/.exec(h);let a,z;if(!m)return new Response(b,{status:200,headers:r.headers});if(m[1]){a=parseInt(m[1],10);z=m[2]?parseInt(m[2],10):n-1}else{z=n-1;a=Math.max(0,n-parseInt(m[2],10))}if(a<0||a>=n||z<a)return new Response(null,{status:416,headers:{'Content-Range':'bytes */'+n}});if(z>=n)z=n-1;const x=new Headers(r.headers),v=b.slice(a,z+1);x.set('Content-Range','bytes '+a+'-'+z+'/'+n);x.set('Content-Length',String(v.byteLength));x.set('Accept-Ranges','bytes');return new Response(v,{status:206,statusText:'Partial Content',headers:x})})}\n");
  printf("self.addEventListener('fetch',e=>{const u=new URL(e.request.url);if(u.origin!==location.origin)return;if(u.pathname===APP&&u.searchParams.has('media')){e.respondWith(caches.open(MEDIA).then(c=>c.match(u.href).then(r=>r?ranged(r,e.request):fetch(e.request))));return}if(e.request.mode==='navigate'&&u.pathname===APP)e.respondWith(fetch(e.request).then(r=>{const x=r.clone();caches.open(SHELL).then(c=>c.put(APP,x));return r}).catch(()=>caches.open(SHELL).then(c=>c.match(APP))))});\n");
}

static void html_header(const char *title) {
  printf("Content-Type: text/html; charset=utf-8\r\n");
  printf("Cache-Control: no-store\r\n\r\n");
  printf("<!doctype html><html lang='en'><head><meta charset='utf-8'>");
  printf("<meta name='viewport' content='width=device-width,initial-scale=1,viewport-fit=cover'>");
  printf("<meta name='theme-color' content='#161616'><title>");
  html_text(title);
  printf(" - Music</title><style>");
  printf("*{box-sizing:border-box}html,body{margin:0;padding:0}body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;background:#f4f4f2;color:#181818;padding-bottom:78px}");
  printf("header{position:sticky;top:0;z-index:5;background:rgba(244,244,242,.96);backdrop-filter:blur(14px);padding:14px 18px;border-bottom:1px solid #ddd}header h1{font-size:27px;margin:0}header small{color:#777}");
  printf(".headrow{max-width:960px;margin:auto;display:flex;align-items:center;gap:14px}.headtitle{flex:1}.mode{display:flex;background:#ddd;border-radius:12px;padding:3px}.mode button{border:0;background:transparent;border-radius:9px;padding:8px 11px;font-size:12px;font-weight:700}.mode [data-offline-count]{font:inherit}.storageGrid [data-offline-count]{font:inherit;color:inherit}.mode button.active{background:white;box-shadow:0 1px 4px #bbb}");
  printf("main{max-width:960px;margin:auto;padding:16px}.search{width:100%%;font-size:18px;padding:13px 15px;border:1px solid #ccc;border-radius:14px;background:white}");
  printf(".grid{display:grid;grid-template-columns:repeat(2,1fr);gap:12px;margin-top:16px}.card{display:block;text-decoration:none;color:inherit;background:white;border:1px solid #e1e1df;border-radius:16px;padding:18px;min-height:94px}.card strong{display:block;font-size:20px;margin-bottom:5px}.card span{color:#777;font-size:14px}");
  printf(".list{margin-top:12px;background:white;border:1px solid #e1e1df;border-radius:16px;overflow:hidden}.row{display:block;text-decoration:none;color:inherit;padding:14px 16px;border-bottom:1px solid #eee}.row:last-child{border-bottom:0}.row strong,.row span{display:block}.row span{color:#777;font-size:14px;margin-top:2px}.song{cursor:pointer}.song.current{background:#ecece7}.metric{display:inline!important}.badge{display:inline!important;font-size:11px!important;border:1px solid #bbb;border-radius:5px;padding:1px 4px;margin-left:5px;color:#555!important}.offlineAction,.queueAction,.albumQueueAction,.albumSaveAction,.albumRemoveAction{display:inline-block!important;float:right;border:1px solid #aaa;border-radius:8px;padding:3px 7px;margin:-2px 0 0 8px!important;font-size:11px!important;color:#333!important;background:#f8f8f8;cursor:pointer}.offlineAction.saved{font-weight:700;background:#e9eee8}.albumOfflineState{display:inline!important}.albumRow{background:#ecece8;border-left:5px solid #555;padding:17px 16px 17px 14px}.albumRow strong{font-size:18px}.albumRow:hover{background:#e5e5df}.albumType{display:inline-block!important;font-size:10px!important;font-weight:700;letter-spacing:.08em;color:#555!important;border:1px solid #999;border-radius:5px;padding:2px 5px;margin:0 0 6px 0!important;background:#f8f8f5}.trackRow{padding-left:30px}.albumBar{margin:12px 0;background:#ecece8;border:1px solid #c8c8c2;border-left:5px solid #555;border-radius:14px;padding:16px}.albumBar strong{font-size:20px}.albumBar strong,.albumBar span{display:block}.albumBar span{color:#666;font-size:14px;margin-top:3px}");
  printf(".player{max-width:960px;margin:10px auto 0;padding:0 16px;display:flex;justify-content:flex-end}.playerbox{width:min(520px,100%%)}.playerin{display:flex;align-items:center;gap:8px;background:white;border:1px solid #ddd;border-radius:14px;padding:8px 10px;box-shadow:0 2px 8px rgba(0,0,0,.06)}.track{flex:1;min-width:0;text-align:right}.track strong,.track span{display:block;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}.track strong{font-size:14px}.track span{font-size:12px;color:#777}.pbut{border:0;background:#eee;border-radius:50%%;width:34px;height:34px;font-size:15px;line-height:34px;padding:0;cursor:pointer}.pbut.main{background:#222;color:white}.qbut{border:1px solid #bbb;background:#f8f8f8;border-radius:9px;padding:7px 9px;font-size:11px;font-weight:700;cursor:pointer}.qselect{border:1px solid #bbb;background:#f8f8f8;border-radius:9px;padding:7px 5px;font-size:11px;font-weight:700;cursor:pointer;max-width:82px}.queuePanel{margin-top:6px;background:white;border:1px solid #ddd;border-radius:14px;box-shadow:0 2px 8px rgba(0,0,0,.06);overflow:hidden}.queueHead{display:flex;align-items:center;justify-content:space-between;padding:9px 11px;border-bottom:1px solid #eee}.queueList{max-height:300px;overflow:auto}.queueRow{display:flex;align-items:center;gap:8px;padding:9px 11px;border-bottom:1px solid #eee}.queueRow:last-child{border-bottom:0}.queueText{flex:1;min-width:0}.queueText strong,.queueText span{display:block;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}.queueText strong{font-size:13px}.queueText span{font-size:11px;color:#777}.queueControls{display:flex;gap:3px}.queueControls button{border:1px solid #bbb;background:#f8f8f8;border-radius:6px;padding:4px 6px;font-size:11px;cursor:pointer}.queueRow.current{background:#ecece7}");
  printf("nav{position:fixed;bottom:0;left:0;right:0;height:62px;z-index:8;background:#fff;border-top:1px solid #ddd;display:flex;justify-content:center;padding-bottom:env(safe-area-inset-bottom)}nav a{width:25%%;max-width:240px;text-decoration:none;color:#555;text-align:center;padding:9px 2px 5px;font-size:12px}nav b{display:block;font-size:20px;line-height:22px;color:#111}");
  printf(".notice{margin-top:16px;background:#fff8d8;border:1px solid #eadb96;border-radius:14px;padding:14px;color:#5f5325}.muted{color:#777}.stats{margin-top:14px;color:#666;font-size:14px}.offlineEmpty{text-align:center;padding:42px 18px;color:#777}.inlineForm{display:inline-block;margin:4px 6px 0 0}.inlineForm button,.inlineForm select{font-size:12px;padding:4px 7px;border:1px solid #aaa;border-radius:7px;background:#f8f8f8}.playlistTools{margin-top:12px}.playlistTools input,.playlistTools textarea,.playlistTools select{width:100%%;margin:5px 0;padding:9px;border:1px solid #ccc;border-radius:9px;background:white}.playlistTools button{margin:4px 6px 0 0;padding:7px 10px;border:1px solid #aaa;border-radius:8px;background:#f8f8f8}");
  printf("[hidden]{display:none!important}@media(min-width:700px){.grid{grid-template-columns:repeat(4,1fr)}}");
  printf("</style></head><body>");
}

static void page_top(const char *title,const char *username,long long user_id,const char *csrf) {
  printf("<header data-user='");
  html_text(username==NULL ? "" : username);
  printf("' data-user-id='%lld' data-csrf='",user_id);
  html_text(csrf==NULL ? "" : csrf);
  printf("'><div class='headrow'><div class='headtitle'><h1>");
  html_text(title);
  printf("</h1><small>Music %s",MUSIC_VERSION);
  if(username!=NULL && *username!='\0') { printf(" - "); html_text(username); }
  printf("</small></div>");
  printf("<form method='post' style='margin:0'><input type='hidden' name='action' value='logout'><input type='hidden' name='csrf' value='");
  html_text(csrf==NULL ? "" : csrf);
  printf("'><button type='submit' style='border:0;background:transparent;font-size:12px'>LOGOUT</button></form>");
  printf("<div class='mode'><button id='modeOnline' type='button' onclick=\"setMusicMode('online')\">ONLINE</button><button id='modeOffline' type='button' onclick=\"setMusicMode('offline')\">OFFLINE <span data-offline-count>0</span></button></div></div></header><main>");
}

static void nav(void) {
  printf("<nav><a href='?view=library'><b>\342\231\253</b>Library</a><a href='?view=search'><b>\342\214\225</b>Search</a><a href='?view=playlists'><b>\342\230\260</b>Playlists</a><a href='?view=more'><b>\342\200\242</b>More</a></nav>");
}

static void player(void) {
  printf("<div class='player'><div class='playerbox'><div class='playerin'>");
  printf("<button class='pbut' type='button' onclick='prevSong()' aria-label='Previous'>&#9664;</button>");
  printf("<button id='playerToggle' class='pbut main' type='button' onclick='togglePlay()' aria-label='Play or pause'>&#9654;</button>");
  printf("<div class='track'><strong id='playerTitle'>No track selected</strong><span id='playerArtist'>Music %s</span></div>",MUSIC_VERSION);
  printf("<button class='pbut' type='button' onclick='nextSong()' aria-label='Next'>&#9654;</button>");
  printf("<select id='queueMode' class='qselect' onchange='setQueueMode(this.value)'><option value='private'>PRIVATE</option><option value='0'>C0</option><option value='1'>C1</option><option value='2'>C2</option><option value='3'>C3</option><option value='4'>C4</option><option value='5'>C5</option><option value='6'>C6</option><option value='7'>C7</option><option value='8'>C8</option><option value='9'>C9</option></select>");
  printf("<button id='queueToggle' class='qbut' type='button' onclick='toggleQueue()'>QUEUE 0</button>");
  printf("</div><div id='queuePanel' class='queuePanel' hidden><div class='queueHead'><strong id='queueTitle'>Private queue</strong><button class='qbut' type='button' onclick='queueClear()'>CLEAR</button></div><div id='queueList' class='queueList'></div></div><audio id='Player' preload='metadata'></audio></div></div>");
  printf("<script>");
  printf("const USER_ID=document.querySelector('header').dataset.userId||'0',CSRF_TOKEN=document.querySelector('header').dataset.csrf||'',MEDIA_CACHE='music-offline-media-v2-'+USER_ID,META_KEY='musicOfflineTracksV2:'+USER_ID,MODE_KEY='musicModeV2:'+USER_ID,QUEUE_KEY='musicQueueV1:'+USER_ID,QUEUE_MODE_KEY='musicQueueModeV1:'+USER_ID;var musicSongs=[],musicIndex=-1,musicPlayer=null,musicMode='online',offlineBlob=null,musicQueue=[],queueIndex=-1,currentTrack=null,sharedQueueId=-1,sharedRevision=-1,sharedState=null,sharedClockOffset=0,sharedBestRtt=1e9,sharedPollTimer=null,sharedApplyTimer=null,sharedBusy=false,sharedPlayedId='';");
  printf("function mediaUrl(id){return new URL('?media='+encodeURIComponent(id),location.href).href}");
  printf("function humanBytes(n){var u=['B','KB','MB','GB','TB'],i=0,v=Number(n)||0;while(v>=1024&&i<u.length-1){v/=1024;i++}return(i?v.toFixed(1):Math.round(v))+' '+u[i]}");
  printf("function migrateOffline(){const om='musicOfflineTracks',oc='music-offline-media-v1',od='musicMode';if(localStorage.getItem(META_KEY)===null&&localStorage.getItem(om)!==null){localStorage.setItem(META_KEY,localStorage.getItem(om));localStorage.removeItem(om);if(localStorage.getItem(MODE_KEY)===null&&localStorage.getItem(od)!==null)localStorage.setItem(MODE_KEY,localStorage.getItem(od));localStorage.removeItem(od);if('caches'in window)caches.open(oc).then(c=>c.keys().then(v=>caches.open(MEDIA_CACHE).then(n=>Promise.all(v.map(r=>c.match(r).then(x=>x?n.put(r,x):null)))))).then(()=>caches.delete(oc)).catch(function(){})}}");
  printf("function offlineMeta(){try{return JSON.parse(localStorage.getItem(META_KEY)||'{}')}catch(e){return {}}}");
  printf("function saveOfflineMeta(m){localStorage.setItem(META_KEY,JSON.stringify(m))}");
  printf("function updateOfflineStats(){const m=offlineMeta(),v=Object.keys(m).map(k=>m[k]),n=v.length,b=v.reduce((a,x)=>a+(Number(x.bytes)||0),0);document.querySelectorAll('[data-offline-count]').forEach(e=>e.textContent=String(n));document.querySelectorAll('[data-offline-size]').forEach(e=>e.textContent=humanBytes(b))}");
  printf("function trackData(s){return{id:String(s.dataset.id),acrid:s.dataset.acrid||'',title:s.dataset.title||'',artist:s.dataset.artist||'',album:s.dataset.album||'',duration:parseInt(s.dataset.duration,10)||0}}");
  printf("function queueKey(x){return x.acrid||('id:'+x.id)}function isShared(){return sharedQueueId>=0}function loadQueue(){try{const q=JSON.parse(sessionStorage.getItem(QUEUE_KEY)||'[]');return Array.isArray(q)?q:[]}catch(e){return []}}function saveQueue(){if(!isShared())sessionStorage.setItem(QUEUE_KEY,JSON.stringify(musicQueue))}");
  printf("function renderQueue(){const box=document.getElementById('queueList'),b=document.getElementById('queueToggle'),t=document.getElementById('queueTitle');if(!box||!b)return;b.textContent=(isShared()?'C'+sharedQueueId:'QUEUE')+' '+musicQueue.length;if(t)t.textContent=isShared()?'C'+sharedQueueId+' shared queue':'Private queue';box.textContent='';if(!musicQueue.length){const e=document.createElement('div');e.className='offlineEmpty';e.textContent='Queue is empty.';box.appendChild(e);return}musicQueue.forEach((x,i)=>{const r=document.createElement('div'),tx=document.createElement('div'),st=document.createElement('strong'),d=document.createElement('span'),c=document.createElement('div');r.className='queueRow'+(i===queueIndex?' current':'');tx.className='queueText';st.textContent=x.title||'Untitled';d.textContent=(x.artist||'')+(x.album?' - '+x.album:'')+(isShared()&&x.addedBy?' - added by '+x.addedBy:'');tx.appendChild(st);tx.appendChild(d);c.className='queueControls';[['PLAY',()=>playQueue(i)],['UP',()=>queueMove(i,-1)],['DOWN',()=>queueMove(i,1)],['X',()=>queueRemove(i)]].forEach(y=>{const z=document.createElement('button');z.type='button';z.textContent=y[0];z.addEventListener('click',y[1]);c.appendChild(z)});r.appendChild(tx);r.appendChild(c);box.appendChild(r)})}");
  printf("function toggleQueue(){const p=document.getElementById('queuePanel');p.hidden=!p.hidden;if(!p.hidden)renderQueue()}");
  printf("function sharedClock(s,t0,t1){const r=t1-t0,o=Number(s.server_ms)-(t0+t1)/2;if(r<sharedBestRtt){sharedBestRtt=r;sharedClockOffset=o}else if(r<sharedBestRtt+100)sharedClockOffset=sharedClockOffset*.9+o*.1}function sharedNow(){return Date.now()+sharedClockOffset}");
  printf("function sharedNormalize(s){s.items=(s.items||[]).map(x=>({id:String(x.id),acrid:x.acrid||'',title:x.title||'',artist:x.artist||'',album:x.album||'',duration:Number(x.duration)||0,position:Number(x.position)||0,addedBy:x.added_by||'',addedAt:Number(x.added_at)||0}));return s}");
  printf("function sharedPrepare(d){if(!d)return;setPlayerText(d);if(!currentTrack||String(currentTrack.id)!==String(d.id)||musicPlayer.dataset.id!==String(d.id)){currentTrack=d;musicPlayer.dataset.id=String(d.id);musicPlayer.src=mediaUrl(d.id);musicPlayer.load()}else currentTrack=d}");
  printf("function sharedSeek(s,play){const rev=sharedRevision,go=()=>{var target,e,now;if(rev!==sharedRevision)return;now=sharedNow();target=Number(s.base_position_ms)/1000;if(play&&now>Number(s.base_time_ms))target+=(now-Number(s.base_time_ms))/1000;e=musicPlayer.currentTime-target;if(Math.abs(e)>.25){try{musicPlayer.currentTime=Math.max(0,target)}catch(x){}musicPlayer.playbackRate=1}else if(play&&!musicPlayer.paused){if(e<-.08)musicPlayer.playbackRate=1.02;else if(e>.08)musicPlayer.playbackRate=.98;else musicPlayer.playbackRate=1}if(play&&musicPlayer.paused)musicPlayer.play().catch(()=>{document.getElementById('playerArtist').textContent=(currentTrack.artist||'')+' - TAP PLAY TO SYNC'})};if(musicPlayer.readyState>=1)go();else musicPlayer.addEventListener('loadedmetadata',go,{once:true})}");
  printf("function sharedApply(s,force){var now,d,delay,id;sharedState=sharedNormalize(s);sharedRevision=Number(s.revision);musicQueue=sharedState.items;queueIndex=musicQueue.findIndex(x=>String(x.id)===String(s.current_id));renderQueue();if(sharedApplyTimer){clearTimeout(sharedApplyTimer);sharedApplyTimer=null}if(!Number(s.current_id)){musicPlayer.pause();musicPlayer.playbackRate=1;currentTrack=null;sharedPlayedId='';document.getElementById('playerTitle').textContent='No track selected';document.getElementById('playerArtist').textContent='C'+sharedQueueId;return}d=musicQueue.find(x=>String(x.id)===String(s.current_id));if(!d)return;now=sharedNow();if(!force&&now<Number(s.base_time_ms)){delay=Math.max(10,Number(s.base_time_ms)-now);if(!currentTrack||musicPlayer.paused)sharedPrepare(d);sharedApplyTimer=setTimeout(()=>sharedApply(sharedState,true),delay+5);return}id=String(d.id);if(Number(s.state)===1&&sharedPlayedId!==id){sharedPlayedId=id;fetch('?event=played&id='+encodeURIComponent(id),{cache:'no-store'}).catch(function(){})}sharedPrepare(d);if(Number(s.state)===1)sharedSeek(s,true);else{musicPlayer.pause();musicPlayer.playbackRate=1;sharedSeek(s,false)}}");
  printf("function sharedAccept(s,t0){const t1=Date.now();sharedClock(s,t0,t1);sharedApply(s,false)}");
  printf("function sharedPoll(){var t0;if(!isShared()||sharedBusy)return;t0=Date.now();sharedBusy=true;fetch('?shared=state&queue='+sharedQueueId,{cache:'no-store'}).then(r=>{if(!r.ok)throw 0;return r.json()}).then(s=>sharedAccept(s,t0)).catch(function(){}).finally(()=>{sharedBusy=false})}");
  printf("function sharedPost(action,data){var fd,t0;if(!isShared())return Promise.resolve();fd=new URLSearchParams();fd.set('action',action);fd.set('queue',String(sharedQueueId));fd.set('csrf',CSRF_TOKEN);fd.set('revision',String(sharedRevision));Object.keys(data||{}).forEach(k=>fd.set(k,String(data[k])));t0=Date.now();sharedBusy=true;return fetch(location.pathname,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded;charset=UTF-8'},body:fd,cache:'no-store'}).then(r=>{if(!r.ok)throw 0;return r.json()}).then(s=>sharedAccept(s,t0)).catch(()=>alert('Shared queue command failed')).finally(()=>{sharedBusy=false})}");
  printf("function setQueueMode(v,initial){const sel=document.getElementById('queueMode');let q;if(sharedPollTimer){clearInterval(sharedPollTimer);sharedPollTimer=null}if(sharedApplyTimer){clearTimeout(sharedApplyTimer);sharedApplyTimer=null}sharedPlayedId='';if(v==='private'){sharedQueueId=-1;sharedRevision=-1;sharedState=null;musicQueue=loadQueue();queueIndex=-1;if(sel)sel.value='private';localStorage.setItem(QUEUE_MODE_KEY,'private');renderQueue();return}q=parseInt(v,10);if(!(q>=0&&q<10)){setQueueMode('private',initial);return}if(musicMode!=='online')setMusicMode('online');sharedQueueId=q;sharedRevision=-1;sharedState=null;musicQueue=[];queueIndex=-1;if(sel)sel.value=String(q);localStorage.setItem(QUEUE_MODE_KEY,String(q));renderQueue();sharedPoll();sharedPollTimer=setInterval(sharedPoll,500)}");
  printf("function queueAdd(d){if(isShared()){sharedPost('shared_add',{song_id:d.id});return}const k=queueKey(d);if(!musicQueue.some(x=>queueKey(x)===k)){musicQueue.push(d);saveQueue()}renderQueue()}");
  printf("function queueAddMany(v){if(isShared()){sharedPost('shared_addmany',{ids:v.map(x=>x.id).join(',')});return}v.forEach(d=>{const k=queueKey(d);if(!musicQueue.some(x=>queueKey(x)===k))musicQueue.push(d)});saveQueue();renderQueue()}");
  printf("function playMany(v){if(isShared()){sharedPost('shared_replace',{ids:v.map(x=>x.id).join(',')});return}const seen={};musicQueue=v.filter(d=>{const k=queueKey(d);if(seen[k])return false;seen[k]=1;return true});queueIndex=-1;saveQueue();renderQueue();if(musicQueue.length)playQueue(0)}");
  printf("function pageTracks(){return Array.from(document.querySelectorAll('#onlinePane .song')).map(trackData)}function queueCurrentPage(){queueAddMany(pageTracks())}function playCurrentPage(){playMany(pageTracks())}");
  printf("function queueRemove(i){if(i<0||i>=musicQueue.length)return;if(isShared()){sharedPost('shared_remove',{song_id:musicQueue[i].id});return}if(i===queueIndex)queueIndex=-1;else if(i<queueIndex)queueIndex--;musicQueue.splice(i,1);saveQueue();renderQueue()}");
  printf("function queueMove(i,d){const j=i+d;if(i<0||i>=musicQueue.length||j<0||j>=musicQueue.length)return;if(isShared()){sharedPost('shared_move',{song_id:musicQueue[i].id,direction:d});return}const x=musicQueue[i];musicQueue[i]=musicQueue[j];musicQueue[j]=x;if(queueIndex===i)queueIndex=j;else if(queueIndex===j)queueIndex=i;saveQueue();renderQueue()}");
  printf("function queueClear(){if(isShared()){sharedPost('shared_clear',{});return}musicQueue=[];queueIndex=-1;saveQueue();renderQueue()}");
  printf("function bindSongs(){const pane=musicMode==='offline'?document.getElementById('offlinePane'):document.getElementById('onlinePane');if(!pane)return;musicSongs=Array.prototype.slice.call(pane.querySelectorAll('.song'));musicSongs.forEach(function(s,i){s.dataset.index=String(i);let q=s.querySelector('.queueAction');if(!q){q=document.createElement('span');q.className='queueAction';q.textContent='+ QUEUE';const line=s.querySelector('span');if(line)line.prepend(q);q.addEventListener('click',function(e){e.preventDefault();e.stopPropagation();queueAdd(trackData(s))})}if(s.dataset.bound)return;s.dataset.bound='1';s.addEventListener('click',function(e){if(e.target.closest('.offlineAction,.queueAction,form,button,select,input,textarea'))return;e.preventDefault();playSong(parseInt(s.dataset.index,10))});const a=s.querySelector('.offlineAction');if(a)a.addEventListener('click',function(e){e.preventDefault();e.stopPropagation();toggleOffline(s,a)})})}");
  printf("function setPlayerText(d){document.getElementById('playerTitle').textContent=d.title||'Untitled';document.getElementById('playerArtist').textContent=(d.artist||'')+(d.album?' - '+d.album:'')}");
  printf("function onlinePlayData(d){const p=document.querySelector('#onlinePane .song[data-id=\\\"'+CSS.escape(String(d.id))+'\\\"] .played');if(p)p.textContent=String((parseInt(p.textContent,10)||0)+1);fetch('?event=played&id='+encodeURIComponent(d.id),{cache:'no-store'}).catch(function(){});musicPlayer.src=mediaUrl(d.id);musicPlayer.play().catch(function(){})}");
  printf("function offlinePlayData(d){caches.open(MEDIA_CACHE).then(c=>c.match(mediaUrl(d.id))).then(r=>{if(!r)throw 0;return r.blob()}).then(b=>{if(offlineBlob)URL.revokeObjectURL(offlineBlob);offlineBlob=URL.createObjectURL(b);musicPlayer.src=offlineBlob;musicPlayer.play().catch(function(){})}).catch(()=>alert('Track is not available offline'))}");
  printf("function playData(d,qi){if(!d)return;currentTrack=d;queueIndex=typeof qi==='number'?qi:-1;setPlayerText(d);musicPlayer.dataset.id=d.id;document.querySelectorAll('.song.current').forEach(x=>x.classList.remove('current'));const row=document.querySelector('#onlinePane .song[data-id=\\\"'+CSS.escape(String(d.id))+'\\\"]');if(row)row.classList.add('current');if(musicMode==='offline')offlinePlayData(d);else onlinePlayData(d);renderQueue()}");
  printf("function playQueue(i){if(i<0||i>=musicQueue.length)return;if(isShared()){sharedPost('shared_play',{song_id:musicQueue[i].id,position_ms:0});return}playData(musicQueue[i],i)}");
  printf("function playSong(i){var d,k,j;if(i<0||i>=musicSongs.length)return;musicIndex=i;d=trackData(musicSongs[i]);if(isShared()){sharedPost('shared_play',{song_id:d.id,position_ms:0});return}k=queueKey(d);j=musicQueue.findIndex(x=>queueKey(x)===k);playData(d,j>=0?j:-1)}");
  printf("function nextSong(){if(isShared()){sharedPost('shared_next',{});return}if(queueIndex>=0&&queueIndex+1<musicQueue.length){playQueue(queueIndex+1);return}if(queueIndex<0&&musicQueue.length){playQueue(0);return}musicPlayer.pause()}");
  printf("function prevSong(){if(isShared()){sharedPost('shared_prev',{});return}if(queueIndex>0){playQueue(queueIndex-1);return}if(musicPlayer&&musicPlayer.currentTime>0)musicPlayer.currentTime=0}");
  printf("function togglePlay(){if(!musicPlayer)return;if(isShared()){if(sharedState&&Number(sharedState.state)===1&&musicPlayer.paused){sharedApply(sharedState,true);return}if(sharedState&&Number(sharedState.state)===1){sharedPost('shared_pause',{});return}sharedPost('shared_resume',{});return}if(!musicPlayer.src){if(queueIndex>=0)playQueue(queueIndex);else if(musicQueue.length)playQueue(0);else if(musicSongs.length)playSong(0);return}if(musicPlayer.paused)musicPlayer.play().catch(function(){});else musicPlayer.pause()}");
  printf("function albumIds(artist,album){const m=offlineMeta();return Object.keys(m).filter(id=>m[id].artist===artist&&m[id].album===album)}");
  printf("function syncAlbums(){document.querySelectorAll('.albumOffline').forEach(x=>{const n=albumIds(x.dataset.artist||'',x.dataset.album||'').length,t=parseInt(x.dataset.total,10)||0,st=x.querySelector('.albumOfflineState'),sa=x.querySelector('.albumSaveAction'),ra=x.querySelector('.albumRemoveAction');if(st)st.textContent='OFFLINE '+n+'/'+t;if(sa){sa.hidden=t>0&&n>=t;sa.textContent=n?'SAVE REST':'SAVE ALBUM OFFLINE'}if(ra)ra.hidden=n===0})}");
  printf("function syncOfflineButtons(){const m=offlineMeta();let changed=false;document.querySelectorAll('#onlinePane .song').forEach(s=>{const a=s.querySelector('.offlineAction'),x=m[s.dataset.id];if(!a)return;if(x){if(s.dataset.acrid&&!x.acrid){x.acrid=s.dataset.acrid;changed=true}a.textContent='OFFLINE';a.classList.add('saved')}else{a.textContent='SAVE OFFLINE';a.classList.remove('saved')}});if(changed)saveOfflineMeta(m);syncAlbums();updateOfflineStats()}");
  printf("function saveTrack(d,a){var m=offlineMeta(),id=String(d.id);if(!('caches'in window))return Promise.reject();if(m[id])return Promise.resolve(false);if(a)a.textContent='SAVING...';if(navigator.storage&&navigator.storage.persist)navigator.storage.persist().catch(function(){});return fetch(mediaUrl(id),{cache:'no-store'}).then(r=>{if(!r.ok)throw 0;const bytes=parseInt(r.headers.get('Content-Length'),10)||0,x=r.clone();return caches.open(MEDIA_CACHE).then(c=>c.put(mediaUrl(id),x)).then(()=>{m=offlineMeta();m[id]={id:id,acrid:d.acrid||'',title:d.title||'',artist:d.artist||'',album:d.album||'',duration:parseInt(d.duration,10)||0,bytes:bytes};saveOfflineMeta(m)})})}");
  printf("function removeTrack(id){return caches.open(MEDIA_CACHE).then(c=>c.delete(mediaUrl(id))).then(()=>{const m=offlineMeta();delete m[id];saveOfflineMeta(m)})}");
  printf("function toggleOffline(s,a){const m=offlineMeta(),id=s.dataset.id;if(m[id])removeTrack(id).then(()=>{syncOfflineButtons();renderOffline()});else saveTrack(trackData(s),a).then(()=>{syncOfflineButtons();renderOffline()}).catch(()=>{a.textContent='SAVE FAILED'})}");
  printf("function albumData(x){const a=x.dataset.artist||'',b=x.dataset.album||'';if(x.classList.contains('albumBar'))return Promise.resolve(Array.from(document.querySelectorAll('#onlinePane .song')).filter(s=>s.dataset.artist===a&&s.dataset.album===b).map(trackData));return fetch(x.href,{cache:'no-store'}).then(r=>{if(!r.ok)throw 0;return r.text()}).then(t=>{const d=new DOMParser().parseFromString(t,'text/html');return Array.from(d.querySelectorAll('#onlinePane .song')).map(trackData)})}");
  printf("function queueAlbum(x,b){const old=b.textContent;b.textContent='LOADING...';albumData(x).then(v=>{queueAddMany(v);b.textContent=old}).catch(()=>b.textContent='FAILED')}");
  printf("function saveAlbum(x,button){button.textContent='LOADING...';albumData(x).then(v=>v.reduce((p,d,i)=>p.then(()=>{button.textContent='SAVING '+(i+1)+'/'+v.length;return saveTrack(d,null)}),Promise.resolve())).then(()=>{syncOfflineButtons();renderOffline()}).catch(()=>{button.textContent='SAVE FAILED';syncOfflineButtons();renderOffline()})}");
  printf("function removeAlbum(x){const ids=albumIds(x.dataset.artist||'',x.dataset.album||'');caches.open(MEDIA_CACHE).then(c=>Promise.all(ids.map(id=>c.delete(mediaUrl(id))))).then(()=>{const m=offlineMeta();ids.forEach(id=>delete m[id]);saveOfflineMeta(m);syncOfflineButtons();renderOffline()})}");
  printf("function bindAlbums(){document.querySelectorAll('.albumOffline').forEach(x=>{if(x.dataset.albumBound)return;x.dataset.albumBound='1';const s=x.querySelector('.albumSaveAction'),r=x.querySelector('.albumRemoveAction');let q=x.querySelector('.albumQueueAction');if(!q&&!x.classList.contains('albumBar')){q=document.createElement('span');q.className='albumQueueAction';q.textContent='+ QUEUE';const line=x.querySelector('span');if(line)line.prepend(q)}if(s)s.addEventListener('click',e=>{e.preventDefault();e.stopPropagation();saveAlbum(x,s)});if(r)r.addEventListener('click',e=>{e.preventDefault();e.stopPropagation();removeAlbum(x)});if(q)q.addEventListener('click',e=>{e.preventDefault();e.stopPropagation();queueAlbum(x,q)})})}");
  printf("function makeOfflineRow(x){const r=document.createElement('div'),b=document.createElement('strong'),d=document.createElement('span'),a=document.createElement('span');r.className='row song';r.dataset.id=x.id;r.dataset.acrid=x.acrid||'';r.dataset.title=x.title;r.dataset.artist=x.artist;r.dataset.album=x.album;r.dataset.duration=String(x.duration||0);b.textContent=x.title;d.textContent=(x.artist||'')+(x.album?' - '+x.album:'')+(x.duration?' - '+x.duration+' s':'')+(x.bytes?' - '+humanBytes(x.bytes):'');a.className='offlineAction saved';a.textContent='REMOVE';d.appendChild(a);r.appendChild(b);r.appendChild(d);return r}");
  printf("function renderOffline(){const box=document.getElementById('offlineSongs'),q=(document.getElementById('offlineSearch').value||'').toLowerCase(),m=offlineMeta(),seen={},v=Object.keys(m).map(k=>m[k]).filter(x=>{const key=x.acrid||('id:'+x.id);if(seen[key])return false;seen[key]=1;return(x.title+' '+x.artist+' '+x.album).toLowerCase().includes(q)}).sort((a,b)=>(a.artist+' '+a.album+' '+a.title).localeCompare(b.artist+' '+b.album+' '+b.title));box.textContent='';if(!v.length){const e=document.createElement('div');e.className='offlineEmpty';e.textContent=Object.keys(m).length?'No offline tracks match the search.':'No tracks saved on this device.';box.appendChild(e)}else{const l=document.createElement('div');l.className='list';v.forEach(x=>l.appendChild(makeOfflineRow(x)));box.appendChild(l)}bindSongs();updateOfflineStats()}");
  printf("function setMusicMode(mode){musicMode=mode==='offline'?'offline':'online';localStorage.setItem(MODE_KEY,musicMode);if(musicPlayer&&musicPlayer.src){musicPlayer.pause();musicPlayer.removeAttribute('src');musicPlayer.load()}document.getElementById('onlinePane').hidden=musicMode!=='online';document.getElementById('offlinePane').hidden=musicMode!=='offline';document.getElementById('modeOnline').classList.toggle('active',musicMode==='online');document.getElementById('modeOffline').classList.toggle('active',musicMode==='offline');musicIndex=-1;if(musicMode==='offline')renderOffline();else bindSongs()}");
  printf("function validateOffline(){if(!('caches'in window))return;const m=offlineMeta();caches.open(MEDIA_CACHE).then(c=>Promise.all(Object.keys(m).map(id=>c.match(mediaUrl(id)).then(r=>{if(!r){delete m[id];return}if(!(m[id].bytes>0)){const n=parseInt(r.headers.get('Content-Length'),10)||0;if(n){m[id].bytes=n;return}return r.clone().blob().then(b=>{m[id].bytes=b.size})}})))).then(()=>{saveOfflineMeta(m);syncOfflineButtons();if(musicMode==='offline')renderOffline()})}");
  printf("function applyPage(doc,url,push){const src=doc.getElementById('onlinePane'),dst=document.getElementById('onlinePane');if(!src||!dst)return false;dst.innerHTML=src.innerHTML;const h=doc.querySelector('header h1'),dh=document.querySelector('header h1');if(h&&dh)dh.textContent=h.textContent;document.title=doc.title;if(push)history.pushState({},'',url);bindAlbums();syncOfflineButtons();if(musicMode==='online')bindSongs();return true}");
  printf("function navigatePage(url,push=true){return fetch(url,{cache:'no-store'}).then(r=>{if(!r.ok)throw 0;return Promise.all([r.text(),r.url])}).then(v=>{const d=new DOMParser().parseFromString(v[0],'text/html');if(!applyPage(d,v[1],push))location.href=v[1]}).catch(()=>{location.href=url})}");
  printf("document.addEventListener('click',function(e){const a=e.target.closest('a[href]');if(!a||e.defaultPrevented||e.button!==0||e.metaKey||e.ctrlKey||e.shiftKey||e.altKey)return;const u=new URL(a.href,location.href);if(u.origin!==location.origin||u.pathname!==location.pathname||u.searchParams.has('media')||u.searchParams.has('sw'))return;e.preventDefault();navigatePage(u.href,true)});");
  printf("document.addEventListener('submit',function(e){const f=e.target;if(!(f instanceof HTMLFormElement))return;if(f.closest('header')){sessionStorage.removeItem(QUEUE_KEY);return;}const method=(f.method||'get').toLowerCase();const fd=new FormData(f);if(e.submitter&&e.submitter.name)fd.set(e.submitter.name,e.submitter.value);if(method==='post')fd.set('csrf',CSRF_TOKEN);if(method==='get'){e.preventDefault();const u=new URL(f.action||location.href,location.href);u.search=new URLSearchParams(fd).toString();navigatePage(u.href,true);return}if(method==='post'){e.preventDefault();fetch(f.action||location.href,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded;charset=UTF-8'},body:new URLSearchParams(fd),cache:'no-store'}).then(r=>Promise.all([r.text(),r.url])).then(v=>{const d=new DOMParser().parseFromString(v[0],'text/html');applyPage(d,v[1],true)}).catch(()=>location.reload())}});");
  printf("window.addEventListener('popstate',()=>navigatePage(location.href,false));");
  printf("document.addEventListener('DOMContentLoaded',function(){musicPlayer=document.getElementById('Player');migrateOffline();if('serviceWorker'in navigator)navigator.serviceWorker.register('?sw=1&u='+encodeURIComponent(USER_ID),{scope:location.pathname}).catch(function(){});bindAlbums();syncOfflineButtons();musicMode=localStorage.getItem(MODE_KEY)||'online';if(!navigator.onLine&&Object.keys(offlineMeta()).length)musicMode='offline';setMusicMode(musicMode);validateOffline();const os=document.getElementById('offlineSearch');if(os)os.addEventListener('input',renderOffline);setQueueMode(localStorage.getItem(QUEUE_MODE_KEY)||'private',true);musicPlayer.addEventListener('play',function(){document.getElementById('playerToggle').innerHTML='&#10074;&#10074;'});musicPlayer.addEventListener('pause',function(){document.getElementById('playerToggle').innerHTML='&#9654;'});musicPlayer.addEventListener('ended',function(){if(isShared())sharedPost('shared_ended',{});else nextSong()});musicPlayer.addEventListener('loadedmetadata',function(){var d,e,m;if(!currentTrack)return;d=Math.round(musicPlayer.duration);if(!isFinite(d)||d<=0)return;currentTrack.duration=d;e=document.querySelector('#onlinePane .song[data-id=\\\"'+CSS.escape(String(currentTrack.id))+'\\\"] .duration');if(e)e.textContent=String(d)+' s';if(isShared())return;if(musicMode==='offline'){m=offlineMeta();if(m[currentTrack.id]&&!(m[currentTrack.id].duration>0)){m[currentTrack.id].duration=d;saveOfflineMeta(m)}return}fetch('?event=duration&id='+encodeURIComponent(currentTrack.id)+'&duration='+encodeURIComponent(d),{cache:'no-store'}).catch(function(){})})});");
  printf("</script>");
}

static void db_unavailable(void) {
  printf("<div class='notice'>Catalog not initialized. Run <b>./music init</b> first.</div>");
}


static void store_stats(const struct music_config *cfg,long long *count,long long *bytes) {
  DIR *dir;
  struct dirent *entry;
  struct stat st;
  char path[VALUE_SIZE*2];
  size_t n;

  *count=0;
  *bytes=0;
  dir=opendir(cfg->store);
  if(dir==NULL)return;
  for(;(entry=readdir(dir))!=NULL;) {
    if(entry->d_name[0]=='.')continue;
    n=strlen(entry->d_name);
    if(n<5 || strcmp(entry->d_name+n-4,".mp3")!=0)continue;
    snprintf(path,sizeof(path),"%s/%s",cfg->store,entry->d_name);
    if(stat(path,&st)==0 && S_ISREG(st.st_mode) && st.st_size>0) {
      (*count)++;
      *bytes+=(long long)st.st_size;
    }
  }
  closedir(dir);
}

static long long store_orphans(const struct music_config *cfg,sqlite3 *db) {
  DIR *dir;
  struct dirent *entry;
  sqlite3_stmt *stmt;
  size_t n;
  size_t i;
  long long id;
  long long count;
  int numeric;

  dir=opendir(cfg->store);
  if(dir==NULL)return 0;
  stmt=NULL;
  count=0;
  if(sqlite3_prepare_v2(db,"SELECT 1 FROM song WHERE id=?1",-1,&stmt,NULL)!=SQLITE_OK) {
    closedir(dir);
    return 0;
  }
  for(;(entry=readdir(dir))!=NULL;) {
    if(entry->d_name[0]=='.')continue;
    n=strlen(entry->d_name);
    if(n<5 || strcasecmp(entry->d_name+n-4,".mp3")!=0)continue;
    numeric=1;
    for(i=0;i<n-4;i++)if(!isdigit((unsigned char)entry->d_name[i])) { numeric=0; break; }
    if(!numeric) {
      count++;
      continue;
    }
    id=atoll(entry->d_name);
    sqlite3_reset(stmt);
    sqlite3_bind_int64(stmt,1,id);
    if(sqlite3_step(stmt)!=SQLITE_ROW)count++;
  }
  sqlite3_finalize(stmt);
  closedir(dir);
  return count;
}

static int store_has_song(const struct music_config *cfg,long long song_id) {
  struct stat st;
  char path[VALUE_SIZE*2];

  snprintf(path,sizeof(path),"%s/%lld.mp3",cfg->store,song_id);
  return stat(path,&st)==0 && S_ISREG(st.st_mode) && st.st_size>0;
}

static void library_view(sqlite3 *db,const struct music_config *cfg,long long user_id) {
  sqlite3_stmt *stmt;
  long long songs;
  long long artists;
  long long albums;
  long long playlists;
  long long source;
  long long source_bytes;
  long long store;
  long long store_bytes;
  char source_size[64];
  char store_size[64];

  printf("<form method='get'><input type='hidden' name='view' value='search'><input class='search' name='q' placeholder='Search title, artist, album or ISRC...' autocomplete='off'></form>");
  if(db==NULL) {
    db_unavailable();
    return;
  }
  songs=db_scalar(db,"SELECT count(DISTINCT a.acrid) FROM acr_result a JOIN song s ON s.id=a.song_id WHERE s.available=1 AND coalesce(a.acrid,'')<>''");
  artists=db_scalar(db,"SELECT count(DISTINCT s.artist) FROM song s JOIN acr_result a ON a.song_id=s.id WHERE s.available=1 AND s.artist<>'' AND coalesce(a.acrid,'')<>''");
  albums=db_scalar(db,"SELECT count(DISTINCT s.album) FROM song s JOIN acr_result a ON a.song_id=s.id WHERE s.available=1 AND s.album<>'' AND coalesce(a.acrid,'')<>''");
  playlists=0;
  stmt=NULL;
  if(sqlite3_prepare_v2(db,"SELECT count(*) FROM playlist WHERE user_id=?1",-1,&stmt,NULL)==SQLITE_OK) {
    sqlite3_bind_int64(stmt,1,user_id);
    if(sqlite3_step(stmt)==SQLITE_ROW)playlists=sqlite3_column_int64(stmt,0);
  }
  if(stmt!=NULL)sqlite3_finalize(stmt);
  printf("<div class='grid'><a class='card' href='?view=artists'><strong>Artists %lld</strong><span>Browse the library</span></a><a class='card' href='?view=albums'><strong>Albums %lld</strong><span>Browse by album</span></a><a class='card' href='?view=playlists'><strong>Playlists %lld</strong><span>Your collections</span></a></div>",artists,albums,playlists);
  source=db_scalar(db,"SELECT count(*) FROM source WHERE type='drive'");
  source_bytes=db_scalar(db,"SELECT coalesce(sum(bytes),0) FROM source WHERE type='drive'");
  store_stats(cfg,&store,&store_bytes);
  size_text(source_size,sizeof(source_size),source_bytes);
  size_text(store_size,sizeof(store_size),store_bytes);
  printf("<div class='grid storageGrid'>");
  printf("<div class='card'><strong>SOURCE %lld</strong><span>%s - Google Drive</span></div>",source,source_size);
  printf("<div class='card'><strong>STORE %lld</strong><span>%s - Server local disk</span></div>",store,store_size);
  printf("<div class='card'><strong>OFFLINE <span data-offline-count>0</span></strong><span><span data-offline-size>0 B</span> - This device</span></div>");
  printf("</div>");
  printf("<div class='stats'>%lld tracks - %lld artists - %lld albums</div>",songs,artists,albums);
}

static void more_view(sqlite3 *db,const struct music_config *cfg,long long user_id,const char *username) {
  sqlite3_stmt *stmt;
  long long songs;
  long long playlists;
  long long playlist_entries;
  long long shared_playlists;
  long long active_sessions;
  long long login_count;
  long long played;
  long long source;
  long long source_bytes;
  long long store;
  long long store_bytes;
  char source_size[64];
  char store_size[64];
  char created[64];
  char stats_since[64];
  char last_login[64];
  char last_seen[64];
  char last_played[64];

  songs=0;
  playlists=0;
  playlist_entries=0;
  shared_playlists=0;
  active_sessions=0;
  login_count=0;
  played=0;
  source=0;
  source_bytes=0;
  store=0;
  store_bytes=0;
  created[0]='\0';
  stats_since[0]='\0';
  snprintf(last_login,sizeof(last_login),"-");
  snprintf(last_seen,sizeof(last_seen),"-");
  snprintf(last_played,sizeof(last_played),"-");
  if(db!=NULL) {
    songs=db_scalar(db,"SELECT count(DISTINCT a.acrid) FROM acr_result a JOIN song s ON s.id=a.song_id WHERE s.available=1 AND coalesce(a.acrid,'')<>''");
    source=db_scalar(db,"SELECT count(*) FROM source WHERE type='drive'");
    source_bytes=db_scalar(db,"SELECT coalesce(sum(bytes),0) FROM source WHERE type='drive'");
    stmt=NULL;
    if(sqlite3_prepare_v2(db,
        "SELECT datetime(u.created,'unixepoch','localtime'),"
        "(SELECT count(*) FROM playlist p WHERE p.user_id=u.id),"
        "coalesce(datetime(us.stats_since,'unixepoch','localtime'),''),coalesce(us.login_count,0),"
        "coalesce(datetime(us.last_login,'unixepoch','localtime'),'-'),"
        "coalesce(datetime(us.last_seen,'unixepoch','localtime'),'-'),coalesce(us.played,0),"
        "coalesce(datetime(us.last_played,'unixepoch','localtime'),'-'),"
        "(SELECT count(*) FROM playlist_song ps JOIN playlist p ON p.id=ps.playlist_id WHERE p.user_id=u.id),"
        "(SELECT count(*) FROM playlist p WHERE p.user_id=u.id AND p.shared=1),"
        "(SELECT count(*) FROM session se WHERE se.user_id=u.id AND se.expires>unixepoch()) "
        "FROM user u LEFT JOIN user_stats us ON us.user_id=u.id WHERE u.id=?1",-1,&stmt,NULL)==SQLITE_OK) {
      sqlite3_bind_int64(stmt,1,user_id);
      if(sqlite3_step(stmt)==SQLITE_ROW) {
        snprintf(created,sizeof(created),"%s",column_text(stmt,0));
        playlists=sqlite3_column_int64(stmt,1);
        snprintf(stats_since,sizeof(stats_since),"%s",column_text(stmt,2));
        login_count=sqlite3_column_int64(stmt,3);
        snprintf(last_login,sizeof(last_login),"%s",column_text(stmt,4));
        snprintf(last_seen,sizeof(last_seen),"%s",column_text(stmt,5));
        played=sqlite3_column_int64(stmt,6);
        snprintf(last_played,sizeof(last_played),"%s",column_text(stmt,7));
        playlist_entries=sqlite3_column_int64(stmt,8);
        shared_playlists=sqlite3_column_int64(stmt,9);
        active_sessions=sqlite3_column_int64(stmt,10);
      }
    }
    if(stmt!=NULL)sqlite3_finalize(stmt);
  }
  store_stats(cfg,&store,&store_bytes);
  size_text(source_size,sizeof(source_size),source_bytes);
  size_text(store_size,sizeof(store_size),store_bytes);
  printf("<div class='grid'>");
  printf("<div class='card'><strong>Music %s</strong><span>Current application version</span></div>",MUSIC_VERSION);
  printf("<div class='card'><strong>"); html_text(username); printf("</strong><span>Authenticated user");
  if(created[0]!='\0') { printf(" - since "); html_text(created); }
  printf("</span></div>");
  printf("<div class='card'><strong>%lld tracks</strong><span>Logical ACRCloud catalog</span></div>",songs);
  printf("<div class='card'><strong>%lld playlists</strong><span>%lld entries - %lld shared</span></div>",playlists,playlist_entries,shared_playlists);
  printf("<div class='card'><strong>%lld logins</strong><span>Last login ",login_count); html_text(last_login); printf("</span></div>");
  printf("<div class='card'><strong>%lld plays</strong><span>Last playback ",played); html_text(last_played); printf("</span></div>");
  printf("<div class='card'><strong>Last activity</strong><span>"); html_text(last_seen); printf("</span></div>");
  printf("<div class='card'><strong>%lld active session%s</strong><span>Current authenticated sessions</span></div>",active_sessions,active_sessions==1 ? "" : "s");
  printf("</div>");
  if(stats_since[0]!='\0') { printf("<div class='stats'>Usage statistics since "); html_text(stats_since); printf("</div>"); }
  printf("<div class='grid storageGrid'>");
  printf("<div class='card'><strong>SOURCE %lld</strong><span>%s - Google Drive</span></div>",source,source_size);
  printf("<div class='card'><strong>STORE %lld</strong><span>%s - Server local disk</span></div>",store,store_size);
  printf("<div class='card'><strong>OFFLINE <span data-offline-count>0</span></strong><span><span data-offline-size>0 B</span> - This user on this device</span></div>");
  printf("</div><div class='list'>");
  printf("<a class='row' href='?view=privacy'><strong>Privacy and cookie information</strong><span>Account data, aggregate usage statistics, cookies and retention</span></a>");
  printf("<div class='row'><strong>Session</strong><span>Secure HttpOnly session; browser expiry is renewed on authenticated requests and server expiry at most once per minute.</span></div>");
  printf("</div>");
}

static void artists_view(sqlite3 *db) {
  sqlite3_stmt *stmt;
  int rc;

  if(db==NULL) {
    db_unavailable();
    return;
  }
  stmt=NULL;
  if(sqlite3_prepare_v2(db,"SELECT s.artist,count(DISTINCT a.acrid) FROM song s JOIN acr_result a ON a.song_id=s.id WHERE s.available=1 AND s.artist<>'' AND coalesce(a.acrid,'')<>'' GROUP BY s.artist ORDER BY s.artist COLLATE NOCASE",-1,&stmt,NULL)!=SQLITE_OK)return;
  printf("<div class='list'>");
  for(;(rc=sqlite3_step(stmt))==SQLITE_ROW;) {
    printf("<a class='row' href='?view=albums&amp;artist=");
    url_value(column_text(stmt,0));
    printf("'><strong>");
    html_text(column_text(stmt,0));
    printf("</strong><span>%d tracks</span></a>",sqlite3_column_int(stmt,1));
  }
  printf("</div>");
  sqlite3_finalize(stmt);
}

static long long album_count(sqlite3 *db,const char *artist,const char *album) {
  sqlite3_stmt *stmt;
  long long count;

  stmt=NULL;
  count=0;
  if(sqlite3_prepare_v2(db,"SELECT count(DISTINCT a.acrid) FROM song s JOIN acr_result a ON a.song_id=s.id WHERE s.available=1 AND s.artist=?1 AND s.album=?2 AND coalesce(a.acrid,'')<>''",-1,&stmt,NULL)!=SQLITE_OK)return 0;
  sqlite3_bind_text(stmt,1,artist,-1,(void (*)(void *))-1);
  sqlite3_bind_text(stmt,2,album,-1,(void (*)(void *))-1);
  if(sqlite3_step(stmt)==SQLITE_ROW)count=sqlite3_column_int64(stmt,0);
  sqlite3_finalize(stmt);
  return count;
}

static void albums_view(sqlite3 *db,const char *artist) {
  sqlite3_stmt *stmt;
  const char *sql;
  int rc;

  if(db==NULL) {
    db_unavailable();
    return;
  }
  stmt=NULL;
  if(*artist=='\0')sql="SELECT s.album,s.artist,count(DISTINCT a.acrid) FROM song s JOIN acr_result a ON a.song_id=s.id WHERE s.available=1 AND s.album<>'' AND coalesce(a.acrid,'')<>'' GROUP BY s.album,s.artist ORDER BY s.album COLLATE NOCASE,s.artist COLLATE NOCASE";
  else sql="SELECT s.album,s.artist,count(DISTINCT a.acrid) FROM song s JOIN acr_result a ON a.song_id=s.id WHERE s.available=1 AND s.artist=?1 AND coalesce(a.acrid,'')<>'' GROUP BY s.album,s.artist ORDER BY s.album COLLATE NOCASE";
  if(sqlite3_prepare_v2(db,sql,-1,&stmt,NULL)!=SQLITE_OK)return;
  if(*artist!='\0')sqlite3_bind_text(stmt,1,artist,-1,(void (*)(void *))-1);
  printf("<div class='list'>");
  for(;(rc=sqlite3_step(stmt))==SQLITE_ROW;) {
    printf("<a class='row albumRow albumOffline' data-artist='");
    html_text(column_text(stmt,1));
    printf("' data-album='");
    html_text(column_text(stmt,0));
    printf("' data-total='%d' href='?view=tracks&amp;artist=",sqlite3_column_int(stmt,2));
    url_value(column_text(stmt,1));
    printf("&amp;album=");
    url_value(column_text(stmt,0));
    printf("'><span class='albumType'>ALBUM</span><strong>");
    html_text(column_text(stmt,0));
    printf("</strong><span><span class='albumSaveAction'>SAVE ALBUM OFFLINE</span><span class='albumRemoveAction' hidden>REMOVE OFFLINE</span>");
    html_text(column_text(stmt,1));
    printf(" - %d tracks - <span class='albumOfflineState'>OFFLINE 0/%d</span></span></a>",sqlite3_column_int(stmt,2),sqlite3_column_int(stmt,2));
  }
  printf("</div>");
  sqlite3_finalize(stmt);
}

static void tracks_view(sqlite3 *db,const struct music_config *cfg,const char *artist,const char *album) {
  sqlite3_stmt *stmt;
  long long total;
  int rc;

  if(db==NULL) {
    db_unavailable();
    return;
  }
  total=album_count(db,artist,album);
  printf("<div class='albumBar albumOffline' data-artist='");
  html_text(artist);
  printf("' data-album='");
  html_text(album);
  printf("' data-total='%lld'><strong>",total);
  html_text(album);
  printf("</strong><span>");
  html_text(artist);
  printf(" - <span class='albumOfflineState'>OFFLINE 0/%lld</span> <span class='albumSaveAction'>SAVE ALBUM OFFLINE</span><span class='albumRemoveAction' hidden>REMOVE OFFLINE</span><br><button class='qbut' type='button' onclick='queueCurrentPage()'>QUEUE ALL</button> <button class='qbut' type='button' onclick='playCurrentPage()'>PLAY ALL</button></span></div>",total);
  stmt=NULL;
  if(sqlite3_prepare_v2(db,"SELECT s.id,s.title,s.duration,s.played,coalesce(s.isrc,''),a.acrid FROM song s JOIN acr_result a ON a.song_id=s.id WHERE s.available=1 AND s.artist=?1 AND s.album=?2 AND coalesce(a.acrid,'')<>'' AND s.id=(SELECT a2.song_id FROM acr_result a2 JOIN song s2 ON s2.id=a2.song_id WHERE a2.acrid=a.acrid AND s2.available=1 ORDER BY a2.first_seen,a2.song_id LIMIT 1) ORDER BY s.title COLLATE NOCASE",-1,&stmt,NULL)!=SQLITE_OK)return;
  sqlite3_bind_text(stmt,1,artist,-1,(void (*)(void *))-1);
  sqlite3_bind_text(stmt,2,album,-1,(void (*)(void *))-1);
  printf("<div class='list'>");
  for(;(rc=sqlite3_step(stmt))==SQLITE_ROW;) {
    printf("<a class='row song trackRow' href='?view=play&amp;id=%lld' data-id='%lld' data-title='",sqlite3_column_int64(stmt,0),sqlite3_column_int64(stmt,0));
    html_text(column_text(stmt,1));
    printf("' data-artist='");
    html_text(artist);
    printf("' data-album='");
    html_text(album);
    printf("' data-duration='%d' data-acrid='",sqlite3_column_int(stmt,2));
    html_text(column_text(stmt,5));
    printf("'><strong>");
    html_text(column_text(stmt,1));
    printf("</strong><span><span class='offlineAction'>SAVE OFFLINE</span><span class='metric duration'>%d s</span> - played <span class='metric played'>%d</span> <span class='badge'>SOURCE</span>",sqlite3_column_int(stmt,2),sqlite3_column_int(stmt,3));
    if(store_has_song(cfg,sqlite3_column_int64(stmt,0)))printf(" <span class='badge'>STORE</span>");
    if(column_text(stmt,4)[0]!='\0') {
      printf(" - ISRC ");
      html_text(column_text(stmt,4));
    }
    printf("</span></a>");
  }
  printf("</div>");
  sqlite3_finalize(stmt);
}


static long long canonical_song_id(sqlite3 *db,long long song_id) {
  sqlite3_stmt *stmt;
  long long id;

  stmt=NULL;
  id=0;
  if(sqlite3_prepare_v2(db,"SELECT a.song_id FROM acr_result requested JOIN acr_result a ON a.acrid=requested.acrid JOIN song s ON s.id=a.song_id WHERE requested.song_id=?1 AND s.available=1 AND coalesce(requested.acrid,'')<>'' ORDER BY a.first_seen,a.song_id LIMIT 1",-1,&stmt,NULL)!=SQLITE_OK)return 0;
  sqlite3_bind_int64(stmt,1,song_id);
  if(sqlite3_step(stmt)==SQLITE_ROW)id=sqlite3_column_int64(stmt,0);
  sqlite3_finalize(stmt);
  return id;
}

static int playlist_create(const struct music_config *cfg,long long user_id,const char *name,const char *description) {
  sqlite3 *db;
  sqlite3_stmt *stmt;
  int rc;

  if(name==NULL || *name=='\0' || strlen(name)>80 || description==NULL || strlen(description)>500)return 0;
  db=db_open_web_write(cfg);
  if(db==NULL)return 0;
  stmt=NULL;
  if(sqlite3_prepare_v2(db,"INSERT INTO playlist(user_id,name,description) VALUES(?1,?2,?3)",-1,&stmt,NULL)!=SQLITE_OK) {
    sqlite3_close(db);
    return 0;
  }
  sqlite3_bind_int64(stmt,1,user_id);
  sqlite3_bind_text(stmt,2,name,-1,(void (*)(void *))-1);
  sqlite3_bind_text(stmt,3,description,-1,(void (*)(void *))-1);
  rc=sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  sqlite3_close(db);
  return rc==SQLITE_DONE;
}

static int playlist_update(const struct music_config *cfg,long long user_id,long long playlist_id,const char *name,const char *description) {
  sqlite3 *db;
  sqlite3_stmt *stmt;
  int rc;

  if(name==NULL || *name=='\0' || strlen(name)>80 || description==NULL || strlen(description)>500)return 0;
  db=db_open_web_write(cfg);
  if(db==NULL)return 0;
  stmt=NULL;
  if(sqlite3_prepare_v2(db,"UPDATE playlist SET name=?1,description=?2 WHERE id=?3 AND user_id=?4",-1,&stmt,NULL)!=SQLITE_OK) {
    sqlite3_close(db);
    return 0;
  }
  sqlite3_bind_text(stmt,1,name,-1,(void (*)(void *))-1);
  sqlite3_bind_text(stmt,2,description,-1,(void (*)(void *))-1);
  sqlite3_bind_int64(stmt,3,playlist_id);
  sqlite3_bind_int64(stmt,4,user_id);
  rc=sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  sqlite3_close(db);
  return rc==SQLITE_DONE;
}

static int playlist_delete(const struct music_config *cfg,long long user_id,long long playlist_id) {
  sqlite3 *db;
  sqlite3_stmt *stmt;
  int rc;

  db=db_open_web_write(cfg);
  if(db==NULL)return 0;
  stmt=NULL;
  if(sqlite3_prepare_v2(db,"DELETE FROM playlist WHERE id=?1 AND user_id=?2",-1,&stmt,NULL)!=SQLITE_OK) {
    sqlite3_close(db);
    return 0;
  }
  sqlite3_bind_int64(stmt,1,playlist_id);
  sqlite3_bind_int64(stmt,2,user_id);
  rc=sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  sqlite3_close(db);
  return rc==SQLITE_DONE;
}

static int playlist_share(const struct music_config *cfg,long long user_id,long long playlist_id,int shared) {
  sqlite3 *db;
  sqlite3_stmt *stmt;
  int rc;

  db=db_open_web_write(cfg);
  if(db==NULL)return 0;
  stmt=NULL;
  if(sqlite3_prepare_v2(db,"UPDATE playlist SET shared=?1 WHERE id=?2 AND user_id=?3",-1,&stmt,NULL)!=SQLITE_OK) {
    sqlite3_close(db);
    return 0;
  }
  sqlite3_bind_int(stmt,1,shared ? 1 : 0);
  sqlite3_bind_int64(stmt,2,playlist_id);
  sqlite3_bind_int64(stmt,3,user_id);
  rc=sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  sqlite3_close(db);
  return rc==SQLITE_DONE;
}

static int playlist_add_song(const struct music_config *cfg,long long user_id,long long playlist_id,long long song_id) {
  sqlite3 *db;
  sqlite3_stmt *stmt;
  long long canonical;
  long long position;
  int exists;
  int rc;

  db=db_open_web_write(cfg);
  if(db==NULL)return 0;
  canonical=canonical_song_id(db,song_id);
  if(canonical==0) {
    sqlite3_close(db);
    return 0;
  }
  stmt=NULL;
  exists=0;
  if(sqlite3_prepare_v2(db,"SELECT count(*) FROM playlist_song ps JOIN acr_result existing ON existing.song_id=ps.song_id JOIN acr_result wanted ON wanted.song_id=?1 WHERE ps.playlist_id=?2 AND existing.acrid=wanted.acrid",-1,&stmt,NULL)==SQLITE_OK) {
    sqlite3_bind_int64(stmt,1,canonical);
    sqlite3_bind_int64(stmt,2,playlist_id);
    if(sqlite3_step(stmt)==SQLITE_ROW)exists=sqlite3_column_int(stmt,0)>0;
  }
  if(stmt!=NULL)sqlite3_finalize(stmt);
  if(exists) {
    sqlite3_close(db);
    return 1;
  }
  position=0;
  stmt=NULL;
  if(sqlite3_prepare_v2(db,"SELECT CASE WHEN count(p.id)>0 THEN coalesce(max(ps.position),0)+1 ELSE 0 END FROM playlist p LEFT JOIN playlist_song ps ON ps.playlist_id=p.id WHERE p.id=?1 AND p.user_id=?2",-1,&stmt,NULL)==SQLITE_OK) {
    sqlite3_bind_int64(stmt,1,playlist_id);
    sqlite3_bind_int64(stmt,2,user_id);
    if(sqlite3_step(stmt)==SQLITE_ROW)position=sqlite3_column_int64(stmt,0);
  }
  if(stmt!=NULL)sqlite3_finalize(stmt);
  if(position<=0) {
    sqlite3_close(db);
    return 0;
  }
  stmt=NULL;
  if(sqlite3_prepare_v2(db,"INSERT INTO playlist_song(playlist_id,song_id,position) VALUES(?1,?2,?3)",-1,&stmt,NULL)!=SQLITE_OK) {
    sqlite3_close(db);
    return 0;
  }
  sqlite3_bind_int64(stmt,1,playlist_id);
  sqlite3_bind_int64(stmt,2,canonical);
  sqlite3_bind_int64(stmt,3,position);
  rc=sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  sqlite3_close(db);
  return rc==SQLITE_DONE;
}

static long long playlist_copy(const struct music_config *cfg,long long user_id,long long source_id) {
  sqlite3 *db;
  sqlite3_stmt *stmt;
  char base[81];
  char name[81];
  char description[501];
  long long new_id;
  int suffix;
  int exists;
  int rc;

  db=db_open_web_write(cfg);
  if(db==NULL)return 0;
  base[0]='\0';
  description[0]='\0';
  stmt=NULL;
  if(sqlite3_prepare_v2(db,"SELECT name,description FROM playlist WHERE id=?1 AND shared=1 AND user_id<>?2",-1,&stmt,NULL)==SQLITE_OK) {
    sqlite3_bind_int64(stmt,1,source_id);
    sqlite3_bind_int64(stmt,2,user_id);
    if(sqlite3_step(stmt)==SQLITE_ROW) {
      snprintf(base,sizeof(base),"%.70s copy",column_text(stmt,0));
      snprintf(description,sizeof(description),"%s",column_text(stmt,1));
    }
  }
  if(stmt!=NULL)sqlite3_finalize(stmt);
  if(base[0]=='\0') {
    sqlite3_close(db);
    return 0;
  }
  suffix=1;
  for(;;) {
    if(suffix==1)snprintf(name,sizeof(name),"%s",base);
    else snprintf(name,sizeof(name),"%.70s %d",base,suffix);
    exists=0;
    stmt=NULL;
    if(sqlite3_prepare_v2(db,"SELECT count(*) FROM playlist WHERE user_id=?1 AND name=?2 COLLATE NOCASE",-1,&stmt,NULL)==SQLITE_OK) {
      sqlite3_bind_int64(stmt,1,user_id);
      sqlite3_bind_text(stmt,2,name,-1,(void (*)(void *))-1);
      if(sqlite3_step(stmt)==SQLITE_ROW)exists=sqlite3_column_int(stmt,0)>0;
    }
    if(stmt!=NULL)sqlite3_finalize(stmt);
    if(!exists)break;
    suffix++;
    if(suffix>999) {
      sqlite3_close(db);
      return 0;
    }
  }
  if(!db_exec(db,"BEGIN IMMEDIATE")) {
    sqlite3_close(db);
    return 0;
  }
  stmt=NULL;
  rc=sqlite3_prepare_v2(db,"INSERT INTO playlist(user_id,name,description,shared) VALUES(?1,?2,?3,0)",-1,&stmt,NULL);
  if(rc==SQLITE_OK) {
    sqlite3_bind_int64(stmt,1,user_id);
    sqlite3_bind_text(stmt,2,name,-1,(void (*)(void *))-1);
    sqlite3_bind_text(stmt,3,description,-1,(void (*)(void *))-1);
    rc=sqlite3_step(stmt);
  }
  if(stmt!=NULL)sqlite3_finalize(stmt);
  new_id=rc==SQLITE_DONE ? sqlite3_last_insert_rowid(db) : 0;
  stmt=NULL;
  if(new_id>0 && sqlite3_prepare_v2(db,"INSERT INTO playlist_song(playlist_id,song_id,position) SELECT ?1,song_id,position FROM playlist_song WHERE playlist_id=?2 ORDER BY position",-1,&stmt,NULL)==SQLITE_OK) {
    sqlite3_bind_int64(stmt,1,new_id);
    sqlite3_bind_int64(stmt,2,source_id);
    rc=sqlite3_step(stmt);
  } else if(new_id>0)rc=SQLITE_OK;
  if(stmt!=NULL)sqlite3_finalize(stmt);
  if(rc==SQLITE_DONE)db_exec(db,"COMMIT");
  else {
    db_exec(db,"ROLLBACK");
    new_id=0;
  }
  sqlite3_close(db);
  return new_id;
}

static int playlist_move_edge(const struct music_config *cfg,long long user_id,long long playlist_id,long long song_id,int direction) {
  sqlite3 *db;
  sqlite3_stmt *stmt;
  long long position;
  long long edge;
  long long temp;
  long long offset;
  int found;
  int rc;

  if(direction!=-2 && direction!=2)return 0;
  db=db_open_web_write(cfg);
  if(db==NULL)return 0;
  position=0;
  edge=0;
  found=0;
  stmt=NULL;
  if(sqlite3_prepare_v2(db,"SELECT ps.position,(SELECT CASE WHEN ?1<0 THEN min(x.position) ELSE max(x.position) END FROM playlist_song x WHERE x.playlist_id=ps.playlist_id) FROM playlist_song ps JOIN playlist p ON p.id=ps.playlist_id WHERE ps.playlist_id=?2 AND ps.song_id=?3 AND p.user_id=?4",-1,&stmt,NULL)==SQLITE_OK) {
    sqlite3_bind_int(stmt,1,direction);
    sqlite3_bind_int64(stmt,2,playlist_id);
    sqlite3_bind_int64(stmt,3,song_id);
    sqlite3_bind_int64(stmt,4,user_id);
    if(sqlite3_step(stmt)==SQLITE_ROW) {
      position=sqlite3_column_int64(stmt,0);
      edge=sqlite3_column_int64(stmt,1);
      found=1;
    }
  }
  if(stmt!=NULL)sqlite3_finalize(stmt);
  if(!found) {
    sqlite3_close(db);
    return 0;
  }
  if(position==edge) {
    sqlite3_close(db);
    return 1;
  }
  temp=-9000000000000000000LL;
  offset=1000000000000LL;
  if(!db_exec(db,"BEGIN IMMEDIATE")) {
    sqlite3_close(db);
    return 0;
  }
  stmt=NULL;
  rc=sqlite3_prepare_v2(db,"UPDATE playlist_song SET position=?1 WHERE playlist_id=?2 AND song_id=?3",-1,&stmt,NULL);
  if(rc==SQLITE_OK) {
    sqlite3_bind_int64(stmt,1,temp);
    sqlite3_bind_int64(stmt,2,playlist_id);
    sqlite3_bind_int64(stmt,3,song_id);
    rc=sqlite3_step(stmt);
  }
  if(stmt!=NULL)sqlite3_finalize(stmt);
  stmt=NULL;
  if(rc==SQLITE_DONE) {
    if(direction<0)rc=sqlite3_prepare_v2(db,"UPDATE playlist_song SET position=position+?1 WHERE playlist_id=?2 AND song_id<>?3 AND position<?4",-1,&stmt,NULL);
    else rc=sqlite3_prepare_v2(db,"UPDATE playlist_song SET position=position+?1 WHERE playlist_id=?2 AND song_id<>?3 AND position>?4",-1,&stmt,NULL);
    if(rc==SQLITE_OK) {
      sqlite3_bind_int64(stmt,1,offset);
      sqlite3_bind_int64(stmt,2,playlist_id);
      sqlite3_bind_int64(stmt,3,song_id);
      sqlite3_bind_int64(stmt,4,position);
      rc=sqlite3_step(stmt);
    }
  }
  if(stmt!=NULL)sqlite3_finalize(stmt);
  stmt=NULL;
  if(rc==SQLITE_DONE) {
    if(direction<0)rc=sqlite3_prepare_v2(db,"UPDATE playlist_song SET position=position-?1+1 WHERE playlist_id=?2 AND song_id<>?3 AND position>=?1",-1,&stmt,NULL);
    else rc=sqlite3_prepare_v2(db,"UPDATE playlist_song SET position=position-?1-1 WHERE playlist_id=?2 AND song_id<>?3 AND position>=?1",-1,&stmt,NULL);
    if(rc==SQLITE_OK) {
      sqlite3_bind_int64(stmt,1,offset);
      sqlite3_bind_int64(stmt,2,playlist_id);
      sqlite3_bind_int64(stmt,3,song_id);
      rc=sqlite3_step(stmt);
    }
  }
  if(stmt!=NULL)sqlite3_finalize(stmt);
  stmt=NULL;
  if(rc==SQLITE_DONE && sqlite3_prepare_v2(db,"UPDATE playlist_song SET position=?1 WHERE playlist_id=?2 AND song_id=?3",-1,&stmt,NULL)==SQLITE_OK) {
    sqlite3_bind_int64(stmt,1,edge);
    sqlite3_bind_int64(stmt,2,playlist_id);
    sqlite3_bind_int64(stmt,3,song_id);
    rc=sqlite3_step(stmt);
  }
  if(stmt!=NULL)sqlite3_finalize(stmt);
  if(rc==SQLITE_DONE)db_exec(db,"COMMIT");
  else db_exec(db,"ROLLBACK");
  sqlite3_close(db);
  return rc==SQLITE_DONE;
}

static int playlist_remove_acrid(const struct music_config *cfg,long long user_id,long long playlist_id,const char *acrid) {
  sqlite3 *db;
  sqlite3_stmt *stmt;
  int rc;

  if(acrid==NULL || *acrid=='\0')return 0;
  db=db_open_web_write(cfg);
  if(db==NULL)return 0;
  stmt=NULL;
  if(sqlite3_prepare_v2(db,"DELETE FROM playlist_song WHERE playlist_id=?1 AND song_id IN (SELECT song_id FROM acr_result WHERE acrid=?2) AND EXISTS(SELECT 1 FROM playlist WHERE id=?1 AND user_id=?3)",-1,&stmt,NULL)!=SQLITE_OK) {
    sqlite3_close(db);
    return 0;
  }
  sqlite3_bind_int64(stmt,1,playlist_id);
  sqlite3_bind_text(stmt,2,acrid,-1,(void (*)(void *))-1);
  sqlite3_bind_int64(stmt,3,user_id);
  rc=sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  sqlite3_close(db);
  return rc==SQLITE_DONE;
}

static int playlist_remove_song(const struct music_config *cfg,long long user_id,long long playlist_id,long long song_id) {
  sqlite3 *db;
  sqlite3_stmt *stmt;
  int rc;

  if(song_id<=0)return 0;
  db=db_open_web_write(cfg);
  if(db==NULL)return 0;
  stmt=NULL;
  if(sqlite3_prepare_v2(db,"DELETE FROM playlist_song WHERE playlist_id=?1 AND song_id=?2 AND EXISTS(SELECT 1 FROM playlist WHERE id=?1 AND user_id=?3)",-1,&stmt,NULL)!=SQLITE_OK) {
    sqlite3_close(db);
    return 0;
  }
  sqlite3_bind_int64(stmt,1,playlist_id);
  sqlite3_bind_int64(stmt,2,song_id);
  sqlite3_bind_int64(stmt,3,user_id);
  rc=sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  sqlite3_close(db);
  return rc==SQLITE_DONE;
}

static int playlist_move_song(const struct music_config *cfg,long long user_id,long long playlist_id,long long stored_song_id,int direction) {
  sqlite3 *db;
  sqlite3_stmt *stmt;
  long long position;
  long long other_song;
  long long other_position;
  int found;
  int rc;

  if(direction!=1 && direction!=-1)return 0;
  db=db_open_web_write(cfg);
  if(db==NULL)return 0;
  position=0;
  found=0;
  stmt=NULL;
  if(sqlite3_prepare_v2(db,"SELECT ps.position FROM playlist_song ps JOIN playlist p ON p.id=ps.playlist_id WHERE ps.playlist_id=?1 AND ps.song_id=?2 AND p.user_id=?3",-1,&stmt,NULL)==SQLITE_OK) {
    sqlite3_bind_int64(stmt,1,playlist_id);
    sqlite3_bind_int64(stmt,2,stored_song_id);
    sqlite3_bind_int64(stmt,3,user_id);
    if(sqlite3_step(stmt)==SQLITE_ROW) {
      position=sqlite3_column_int64(stmt,0);
      found=1;
    }
  }
  if(stmt!=NULL)sqlite3_finalize(stmt);
  if(!found) {
    sqlite3_close(db);
    return 0;
  }
  other_song=0;
  other_position=0;
  stmt=NULL;
  if(direction<0)rc=sqlite3_prepare_v2(db,"SELECT song_id,position FROM playlist_song WHERE playlist_id=?1 AND position<?2 ORDER BY position DESC LIMIT 1",-1,&stmt,NULL);
  else rc=sqlite3_prepare_v2(db,"SELECT song_id,position FROM playlist_song WHERE playlist_id=?1 AND position>?2 ORDER BY position LIMIT 1",-1,&stmt,NULL);
  if(rc==SQLITE_OK) {
    sqlite3_bind_int64(stmt,1,playlist_id);
    sqlite3_bind_int64(stmt,2,position);
    if(sqlite3_step(stmt)==SQLITE_ROW) {
      other_song=sqlite3_column_int64(stmt,0);
      other_position=sqlite3_column_int64(stmt,1);
    }
  }
  if(stmt!=NULL)sqlite3_finalize(stmt);
  if(other_song==0) {
    sqlite3_close(db);
    return 1;
  }
  if(!db_exec(db,"BEGIN IMMEDIATE")) {
    sqlite3_close(db);
    return 0;
  }
  stmt=NULL;
  rc=sqlite3_prepare_v2(db,"UPDATE playlist_song SET position=-1 WHERE playlist_id=?1 AND song_id=?2",-1,&stmt,NULL);
  if(rc==SQLITE_OK) {
    sqlite3_bind_int64(stmt,1,playlist_id);
    sqlite3_bind_int64(stmt,2,stored_song_id);
    rc=sqlite3_step(stmt);
  }
  if(stmt!=NULL)sqlite3_finalize(stmt);
  stmt=NULL;
  if(rc==SQLITE_DONE && sqlite3_prepare_v2(db,"UPDATE playlist_song SET position=?1 WHERE playlist_id=?2 AND song_id=?3",-1,&stmt,NULL)==SQLITE_OK) {
    sqlite3_bind_int64(stmt,1,position);
    sqlite3_bind_int64(stmt,2,playlist_id);
    sqlite3_bind_int64(stmt,3,other_song);
    rc=sqlite3_step(stmt);
  }
  if(stmt!=NULL)sqlite3_finalize(stmt);
  stmt=NULL;
  if(rc==SQLITE_DONE && sqlite3_prepare_v2(db,"UPDATE playlist_song SET position=?1 WHERE playlist_id=?2 AND song_id=?3",-1,&stmt,NULL)==SQLITE_OK) {
    sqlite3_bind_int64(stmt,1,other_position);
    sqlite3_bind_int64(stmt,2,playlist_id);
    sqlite3_bind_int64(stmt,3,stored_song_id);
    rc=sqlite3_step(stmt);
  }
  if(stmt!=NULL)sqlite3_finalize(stmt);
  if(rc==SQLITE_DONE)db_exec(db,"COMMIT");
  else db_exec(db,"ROLLBACK");
  sqlite3_close(db);
  return rc==SQLITE_DONE;
}

static void playlists_view(sqlite3 *db,long long user_id) {
  sqlite3_stmt *stmt;
  int rc;
  int any;

  if(db==NULL) {
    db_unavailable();
    return;
  }
  printf("<div class='list'><div class='row'><strong>Create playlist</strong><form class='playlistTools' method='post'><input type='hidden' name='action' value='playlist_create'><input name='name' maxlength='80' placeholder='Name' required><textarea name='description' maxlength='500' placeholder='Description'></textarea><button type='submit'>CREATE</button></form></div></div>");
  stmt=NULL;
  if(sqlite3_prepare_v2(db,"SELECT p.id,p.name,p.description,p.shared,count(ps.song_id),count(DISTINCT CASE WHEN coalesce(a.acrid,'')<>'' THEN a.acrid END) FROM playlist p LEFT JOIN playlist_song ps ON ps.playlist_id=p.id LEFT JOIN acr_result a ON a.song_id=ps.song_id WHERE p.user_id=?1 GROUP BY p.id ORDER BY p.name COLLATE NOCASE",-1,&stmt,NULL)!=SQLITE_OK)return;
  sqlite3_bind_int64(stmt,1,user_id);
  printf("<div class='list'>");
  any=0;
  for(;(rc=sqlite3_step(stmt))==SQLITE_ROW;) {
    any=1;
    printf("<a class='row' href='?view=playlist&amp;id=%lld'><strong>",sqlite3_column_int64(stmt,0));
    html_text(column_text(stmt,1));
    printf("</strong><span>%d/%d recognized",sqlite3_column_int(stmt,5),sqlite3_column_int(stmt,4));
    if(sqlite3_column_int(stmt,3))printf(" - shared");
    if(column_text(stmt,2)[0]!='\0') { printf(" - "); html_text(column_text(stmt,2)); }
    printf("</span></a>");
  }
  if(!any)printf("<div class='row muted'>No playlists yet.</div>");
  printf("</div>");
  sqlite3_finalize(stmt);
  stmt=NULL;
  if(sqlite3_prepare_v2(db,"SELECT p.id,p.name,p.description,u.username,count(ps.song_id),count(DISTINCT CASE WHEN coalesce(a.acrid,'')<>'' THEN a.acrid END) FROM playlist p JOIN user u ON u.id=p.user_id LEFT JOIN playlist_song ps ON ps.playlist_id=p.id LEFT JOIN acr_result a ON a.song_id=ps.song_id WHERE p.shared=1 AND p.user_id<>?1 GROUP BY p.id ORDER BY p.name COLLATE NOCASE",-1,&stmt,NULL)!=SQLITE_OK)return;
  sqlite3_bind_int64(stmt,1,user_id);
  any=0;
  for(;(rc=sqlite3_step(stmt))==SQLITE_ROW;) {
    if(!any)printf("<div class='stats'>Shared playlists</div><div class='list'>");
    any=1;
    printf("<a class='row' href='?view=playlist&amp;id=%lld'><strong>",sqlite3_column_int64(stmt,0));
    html_text(column_text(stmt,1));
    printf("</strong><span>%d/%d recognized - ",sqlite3_column_int(stmt,5),sqlite3_column_int(stmt,4));
    html_text(column_text(stmt,3));
    if(column_text(stmt,2)[0]!='\0') { printf(" - "); html_text(column_text(stmt,2)); }
    printf("</span></a>");
  }
  if(any)printf("</div>");
  sqlite3_finalize(stmt);
}

static void playlist_add_form(sqlite3 *db,long long user_id,long long song_id) {
  sqlite3_stmt *stmt;
  int rc;
  int any;

  stmt=NULL;
  any=0;
  if(sqlite3_prepare_v2(db,"SELECT id,name FROM playlist WHERE user_id=?1 ORDER BY name COLLATE NOCASE",-1,&stmt,NULL)!=SQLITE_OK)return;
  sqlite3_bind_int64(stmt,1,user_id);
  printf("<form class='playlistTools' method='post'><input type='hidden' name='action' value='playlist_add'><input type='hidden' name='song_id' value='%lld'><select name='playlist_id' required><option value=''>Add to playlist...</option>",song_id);
  for(;(rc=sqlite3_step(stmt))==SQLITE_ROW;) {
    any=1;
    printf("<option value='%lld'>",sqlite3_column_int64(stmt,0));
    html_text(column_text(stmt,1));
    printf("</option>");
  }
  printf("</select>");
  if(any)printf("<button type='submit'>ADD</button>");
  else printf("<span class='muted'>Create a playlist first.</span>");
  printf("</form>");
  sqlite3_finalize(stmt);
}

static void playlist_view(sqlite3 *db,const struct music_config *cfg,long long user_id,long long playlist_id) {
  sqlite3_stmt *stmt;
  long long owner_id;
  int shared;
  int rc;
  int any;

  if(db==NULL) {
    db_unavailable();
    return;
  }
  owner_id=0;
  shared=0;
  stmt=NULL;
  if(sqlite3_prepare_v2(db,"SELECT p.user_id,p.name,p.description,p.shared,u.username FROM playlist p JOIN user u ON u.id=p.user_id WHERE p.id=?1 AND (p.user_id=?2 OR p.shared=1)",-1,&stmt,NULL)!=SQLITE_OK)return;
  sqlite3_bind_int64(stmt,1,playlist_id);
  sqlite3_bind_int64(stmt,2,user_id);
  if(sqlite3_step(stmt)!=SQLITE_ROW) {
    printf("<div class='notice'>Playlist not found.</div>");
    sqlite3_finalize(stmt);
    return;
  }
  owner_id=sqlite3_column_int64(stmt,0);
  shared=sqlite3_column_int(stmt,3);
  printf("<div class='albumBar'><strong>");
  html_text(column_text(stmt,1));
  printf("</strong><span>");
  if(column_text(stmt,2)[0]!='\0')html_text(column_text(stmt,2));
  if(owner_id!=user_id) { printf(" - shared by "); html_text(column_text(stmt,4)); }
  printf("</span>");
  if(owner_id!=user_id)printf("<form class='inlineForm' method='post'><input type='hidden' name='action' value='playlist_copy'><input type='hidden' name='playlist_id' value='%lld'><button type='submit'>COPY TO MY PLAYLISTS</button></form>",playlist_id);
  if(owner_id==user_id) {
    printf("<form class='playlistTools' method='post'><input type='hidden' name='action' value='playlist_update'><input type='hidden' name='playlist_id' value='%lld'><input name='name' maxlength='80' value='",playlist_id);
    html_text(column_text(stmt,1));
    printf("' required><textarea name='description' maxlength='500'>");
    html_text(column_text(stmt,2));
    printf("</textarea><button type='submit'>SAVE</button></form>");
    printf("<form class='inlineForm' method='post'><input type='hidden' name='action' value='playlist_share'><input type='hidden' name='playlist_id' value='%lld'><input type='hidden' name='shared' value='%d'><button type='submit'>%s</button></form>",playlist_id,shared ? 0 : 1,shared ? "MAKE PRIVATE" : "SHARE");
    printf("<form class='inlineForm' method='post' onsubmit=\"return confirm('Delete playlist?')\"><input type='hidden' name='action' value='playlist_delete'><input type='hidden' name='playlist_id' value='%lld'><button type='submit'>DELETE</button></form>",playlist_id);
  }
  printf("<br><button class='qbut' type='button' onclick='queueCurrentPage()'>QUEUE ALL</button> <button class='qbut' type='button' onclick='playCurrentPage()'>PLAY ALL</button>");
  printf("</div>");
  sqlite3_finalize(stmt);
  stmt=NULL;
  if(sqlite3_prepare_v2(db,"SELECT min(ps.position),(SELECT ps2.song_id FROM playlist_song ps2 JOIN acr_result r2 ON r2.song_id=ps2.song_id WHERE ps2.playlist_id=ps.playlist_id AND r2.acrid=r.acrid ORDER BY ps2.position LIMIT 1),c.id,c.title,c.artist,c.album,c.duration,coalesce(c.isrc,''),r.acrid,1 FROM playlist_song ps JOIN acr_result r ON r.song_id=ps.song_id JOIN acr_result a ON a.acrid=r.acrid JOIN song c ON c.id=a.song_id AND c.available=1 WHERE ps.playlist_id=?1 AND coalesce(r.acrid,'')<>'' AND c.id=(SELECT a2.song_id FROM acr_result a2 JOIN song s2 ON s2.id=a2.song_id WHERE a2.acrid=r.acrid AND s2.available=1 ORDER BY a2.first_seen,a2.song_id LIMIT 1) GROUP BY r.acrid UNION ALL SELECT ps.position,ps.song_id,ps.song_id,coalesce(nullif(src.original_title,''),s.title),coalesce(nullif(src.original_artist,''),s.artist),coalesce(nullif(src.original_album,''),s.album),s.duration,coalesce(s.isrc,''),'',0 FROM playlist_song ps JOIN song s ON s.id=ps.song_id LEFT JOIN source src ON src.song_id=s.id AND src.type='drive' LEFT JOIN acr_result r ON r.song_id=ps.song_id WHERE ps.playlist_id=?1 AND (r.song_id IS NULL OR coalesce(r.acrid,'')='') ORDER BY 1",-1,&stmt,NULL)!=SQLITE_OK)return;
  sqlite3_bind_int64(stmt,1,playlist_id);
  printf("<div class='list'>");
  any=0;
  for(;(rc=sqlite3_step(stmt))==SQLITE_ROW;) {
    any=1;
    if(sqlite3_column_int(stmt,9)) {
      printf("<div class='row song' data-id='%lld' data-title='",sqlite3_column_int64(stmt,2));
      html_text(column_text(stmt,3));
      printf("' data-artist='"); html_text(column_text(stmt,4));
      printf("' data-album='"); html_text(column_text(stmt,5));
      printf("' data-duration='%d' data-acrid='",sqlite3_column_int(stmt,6)); html_text(column_text(stmt,8));
      printf("'><strong>"); html_text(column_text(stmt,3));
      printf("</strong><span><span class='offlineAction'>SAVE OFFLINE</span>"); html_text(column_text(stmt,4)); printf(" - "); html_text(column_text(stmt,5));
      if(sqlite3_column_int(stmt,6)>0)printf(" - %d s",sqlite3_column_int(stmt,6));
      if(store_has_song(cfg,sqlite3_column_int64(stmt,2)))printf(" <span class='badge'>STORE</span>");
    } else {
      printf("<div class='row'><strong>"); html_text(column_text(stmt,3));
      printf("</strong><span>"); html_text(column_text(stmt,4));
      if(column_text(stmt,5)[0]!='\0') { printf(" - "); html_text(column_text(stmt,5)); }
      printf(" <span class='badge'>PENDING ACR</span>");
    }
    if(owner_id==user_id) {
      printf("<br><form class='inlineForm' method='post'><input type='hidden' name='action' value='playlist_move'><input type='hidden' name='playlist_id' value='%lld'><input type='hidden' name='stored_song_id' value='%lld'><button name='direction' value='-2' type='submit'>TOP</button><button name='direction' value='-1' type='submit'>UP</button><button name='direction' value='1' type='submit'>DOWN</button><button name='direction' value='2' type='submit'>BOTTOM</button></form>",playlist_id,sqlite3_column_int64(stmt,1));
      if(sqlite3_column_int(stmt,9)) {
        printf("<form class='inlineForm' method='post'><input type='hidden' name='action' value='playlist_remove'><input type='hidden' name='playlist_id' value='%lld'><input type='hidden' name='acrid' value='",playlist_id);
        html_text(column_text(stmt,8));
        printf("'><button type='submit'>REMOVE</button></form>");
      } else {
        printf("<form class='inlineForm' method='post'><input type='hidden' name='action' value='playlist_remove'><input type='hidden' name='playlist_id' value='%lld'><input type='hidden' name='stored_song_id' value='%lld'><button type='submit'>REMOVE</button></form>",playlist_id,sqlite3_column_int64(stmt,1));
      }
    }
    printf("</span></div>");
  }
  if(!any)printf("<div class='row muted'>No tracks in this playlist yet.</div>");
  printf("</div>");
  sqlite3_finalize(stmt);
}

static void play_view(sqlite3 *db,long long user_id,long long song_id) {
  sqlite3_stmt *stmt;
  int rc;
  int score;

  if(db==NULL) {
    db_unavailable();
    return;
  }
  stmt=NULL;
  if(sqlite3_prepare_v2(db,"SELECT s.id,s.title,s.artist,s.album,coalesce(a.release_date,''),coalesce(a.label,''),coalesce(a.isrc,''),coalesce(a.upc,''),coalesce(a.acrid,''),a.score,s.duration,s.played FROM acr_result requested JOIN acr_result a ON a.acrid=requested.acrid JOIN song s ON s.id=a.song_id WHERE requested.song_id=?1 AND s.available=1 AND coalesce(requested.acrid,'')<>'' ORDER BY a.first_seen,a.song_id LIMIT 1",-1,&stmt,NULL)!=SQLITE_OK)return;
  sqlite3_bind_int64(stmt,1,song_id);
  rc=sqlite3_step(stmt);
  if(rc!=SQLITE_ROW) {
    printf("<div class='notice'>Track is not ACRCloud-recognized yet.</div>");
    sqlite3_finalize(stmt);
    return;
  }
  printf("<div class='list'><div class='row'><strong>");
  song_id=sqlite3_column_int64(stmt,0);
  html_text(column_text(stmt,1));
  printf("</strong><span>");
  html_text(column_text(stmt,2));
  printf(" · ");
  html_text(column_text(stmt,3));
  printf("</span>");
  printf("<div class='stats'>");
  if(column_text(stmt,4)[0]!='\0') { printf("Release: "); html_text(column_text(stmt,4)); printf("<br>"); }
  if(column_text(stmt,5)[0]!='\0') { printf("Label: "); html_text(column_text(stmt,5)); printf("<br>"); }
  if(column_text(stmt,6)[0]!='\0') { printf("ISRC: "); html_text(column_text(stmt,6)); printf("<br>"); }
  if(column_text(stmt,7)[0]!='\0') { printf("UPC: "); html_text(column_text(stmt,7)); printf("<br>"); }
  if(column_text(stmt,8)[0]!='\0') { printf("ACRID: "); html_text(column_text(stmt,8)); printf("<br>"); }
  score=sqlite3_column_int(stmt,9);
  if(score>0)printf("ACRCloud score: %.1f<br>",(double)score/10000.0);
  if(sqlite3_column_int(stmt,10)>0)printf("Duration: %d s<br>",sqlite3_column_int(stmt,10));
  printf("Played: %d</div>",sqlite3_column_int(stmt,11));
  playlist_add_form(db,user_id,song_id);
  printf("<audio controls autoplay style='width:100%%;margin-top:14px' src='?media=%lld'></audio></div></div>",song_id);
  sqlite3_finalize(stmt);
}

static void search_view(sqlite3 *db,const struct music_config *cfg,const char *query) {
  sqlite3_stmt *stmt;
  char pattern[VALUE_SIZE+3];
  int rc;

  printf("<form method='get'><input type='hidden' name='view' value='search'><input class='search' name='q' value='");
  html_text(query);
  printf("' placeholder='Search title, artist, album or ISRC...' autofocus></form>");
  if(*query=='\0')return;
  if(db==NULL) {
    db_unavailable();
    return;
  }
  snprintf(pattern,sizeof(pattern),"%%%s%%",query);
  stmt=NULL;
  if(sqlite3_prepare_v2(db,"SELECT s.id,s.title,s.artist,s.album,s.duration,coalesce(s.isrc,''),a.acrid FROM song s JOIN acr_result a ON a.song_id=s.id WHERE s.available=1 AND coalesce(a.acrid,'')<>'' AND s.id=(SELECT a2.song_id FROM acr_result a2 JOIN song s2 ON s2.id=a2.song_id WHERE a2.acrid=a.acrid AND s2.available=1 ORDER BY a2.first_seen,a2.song_id LIMIT 1) AND (s.title LIKE ?1 OR s.artist LIKE ?1 OR s.album LIKE ?1 OR s.isrc LIKE ?1) ORDER BY s.artist COLLATE NOCASE,s.album COLLATE NOCASE,s.title COLLATE NOCASE LIMIT 250",-1,&stmt,NULL)!=SQLITE_OK)return;
  sqlite3_bind_text(stmt,1,pattern,-1,(void (*)(void *))-1);
  printf("<div class='list'>");
  for(;(rc=sqlite3_step(stmt))==SQLITE_ROW;) {
    printf("<a class='row song' href='?view=play&amp;id=%lld' data-id='%lld' data-title='",sqlite3_column_int64(stmt,0),sqlite3_column_int64(stmt,0));
    html_text(column_text(stmt,1));
    printf("' data-artist='");
    html_text(column_text(stmt,2));
    printf("' data-album='");
    html_text(column_text(stmt,3));
    printf("' data-duration='%d' data-acrid='",sqlite3_column_int(stmt,4));
    html_text(column_text(stmt,6));
    printf("'><strong>");
    html_text(column_text(stmt,1));
    printf("</strong><span><span class='offlineAction'>SAVE OFFLINE</span>");
    html_text(column_text(stmt,2));
    printf(" - ");
    html_text(column_text(stmt,3));
    printf(" - <span class='metric duration'>%d s</span> <span class='badge'>SOURCE</span>",sqlite3_column_int(stmt,4));
    if(store_has_song(cfg,sqlite3_column_int64(stmt,0)))printf(" <span class='badge'>STORE</span>");
    if(column_text(stmt,5)[0]!='\0') {
      printf(" - ");
      html_text(column_text(stmt,5));
    }
    printf("</span></a>");
  }
  printf("</div>");
  sqlite3_finalize(stmt);
}


static void cgi_main(void) {
  struct music_config cfg;
  sqlite3 *db;
  char view[VALUE_SIZE];
  char artist[VALUE_SIZE];
  char album[VALUE_SIZE];
  char query[VALUE_SIZE];
  char media[VALUE_SIZE];
  char sw[VALUE_SIZE];
  char event[VALUE_SIZE];
  char durationbuf[VALUE_SIZE];
  char idbuf[VALUE_SIZE];
  char username[VALUE_SIZE];
  char action[VALUE_SIZE];
  char login_name[VALUE_SIZE];
  char login_password[VALUE_SIZE];
  char playlist_name[VALUE_SIZE];
  char playlist_description[VALUE_SIZE];
  char playlist_idbuf[VALUE_SIZE];
  char playlist_songbuf[VALUE_SIZE];
  char playlist_acrid[VALUE_SIZE];
  char playlist_sharedbuf[VALUE_SIZE];
  char playlist_directionbuf[VALUE_SIZE];
  char shared[VALUE_SIZE];
  char shared_queuebuf[VALUE_SIZE];
  char submitted_csrf[VALUE_SIZE];
  char csrf[65];
  char session_token[128];
  char *body;
  long long song_id;
  long long user_id;
  int logged_in;

  load_config(&cfg);
  body=post_body();
  form_value(action,sizeof(action),body,"action");
  query_value(view,sizeof(view),"view");
  if(strcmp(view,"privacy")==0 && action[0]=='\0') {
    privacy_notice_page();
    free(body);
    return;
  }
  if(strcmp(action,"privacy_accept")==0) {
    set_notice_cookie();
    redirect_home();
    free(body);
    return;
  }
  if(!notice_accepted()) {
    privacy_notice_page();
    free(body);
    return;
  }
  logged_in=session_user(&cfg,&user_id,username,sizeof(username),session_token,sizeof(session_token));
  if(!logged_in && strcmp(action,"login")==0) {
    form_value(login_name,sizeof(login_name),body,"username");
    form_value(login_password,sizeof(login_password),body,"password");
    if(login_user(&cfg,login_name,login_password,session_token,sizeof(session_token))) {
      memset(login_password,0,sizeof(login_password));
      set_session_cookie(session_token);
      redirect_home();
      free(body);
      return;
    }
    memset(login_password,0,sizeof(login_password));
    login_page(&cfg,1);
    free(body);
    return;
  }
  if(!logged_in) {
    login_page(&cfg,0);
    free(body);
    return;
  }
  if(!csrf_from_session(session_token,csrf,sizeof(csrf))) {
    free(body);
    printf("Status: 500 Internal Server Error\r\nContent-Type: text/plain\r\n\r\nSession security error\n");
    return;
  }
  if(strcmp(action,"logout")==0 || strncmp(action,"playlist_",9)==0 || strncmp(action,"shared_",7)==0) {
    form_value(submitted_csrf,sizeof(submitted_csrf),body,"csrf");
    if(!secure_equal(csrf,submitted_csrf)) {
      free(body);
      printf("Status: 403 Forbidden\r\nContent-Type: text/plain\r\nCache-Control: no-store\r\n\r\nInvalid request token\n");
      return;
    }
  }
  if(strcmp(action,"logout")==0) {
    session_logout(&cfg);
    clear_session_cookie();
    redirect_home();
    free(body);
    return;
  }
  set_session_cookie(session_token);
  if(strncmp(action,"shared_",7)==0) {
    shared_queue_action_cgi(&cfg,action,body,user_id);
    free(body);
    return;
  }
  if(strncmp(action,"playlist_",9)==0) {
    long long playlist_id;
    long long playlist_song_id;
    int shared;
    int direction;
    int ok;

    form_value(playlist_name,sizeof(playlist_name),body,"name");
    form_value(playlist_description,sizeof(playlist_description),body,"description");
    form_value(playlist_idbuf,sizeof(playlist_idbuf),body,"playlist_id");
    form_value(playlist_songbuf,sizeof(playlist_songbuf),body,"song_id");
    if(playlist_songbuf[0]=='\0')form_value(playlist_songbuf,sizeof(playlist_songbuf),body,"stored_song_id");
    form_value(playlist_acrid,sizeof(playlist_acrid),body,"acrid");
    form_value(playlist_sharedbuf,sizeof(playlist_sharedbuf),body,"shared");
    form_value(playlist_directionbuf,sizeof(playlist_directionbuf),body,"direction");
    playlist_id=atoll(playlist_idbuf);
    playlist_song_id=atoll(playlist_songbuf);
    shared=atoi(playlist_sharedbuf);
    direction=atoi(playlist_directionbuf);
    ok=0;
    if(strcmp(action,"playlist_create")==0)ok=playlist_create(&cfg,user_id,playlist_name,playlist_description);
    else if(strcmp(action,"playlist_update")==0)ok=playlist_update(&cfg,user_id,playlist_id,playlist_name,playlist_description);
    else if(strcmp(action,"playlist_delete")==0)ok=playlist_delete(&cfg,user_id,playlist_id);
    else if(strcmp(action,"playlist_share")==0)ok=playlist_share(&cfg,user_id,playlist_id,shared);
    else if(strcmp(action,"playlist_add")==0)ok=playlist_add_song(&cfg,user_id,playlist_id,playlist_song_id);
    else if(strcmp(action,"playlist_copy")==0) {
      long long copied;
      copied=playlist_copy(&cfg,user_id,playlist_id);
      if(copied>0) {
        playlist_id=copied;
        ok=1;
      }
    }
    else if(strcmp(action,"playlist_remove")==0) {
      if(playlist_acrid[0]!='\0')ok=playlist_remove_acrid(&cfg,user_id,playlist_id,playlist_acrid);
      else ok=playlist_remove_song(&cfg,user_id,playlist_id,playlist_song_id);
    }
    else if(strcmp(action,"playlist_move")==0) {
      if(direction==-2 || direction==2)ok=playlist_move_edge(&cfg,user_id,playlist_id,playlist_song_id,direction);
      else ok=playlist_move_song(&cfg,user_id,playlist_id,playlist_song_id,direction);
    }
    free(body);
    if(strcmp(action,"playlist_create")==0 || strcmp(action,"playlist_delete")==0 || !ok || playlist_id<=0)printf("Status: 303 See Other\r\nLocation: %s?view=playlists\r\nCache-Control: no-store\r\n\r\n",cookie_path());
    else printf("Status: 303 See Other\r\nLocation: %s?view=playlist&id=%lld\r\nCache-Control: no-store\r\n\r\n",cookie_path(),playlist_id);
    return;
  }
  free(body);
  query_value(shared,sizeof(shared),"shared");
  query_value(shared_queuebuf,sizeof(shared_queuebuf),"queue");
  if(strcmp(shared,"state")==0) {
    shared_queue_state_cgi(&cfg,atoi(shared_queuebuf));
    return;
  }
  query_value(sw,sizeof(sw),"sw");
  if(sw[0]!='\0') {
    service_worker_cgi(user_id);
    return;
  }
  query_value(view,sizeof(view),"view");
  query_value(artist,sizeof(artist),"artist");
  query_value(album,sizeof(album),"album");
  query_value(query,sizeof(query),"q");
  query_value(media,sizeof(media),"media");
  query_value(event,sizeof(event),"event");
  query_value(durationbuf,sizeof(durationbuf),"duration");
  query_value(idbuf,sizeof(idbuf),"id");
  song_id=atoll(idbuf);
  if(media[0]!='\0') {
    media_cgi(&cfg,atoll(media));
    return;
  }
  if(event[0]!='\0') {
    activity_cgi(&cfg,user_id,song_id,event,atoi(durationbuf));
    return;
  }
  if(view[0]=='\0')snprintf(view,sizeof(view),"library");
  db=db_open(&cfg,0);
  html_header(view);
  page_top(view,username,user_id,csrf);
  player();
  printf("<section id='onlinePane'>");
  if(strcmp(view,"library")==0)library_view(db,&cfg,user_id);
  else if(strcmp(view,"artists")==0)artists_view(db);
  else if(strcmp(view,"albums")==0)albums_view(db,artist);
  else if(strcmp(view,"tracks")==0)tracks_view(db,&cfg,artist,album);
  else if(strcmp(view,"search")==0)search_view(db,&cfg,query);
  else if(strcmp(view,"playlists")==0)playlists_view(db,user_id);
  else if(strcmp(view,"playlist")==0)playlist_view(db,&cfg,user_id,song_id);
  else if(strcmp(view,"play")==0)play_view(db,user_id,song_id);
  else if(strcmp(view,"more")==0)more_view(db,&cfg,user_id,username);
  else printf("<div class='notice'>Unknown section.</div>");
  printf("</section><section id='offlinePane' hidden><input id='offlineSearch' class='search' placeholder='Search offline music...' autocomplete='off'><div id='offlineSongs'></div></section>");
  printf("</main>");
  nav();
  printf("</body></html>\n");
  if(db!=NULL)sqlite3_close(db);
}

static void usage(const char *name) {
  printf("Music %s\n",MUSIC_VERSION);
  printf("usage: %s <command>\n\n",name);
  printf("commands:\n");
  printf("  help       show this help\n");
  printf("  version    show version\n");
  printf("  init       create or upgrade the SQLite catalog\n");
  printf("  drive      verify Google Drive access and Music folder\n");
  printf("  scan       scan Google Drive into the local catalog\n");
  printf("  check      verify catalog integrity, classification, STORE and queues\n");
  printf("  store ID           download one song from SOURCE to STORE\n");
  printf("  store classified   download all classified catalog tracks missing from STORE\n");
  printf("  user add NAME      create a local user account\n");
  printf("  user list          list local user accounts\n");
  printf("  user info NAME     show aggregate account and usage statistics\n");
  printf("  user password NAME reset a user password and sessions\n");
  printf("  user delete NAME   delete a user and owned sessions/playlists\n");
  printf("  user disable NAME  disable login and remove active sessions\n");
  printf("  user enable NAME   enable login for an existing user\n");
  printf("  acr ID [--force]   identify with ACRCloud; reuse saved result by default\n");
  printf("  acr random N       identify N random songs, excluding final NO MATCH entries\n");
  printf("  acr retry N        retry up to N final NO MATCH songs\n");
  printf("  acr sync           reparse saved JSON and update the catalog offline\n");
}

static int cli_main(int argc,char **argv) {
  struct music_config cfg;
  sqlite3 *db;
  struct stat st;

  load_config(&cfg);
  if(argc<2 || strcmp(argv[1],"help")==0) {
    usage(argv[0]);
    return 0;
  }
  if(strcmp(argv[1],"version")==0) {
    printf("%s\n",MUSIC_VERSION);
    return 0;
  }
  if(strcmp(argv[1],"init")==0) {
    if(!db_init(&cfg)) {
      fprintf(stderr,"cannot initialize %s\n",cfg.db);
      return 1;
    }
    printf("catalog initialized: %s\n",cfg.db);
    return 0;
  }
  if(strcmp(argv[1],"drive")==0)return drive_test(&cfg) ? 0 : 1;
  if(strcmp(argv[1],"scan")==0)return drive_scan(&cfg) ? 0 : 1;
  if(strcmp(argv[1],"store")==0) {
    char path[VALUE_SIZE*2];
    long long song_id;

    if(argc==3 && strcmp(argv[2],"classified")==0)return store_classified(&cfg) ? 0 : 1;
    if(argc!=3) {
      fprintf(stderr,"usage: %s store ID|classified\n",argv[0]);
      return 1;
    }
    song_id=atoll(argv[2]);
    if(!store_song(&cfg,song_id,path,sizeof(path),1)) {
      fprintf(stderr,"cannot store song %lld\n",song_id);
      return 1;
    }
    return 0;
  }
  if(strcmp(argv[1],"user")==0) {
    if(argc==3 && strcmp(argv[2],"list")==0)return user_list(&cfg) ? 0 : 1;
    if(argc==4 && strcmp(argv[2],"info")==0)return user_info(&cfg,argv[3]) ? 0 : 1;
    if(argc==4 && strcmp(argv[2],"add")==0) {
      if(user_add(&cfg,argv[3])) { printf("user added: %s\n",argv[3]); return 0; }
      return 1;
    }
    if(argc==4 && strcmp(argv[2],"password")==0) {
      if(user_password(&cfg,argv[3])) { printf("user password updated: %s\n",argv[3]); return 0; }
      return 1;
    }
    if(argc==4 && strcmp(argv[2],"delete")==0) {
      if(user_delete(&cfg,argv[3])) { printf("user deleted: %s\n",argv[3]); return 0; }
      return 1;
    }
    if(argc==4 && strcmp(argv[2],"disable")==0) {
      if(user_set_enabled(&cfg,argv[3],0)) { printf("user disabled: %s\n",argv[3]); return 0; }
      return 1;
    }
    if(argc==4 && strcmp(argv[2],"enable")==0) {
      if(user_set_enabled(&cfg,argv[3],1)) { printf("user enabled: %s\n",argv[3]); return 0; }
      return 1;
    }
    fprintf(stderr,"usage: %s user add NAME\n       %s user list\n       %s user info NAME\n       %s user password NAME\n       %s user delete NAME\n       %s user disable NAME\n       %s user enable NAME\n",argv[0],argv[0],argv[0],argv[0],argv[0],argv[0],argv[0]);
    return 1;
  }
  if(strcmp(argv[1],"acr")==0) {
    long long song_id;
    int force;
    int count;

    if(argc==3 && strcmp(argv[2],"sync")==0)return acr_sync(&cfg) ? 0 : 1;
    if(argc==4 && strcmp(argv[2],"random")==0) {
      count=atoi(argv[3]);
      return acr_random(&cfg,count) ? 0 : 1;
    }
    if(argc==4 && strcmp(argv[2],"retry")==0) {
      count=atoi(argv[3]);
      return acr_retry_nomatch(&cfg,count) ? 0 : 1;
    }
    if(argc<3 || argc>4 || (argc==4 && strcmp(argv[3],"--force")!=0)) {
      fprintf(stderr,"usage: %s acr ID [--force]\n",argv[0]);
      fprintf(stderr,"       %s acr random N\n",argv[0]);
      fprintf(stderr,"       %s acr retry N\n",argv[0]);
      fprintf(stderr,"       %s acr sync\n",argv[0]);
      return 1;
    }
    song_id=atoll(argv[2]);
    force=argc==4;
    return acr_song(&cfg,song_id,force)>0 ? 0 : 1;
  }
  if(strcmp(argv[1],"check")==0) {
    sqlite3_stmt *stmt;
    long long source_count;
    long long source_bytes;
    long long source_unavailable;
    long long store_n;
    long long store_bytes;
    long long store_bad;
    long long schema_version;
    long long physical;
    long long available;
    long long recognized;
    long long no_match;
    long long pending;
    long long users;
    long long enabled;
    long long sessions;
    long long stats_users;
    long long fk_errors;
    char source_size[64];
    char store_size[64];
    int quick_ok;
    int health_ok;
    int warning;
    int rc;

    health_ok=1;
    warning=0;
    printf("music %s: executable ok\n",MUSIC_VERSION);
    printf("catalog: %s\n",cfg.db);
    db=db_open(&cfg,0);
    if(db==NULL || db_scalar(db,"SELECT count(*) FROM sqlite_master WHERE type='table' AND name='meta'")==0) {
      if(db!=NULL)sqlite3_close(db);
      printf("catalog status: not initialized\n");
      return 1;
    }
    schema_version=db_scalar(db,"SELECT CAST(value AS INTEGER) FROM meta WHERE key='schema'");
    quick_ok=db_quick_check(db);
    fk_errors=db_foreign_key_errors(db);
    if(schema_version!=SCHEMA_VERSION || !quick_ok || fk_errors!=0)health_ok=0;
    printf("schema: %lld (expected %d)\n",schema_version,SCHEMA_VERSION);
    printf("integrity: quick_check %s, foreign keys %s",quick_ok ? "ok" : "FAILED",fk_errors==0 ? "ok" : "FAILED");
    if(fk_errors>0)printf(" (%lld errors)",fk_errors);
    printf("\n");
    physical=db_scalar(db,"SELECT count(*) FROM song");
    available=db_scalar(db,"SELECT count(*) FROM song WHERE available=1");
    recognized=db_scalar(db,"SELECT count(DISTINCT a.acrid) FROM acr_result a JOIN song s ON s.id=a.song_id WHERE s.available=1 AND coalesce(a.acrid,'')<>''");
    no_match=db_scalar(db,"SELECT count(*) FROM acr_failure f JOIN song s ON s.id=f.song_id WHERE s.available=1 AND f.terminal=1 AND NOT EXISTS(SELECT 1 FROM acr_result a WHERE a.song_id=s.id)");
    pending=db_scalar(db,"SELECT count(*) FROM song s WHERE s.available=1 AND NOT EXISTS(SELECT 1 FROM acr_result a WHERE a.song_id=s.id) AND NOT EXISTS(SELECT 1 FROM acr_failure f WHERE f.song_id=s.id AND f.terminal=1)");
    printf("catalog: %lld physical, %lld available, %lld unavailable\n",physical,available,physical-available);
    printf("ACR: %lld logical recognized, %lld NO MATCH, %lld pending\n",recognized,no_match,pending);
    source_count=db_scalar(db,"SELECT count(*) FROM source src JOIN song s ON s.id=src.song_id WHERE src.type='drive' AND s.available=1");
    source_bytes=db_scalar(db,"SELECT coalesce(sum(src.bytes),0) FROM source src JOIN song s ON s.id=src.song_id WHERE src.type='drive' AND s.available=1");
    source_unavailable=db_scalar(db,"SELECT count(*) FROM source src JOIN song s ON s.id=src.song_id WHERE src.type='drive' AND s.available=0");
    size_text(source_size,sizeof(source_size),source_bytes);
    printf("SOURCE: %lld, %s",source_count,source_size);
    if(source_unavailable>0)printf("; %lld historical unavailable",source_unavailable);
    printf("\n");
    store_stats(&cfg,&store_n,&store_bytes);
    store_bad=store_orphans(&cfg,db);
    size_text(store_size,sizeof(store_size),store_bytes);
    printf("STORE: %lld, %s (%s), %lld orphan%s\n",store_n,store_size,stat(cfg.store,&st)==0 && S_ISDIR(st.st_mode) ? cfg.store : "not found",store_bad,store_bad==1 ? "" : "s");
    if(store_bad>0)warning=1;
    users=db_scalar(db,"SELECT count(*) FROM user");
    enabled=db_scalar(db,"SELECT count(*) FROM user WHERE enabled=1");
    sessions=db_scalar(db,"SELECT count(*) FROM session WHERE expires>unixepoch()");
    stats_users=db_scalar(db,"SELECT count(*) FROM user_stats");
    printf("users: %lld total, %lld enabled, %lld active sessions, %lld statistics rows\n",users,enabled,sessions,stats_users);
    if(stats_users!=users)warning=1;
    printf("shared queues:");
    stmt=NULL;
    if(sqlite3_prepare_v2(db,"SELECT q.id,q.state,count(s.song_id) FROM shared_queue q LEFT JOIN shared_queue_song s ON s.queue_id=q.id GROUP BY q.id ORDER BY q.id",-1,&stmt,NULL)==SQLITE_OK) {
      for(;(rc=sqlite3_step(stmt))==SQLITE_ROW;)printf(" C%d=%d/%s",sqlite3_column_int(stmt,0),sqlite3_column_int(stmt,2),sqlite3_column_int(stmt,1) ? "PLAY" : "PAUSE");
      sqlite3_finalize(stmt);
    }
    printf("\n");
    sqlite3_close(db);
    printf("OFFLINE: device-side browser storage; size is shown in the web interface\n");
    printf("status: %s\n",!health_ok ? "FAILED" : warning ? "warning" : "ok");
    return health_ok ? 0 : 1;
  }
  fprintf(stderr,"unknown command: %s\n",argv[1]);
  usage(argv[0]);
  return 1;
}

int main(int argc,char **argv) {
  if(is_cgi()) {
    cgi_main();
    return 0;
  }
  return cli_main(argc,argv);
}
