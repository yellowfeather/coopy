/*
** Copyright (c) 2007 D. Richard Hipp
**
** This program is free software; you can redistribute it and/or
** modify it under the terms of the GNU General Public
** License version 2 as published by the Free Software Foundation.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
** General Public License for more details.
** 
** You should have received a copy of the GNU General Public
** License along with this library; if not, write to the
** Free Software Foundation, Inc., 59 Temple Place - Suite 330,
** Boston, MA  02111-1307, USA.
**
** Author contact information:
**   drh@hwaci.com
**   http://www.hwaci.com/drh/
**
*******************************************************************************
**
** This file contains code to implement the "info" command.  The
** "info" command gives command-line access to information about
** the current tree, or a particular artifact or check-in.
*/
#include "config.h"
#include "info.h"
#include <assert.h>

/*
** Return a string (in memory obtained from malloc) holding a 
** comma-separated list of tags that apply to check-in with 
** record-id rid.  If the "propagatingOnly" flag is true, then only
** show branch tags (tags that propagate to children).
**
** Return NULL if there are no such tags.
*/
char *info_tags_of_checkin(int rid, int propagatingOnly){
  char *zTags;
  zTags = db_text(0, "SELECT group_concat(substr(tagname, 5), ', ')"
                     "  FROM tagxref, tag"
                     " WHERE tagxref.rid=%d AND tagxref.tagtype>%d"
                     "   AND tag.tagid=tagxref.tagid"
                     "   AND tag.tagname GLOB 'sym-*'",
                     rid, propagatingOnly!=0);
  return zTags;
}


/*
** Print common information about a particular record.
**
**     *  The UUID
**     *  The record ID
**     *  mtime and ctime
**     *  who signed it
*/
void show_common_info(int rid, const char *zUuidName, int showComment){
  Stmt q;
  char *zComment = 0;
  char *zTags;
  char *zDate;
  char *zUuid;
  zUuid = db_text(0, "SELECT uuid FROM blob WHERE rid=%d", rid);
  if( zUuid ){
    zDate = db_text(0, 
      "SELECT datetime(mtime) || ' UTC' FROM event WHERE objid=%d",
      rid
    );
         /* 01234567890123 */
    printf("%-13s %s %s\n", zUuidName, zUuid, zDate ? zDate : "");
    free(zUuid);
    free(zDate);
  }
  db_prepare(&q, "SELECT uuid, pid FROM plink JOIN blob ON pid=rid "
                 " WHERE cid=%d", rid);
  while( db_step(&q)==SQLITE_ROW ){
    const char *zUuid = db_column_text(&q, 0);
    zDate = db_text("", 
      "SELECT datetime(mtime) || ' UTC' FROM event WHERE objid=%d",
      db_column_int(&q, 1)
    );
    printf("parent:       %s %s\n", zUuid, zDate);
    free(zDate);
  }
  db_finalize(&q);
  db_prepare(&q, "SELECT uuid, cid FROM plink JOIN blob ON cid=rid "
                 " WHERE pid=%d", rid);
  while( db_step(&q)==SQLITE_ROW ){
    const char *zUuid = db_column_text(&q, 0);
    zDate = db_text("", 
      "SELECT datetime(mtime) || ' UTC' FROM event WHERE objid=%d",
      db_column_int(&q, 1)
    );
    printf("child:        %s %s\n", zUuid, zDate);
    free(zDate);
  }
  db_finalize(&q);
  zTags = info_tags_of_checkin(rid, 0);
  if( zTags && zTags[0] ){
    printf("tags:         %s\n", zTags);
  }
  free(zTags);
  if( zComment ){
    printf("comment:\n%s\n", zComment);
    free(zComment);
  }
}


/*
** COMMAND: info
**
** Usage: %fossil info ?ARTIFACT-ID|FILENAME?
**
** With no arguments, provide information about the current tree.
** If an argument is specified, provide information about the object
** in the respository of the current tree that the argument refers
** to.  Or if the argument is the name of a repository, show
** information about that repository.
*/
void info_cmd(void){
  i64 fsize;
  if( g.argc!=2 && g.argc!=3 ){
    usage("?FILENAME|ARTIFACT-ID?");
  }
  if( g.argc==3 && (fsize = file_size(g.argv[2]))>0 && (fsize&0x1ff)==0 ){
    db_open_config(0);
    db_record_repository_filename(g.argv[2]);
    db_open_repository(g.argv[2]);
    printf("project-name: %s\n", db_get("project-name", "<unnamed>"));
    printf("project-code: %s\n", db_get("project-code", "<none>"));
    printf("server-code:  %s\n", db_get("server-code", "<none>"));
    return;
  }
  db_must_be_within_tree();
  if( g.argc==2 ){
    int vid;
         /* 012345678901234 */
    db_record_repository_filename(0);
    printf("project-name: %s\n", db_get("project-name", "<unnamed>"));
    printf("repository:   %s\n", db_lget("repository", ""));
    printf("local-root:   %s\n", g.zLocalRoot);
#ifdef __MINGW32__
    if( g.zHome ){
      printf("user-home:    %s\n", g.zHome);
    }
#endif
    printf("project-code: %s\n", db_get("project-code", ""));
    printf("server-code:  %s\n", db_get("server-code", ""));
    vid = db_lget_int("checkout", 0);
    if( vid==0 ){
      printf("checkout:     nil\n");
    }else{
      show_common_info(vid, "checkout:", 1);
    }
  }else{
    int rid;
    rid = name_to_rid(g.argv[2]);
    if( rid==0 ){
      fossil_panic("no such object: %s\n", g.argv[2]);
    }
    show_common_info(rid, "uuid:", 1);
  }
}

