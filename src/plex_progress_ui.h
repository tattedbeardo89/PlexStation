// SPDX-License-Identifier: GPL-3.0-or-later
ProgressOutbox outbox;
ListeningStore progress_disk;
bool progress_disk_loaded=false;
std::string outbox_scope,timeline_scope;
ProgressUpdate timeline_update;
void save_outbox(){if(!outbox_scope.empty() && !progress_disk.put(outbox_scope,"outbox",outbox.json()))message="Progress retry storage failed; queued updates remain in this session.";}
void load_outbox(){
    if(!progress_disk_loaded){progress_disk.load(std::string(plat_data_dir())+"/plexstation-progress.json");progress_disk_loaded=true;}
    outbox_scope=account_id+"/"+selected_server.id;
    outbox.restore(progress_disk.get(outbox_scope,"outbox"),plat_time());
}
void record_progress(const Item &item,const std::string &state,int64_t position){
    if(account_id.empty() || selected_server.id.empty() || item.id.empty())return;
    if(outbox_scope!=account_id+"/"+selected_server.id)load_outbox();
    if(!outbox.put(item,state,position,static_cast<int64_t>(std::time(nullptr)))){message="Progress retry queue is full; reconnect to save pending updates.";return;}
    save_outbox();
}
void capture_timeline(const Item &item,const std::string &state,int64_t position){
    timeline_scope=outbox_scope;timeline_update={item,state,position,0,0,0,0};
    for(const auto &u:outbox.entries)if(u.item.id==item.id){timeline_update=u;break;}
}
void timeline_complete(const Result &r){
    if(timeline_scope!=outbox_scope)return;
    if(r)outbox.ack(timeline_update.item.id,timeline_update.sequence);
    else {outbox.failed(timeline_update.item.id,timeline_update.sequence,plat_time());message="Progress is queued locally; retry after reconnect.";}
    save_outbox();
}
void retry_progress(){
    if(job!=None || progress_pending || account_id.empty() || base.empty() || outbox_scope!=account_id+"/"+selected_server.id)return;
    auto it=std::find_if(outbox.entries.begin(),outbox.entries.end(),[](const ProgressUpdate &u){return u.attempts>0 && u.due<=plat_time();});
    // Restored updates also need a fresh authorized metadata read before writing.
    if(it==outbox.entries.end())it=std::find_if(outbox.entries.begin(),outbox.entries.end(),[](const ProgressUpdate &u){return u.due>0 && u.due<=plat_time();});
    if(it==outbox.entries.end())return;
    auto update=*it;timeline_update=update;timeline_scope=outbox_scope;auto b=base,t=server_token;
    submit(Timeline,[update,b,t]{
        auto meta=client->call("GET",b,"/library/metadata/"+encode(update.item.id),t);if(!meta)return meta;
        auto entries=items(meta.data,false);if(entries.empty() || entries[0].id!=update.item.id)return Result{{},"Progress target no longer available",0};
        if(entries[0].last_played>update.updated)return Result{Json{{"superseded",true}},"",200};
        return client->timeline(b,t,update.item,update.state,update.position);
    });
}