/*
** Show information about all tags on a given node.
*/
static void showTags(int rid, const char *zNotGlob){
  Stmt q;
  int cnt = 0;
  db_prepare(&q,
    "SELECT tag.tagid, tagname, "
    "       (SELECT uuid FROM blob WHERE rid=tagxref.srcid AND rid!=%d),"
    "       value, datetime(tagxref.mtime,'localtime'), tagtype,"
    "       (SELECT uuid FROM blob WHERE rid=tagxref.origid AND rid!=%d)"
    "  FROM tagxref JOIN tag ON tagxref.tagid=tag.tagid"
    " WHERE tagxref.rid=%d AND tagname NOT GLOB '%s'"
    " ORDER BY tagname", rid, rid, rid, zNotGlob
  );
  while( db_step(&q)==SQLITE_ROW ){
    const char *zTagname = db_column_text(&q, 1);
    const char *zSrcUuid = db_column_text(&q, 2);
    const char *zValue = db_column_text(&q, 3);
    const char *zDate = db_column_text(&q, 4);
    int tagtype = db_column_int(&q, 5);
    const char *zOrigUuid = db_column_text(&q, 6);
    cnt++;
    if( cnt==1 ){
      @ <div class="section">Tags And Properties</div>
      @ <ul>
    }
    @ <li>
    if( tagtype==0 ){
      @ <b><s>%h(zTagname)</s></b> cancelled
    }else if( zValue ){
      @ <b>%h(zTagname)=%h(zValue)</b>
    }else {
      @ <b>%h(zTagname)</b>
    }
    if( tagtype==2 ){
      if( zOrigUuid && zOrigUuid[0] ){
        @ inherited from
        hyperlink_to_uuid(zOrigUuid);
      }else{
        @ propagates to descendants
      }
      if( zValue && strcmp(zTagname,"branch")==0 ){
        @ &nbsp;&nbsp;
        @ <a href="%s(g.zBaseURL)/timeline?t=%T(zValue)">branch timeline</a>
      }
    }
    if( zSrcUuid && zSrcUuid[0] ){
      if( tagtype==0 ){
        @ by
      }else{
        @ added by
      }
      hyperlink_to_uuid(zSrcUuid);
      @ on
      hyperlink_to_date(zDate,0);
    }
  }
  db_finalize(&q);
  if( cnt ){
    @ </ul>
  }
}


/*
** Append the difference between two RIDs to the output
*/
static void append_diff(int fromid, int toid){
  Blob from, to, out;
  content_get(fromid, &from);
  content_get(toid, &to);
  blob_zero(&out);
  text_diff(&from, &to, &out, 5);
  @ %h(blob_str(&out))
  blob_reset(&from);
  blob_reset(&to);
  blob_reset(&out);  
}


/*
** WEBPAGE: vinfo
** WEBPAGE: ci
** URL:  /ci?name=RID|ARTIFACTID
**
** Display information about a particular check-in. 
**
** We also jump here from /info if the name is a version.
**
** If the /ci page is used (instead of /vinfo or /info) then the
** default behavior is to show unified diffs of all file changes.
** With /vinfo and /info, only a list of the changed files are
** shown, without diffs.  This behavior is inverted if the
** "show-version-diffs" setting is turned on.
*/
void ci_page(void){
  Stmt q;
  int rid;
  int isLeaf;
  int showDiff;
  const char *zName;

  login_check_credentials();
  if( !g.okRead ){ login_needed(); return; }
  zName = PD("name","0");
  rid = name_to_rid(zName);
  if( rid==0 ){
    style_header("Check-in Information Error");
    @ No such object: %h(g.argv[2])
    style_footer();
    return;
  }
  isLeaf = !db_exists("SELECT 1 FROM plink WHERE pid=%d", rid);
  db_prepare(&q, 
     "SELECT uuid, datetime(mtime, 'localtime'), user, comment"
     "  FROM blob, event"
     " WHERE blob.rid=%d"
     "   AND event.objid=%d",
     rid, rid
  );
  if( db_step(&q)==SQLITE_ROW ){
    const char *zUuid = db_column_text(&q, 0);
    char *zTitle = mprintf("Check-in [%.10s]", zUuid);
    char *zEUser, *zEComment;
    const char *zUser;
    const char *zComment;
    const char *zDate;
    style_header(zTitle);
    login_anonymous_available();
    free(zTitle);
    zEUser = db_text(0,
                   "SELECT value FROM tagxref WHERE tagid=%d AND rid=%d",
                    TAG_USER, rid);
    zEComment = db_text(0, 
                   "SELECT value FROM tagxref WHERE tagid=%d AND rid=%d",
                   TAG_COMMENT, rid);
    zUser = db_column_text(&q, 2);
    zComment = db_column_text(&q, 3);
    zDate = db_column_text(&q,1);
    @ <div class="section">Overview</div>
    @ <p><table class="label-value">
    @ <tr><th>SHA1&nbsp;Hash:</th><td>%s(zUuid)
    if( g.okSetup ){
      @ (Record ID: %d(rid))
    }
    @ </td></tr>
    @ <tr><th>Date:</th><td>
    hyperlink_to_date(zDate, "</td></tr>");
    if( zEUser ){
      @ <tr><th>Edited&nbsp;User:</th><td>
      hyperlink_to_user(zEUser,zDate,"</td></tr>");
      @ <tr><th>Original&nbsp;User:</th><td>
      hyperlink_to_user(zUser,zDate,"</td></tr>");
    }else{
      @ <tr><th>User:</th><td>
      hyperlink_to_user(zUser,zDate,"</td></tr>");
    }
    if( zEComment ){
      @ <tr><th>Edited&nbsp;Comment:</th><td>%w(zEComment)</td></tr>
      @ <tr><th>Original&nbsp;Comment:</th><td>%w(zComment)</td></tr>
    }else{
      @ <tr><th>Comment:</th><td>%w(zComment)</td></tr>
    }
    if( g.okAdmin ){
      db_prepare(&q, 
         "SELECT rcvfrom.ipaddr, user.login, datetime(rcvfrom.mtime)"
         "  FROM blob JOIN rcvfrom USING(rcvid) LEFT JOIN user USING(uid)"
         " WHERE blob.rid=%d",
         rid
      );
      if( db_step(&q)==SQLITE_ROW ){
        const char *zIpAddr = db_column_text(&q, 0);
        const char *zUser = db_column_text(&q, 1);
        const char *zDate = db_column_text(&q, 2);
        if( zUser==0 || zUser[0]==0 ) zUser = "unknown";
        @ <tr><th>Received&nbsp;From:</th>
        @ <td>%h(zUser) @ %h(zIpAddr) on %s(zDate)</td></tr>
      }
      db_finalize(&q);
    }
    if( g.okHistory ){
      char *zShortUuid = mprintf("%.10s", zUuid);
      const char *zProjName = db_get("project-name", "unnamed");
      @ <tr><th>Timelines:</th><td>
      @    <a href="%s(g.zBaseURL)/timeline?p=%d(rid)">ancestors</a>
      @    | <a href="%s(g.zBaseURL)/timeline?d=%d(rid)">descendants</a>
      @    | <a href="%s(g.zBaseURL)/timeline?d=%d(rid)&p=%d(rid)">both</a>
      db_prepare(&q, "SELECT substr(tag.tagname,5) FROM tagxref, tag "
                     " WHERE rid=%d AND tagtype>0 "
                     "   AND tag.tagid=tagxref.tagid "
                     "   AND +tag.tagname GLOB 'sym-*'", rid);
      while( db_step(&q)==SQLITE_ROW ){
        const char *zTagName = db_column_text(&q, 0);
        @  | <a href="%s(g.zBaseURL)/timeline?t=%T(zTagName)">%h(zTagName)</a>
      }
      db_finalize(&q);
      @ </td></tr>
      @ <tr><th>Other&nbsp;Links:</th>
      @   <td>
      @     <a href="%s(g.zBaseURL)/dir?ci=%s(zShortUuid)">files</a>
      @     | <a href="%s(g.zBaseURL)/zip/%s(zProjName)-%s(zShortUuid).zip?uuid=%s(zUuid)">
      @         ZIP archive</a>
      @     | <a href="%s(g.zBaseURL)/artifact/%d(rid)">manifest</a>
      if( g.okWrite ){
        @     | <a href="%s(g.zBaseURL)/ci_edit?r=%d(rid)">edit</a>
      }
      @   </td>
      @ </tr>
      free(zShortUuid);
    }
    @ </table></p>
  }else{
    style_header("Check-in Information");
    login_anonymous_available();
  }
  db_finalize(&q);
  showTags(rid, "");
  @ <div class="section">Changes</div>
  showDiff = g.zPath[0]!='c';
  if( db_get_boolean("show-version-diffs", 0)==0 ){
    showDiff = !showDiff;
    if( showDiff ){
      @ <a href="%s(g.zBaseURL)/vinfo/%T(zName)">[hide&nbsp;diffs]</a><br/>
    }else{
      @ <a href="%s(g.zBaseURL)/ci/%T(zName)">[show&nbsp;diffs]</a><br/>
    }
  }else{
    if( showDiff ){
      @ <a href="%s(g.zBaseURL)/ci/%T(zName)">[hide&nbsp;diffs]</a><br/>
    }else{
      @ <a href="%s(g.zBaseURL)/vinfo/%T(zName)">[show&nbsp;diffs]</a><br/>
    }
  }
  db_prepare(&q,
     "SELECT pid, fid, name, substr(a.uuid,1,10), substr(b.uuid,1,10)"
     "  FROM mlink JOIN filename ON filename.fnid=mlink.fnid"
     "         LEFT JOIN blob a ON a.rid=pid"
     "         LEFT JOIN blob b ON b.rid=fid"
     " WHERE mlink.mid=%d"
     " ORDER BY name",
     rid
  );
  while( db_step(&q)==SQLITE_ROW ){
    int pid = db_column_int(&q,0);
    int fid = db_column_int(&q,1);
    const char *zName = db_column_text(&q,2);
    const char *zOld = db_column_text(&q,3);
    const char *zNew = db_column_text(&q,4);
    if( !g.okHistory ){
      if( zNew==0 ){
        @ <p>Deleted %h(zName)</p>
        continue;
      }else{
        @ <p>Changes to %h(zName)</p>
      }
    }else if( zOld && zNew ){
      @ <p>Modified <a href="%s(g.zBaseURL)/finfo?name=%T(zName)">%h(zName)</a>
      @ from <a href="%s(g.zBaseURL)/artifact/%s(zOld)">[%s(zOld)]</a>
      @ to <a href="%s(g.zBaseURL)/artifact/%s(zNew)">[%s(zNew)].</a>
      if( !showDiff ){
        @ &nbsp;&nbsp;
        @ <a href="%s(g.zBaseURL)/fdiff?v1=%d(pid)&v2=%d(fid)">[diff]</a>
      }
    }else if( zOld ){
      @ <p>Deleted <a href="%s(g.zBaseURL)/finfo?name=%T(zName)">%h(zName)</a>
      @ version <a href="%s(g.zBaseURL)/artifact/%s(zOld)">[%s(zOld)]</a></p>
      continue;
    }else{
      @ <p>Added <a href="%s(g.zBaseURL)/finfo?name=%T(zName)">%h(zName)</a>
      @ version <a href="%s(g.zBaseURL)/artifact/%s(zNew)">[%s(zNew)]</a></p>
    }
    if( showDiff ){
      @ <blockquote><pre>
      append_diff(pid, fid);
      @ </pre></blockquote>
    }
  }
  db_finalize(&q);
  style_footer();
}

/*
** WEBPAGE: winfo
** URL:  /winfo?name=RID
**
** Return information about a wiki page.
*/
void winfo_page(void){
  Stmt q;
  int rid;

  login_check_credentials();
  if( !g.okRdWiki ){ login_needed(); return; }
  rid = name_to_rid(PD("name","0"));
  if( rid==0 ){
    style_header("Wiki Page Information Error");
    @ No such object: %h(g.argv[2])
    style_footer();
    return;
  }
  db_prepare(&q, 
     "SELECT substr(tagname, 6, 1000), uuid,"
     "       datetime(event.mtime, 'localtime'), user"
     "  FROM tagxref, tag, blob, event"
     " WHERE tagxref.rid=%d"
     "   AND tag.tagid=tagxref.tagid"
     "   AND tag.tagname LIKE 'wiki-%%'"
     "   AND blob.rid=%d"
     "   AND event.objid=%d",
     rid, rid, rid
  );
  if( db_step(&q)==SQLITE_ROW ){
    const char *zName = db_column_text(&q, 0);
    const char *zUuid = db_column_text(&q, 1);
    char *zTitle = mprintf("Wiki Page %s", zName);
    const char *zDate = db_column_text(&q,2);
    const char *zUser = db_column_text(&q,3);
    style_header(zTitle);
    free(zTitle);
    login_anonymous_available();
    @ <div class="section">Overview</div>
    @ <p><table class="label-value">
    @ <tr><th>Version:</th><td>%s(zUuid)</td></tr>
    @ <tr><th>Date:</th><td>
    hyperlink_to_date(zDate, "</td></tr>");
    if( g.okSetup ){
      @ <tr><th>Record ID:</th><td>%d(rid)</td></tr>
    }
    @ <tr><th>Original&nbsp;User:</th><td>
    hyperlink_to_user(zUser, zDate, "</td></tr>");
    if( g.okHistory ){
      @ <tr><th>Commands:</th>
      @   <td>
      /* @     <a href="%s(g.zBaseURL)/wdiff/%d(rid)">diff</a> | */
      @     <a href="%s(g.zBaseURL)/whistory?name=%t(zName)">history</a>
      @     | <a href="%s(g.zBaseURL)/artifact/%d(rid)">raw-text</a>
      @   </td>
      @ </tr>
    }
    @ </table></p>
  }else{
    style_header("Wiki Information");
    rid = 0;
  }
  db_finalize(&q);
  showTags(rid, "wiki-*");
  if( rid ){
    Blob content;
    Manifest m;
    memset(&m, 0, sizeof(m));
    blob_zero(&m.content);
    content_get(rid, &content);
    manifest_parse(&m, &content);
    if( m.type==CFTYPE_WIKI ){
      Blob wiki;
      blob_init(&wiki, m.zWiki, -1);
      @ <div class="section">Content</div>
      wiki_convert(&wiki, 0, 0);
      blob_reset(&wiki);
    }
    manifest_clear(&m);
  }
  style_footer();
}

/*
** WEBPAGE: vdiff
** URL: /vdiff?name=RID
**
** Show all differences for a particular check-in.
*/
void vdiff_page(void){
  int rid;
  Stmt q;
  char *zUuid;

  login_check_credentials();
  if( !g.okRead ){ login_needed(); return; }
  login_anonymous_available();

  rid = name_to_rid(PD("name",""));
  if( rid==0 ){
    fossil_redirect_home();
  }
  zUuid = db_text(0, "SELECT uuid FROM blob WHERE rid=%d", rid);
  style_header("Check-in [%.10s]", zUuid);
  db_prepare(&q,
    "SELECT datetime(mtime), "
    "       coalesce(event.ecomment,event.comment),"
    "       coalesce(event.euser,event.user)"
    "  FROM event WHERE type='ci' AND objid=%d",
    rid
  );
  while( db_step(&q)==SQLITE_ROW ){
    const char *zDate = db_column_text(&q, 0);
    const char *zUser = db_column_text(&q, 2);
    const char *zComment = db_column_text(&q, 1);
    @ <h2>Check-in %s(zUuid)</h2>
    @ <p>Made by
    hyperlink_to_user(zUser,zDate," on");
    hyperlink_to_date(zDate, ":");
    @ %w(zComment). 
    if( g.okHistory ){
      @ <a href="%s(g.zBaseURL)/ci/%s(zUuid)">[details]</a>
    }
    @ </p><hr>
  }
  db_finalize(&q);
  db_prepare(&q,
     "SELECT pid, fid, name"
     "  FROM mlink, filename"
     " WHERE mlink.mid=%d"
     "   AND filename.fnid=mlink.fnid"
     " ORDER BY name",
     rid
  );
  while( db_step(&q)==SQLITE_ROW ){
    int pid = db_column_int(&q,0);
    int fid = db_column_int(&q,1);
    const char *zName = db_column_text(&q,2);
    if( g.okHistory ){
      @ <p><a href="%s(g.zBaseURL)/finfo?name=%T(zName)">%h(zName)</a></p>
    }else{
      @ <p>%h(zName)</p>
    }
    @ <blockquote><pre>
    append_diff(pid, fid);
    @ </pre></blockquote>
  }
  db_finalize(&q);
  style_footer();
}

/*
** Write a description of an object to the www reply.
**
** If the object is a file then mention:
**
**     * It's artifact ID
**     * All its filenames
**     * The check-in it was part of, with times and users
**
** If the object is a manifest, then mention:
**
**     * It's artifact ID
**     * date of check-in
**     * Comment & user
*/
static void object_description(
  int rid,                 /* The artifact ID */
  int linkToView,          /* Add viewer link if true */
  Blob *pDownloadName      /* Fill with an appropriate download name */
){
  Stmt q;
  int cnt = 0;
  int nWiki = 0;
  db_prepare(&q,
    "SELECT filename.name, datetime(event.mtime),"
    "       coalesce(event.ecomment,event.comment),"
    "       coalesce(event.euser,event.user),"
    "       b.uuid"
    "  FROM mlink, filename, event, blob a, blob b"
    " WHERE filename.fnid=mlink.fnid"
    "   AND event.objid=mlink.mid"
    "   AND a.rid=mlink.fid"
    "   AND b.rid=mlink.mid"
    "   AND mlink.fid=%d",
    rid
  );
  while( db_step(&q)==SQLITE_ROW ){
    const char *zName = db_column_text(&q, 0);
    const char *zDate = db_column_text(&q, 1);
    const char *zCom = db_column_text(&q, 2);
    const char *zUser = db_column_text(&q, 3);
    const char *zVers = db_column_text(&q, 4);
    if( cnt>0 ){
      @ Also file
    }else{
      @ File
    }
    if( g.okHistory ){
      @ <a href="%s(g.zBaseURL)/finfo?name=%T(zName)">%h(zName)</a>
    }else{
      @ %h(zName)
    }
    @ part of check-in
    hyperlink_to_uuid(zVers);
    @ - %w(zCom) by 
    hyperlink_to_user(zUser,zDate," on");
    hyperlink_to_date(zDate,".");
    cnt++;
    if( pDownloadName && blob_size(pDownloadName)==0 ){
      blob_append(pDownloadName, zName, -1);
    }
  }
  db_finalize(&q);
  db_prepare(&q, 
    "SELECT substr(tagname, 6, 10000), datetime(event.mtime),"
    "       coalesce(event.euser, event.user)"
    "  FROM tagxref, tag, event"
    " WHERE tagxref.rid=%d"
    "   AND tag.tagid=tagxref.tagid" 
    "   AND tag.tagname LIKE 'wiki-%%'"
    "   AND event.objid=tagxref.rid",
    rid
  );
  while( db_step(&q)==SQLITE_ROW ){
    const char *zPagename = db_column_text(&q, 0);
    const char *zDate = db_column_text(&q, 1);
    const char *zUser = db_column_text(&q, 2);
    if( cnt>0 ){
      @ Also wiki page
    }else{
      @ Wiki page
    }
    if( g.okHistory ){
      @ [<a href="%s(g.zBaseURL)/wiki?name=%t(zPagename)">%h(zPagename)</a>]
    }else{
      @ [%h(zPagename)]
    }
    @ by
    hyperlink_to_user(zUser,zDate," on");
    hyperlink_to_date(zDate,".");
    nWiki++;
    cnt++;
    if( pDownloadName && blob_size(pDownloadName)==0 ){
      blob_append(pDownloadName, zPagename, -1);
    }
  }
  db_finalize(&q);
  if( nWiki==0 ){
    db_prepare(&q,
      "SELECT datetime(mtime), user, comment, type, uuid"
      "  FROM event, blob"
      " WHERE event.objid=%d"
      "   AND blob.rid=%d",
      rid, rid
    );
    while( db_step(&q)==SQLITE_ROW ){
      const char *zDate = db_column_text(&q, 0);
      const char *zUser = db_column_text(&q, 1);
      const char *zCom = db_column_text(&q, 2);
      const char *zType = db_column_text(&q, 3);
      const char *zUuid = db_column_text(&q, 4);
      if( cnt>0 ){
        @ Also
      }
      if( zType[0]=='w' ){
        @ Wiki edit
      }else if( zType[0]=='t' ){
        @ Ticket change
      }else if( zType[0]=='c' ){
        @ Manifest of check-in
      }else{
        @ Control file referencing
      }
      hyperlink_to_uuid(zUuid);
      @ - %w(zCom) by
      hyperlink_to_user(zUser,zDate," on");
      hyperlink_to_date(zDate, ".");
      if( pDownloadName && blob_size(pDownloadName)==0 ){
        blob_append(pDownloadName, zUuid, -1);
      }
      cnt++;
    }
    db_finalize(&q);
  }
  if( cnt==0 ){
    char *zUuid = db_text(0, "SELECT uuid FROM blob WHERE rid=%d", rid);
    @ Control artifact.
    if( pDownloadName && blob_size(pDownloadName)==0 ){
      blob_append(pDownloadName, zUuid, -1);
    }
  }else if( linkToView && g.okHistory ){
    @ <a href="%s(g.zBaseURL)/artifact/%d(rid)">[view]</a>
  }
}


/*
** WEBPAGE: fdiff
**
** Two arguments, v1 and v2, are integers.  Show the difference between
** the two records.
*/
void diff_page(void){
  int v1 = name_to_rid(PD("v1","0"));
  int v2 = name_to_rid(PD("v2","0"));
  Blob c1, c2, diff;

  login_check_credentials();
  if( !g.okRead ){ login_needed(); return; }
  style_header("Diff");
  @ <h2>Differences From:</h2>
  @ <blockquote>
  object_description(v1, 1, 0);
  @ </blockquote>
  @ <h2>To:</h2>
  @ <blockquote>
  object_description(v2, 1, 0);
  @ </blockquote>
  @ <hr>
  @ <blockquote><pre>
  content_get(v1, &c1);
  content_get(v2, &c2);
  blob_zero(&diff);
  text_diff(&c1, &c2, &diff, 4);
  blob_reset(&c1);
  blob_reset(&c2);
  @ %h(blob_str(&diff))
  @ </pre></blockquote>
  blob_reset(&diff);
  style_footer();
}

/*
** WEBPAGE: raw
** URL: /raw?name=ARTIFACTID&m=TYPE
** 
** Return the uninterpreted content of an artifact.  Used primarily
** to view artifacts that are images.
*/
void rawartifact_page(void){
  int rid;
  const char *zMime;
  Blob content;

  rid = name_to_rid(PD("name","0"));
  zMime = PD("m","application/x-fossil-artifact");
  login_check_credentials();
  if( !g.okRead ){ login_needed(); return; }
  if( rid==0 ){ cgi_redirect("/home"); }
  content_get(rid, &content);
  cgi_set_content_type(zMime);
  cgi_set_content(&content);
}

/*
** Render a hex dump of a file.
*/
static void hexdump(Blob *pBlob){
  const unsigned char *x;
  int n, i, j, k;
  char zLine[100];
  static const char zHex[] = "0123456789abcdef";

  x = (const unsigned char*)blob_buffer(pBlob);
  n = blob_size(pBlob);
  for(i=0; i<n; i+=16){
    j = 0;
    zLine[0] = zHex[(i>>24)&0xf];
    zLine[1] = zHex[(i>>16)&0xf];
    zLine[2] = zHex[(i>>8)&0xf];
    zLine[3] = zHex[i&0xf];
    zLine[4] = ':';
    sprintf(zLine, "%04x: ", i);
    for(j=0; j<16; j++){
      k = 5+j*3;
      zLine[k] = ' ';
      if( i+j<n ){
        unsigned char c = x[i+j];
        zLine[k+1] = zHex[c>>4];
        zLine[k+2] = zHex[c&0xf];
      }else{
        zLine[k+1] = ' ';
        zLine[k+2] = ' ';
      }
    }
    zLine[53] = ' ';
    zLine[54] = ' ';
    for(j=0; j<16; j++){
      k = j+55;
      if( i+j<n ){
        unsigned char c = x[i+j];
        if( c>=0x20 && c<=0x7e ){
          zLine[k] = c;
        }else{
          zLine[k] = '.';
        }
      }else{
        zLine[k] = 0;
      }
    }
    zLine[71] = 0;
    @ %h(zLine)
  }
}

/*
** WEBPAGE: hexdump
** URL: /hexdump?name=ARTIFACTID
** 
** Show the complete content of a file identified by ARTIFACTID
** as preformatted text.
*/
void hexdump_page(void){
  int rid;
  Blob content;
  Blob downloadName;
  char *zUuid;

  rid = name_to_rid(PD("name","0"));
  login_check_credentials();
  if( !g.okRead ){ login_needed(); return; }
  if( rid==0 ){ cgi_redirect("/home"); }
  if( g.okAdmin ){
    const char *zUuid = db_text("", "SELECT uuid FROM blob WHERE rid=%d", rid);
    if( db_exists("SELECT 1 FROM shun WHERE uuid='%s'", zUuid) ){
      style_submenu_element("Unshun","Unshun", "%s/shun?uuid=%s&sub=1",
            g.zTop, zUuid);
    }else{
      style_submenu_element("Shun","Shun", "%s/shun?shun=%s#addshun",
            g.zTop, zUuid);
    }
  }
  style_header("Hex Artifact Content");
  zUuid = db_text("?","SELECT uuid FROM blob WHERE rid=%d", rid);
  @ <h2>Artifact %s(zUuid):</h2>
  @ <blockquote>
  blob_zero(&downloadName);
  object_description(rid, 0, &downloadName);
  style_submenu_element("Download", "Download", 
        "%s/raw/%T?name=%d", g.zBaseURL, blob_str(&downloadName), rid);
  @ </blockquote>
  @ <hr>
  content_get(rid, &content);
  @ <blockquote><pre>
  hexdump(&content);
  @ </pre></blockquote>
  style_footer();
}

/*
** WEBPAGE: artifact
** URL: /artifact?name=ARTIFACTID
** 
** Show the complete content of a file identified by ARTIFACTID
** as preformatted text.
*/
void artifact_page(void){
  int rid;
  Blob content;
  const char *zMime;
  Blob downloadName;
  int renderAsWiki = 0;
  int renderAsHtml = 0;
  const char *zUuid;

  rid = name_to_rid(PD("name","0"));
  login_check_credentials();
  if( !g.okRead ){ login_needed(); return; }
  if( rid==0 ){ cgi_redirect("/home"); }
  if( g.okAdmin ){
    const char *zUuid = db_text("", "SELECT uuid FROM blob WHERE rid=%d", rid);
    if( db_exists("SELECT 1 FROM shun WHERE uuid='%s'", zUuid) ){
      style_submenu_element("Unshun","Unshun", "%s/shun?uuid=%s&sub=1",
            g.zTop, zUuid);
    }else{
      style_submenu_element("Shun","Shun", "%s/shun?shun=%s#addshun",
            g.zTop, zUuid);
    }
  }
  style_header("Artifact Content");
  zUuid = db_text("?", "SELECT uuid FROM blob WHERE rid=%d", rid);
  @ <h2>Artifact %s(zUuid)</h2>
  @ <blockquote>
  blob_zero(&downloadName);
  object_description(rid, 0, &downloadName);
  style_submenu_element("Download", "Download", 
          "%s/raw/%T?name=%d", g.zTop, blob_str(&downloadName), rid);
  zMime = mimetype_from_name(blob_str(&downloadName));
  if( zMime ){
    if( strcmp(zMime, "text/html")==0 ){
      if( P("txt") ){
        style_submenu_element("Html", "Html",
                              "%s/artifact?name=%d", g.zTop, rid);
      }else{
        renderAsHtml = 1;
        style_submenu_element("Text", "Text",
                              "%s/artifact?name=%d&txt=1", g.zTop, rid);
      }
    }else if( strcmp(zMime, "application/x-fossil-wiki")==0 ){
      if( P("txt") ){
        style_submenu_element("Wiki", "Wiki",
                              "%s/artifact?name=%d", g.zTop, rid);
      }else{
        renderAsWiki = 1;
        style_submenu_element("Text", "Text",
                              "%s/artifact?name=%d&txt=1", g.zTop, rid);
      }
    }
  }
  @ </blockquote>
  @ <hr>
  content_get(rid, &content);
  if( renderAsWiki ){
    wiki_convert(&content, 0, 0);
  }else if( renderAsHtml ){
    @ <div>
    cgi_append_content(blob_buffer(&content), blob_size(&content));
    @ </div>
  }else{
    zMime = mimetype_from_content(&content);
    @ <blockquote>
    if( zMime==0 ){
      @ <pre>
      @ %h(blob_str(&content))
      @ </pre>
      style_submenu_element("Hex","Hex", "%s/hexdump?name=%d", g.zTop, rid);
    }else if( strncmp(zMime, "image/", 6)==0 ){
      @ <img src="%s(g.zBaseURL)/raw?name=%d(rid)&m=%s(zMime)"></img>
      style_submenu_element("Hex","Hex", "%s/hexdump?name=%d", g.zTop, rid);
    }else{
      @ <pre>
      hexdump(&content);
      @ </pre>
    }
    @ </blockquote>
  }
  style_footer();
}  

/*
** WEBPAGE: tinfo
** URL: /tinfo?name=ARTIFACTID
**
** Show the details of a ticket change control artifact.
*/
void tinfo_page(void){
  int rid;
  Blob content;
  char *zDate;
  const char *zUuid;
  char zTktName[20];
  Manifest m;

  login_check_credentials();
  if( !g.okRdTkt ){ login_needed(); return; }
  rid = name_to_rid(PD("name","0"));
  if( rid==0 ){ fossil_redirect_home(); }
  zUuid = db_text("", "SELECT uuid FROM blob WHERE rid=%d", rid);
  if( g.okAdmin ){
    if( db_exists("SELECT 1 FROM shun WHERE uuid='%s'", zUuid) ){
      style_submenu_element("Unshun","Unshun", "%s/shun?uuid=%s&sub=1",
            g.zTop, zUuid);
    }else{
      style_submenu_element("Shun","Shun", "%s/shun?shun=%s#addshun",
            g.zTop, zUuid);
    }
  }
  content_get(rid, &content);
  if( manifest_parse(&m, &content)==0 ){
    fossil_redirect_home();
  }
  if( m.type!=CFTYPE_TICKET ){
    fossil_redirect_home();
  }
  style_header("Ticket Change Details");
  zDate = db_text(0, "SELECT datetime(%.12f)", m.rDate);
  memcpy(zTktName, m.zTicketUuid, 10);
  zTktName[10] = 0;
  if( g.okHistory ){
    @ <h2>Changes to ticket <a href="%s(m.zTicketUuid)">%s(zTktName)</a></h2>
    @
    @ <p>By %h(m.zUser) on %s(zDate).  See also:
    @ <a href="%s(g.zTop)/artifact/%T(zUuid)">artifact content</a>, and
    @ <a href="%s(g.zTop)/tkthistory/%s(m.zTicketUuid)">ticket history</a>
    @ </p>
  }else{
    @ <h2>Changes to ticket %s(zTktName)</h2>
    @
    @ <p>By %h(m.zUser) on %s(zDate).
    @ </p>
  }
  @
  @ <ol>
  free(zDate);
  ticket_output_change_artifact(&m);
  manifest_clear(&m);
  style_footer();
}


/*
** WEBPAGE: info
** URL: info/ARTIFACTID
**
** The argument is a artifact ID which might be a baseline or a file or
** a ticket changes or a wiki editor or something else. 
**
** Figure out what the artifact ID is and jump to it.
*/
void info_page(void){
  const char *zName;
  Blob uuid;
  int rid;
  
  zName = P("name");
  if( zName==0 ) fossil_redirect_home();
  if( validate16(zName, strlen(zName))
   && db_exists("SELECT 1 FROM ticket WHERE tkt_uuid LIKE '%q%%'", zName) ){
    tktview_page();
    return;
  }
  blob_set(&uuid, zName);
  if( name_to_uuid(&uuid, 1) ){
    fossil_redirect_home();
  }
  zName = blob_str(&uuid);
  rid = db_int(0, "SELECT rid FROM blob WHERE uuid='%s'", zName);
  if( rid==0 ){
    style_header("Broken Link");
    @ <p>No such object: %h(zName)</p>
    style_footer();
    return;
  }
  if( db_exists("SELECT 1 FROM mlink WHERE mid=%d", rid) ){
    ci_page();
  }else
  if( db_exists("SELECT 1 FROM tagxref JOIN tag USING(tagid)"
                " WHERE rid=%d AND tagname LIKE 'wiki-%%'", rid) ){
    winfo_page();
  }else
  if( db_exists("SELECT 1 FROM tagxref JOIN tag USING(tagid)"
                " WHERE rid=%d AND tagname LIKE 'tkt-%%'", rid) ){
    tinfo_page();
  }else
  if( db_exists("SELECT 1 FROM plink WHERE cid=%d", rid) ){
    ci_page();
  }else
  if( db_exists("SELECT 1 FROM plink WHERE pid=%d", rid) ){
    ci_page();
  }else
  {
    artifact_page();
  }
}

/*
** WEBPAGE: ci_edit
** URL:  ci_edit?r=RID&c=NEWCOMMENT&u=NEWUSER
**
** Present a dialog for updating properties of a baseline:
**
**     *  The check-in user
**     *  The check-in comment
**     *  The background color.
*/
void ci_edit_page(void){
  int rid;
  const char *zComment;         /* Current comment on the check-in */
  const char *zNewComment;      /* Revised check-in comment */
  const char *zUser;            /* Current user for the check-in */
  const char *zNewUser;         /* Revised user */
  const char *zDate;            /* Current date of the check-in */
  const char *zNewDate;         /* Revised check-in date */
  const char *zColor;       
  const char *zNewColor;
  const char *zNewTagFlag;
  const char *zNewTag;
  const char *zNewBrFlag;
  const char *zNewBranch;
  const char *zCloseFlag;
  int fPropagateColor;
  char *zUuid;
  Blob comment;
  Stmt q;
  static const struct SampleColors {
     const char *zCName;
     const char *zColor;
  } aColor[] = {
     { "(none)",  "" },
     { "#f2dcdc", "#f2dcdc" },
     { "#f0ffc0", "#f0ffc0" },
     { "#bde5d6", "#bde5d6" },
     { "#c0ffc0", "#c0ffc0" },
     { "#c0fff0", "#c0fff0" },
     { "#c0f0ff", "#c0f0ff" },
     { "#d0c0ff", "#d0c0ff" },
     { "#ffc0ff", "#ffc0ff" },
     { "#ffc0d0", "#ffc0d0" },
     { "#fff0c0", "#fff0c0" },
     { "#c0c0c0", "#c0c0c0" },
  };
  int nColor = sizeof(aColor)/sizeof(aColor[0]);
  int i;
  
  login_check_credentials();
  if( !g.okWrite ){ login_needed(); return; }
  rid = atoi(PD("r","0"));
  zUuid = db_text(0, "SELECT uuid FROM blob WHERE rid=%d", rid);
  zComment = db_text(0, "SELECT coalesce(ecomment,comment)"
                        "  FROM event WHERE objid=%d", rid);
  if( zComment==0 ) fossil_redirect_home();
  if( P("cancel") ){
    cgi_redirectf("ci?name=%d", rid);
  }
  zNewComment = PD("c",zComment);
  zUser = db_text(0, "SELECT coalesce(euser,user)"
                     "  FROM event WHERE objid=%d", rid);
  if( zUser==0 ) fossil_redirect_home();
  zNewUser = PD("u",zUser);
  zDate = db_text(0, "SELECT datetime(mtime)"
                     "  FROM event WHERE objid=%d", rid);
  if( zDate==0 ) fossil_redirect_home();
  zNewDate = PD("dt",zDate);
  zColor = db_text("", "SELECT bgcolor"
                        "  FROM event WHERE objid=%d", rid);
  zNewColor = PD("clr",zColor);
  fPropagateColor = P("pclr")!=0;
  zNewTagFlag = P("newtag") ? " checked" : "";
  zNewTag = PD("tagname","");
  zNewBrFlag = P("newbr") ? " checked" : "";
  zNewBranch = PD("brname","");
  zCloseFlag = P("close") ? " checked" : "";
  if( P("apply") ){
    Blob ctrl;
    char *zDate;
    int nChng = 0;

    login_verify_csrf_secret();
    blob_zero(&ctrl);
    zDate = db_text(0, "SELECT datetime('now')");
    zDate[10] = 'T';
    blob_appendf(&ctrl, "D %s\n", zDate);
    db_multi_exec("CREATE TEMP TABLE newtags(tag UNIQUE, prefix, value)");
    if( zNewColor[0] && strcmp(zColor,zNewColor)!=0 ){
      char *zPrefix = "+";
      if( fPropagateColor ){
        zPrefix = "*";
      }
      db_multi_exec("REPLACE INTO newtags VALUES('bgcolor',%Q,%Q)",
                    zPrefix, zNewColor);
    }
    if( zNewColor[0]==0 && zColor[0]!=0 ){
      db_multi_exec("REPLACE INTO newtags VALUES('bgcolor','-',NULL)");
    }
    if( strcmp(zComment,zNewComment)!=0 ){
      db_multi_exec("REPLACE INTO newtags VALUES('comment','+',%Q)",
                    zNewComment);
    }
    if( strcmp(zDate,zNewDate)!=0 ){
      db_multi_exec("REPLACE INTO newtags VALUES('date','+',%Q)",
                    zNewDate);
    }
    if( strcmp(zUser,zNewUser)!=0 ){
      db_multi_exec("REPLACE INTO newtags VALUES('user','+',%Q)", zNewUser);
    }
    db_prepare(&q,
       "SELECT tag.tagid, tagname FROM tagxref, tag"
       " WHERE tagxref.rid=%d AND tagtype>0 AND tagxref.tagid=tag.tagid",
       rid
    );
    while( db_step(&q)==SQLITE_ROW ){
      int tagid = db_column_int(&q, 0);
      const char *zTag = db_column_text(&q, 1);
      char zLabel[30];
      sprintf(zLabel, "c%d", tagid);
      if( P(zLabel) ){
        db_multi_exec("REPLACE INTO newtags VALUES(%Q,'-',NULL)", zTag);
      }
    }
    db_finalize(&q);
    if( zCloseFlag[0] ){
      db_multi_exec("REPLACE INTO newtags VALUES('closed','+',NULL)");
    }
    if( zNewTagFlag[0] ){
      db_multi_exec("REPLACE INTO newtags VALUES('sym-%q','+',NULL)", zNewTag);
    }
    if( zNewBrFlag[0] ){
      db_multi_exec(
        "REPLACE INTO newtags "
        " SELECT tagname, '-', NULL FROM tagxref, tag"
        "  WHERE tagxref.rid=%d AND tagtype==2"
        "    AND tagname GLOB 'sym-*'"
        "    AND tag.tagid=tagxref.tagid",
        rid
      );
      db_multi_exec("REPLACE INTO newtags VALUES('branch','*',%Q)", zNewBranch);
      db_multi_exec("REPLACE INTO newtags VALUES('sym-%q','*',NULL)",
                    zNewBranch);
    }
    db_prepare(&q, "SELECT tag, prefix, value FROM newtags"
                   " ORDER BY prefix || tag");
    while( db_step(&q)==SQLITE_ROW ){
      const char *zTag = db_column_text(&q, 0);
      const char *zPrefix = db_column_text(&q, 1);
      const char *zValue = db_column_text(&q, 2);
      nChng++;
      if( zValue ){
        blob_appendf(&ctrl, "T %s%F %s %F\n", zPrefix, zTag, zUuid, zValue);
      }else{
        blob_appendf(&ctrl, "T %s%F %s\n", zPrefix, zTag, zUuid);
      }
    }
    db_finalize(&q);
    if( nChng>0 ){
      int nrid;
      Blob cksum;
      blob_appendf(&ctrl, "U %F\n", g.zLogin);
      md5sum_blob(&ctrl, &cksum);
      blob_appendf(&ctrl, "Z %b\n", &cksum);
      db_begin_transaction();
      g.markPrivate = content_is_private(rid);
      nrid = content_put(&ctrl, 0, 0);
      manifest_crosslink(nrid, &ctrl);
      db_end_transaction(0);
    }
    cgi_redirectf("ci?name=%d", rid);
  }
  blob_zero(&comment);
  blob_append(&comment, zNewComment, -1);
  zUuid[10] = 0;
  style_header("Edit Check-in [%s]", zUuid);
  if( P("preview") ){
    Blob suffix;
    int nTag = 0;
    @ <b>Preview:</b>
    @ <blockquote>
    @ <table border=0>
    if( zNewColor && zNewColor[0] ){
      @ <tr><td bgcolor="%h(zNewColor)">
    }else{
      @ <tr><td>
    }
    wiki_convert(&comment, 0, WIKI_INLINE);
    blob_zero(&suffix);
    blob_appendf(&suffix, "(user: %h", zNewUser);
    db_prepare(&q, "SELECT substr(tagname,5) FROM tagxref, tag"
                   " WHERE tagname GLOB 'sym-*' AND tagxref.rid=%d"
                   "   AND tagtype>1 AND tag.tagid=tagxref.tagid",
                   rid);
    while( db_step(&q)==SQLITE_ROW ){
      const char *zTag = db_column_text(&q, 0);
      if( nTag==0 ){
        blob_appendf(&suffix, ", tags: %h", zTag);
      }else{
        blob_appendf(&suffix, ", %h", zTag);
      }
      nTag++;
    }
    db_finalize(&q);
    blob_appendf(&suffix, ")");
    @ %s(blob_str(&suffix))
    @ </td></tr></table>
    @ </blockquote>
    @ <hr>
    blob_reset(&suffix);
  }
  @ <p>Make changes to attributes of check-in
  @ [<a href="ci?name=%d(rid)">%s(zUuid)</a>]:</p>
  @ <form action="%s(g.zBaseURL)/ci_edit" method="POST">
  login_insert_csrf_secret();
  @ <input type="hidden" name="r" value="%d(rid)">
  @ <table border="0" cellspacing="10">

  @ <tr><td align="right" valign="top"><b>User:</b></td>
  @ <td valign="top">
  @   <input type="text" name="u" size="20" value="%h(zNewUser)">
  @ </td></tr>

  @ <tr><td align="right" valign="top"><b>Comment:</b></td>
  @ <td valign="top">
  @ <textarea name="c" rows="10" cols="80">%h(zNewComment)</textarea>
  @ </td></tr>

  @ <tr><td align="right" valign="top"><b>Check-in Time:</b></td>
  @ <td valign="top">
  @   <input type="text" name="dt" size="20" value="%h(zNewDate)">
  @ </td></tr>

  @ <tr><td align="right" valign="top"><b>Background Color:</b></td>
  @ <td valign="top">
  @ <table border=0 cellpadding=0 cellspacing=1>
  @ <tr><td colspan="6" align="left">
  if( fPropagateColor ){
    @ <input type="checkbox" name="pclr" checked>
  }else{
    @ <input type="checkbox" name="pclr">
  }
  @ Propagate color to descendants</input></td></tr>
  @ <tr>
  for(i=0; i<nColor; i++){
    if( aColor[i].zColor[0] ){
      @ <td bgcolor="%h(aColor[i].zColor)">
    }else{
      @ <td>
    }
    if( strcmp(zNewColor, aColor[i].zColor)==0 ){
      @ <input type="radio" name="clr" value="%h(aColor[i].zColor)" checked>
    }else{
      @ <input type="radio" name="clr" value="%h(aColor[i].zColor)">
    }
    @ %h(aColor[i].zCName)</input></td>
    if( (i%6)==5 && i+1<nColor ){
      @ </tr><tr>
    }
  }
  @ </tr>
  @ </table>
  @ </td></tr>

  @ <tr><td align="right" valign="top"><b>Tags:</b></td>
  @ <td valign="top">
  @ <input type="checkbox" name="newtag"%s(zNewTagFlag)>
  @ Add the following new tag name to this check-in:
  @ <input type="text" width="15" name="tagname" value="%h(zNewTag)">
  db_prepare(&q,
     "SELECT tag.tagid, tagname FROM tagxref, tag"
     " WHERE tagxref.rid=%d AND tagtype>0 AND tagxref.tagid=tag.tagid"
     " ORDER BY CASE WHEN tagname GLOB 'sym-*' THEN substr(tagname,5)"
     "               ELSE tagname END",
     rid
  );
  while( db_step(&q)==SQLITE_ROW ){
    int tagid = db_column_int(&q, 0);
    const char *zTagName = db_column_text(&q, 1);
    char zLabel[30];
    sprintf(zLabel, "c%d", tagid);
    if( P(zLabel) ){
      @ <br><input type="checkbox" name="c%d(tagid)" checked>
    }else{
      @ <br><input type="checkbox" name="c%d(tagid)">
    }
    if( strncmp(zTagName, "sym-", 4)==0 ){
      @ Cancel tag <b>%h(&zTagName[4])</b>
    }else{
      @ Cancel special tag <b>%h(zTagName)</b>
    }
  }
  db_finalize(&q);
  @ </td></tr>

  if( db_exists("SELECT 1 FROM tagxref WHERE rid=%d AND tagid=%d AND srcid>0",
                rid, TAG_BRANCH)==0 ){
    @ <tr><td align="right" valign="top"><b>Branching:</b></td>
    @ <td valign="top">
    @ <input type="checkbox" name="newbr"%s(zNewBrFlag)>
    @ Make this check-in the start of a new branch named:
    @ <input type="text" width="15" name="brname" value="%h(zNewBranch)">
    @ </td></tr>
  }

  if( is_a_leaf(rid)
   && !db_exists("SELECT 1 FROM tagxref "
                 " WHERE tagid=%d AND rid=%d AND tagtype>0",
                 TAG_CLOSED, rid)
  ){
    @ <tr><td align="right" valign="top"><b>Leaf Closure:</b></td>
    @ <td valign="top">
    @ <input type="checkbox" name="close"%s(zCloseFlag)>
    @ Mark this leaf as "closed" so that it no longer appears on the
    @ "leaves" page and is no longer labeled as a "<b>Leaf</b>".
    @ </td></tr>
  }


  @ <tr><td colspan="2">
  @ <input type="submit" name="preview" value="Preview">
  @ <input type="submit" name="apply" value="Apply Changes">
  @ <input type="submit" name="cancel" value="Cancel">
  @ </td></tr>
  @ </table>
  @ </form>
  style_footer();
}